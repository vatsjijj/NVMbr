#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/compiler.h"
#include "include/common.h"
#include "include/scanner.h"
#include "include/memory.h"

#ifdef DEBUG_PRINT_CODE
#include "include/debug.h"
#endif

typedef struct {
  Token current;
  Token prev;
  bool has_error;
  bool panic;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGN,
  PREC_OR,
  PREC_AND,
  PREC_EQU,
  PREC_COMP,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY,
} Prec;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Prec prec;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool is_captured;
} Local;

typedef struct {
  uint8_t index;
  bool is_local;
} Upval;

typedef enum {
  TYPE_FUNC,
  TYPE_INIT,
  TYPE_METHOD,
  TYPE_SCRIPT,
} FuncType;

typedef struct Compiler {
  struct Compiler* enclosing;
  ObjFunc* function;
  FuncType type;
  Local locals[UINT8_COUNT];
  int local_count;
  Upval upvals[UINT8_COUNT];
  int scope_depth;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool has_superclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* current_class = NULL;

static Chunk* current_chunk() {
  return &current->function->chunk;
}

static void error_at(Token* token, const char* message) {
  if (parser.panic) return;

  parser.panic = true;

  fprintf(stderr, "[ line %d ] Err", token->line);

  if (token->type == T_EOS) {
    fprintf(stderr, " at end");
  }
  else if (token->type == T_ERR) {
    // Literally nothing. :)
  }
  else {
    fprintf(stderr, " at `%.*s`", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.has_error = true;
}

static void error(const char* message) {
  error_at(&parser.prev, message);
}

static void err_at_curr(const char* message) {
  error_at(&parser.current, message);
}

static void adv() {
  parser.prev = parser.current;

  for (;;) {
    parser.current = scan_token();

    if (parser.current.type != T_ERR) break;

    err_at_curr(parser.current.start);
  }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    adv();
    return;
  }
  err_at_curr(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  adv();

  return true;
}

static void rel_byte(uint8_t byte) {
  write_chunk(current_chunk(), byte, parser.prev.line);
}

static void rel_bytes(uint8_t byte1, uint8_t byte2) {
  rel_byte(byte1);
  rel_byte(byte2);
}

static void rel_return() {
  if (current->type == TYPE_INIT) {
    rel_bytes(OP_GET_LOCAL, 0);
  }
  else {
    rel_byte(OP_NIL);
  }
  rel_byte(OP_RETURN);
}

static uint8_t make_const(Value value) {
  int constant = add_const(current_chunk(), value);

  if (constant > UINT8_MAX) {
    error("Too many consts in one chunk.");
  }

  return (uint8_t)constant;
}

static void rel_const(Value value) {
  rel_bytes(OP_CONSTANT, make_const(value));
}

static void patch_jump(int offset) {
  int jump = current_chunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much to jump over.");
  }

  current_chunk()->code[offset] = (jump >> 8) & 0xff;
  current_chunk()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(Compiler* compiler, FuncType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->local_count = 0;
  compiler->scope_depth = 0;
  compiler->function = new_func();

  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name = copy_string(parser.prev.start, parser.prev.length);
  }

  Local* local = &current->locals[current->local_count++];
  local->depth = 0;
  local->is_captured = false;

  if (type != TYPE_FUNC) {
    local->name.start = "this";
    local->name.length = 4;
  }
  else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static ObjFunc* end_compiler() {
  rel_return();

  ObjFunc* function = current->function;

  #ifdef DEBUG_PRINT_CODE
    if (!parser.has_error) {
      disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
    }
  #endif

  current = current->enclosing;
  return function;
}

static void begin_scope() {
  current->scope_depth++;
}

static void end_scope() {
  current->scope_depth--;

  while (current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth) {
    if (current->locals[current->local_count - 1].is_captured) {
      rel_byte(OP_CLOSE_UPVAL);
    }
    else {
      rel_byte(OP_POP);
    }
    current->local_count--;
  }
}

static void expr();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_prec(Prec prec);

static uint8_t ident_const(Token* name) {
  return make_const(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool ident_equ(Token* a, Token* b) {
  if (a->length != b->length) return false;

  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler* compiler, Token* name) {
  for (int i = compiler->local_count - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];

    if (ident_equ(name, &local->name)) {
      if (local->depth == -1) {
        error("Cannot read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static int add_upval(Compiler* compiler, uint8_t index, bool is_local) {
  int upval_count = compiler->function->upval_count;

  for (int i = 0; i < upval_count; i++) {
    Upval* upval = &compiler->upvals[i];

    if (upval->index == index && upval->is_local == is_local) {
      return i;
    }
  }

  if (upval_count == UINT8_COUNT) {
    error("Too many closure variables in function.");

    return 0;
  }

  compiler->upvals[upval_count].is_local = is_local;
  compiler->upvals[upval_count].index = index;

  return compiler->function->upval_count++;
}

static int resolve_upval(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolve_local(compiler->enclosing, name);

  if (local != -1) {
    compiler->enclosing->locals[local].is_captured = true;

    return add_upval(compiler, (uint8_t)local, true);
  }

  int upval = resolve_upval(compiler->enclosing, name);

  if (upval != -1) {
    return add_upval(compiler, (uint8_t)upval, false);
  }
  return -1;
}

static void add_local(Token name) {
  if (current->local_count == UINT8_COUNT) {
    error("Too many local variables in function.");

    return;
  }

  Local* local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
  local->is_captured = false;
}

static void declare_var() {
  if (current->scope_depth == 0) return;

  Token* name = &parser.prev;

  for (int i = current->local_count - 1; i >= 0; i--) {
    Local* local = &current->locals[i];

    if (local->depth != -1 && local->depth < current->scope_depth) {
      break;
    }

    if (ident_equ(name, &local->name)) {
      error("There is a duplicate variable in this scope.");
    }
  }
  add_local(*name);
}

static uint8_t parse_var(const char* err_message) {
  consume(T_IDENT, err_message);

  declare_var();
  if (current->scope_depth > 0) return 0;

  return ident_const(&parser.prev);
}

static void mark_init() {
  if (current->scope_depth == 0) return;

  current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void def_var(uint8_t global) {
  if (current->scope_depth > 0) {
    mark_init();
    return;
  }
  rel_bytes(OP_DEF_GLOBAL, global);
}

static uint8_t argument_list() {
  uint8_t arg_count = 0;
  if (!check(T_RPAREN)) {
    do {
      expr();

      if (arg_count == 255) {
        error("Cannot have more than 255 arguments.");
      }

      arg_count++;
    }
    while (match(T_COMMA));
  }
  consume(T_RPAREN, "Expected `)` after arguments.");

  return arg_count;
}

static int rel_jump(uint8_t instruct) {
  rel_byte(instruct);
  rel_byte(0xff);
  rel_byte(0xff);

  return current_chunk()->count - 2;
}

static void and_(bool can_assign) {
  int end_jump = rel_jump(OP_JUMP_IF_FALSE);

  rel_byte(OP_POP);
  parse_prec(PREC_AND);

  patch_jump(end_jump);
}

static void bin(bool can_assign) {
  TokenType op_type = parser.prev.type;
  ParseRule* rule = get_rule(op_type);
  parse_prec((Prec)(rule->prec + 1));

  switch (op_type) {
    case T_BANG_EQU:    rel_bytes(OP_EQU, OP_NOT); break;
    case T_EQU_EQU:     rel_byte(OP_EQU); break;
    case T_GREATER:     rel_byte(OP_GREATER); break;
    case T_GREATER_EQU: rel_bytes(OP_LESS, OP_NOT); break;
    case T_LESS:        rel_byte(OP_LESS); break;
    case T_LESS_EQU:    rel_bytes(OP_GREATER, OP_NOT); break;
    case T_PLUS:        rel_byte(OP_ADD); break;
    case T_MINUS:       rel_byte(OP_SUB); break;
    case T_STAR:        rel_byte(OP_MUL); break;
    case T_SLASH:       rel_byte(OP_DIV); break;
    default: return;
  }
}

static void call(bool can_assign) {
  uint8_t arg_count = argument_list();
  rel_bytes(OP_CALL, arg_count);
}

static void colon(bool can_assign) {
  consume(T_IDENT, "Expected property name after `:`.");

  uint8_t name = ident_const(&parser.prev);

  if (can_assign && match(T_LARROW)) {
    expr();
    rel_bytes(OP_SET_PROP, name);
  }
  else if (match(T_LPAREN)) {
    uint8_t arg_count = argument_list();

    rel_bytes(OP_INVOKE, name);
    rel_byte(arg_count);
  }
  else {
    rel_bytes(OP_GET_PROP, name);
  }
}

static void literal(bool can_assign) {
  switch (parser.prev.type) {
    case T_FALSE: rel_byte(OP_FALSE); break;
    case T_NIL: rel_byte(OP_NIL); break;
    case T_TRUE: rel_byte(OP_TRUE); break;
    default: return;
  }
}

static void group(bool can_assign) {
  expr();
  consume(T_RPAREN, "Expected `)` after expression.");
}

static void num(bool can_assign) {
  double value = strtod(parser.prev.start, NULL);

  rel_const(NUM_VAL(value));
}

static void or_(bool can_assign) {
  int else_jump = rel_jump(OP_JUMP_IF_FALSE);
  int end_jump = rel_jump(OP_JUMP);

  patch_jump(else_jump);
  rel_byte(OP_POP);

  parse_prec(PREC_OR);
  patch_jump(end_jump);
}

static void string(bool can_assign) {
  rel_const(OBJ_VAL(copy_string(parser.prev.start + 1, parser.prev.length - 2)));
}

static void named_variable(Token name, bool can_assign) {
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);

  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  }
  else if ((arg = resolve_upval(current, &name)) != -1) {
    get_op = OP_GET_UPVAL;
    set_op = OP_SET_UPVAL;
  }
  else {
    arg = ident_const(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }

  if (can_assign && match(T_LARROW)) {
    expr();
    rel_bytes(set_op, (uint8_t)arg);
  }
  else {
    rel_bytes(get_op, (uint8_t)arg);
  }
}

static void variable(bool can_assign) {
  named_variable(parser.prev, can_assign);
}

static Token synth_token(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);

  return token;
}

static void super_(bool can_assign) {

  if (current_class == NULL) {
    error("Cannot use `super` outside of a class.");
  }
  else if (!current_class->has_superclass) {
    error("Cannot use `super` inside of a class with no superclass.");
  }

  consume(T_COLON, "Expected `:` after `super`.");
  consume(T_IDENT, "Expected a superclass method name.");

  uint8_t name = ident_const(&parser.prev);

  named_variable(synth_token("this"), false);

  if (match(T_LPAREN)) {
    uint8_t arg_count = argument_list();

    named_variable(synth_token("super"), false);

    rel_bytes(OP_INVOKE_SUPER, name);
    rel_byte(arg_count);
  }
  else {
    named_variable(synth_token("super"), false);
    rel_bytes(OP_GET_SUPER, name);
  }
}

static void this_(bool can_assign) {
  if (current_class == NULL) {
    error("Using `this` outside of a class is not allowed.");
    return;
  }
  variable(false);
}

static void unary(bool can_assign) {
  TokenType op_type = parser.prev.type;

  parse_prec(PREC_UNARY);

  switch (op_type) {
    case T_BANG: rel_byte(OP_NOT); break;
    case T_MINUS: rel_byte(OP_NEGATE); break;
    default: return;
  }
}

ParseRule rules[] = {
  [T_LPAREN]        = {group,    call,   PREC_CALL},
  [T_RPAREN]        = {NULL,     NULL,   PREC_NONE},
  [T_LBRACE]        = {NULL,     NULL,   PREC_NONE},
  [T_RBRACE]        = {NULL,     NULL,   PREC_NONE},
  [T_LBRACK]        = {NULL,     NULL,   PREC_NONE},
  [T_RBRACK]        = {NULL,     NULL,   PREC_NONE},
  [T_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [T_DOT]           = {NULL,     NULL,   PREC_NONE},
  [T_MINUS]         = {unary,    bin,    PREC_TERM},
  [T_PLUS]          = {NULL,     bin,    PREC_TERM},
  [T_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [T_COLON]         = {NULL,     colon,  PREC_CALL},
  [T_SLASH]         = {NULL,     bin,    PREC_FACTOR},
  [T_STAR]          = {NULL,     bin,    PREC_FACTOR},
  [T_BANG]          = {unary,    NULL,   PREC_NONE},
  [T_BANG_EQU]      = {NULL,     bin,    PREC_EQU},
  [T_EQU]           = {NULL,     NULL,   PREC_NONE},
  [T_TILDE]         = {NULL,     NULL,   PREC_NONE},
  [T_LARROW]        = {NULL,     NULL,   PREC_NONE},
  [T_RARROW]        = {NULL,     NULL,   PREC_NONE},
  [T_EQU_EQU]       = {NULL,     bin,    PREC_EQU},
  [T_GREATER]       = {NULL,     bin,    PREC_COMP},
  [T_GREATER_EQU]   = {NULL,     bin,    PREC_COMP},
  [T_LESS]          = {NULL,     bin,    PREC_COMP},
  [T_LESS_EQU]      = {NULL,     bin,    PREC_COMP},
  [T_IDENT]         = {variable, NULL,   PREC_NONE},
  [T_STRING]        = {string,   NULL,   PREC_NONE},
  [T_NUM]           = {num,      NULL,   PREC_NONE},
  [T_AND]           = {NULL,     and_,   PREC_AND},
  [T_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [T_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [T_FALSE]         = {literal,  NULL,   PREC_NONE},
  [T_FOR]           = {NULL,     NULL,   PREC_NONE},
  [T_FUN]           = {NULL,     NULL,   PREC_NONE},
  [T_IF]            = {NULL,     NULL,   PREC_NONE},
  [T_NIL]           = {literal,  NULL,   PREC_NONE},
  [T_OR]            = {NULL,     or_,    PREC_OR},
  [T_DO]            = {NULL,     NULL,   PREC_NONE},
  [T_END]           = {NULL,     NULL,   PREC_NONE},
  [T_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [T_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [T_SUPER]         = {super_,   NULL,   PREC_NONE},
  [T_THIS]          = {this_,    NULL,   PREC_NONE},
  [T_TRUE]          = {literal,  NULL,   PREC_NONE},
  [T_VAR]           = {NULL,     NULL,   PREC_NONE},
  [T_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [T_ERR]           = {NULL,     NULL,   PREC_NONE},
  [T_EOS]           = {NULL,     NULL,   PREC_NONE},
};

static void parse_prec(Prec prec) {
  adv();
  ParseFn prefix_rule = get_rule(parser.prev.type)->prefix;

  if (prefix_rule == NULL) {
    error("Expected expression.");
    return;
  }

  bool can_assign = prec <= PREC_ASSIGN;
  prefix_rule(can_assign);

  while (prec <= get_rule(parser.current.type)->prec) {
    adv();
    ParseFn infix_rule = get_rule(parser.prev.type)->infix;
    infix_rule(can_assign);
  }

  if (can_assign && match(T_LARROW)) {
    error("Invalid assignment target.");
  }
}

static ParseRule* get_rule(TokenType type) {
  return &rules[type];
}

static void expr() {
  parse_prec(PREC_ASSIGN);
}

static void block() {
  while (!check(T_END) && !check(T_EOS)) {
    declaration();
  }
  consume(T_END, "Expected `end` after `do`.");
}

static void function(FuncType type) {
  Compiler compiler;
  init_compiler(&compiler, type);

  begin_scope();

  consume(T_LPAREN, "Expected `(` after function name.");
  if (!check(T_RPAREN)) {
    do {
      current->function->arity++;

      if (current->function->arity > 255) {
        err_at_curr("Cannot have more than 255 parameters.");
      }

      uint8_t constant = parse_var("Expected variable name.");
      def_var(constant);
    }
    while (match(T_COMMA));
  }
  consume(T_RPAREN, "Expected `)` after parameters.");
  consume(T_RARROW, "Expected `->` before the function body.");

  block();

  ObjFunc* function = end_compiler();
  rel_bytes(OP_CLOSURE, make_const(OBJ_VAL(function)));

  for (int i = 0; i < function->upval_count; i++) {
    rel_byte(compiler.upvals[i].is_local ? 1 : 0);
    rel_byte(compiler.upvals[i].index);
  }
}

static void method() {
  consume(T_IDENT, "Expected a named method.");
  uint8_t constant = ident_const(&parser.prev);

  FuncType type = TYPE_METHOD;

  if (parser.prev.length == 4 && memcmp(parser.prev.start, "init", 4) == 0) {
    type = TYPE_INIT;
  }
  function(type);
  rel_bytes(OP_METHOD, constant);
}

static void class_decl() {
  consume(T_IDENT, "Expected a named class.");
  Token class_name = parser.prev;
  uint8_t name_const = ident_const(&parser.prev);

  declare_var();

  rel_bytes(OP_CLASS, name_const);
  def_var(name_const);

  ClassCompiler class_compiler;
  class_compiler.has_superclass = false;
  class_compiler.enclosing = current_class;
  current_class = &class_compiler;

  if (match(T_LESS)) {
    consume(T_IDENT, "Expected a named superclass.");
    variable(false);

    if (ident_equ(&class_name, &parser.prev)) {
      error("Classes cannot inherit from themself.");
    }

    begin_scope();

    add_local(synth_token("super"));
    def_var(0);
    named_variable(class_name, false);
    rel_byte(OP_INHERIT);

    class_compiler.has_superclass = true;
  }

  named_variable(class_name, false);

  consume(T_LBRACK, "Expected `[` before class body.");

  while (!check(T_RBRACK) && !check(T_EOS)) {
    method();
  }

  consume(T_RBRACK, "Expected `]` after class body.");
  rel_byte(OP_POP);

  if  (class_compiler.has_superclass) {
    end_scope();
  }
  current_class = current_class->enclosing;
}

static void func_decl() {
  uint8_t global = parse_var("Expected a named function.");

  mark_init();
  function(TYPE_FUNC);

  def_var(global);
}

static void decl_var() {
  uint8_t global = parse_var("Expected variable name.");

  if (match(T_LARROW)) {
    expr();
  }
  else {
    rel_byte(OP_NIL);
  }
  consume(T_DOT, "Expected `.` after variable declaration.");
  def_var(global);
}

static void expr_statement() {
  expr();
  consume(T_DOT, "Expected `.` after expression.");
  rel_byte(OP_POP);
}

static void if_statement() {
  consume(T_LPAREN, "Expected `(` after `if`.");
  expr();
  consume(T_RPAREN, "Expected `)` after condition.");

  int go_jump = rel_jump(OP_JUMP_IF_FALSE);
  rel_byte(OP_POP);
  statement();

  int else_jump = rel_jump(OP_JUMP);

  patch_jump(go_jump);
  rel_byte(OP_POP);

  if (match(T_ELSE)) statement();
  patch_jump(else_jump);
}

static void print_statement() {
  expr();
  consume(T_DOT, "Expected `.` after value.");
  rel_byte(OP_PRINT);
}

static void return_statement() {
  if (current->type == TYPE_SCRIPT) {
    error("Cannot return from top-level.");
  }

  if (match(T_DOT)) {
    rel_return();
  }
  else {
    if (current->type == TYPE_INIT) {
      error("Cannot return a value from an initializer.");
    }
    expr();
    consume(T_DOT, "Expected `.` after return value.");
    rel_byte(OP_RETURN);
  }
}

static void sync() {
  parser.panic = false;

  while (parser.current.type != T_EOS) {
    if (parser.prev.type == T_DOT) return;

    switch (parser.current.type) {
      case T_CLASS:
      case T_FUN:
      case T_VAR:
      case T_FOR:
      case T_IF:
      case T_WHILE:
      case T_PRINT:
      case T_RETURN:
        return;
      default:
        ;
    }

    adv();
  }
}

static void declaration() {
  if (match(T_CLASS)) {
    class_decl();
  }
  else if (match(T_FUN)) {
    func_decl();
  }
  else if (match(T_VAR)) {
    decl_var();
  }
  else {
    statement();
  }

  if (parser.panic) sync();
}

static void statement() {
  if (match(T_PRINT)) {
    print_statement();
  }
  else if (match(T_IF)) {
    if_statement();
  }
  else if (match(T_RETURN)) {
    return_statement();
  }
  else if (match(T_RARROW)) {
    begin_scope();
    block();
    end_scope();
  }
  else {
    expr_statement();
  }
}

ObjFunc* compile(const char* src) {
  init_scanner(src);

  Compiler compiler;

  init_compiler(&compiler, TYPE_SCRIPT);

  parser.has_error = false;
  parser.panic = false;

  adv();

  while (!match(T_EOS)) {
    declaration();
  }

  ObjFunc* function = end_compiler();
  return parser.has_error ? NULL : function;
}

void mark_compiler_root() {
  Compiler* compiler = current;

  while (compiler != NULL) {
    mark_obj((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}
