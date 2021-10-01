#ifndef nvmbr_scanner_h
#define nvmbr_scanner_h

typedef enum {
  // Single character.
  T_LPAREN,
  T_RPAREN,
  T_LBRACE,
  T_RBRACE,
  T_LBRACK,
  T_RBRACK,
  T_COMMA,
  T_DOT,
  T_PLUS,
  T_SEMICOLON,
  T_COLON,
  T_SLASH,
  T_STAR,

  // One/two character.
  T_MINUS,
  T_RARROW,
  T_BANG,
  T_BANG_EQU,
  T_EQU,
  T_EQU_EQU,
  T_GREATER,
  T_GREATER_EQU,
  T_LESS,
  T_LESS_EQU,
  T_LARROW,

  // Literals.
  T_IDENT,
  T_STRING,
  T_NUM,

  // Keywords.
  T_AND,
  T_CLASS,
  T_ELSE,
  T_FALSE,
  T_FOR,
  T_FUN,
  T_IF,
  T_NIL,
  T_OR,
  T_PRINT,
  T_RETURN,
  T_SUPER,
  T_THIS,
  T_TRUE,
  T_VAR,
  T_WHILE,
  T_DO,
  T_END,
  T_NL,

  T_ERR,
  T_EOS,
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
} Token;

void init_scanner(const char* src);
Token scan_token();

#endif
