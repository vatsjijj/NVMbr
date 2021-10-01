#ifndef nvmbr_compiler_h
#define nvmbr_compiler_h
#include "vm.h"
#include "object.h"
ObjFunc* compile(const char* src);
void mark_compiler_root();
#endif
