#include <stdio.h>
#include <string.h>
#include "include/object.h"
#include "include/memory.h"
#include "include/value.h"

void init_val_arr(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void write_val_arr(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;

    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}

void free_val_arr(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  init_val_arr(array);
}

void print_val(Value value) {
  #ifdef NAN_TAGGING
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  }
  else if (IS_NIL(value)) {
    printf("nil");
  }
  else if (IS_NUM(value)) {
    printf("%g", AS_NUM(value));
  }
  else if (IS_OBJ(value)) {
    print_obj(value);
  }
  #else

  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUM: printf("%g", AS_NUM(value)); break;
    case VAL_OBJ: print_obj(value); break;
  }
  #endif
}

bool value_equ(Value a, Value b) {
  #ifdef NAN_TAGGING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) == AS_NUM(b);
  }
  return a == b;
  #else
  
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_BOOL:  return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:   return true;
    case VAL_NUM:   return AS_NUM(a) == AS_NUM(b);
    case VAL_OBJ:   return AS_OBJ(a) == AS_OBJ(b);
    default:        return false;
  }
  #endif
}
