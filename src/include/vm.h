#ifndef nvmbr_vm_h
#define nvmbr_vm_h
#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
typedef struct {
  ObjClose* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frame_count;
  Value stack[STACK_MAX];
  Value* stack_top;
  Table globals;
  Table strings;
  ObjString* init_string;
  ObjUpval* open_upvals;
  size_t alloced_bytes;
  size_t next_gc;
  Obj* objects;
  int gcount;
  int gcap;
  Obj** gstack;
} VM;

typedef enum {
  INTERP_OK,
  INTERP_COMPILE_ERR,
  INTERP_RUNTIME_ERR,
} InterpResult;

extern VM vm;

void init_vm();
void free_vm();
InterpResult interp(const char* src);
void push(Value value);
Value pop();
#endif
