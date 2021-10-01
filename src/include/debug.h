#ifndef nvmbr_debug_h
#define nvmbr_debug_h
#include "chunk.h"
void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruct(Chunk* chunk, int offset);
#endif
