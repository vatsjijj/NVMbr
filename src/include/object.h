#ifndef nvmbr_object_h
#define nvmbr_object_h
#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value)  is_obj_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)         is_obj_type(value, OBJ_CLASS)
#define IS_CLOSURE(value)       is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNC(value)          is_obj_type(value, OBJ_FUNC)
#define IS_INST(value)          is_obj_type(value, OBJ_INST)
#define IS_NATIVE(value)        is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value)        is_obj_type(value, OBJ_STRING)

#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClose*)AS_OBJ(value))
#define AS_FUNC(value)          ((ObjFunc*)AS_OBJ(value))
#define AS_INST(value)          ((ObjInst*)AS_OBJ(value))
#define AS_NATIVE(value) \
  (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNC,
  OBJ_INST,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVAL,
} ObjType;

struct Obj {
  ObjType type;
  bool is_marked;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  int arity;
  int upval_count;
  Chunk chunk;
  ObjString* name;
} ObjFunc;

typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

typedef struct ObjUpval {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpval* next;
} ObjUpval;

typedef struct {
  Obj obj;
  ObjFunc* function;
  ObjUpval** upvals;
  int upval_count;
} ObjClose;

typedef struct {
  Obj obj;
  ObjString* name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  // Gettin' klassy.
  ObjClass* klass;
  Table fields;
} ObjInst;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClose* method;
} ObjBoundMethod;

ObjBoundMethod* new_bound_method(Value receiver, ObjClose* method);
ObjClass* new_class(ObjString* name);
ObjClose* new_close(ObjFunc* function);
ObjFunc* new_func();
ObjInst* new_inst(ObjClass* klass);
ObjNative* new_native(NativeFn function);
ObjString* take_string(char* chars, int length);
ObjString* copy_string(const char* chars, int length);
ObjUpval* new_upval(Value* slot);
void print_obj(Value value);

static inline bool is_obj_type(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
