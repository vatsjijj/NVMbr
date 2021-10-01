// My memory is failing me!! Or... Wait, is it?

#include "include/memory.h"
#include "include/compiler.h"
#include "include/vm.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef DEBUG_LOG_GC
#include "include/debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t old_size, size_t new_size) {
  vm.alloced_bytes += new_size - old_size;

  if (new_size > old_size) {

    #ifdef DEBUG_STRESS_GC
    garbage_collect();
    #endif

    if (vm.alloced_bytes > vm.next_gc) {
      garbage_collect();
    }
  }

  if (new_size == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, new_size);

  if (result == NULL) exit(1);

  return result;
}

void mark_obj(Obj* object) {
  if (object == NULL) return;

  if (object->is_marked) return;

  #ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  print_val(OBJ_VAL(object));
  printf("\n");
  #endif

  object->is_marked = true;

  if (vm.gcap < vm.gcount + 1) {
    vm.gcap = GROW_CAPACITY(vm.gcap);
    vm.gstack = (Obj**)realloc(vm.gstack, sizeof(Obj*) * vm.gcap);

    if (vm.gstack == NULL) exit(1);
  }
  vm.gstack[vm.gcount++] = object;
}

void mark_val(Value value) {
  if (IS_OBJ(value)) mark_obj(AS_OBJ(value));
}

static void mark_array(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    mark_val(array->values[i]);
  }
}

static void bobj(Obj* object) {

  #ifdef DEBUG_LOG_GC
  printf("%p bn ", (void*)object);
  print_val(OBJ_VAL(object));
  printf("\n");
  #endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;

      mark_val(bound->receiver);
      mark_obj((Obj*)bound->method);

      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;

      mark_obj((Obj*)klass->name);
      mark_table(&klass->methods);

      break;
    }
    case OBJ_CLOSURE: {
      ObjClose* closure = (ObjClose*)object;

      mark_obj((Obj*)closure->function);

      for (int i = 0; i < closure->upval_count; i++) {
        mark_obj((Obj*)closure->upvals[i]);
      }
      break;
    }
    case OBJ_FUNC: {
      ObjFunc* function = (ObjFunc*)object;

      mark_obj((Obj*)function->name);
      mark_array(&function->chunk.constants);

      break;
    }
    case OBJ_INST: {
      ObjInst* inst = (ObjInst*)object;

      mark_obj((Obj*)inst->klass);
      mark_table(&inst->fields);

      break;
    }
    case OBJ_UPVAL:
      mark_val(((ObjUpval*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}

static void free_obj_s(Obj* object) {
  #ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
  #endif

  // Never forget a break statement...
  switch (object->type) {
    case OBJ_BOUND_METHOD:
      FREE(ObjBoundMethod, object);
      break;
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;

      free_table(&klass->methods);
      FREE(ObjClass, object);

      break;
    }
    case OBJ_CLOSURE: {
      ObjClose* closure = (ObjClose*)object;

      FREE_ARRAY(ObjUpval*, closure->upvals, closure->upval_count);
      FREE(ObjClose, object);

      break;
    }
    case OBJ_FUNC: {
      ObjFunc* function = (ObjFunc*)object;

      free_chunk(&function->chunk);
      FREE(ObjFunc, object);

      break;
    }
    case OBJ_INST: {
      ObjInst* inst = (ObjInst*)object;

      free_table(&inst->fields);
      FREE(ObjInst, object);

      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;

      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);

      break;
    }
    case OBJ_UPVAL: {
      printf("upval");
      break;
    }
  }
}

static void mark_root() {
  for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
    mark_val(*slot);
  }

  for (int i = 0; i < vm.frame_count; i++) {
    mark_obj((Obj*)vm.frames[i].closure);
  }

  for (ObjUpval* upval = vm.open_upvals; upval != NULL; upval = upval->next) {
    mark_obj((Obj*)upval);
  }

  mark_table(&vm.globals);
  mark_compiler_root();
  mark_obj((Obj*)vm.init_string);
}

static void trace_refs() {
  while (vm.gcount > 0) {
    Obj* object = vm.gstack[--vm.gcount];
    bobj(object);
  }
}

static void sweep() {
  Obj* prev = NULL;
  Obj* object = vm.objects;

  while (object != NULL) {
    if (object->is_marked) {
      object->is_marked = false;
      prev = object;
      object = object->next;
    }
    else {
      Obj* unreached = object;

      object = object->next;

      if (prev != NULL) {
        prev->next = object;
      }
      else {
        vm.objects = object;
      }
      free_obj_s(unreached);
    }
  }
}

void garbage_collect() {
  #ifdef DEBUG_LOG_GC
  printf("-- begin gc\n");
  size_t before = vm.alloced_bytes;
  #endif

  mark_root();
  trace_refs();
  table_rmwhi(&vm.strings);
  sweep();

  vm.next_gc = vm.alloced_bytes * GC_HEAP_GROW_FACTOR;

  #ifdef DEBUG_LOG_GC
  printf("-- end gc\n");
  printf("    collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.alloced_bytes, before, vm.alloced_bytes, vm.next_gc);
  #endif
}

void free_obj() {
  Obj* object = vm.objects;

  while (object != NULL) {
    Obj* next = object->next;

    free_obj_s(object);
    
    object = next;
  }
  free(vm.gstack);
}
