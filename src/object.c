#include <stdio.h>
#include <string.h>
#include "include/memory.h"
#include "include/object.h"
#include "include/value.h"
#include "include/vm.h"
#include "include/table.h"

#define ALLOCATE_OBJ(type, object_type) \
  (type*)allocate_obj(sizeof(type), object_type)

static Obj* allocate_obj(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);

  object->type = type;
  object->is_marked = false;

  object->next = vm.objects;
  vm.objects = object;

  #ifdef DEBUG_LOG_GC
  printf("%p alloc %zu for %d\n", (void*)object, size, type);
  #endif

  return object;
}

ObjBoundMethod* new_bound_method(Value receiver, ObjClose* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);

  bound->receiver = receiver;
  bound->method = method;

  return bound;
}

ObjClass* new_class(ObjString* name) {
  // klassic.
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);

  klass->name = name;
  init_table(&klass->methods);

  return klass;
}

ObjClose* new_close(ObjFunc* function) {
  ObjUpval** upvals = ALLOCATE(ObjUpval*, function->upval_count);

  for (int i = 0; i < function->upval_count; i++) {
    upvals[i] = NULL;
  }

  ObjClose* closure = ALLOCATE_OBJ(ObjClose, OBJ_CLOSURE);
  closure->function = function;
  closure->upvals = upvals;
  closure->upval_count = function->upval_count;

  return closure;
}

ObjFunc* new_func() {
  ObjFunc* function = ALLOCATE_OBJ(ObjFunc, OBJ_FUNC);

  function->arity = 0;
  function->upval_count = 0;
  function->name = NULL;
  init_chunk(&function->chunk);

  return function;
}

ObjInst* new_inst(ObjClass* klass) {
  ObjInst* inst = ALLOCATE_OBJ(ObjInst, OBJ_INST);

  // My God... How many klass jokes do I need to make??
  inst->klass = klass;
  init_table(&inst->fields);

  return inst;
}

ObjNative* new_native(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);

  native->function = function;

  return native;
}

static ObjString* allocate_string(char* chars, int length, uint32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string));
  set_table(&vm.strings, string, NIL_VAL);
  pop();

  return string;
}

static uint32_t hash_string(const char* key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

ObjString* take_string(char* chars, int length) {
  uint32_t hash = hash_string(chars, length);
  ObjString* interned = table_find_string(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocate_string(chars, length, hash);
}

ObjString* copy_string(const char* chars, int length) {
  uint32_t hash = hash_string(chars, length);
  ObjString* interned = table_find_string(&vm.strings, chars, length, hash);

  if (interned != NULL) return interned;

  char* heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';

  return allocate_string(heap_chars, length, hash);
}

ObjUpval* new_upval(Value* slot) {
  ObjUpval* upval = ALLOCATE_OBJ(ObjUpval, OBJ_UPVAL);

  upval->closed = NIL_VAL;
  upval->location = slot;
  upval->next = NULL;

  return upval;
}

static void print_func(ObjFunc* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void print_obj(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
      print_func(AS_BOUND_METHOD(value)->method->function);
      break;
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      print_func(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNC:
      print_func(AS_FUNC(value));
      break;
    case OBJ_INST:
      printf("%s instance", AS_INST(value)->klass->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_UPVAL:
      printf("upval");
      break;
  }
}
