#ifndef nvmbr_value_h
#define nvmbr_value_h
#include <string.h>
#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_TAGGING
#define SIGN_BIT          ((uint64_t)0x8000000000000000)
#define QNAN              ((uint64_t)0x7ffc000000000000)

#define TAG_NIL           1
#define TAG_FALSE         2
#define TAG_TRUE          3

typedef uint64_t Value;

#define BOOL_VAL(b)       ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL         ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL          ((Value)(uint64_t)(QNAN | TAG_TRUE))

#define IS_BOOL(value)    (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)     ((value) == NIL_VAL)
#define IS_NUM(value)     (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
  (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)    ((value) == TRUE_VAL)
#define AS_NUM(value)     val_to_num(value)
#define AS_OBJ(value) \
  ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define NIL_VAL           ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUM_VAL(num)      num_to_val(num)
#define OBJ_VAL(obj) \
  (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double val_to_num(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

static inline Value num_to_val(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}
#else

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUM,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double num;
    Obj* obj;
  } as;
} Value;

#define IS_BOOL(value)  ((value).type == VAL_BOOL)
#define IS_NIL(value)   ((value).type == VAL_NIL)
#define IS_NUM(value)   ((value).type == VAL_NUM)
#define IS_OBJ(value)   ((value).type == VAL_OBJ)

#define AS_OBJ(value)   ((value).as.obj)
#define AS_BOOL(value)  ((value).as.boolean)
#define AS_NUM(value)   ((value).as.num)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL         ((Value){VAL_NIL, {.num = 0}})
#define NUM_VAL(value)  ((Value){VAL_NUM, {.num = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool value_equ(Value a, Value b);
void init_val_arr(ValueArray* array);
void write_val_arr(ValueArray* array, Value value);
void free_val_arr(ValueArray* array);
void print_val(Value value);

#endif
