#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "include/common.h"
#include "include/vm.h"
#include "include/debug.h"
#include "include/compiler.h"
#include "include/object.h"
#include "include/memory.h"

VM vm;

static Value clock_native(int arg_count, Value* args) {
  return NUM_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void reset_stack() {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
  vm.open_upvals = NULL;
}

static void runtime_err(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fputs("\n", stderr);

  for (int i = vm.frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunc* function = frame->closure->function;
    size_t instruct = frame->ip - function->chunk.code - 1;

    fprintf(stderr, "[ line %d ] in ", function->chunk.lines[instruct]);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    }
    else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
  reset_stack();
}

static void define_native(const char* name, NativeFn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(new_native(function)));

  set_table(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

  pop();
  pop();
}

void init_vm() {
  reset_stack();

  vm.objects = NULL;
  vm.alloced_bytes = 0;
  vm.next_gc = 1024 * 1024;
  vm.gcount = 0;
  vm.gcap = 0;
  vm.gstack = NULL;

  init_table(&vm.globals);
  init_table(&vm.strings);

  vm.init_string = NULL;
  vm.init_string = copy_string("init", 4);

  define_native("clock", clock_native);
}

void free_vm() {
  free_table(&vm.globals);
  free_table(&vm.strings);

  vm.init_string = NULL;

  free_obj();
}

void push(Value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

Value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static Value peek(int dist) {
  return vm.stack_top[-1 - dist];
}

static bool call(ObjClose* closure, int arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_err("Expected %d arguments, but got %d instead.", closure->function->arity, arg_count);
    return false;
  }

  if (vm.frame_count == FRAMES_MAX) {
    runtime_err("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frame_count++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stack_top - arg_count - 1;

  return true;
}

static bool call_val(Value callee, int arg_count) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);

        vm.stack_top[-arg_count - 1] = bound->receiver;

        return call(bound->method, arg_count);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);

        vm.stack_top[-arg_count - 1] = OBJ_VAL(new_inst(klass));

        Value initializer;

        if (get_table(&klass->methods, vm.init_string, &initializer)) {
          return call(AS_CLOSURE(initializer), arg_count);
        }
        else if (arg_count != 0) {
          runtime_err("Expected no arguments but got %d instead.", arg_count);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), arg_count);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(arg_count, vm.stack_top - arg_count);

        vm.stack_top -= arg_count + 1;

        push(result);

        return true;
      }
      default:
        break;
    }
  }
  runtime_err("Can only call functions and classes.");
  return false;
}

static bool invoke_from_class(ObjClass* klass, ObjString* name, int arg_count) {
  Value method;

  if (!get_table(&klass->methods, name, &method)) {
    runtime_err("Undefined property `%s`.", name->chars);
    return false;
  }

  return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(ObjString* name, int arg_count) {
  Value receiver = peek(arg_count);

  if (!IS_INST(receiver)) {
    runtime_err("Only instances can have methods.");
    return false;
  }

  ObjInst* inst = AS_INST(receiver);
  Value value;

  if (get_table(&inst->fields, name, &value)) {
    vm.stack_top[-arg_count - 1] = value;
    return call_val(value, arg_count);
  }

  return invoke_from_class(inst->klass, name, arg_count);
}

static bool bind_method(ObjClass* klass, ObjString* name) {
  Value method;

  if (!get_table(&klass->methods, name, &method)) {
    runtime_err("Undefined property `%s`.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = new_bound_method(peek(0), AS_CLOSURE(method));

  pop();
  push(OBJ_VAL(bound));

  return true;
}

static ObjUpval* capture_upval(Value* local) {
  ObjUpval* prev_upval = NULL;
  ObjUpval* upval = vm.open_upvals;

  while (upval != NULL && upval->location > local) {
    prev_upval = upval;
    upval = upval->next;
  }

  if (upval != NULL && upval->location == local) {
    return upval;
  }

  ObjUpval* created_upval = new_upval(local);

  if (prev_upval == NULL) {
    vm.open_upvals = created_upval;
  }
  else {
    prev_upval->next = created_upval;
  }
  return created_upval;
}

static void close_upvals(Value* last) {
  while (vm.open_upvals != NULL && vm.open_upvals->location >= last) {
    ObjUpval* upval = vm.open_upvals;

    upval->closed = *upval->location;
    upval->location = &upval->closed;
    vm.open_upvals = upval->next;
  }
}

static void def_method(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));

  set_table(&klass->methods, name, method);
  pop();
}

static bool is_false(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concat() {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);

  chars[length] = '\0';

  ObjString* result = take_string(chars, length);

  pop();
  pop();

  push(OBJ_VAL(result));
}

static InterpResult run() {
  CallFrame* frame = &vm.frames[vm.frame_count - 1];

  #define READ_BYTE() (*frame->ip++)
  #define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_CONST() \
      (frame->closure->function->chunk.constants.values[READ_BYTE()])
  #define READ_STRING() AS_STRING(READ_CONST())
  #define BINARY_OP(value_type, op) \
    do { \
      if (!IS_NUM(peek(0)) || !IS_NUM(peek(1))) { \
        runtime_err("Operands must be numbers."); \
        return INTERP_RUNTIME_ERR; \
      } \
      double b = AS_NUM(pop()); \
      double a = AS_NUM(pop()); \
      push(value_type(a op b)); \
    } while (false)

  for (;;) {
    #ifdef DEBUG_TRACE_EXEC
      printf("      ");

      for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
        printf("[ ");
        print_val(*slot);
        printf(" ]");
      }

      printf("\n");
      disassemble_instruct(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
    #endif

    uint8_t instruct;

    switch (instruct = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONST();

        push(constant);

        break;
      }
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();

        push(frame->slots[slot]);

        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();

        frame->slots[slot] = peek(0);

        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;

        if (!get_table(&vm.globals, name, &value)) {
          runtime_err("Undefined variable `%s`.", name->chars);
          return INTERP_RUNTIME_ERR;
        }

        push(value);

        break;
      }
      case OP_DEF_GLOBAL: {
        ObjString* name = READ_STRING();

        set_table(&vm.globals, name, peek(0));
        pop();

        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();

        if (set_table(&vm.globals, name, peek(0))) {
          del_table(&vm.globals, name);
          runtime_err("Undefined variable `%s`.", name->chars);
          return INTERP_RUNTIME_ERR;
        }

        break;
      }
      case OP_GET_UPVAL: {
        uint8_t slot = READ_BYTE();

        push(*frame->closure->upvals[slot]->location);

        break;
      }
      case OP_SET_UPVAL: {
        uint8_t slot = READ_BYTE();

        *frame->closure->upvals[slot]->location = peek(0);

        break;
      }
      case OP_GET_PROP: {
        if (!IS_INST(peek(0))) {
          runtime_err("Only instances can have properties.");
          return INTERP_RUNTIME_ERR;
        }

        ObjInst* inst = AS_INST(peek(0));
        ObjString* name = READ_STRING();

        Value value;

        if (get_table(&inst->fields, name, &value)) {
          pop();
          push(value);

          break;
        }

        if (!bind_method(inst->klass, name)) {
          return INTERP_RUNTIME_ERR;
        }
        break;
      }
      case OP_SET_PROP: {
        if (!IS_INST(peek(1))) {
          runtime_err("Only instances can have fields.");
          return INTERP_RUNTIME_ERR;
        }

        ObjInst* inst = AS_INST(peek(1));

        set_table(&inst->fields, READ_STRING(), peek(0));

        Value value = pop();

        pop();
        push(value);

        break;
      }
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());

        if (!bind_method(superclass, name)) {
          return INTERP_RUNTIME_ERR;
        }

        break;
      }
      case OP_EQU: {
        Value b = pop();
        Value a = pop();

        push(BOOL_VAL(value_equ(a, b)));

        break;
      }
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concat();
        }
        else if (IS_NUM(peek(0)) && IS_NUM(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());

          push(NUM_VAL(a + b));
        }
        else {
          runtime_err("Operands must be two numbers/two strings.");
          return INTERP_RUNTIME_ERR;
        }
        break;
      }
      case OP_SUB:      BINARY_OP(NUM_VAL, -); break;
      case OP_MUL:      BINARY_OP(NUM_VAL, *); break;
      case OP_DIV:      BINARY_OP(NUM_VAL, /); break;
      case OP_DUP: push(peek(0)); break;
      case OP_NOT:
        push(BOOL_VAL(is_false(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUM(peek(0))) {
          runtime_err("Operand must be a number.");
          return INTERP_RUNTIME_ERR;
        }
        push(NUM_VAL(-AS_NUM(pop())));

        break;
      case OP_PRINT: {
        print_val(pop());
        printf("\n");

        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();

        frame->ip += offset;

        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();

        if (is_false(peek(0))) frame->ip += offset;

        break;
      }
      case OP_CALL: {
        int arg_count = READ_BYTE();

        if (!call_val(peek(arg_count), arg_count)) {
          return INTERP_RUNTIME_ERR;
        }

        frame = &vm.frames[vm.frame_count - 1];

        break;
      }
      case OP_INVOKE: {
        ObjString* method = READ_STRING();
        int arg_count = READ_BYTE();

        if (!invoke(method, arg_count)) {
          return INTERP_RUNTIME_ERR;
        }

        frame = &vm.frames[vm.frame_count - 1];

        break;
      }
      case OP_INVOKE_SUPER: {
        ObjString* method = READ_STRING();
        int arg_count = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());

        if (!invoke_from_class(superclass, method, arg_count)) {
          return INTERP_RUNTIME_ERR;
        }

        frame = &vm.frames[vm.frame_count - 1];

        break;
      }
      case OP_CLOSURE: {
        ObjFunc* function = AS_FUNC(READ_CONST());
        ObjClose* closure = new_close(function);

        push(OBJ_VAL(closure));

        for (int i = 0; i < closure->upval_count; i++) {
          uint8_t is_local = READ_BYTE();
          uint8_t index = READ_BYTE();

          if (is_local) {
            closure->upvals[i] = capture_upval(frame->slots + index);
          }
          else {
            closure->upvals[i] = frame->closure->upvals[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVAL:
        close_upvals(vm.stack_top - 1);

        pop();

        break;
      case OP_RETURN: {
        Value result = pop();

        close_upvals(frame->slots);

        vm.frame_count--;

        if (vm.frame_count == 0) {
          pop();
          return INTERP_OK;
        }

        vm.stack_top = frame->slots;

        push(result);

        frame = &vm.frames[vm.frame_count - 1];

        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(new_class(READ_STRING())));
        break;
      case OP_INHERIT: {
        Value superclass = peek(1);

        if (!IS_CLASS(superclass)) {
          runtime_err("Superclasses must be a class.");
          return INTERP_RUNTIME_ERR;
        }

        ObjClass* subclass = AS_CLASS(peek(0));

        table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);

        pop();

        break;
      }
      case OP_METHOD:
        def_method(READ_STRING());
        break;
    }
  }
  #undef READ_BYTE
  #undef READ_SHORT
  #undef READ_CONST
  #undef READ_STRING
  #undef BINARY_OP
}

InterpResult interp(const char* src) {
  ObjFunc* function = compile(src);

  if (function == NULL) return INTERP_COMPILE_ERR;

  push(OBJ_VAL(function));

  ObjClose* closure = new_close(function);

  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
