#include <stdio.h>
#include "include/debug.h"
#include "include/object.h"
#include "include/value.h"

void disassemble_chunk(Chunk* chunk, const char* name) {
  printf("[ %s ]\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassemble_instruct(chunk, offset);
  }
}

static int const_instruct(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];

  printf("%-16s %4d '", name, constant);
  print_val(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2;
}

static int invoke_instruct(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  uint8_t arg_count = chunk->code[offset + 2];

  printf("%-16s (%d args) %4d `", name, arg_count, constant);
  print_val(chunk->constants.values[constant]);
  printf("`\n");

  return offset + 3;
}

static int simple_instruct(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byte_instruct(const char* name, Chunk* chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];

  printf("%-16s %4d\n", name, slot);

  return offset + 2;
}

static int jump_instruct(const char* name, int sign, Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);

  jump |= chunk->code[offset + 2];

  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);

  return offset + 3;
}

int disassemble_instruct(Chunk* chunk, int offset) {
  printf("%04d ", offset);

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("    | ");
  }
  else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruct = chunk->code[offset];

  switch (instruct) {
    case OP_CONSTANT:
      return const_instruct("CONSTANT", chunk, offset);
    case OP_NIL:
      return simple_instruct("NIL", offset);
    case OP_TRUE:
      return simple_instruct("TRUE", offset);
    case OP_FALSE:
      return simple_instruct("FALSE", offset);
    case OP_POP:
      return simple_instruct("POP", offset);
    case OP_GET_LOCAL:
      return byte_instruct("GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byte_instruct("SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return const_instruct("GET_GLOBAL", chunk, offset);
    case OP_DEF_GLOBAL:
      return const_instruct("DEF_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return const_instruct("SET_GLOBAL", chunk, offset);
    case OP_GET_UPVAL:
      return byte_instruct("GET_UPVAL", chunk, offset);
    case OP_SET_UPVAL:
      return byte_instruct("SET_UPVAL", chunk, offset);
    case OP_GET_PROP:
      return const_instruct("GET_PROP", chunk, offset);
    case OP_SET_PROP:
      return const_instruct("SET_PROP", chunk, offset);
    case OP_GET_SUPER:
      return const_instruct("GET_SUPER", chunk, offset);
    case OP_EQU:
      return simple_instruct("EQU", offset);
    case OP_GREATER:
      return simple_instruct("GREATER", offset);
    case OP_LESS:
      return simple_instruct("LESS", offset);
    case OP_LARROW:
      return simple_instruct("LARROW", offset);
    case OP_ADD:
      return simple_instruct("ADD", offset);
    case OP_SUB:
      return simple_instruct("SUB", offset);
    case OP_MUL:
      return simple_instruct("MUL", offset);
    case OP_DIV:
      return simple_instruct("DIV", offset);
    case OP_NOT:
      return simple_instruct("NOT", offset);
    case OP_NEGATE:
      return simple_instruct("NEGATE", offset);
    case OP_PRINT:
      return simple_instruct("PRINT", offset);
    case OP_JUMP:
      return jump_instruct("JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jump_instruct("JUMP_IF_FALSE", 1, chunk, offset);
    case OP_CALL:
      return byte_instruct("CALL", chunk, offset);
    case OP_INVOKE:
      return invoke_instruct("INVOKE", chunk, offset);
    case OP_INVOKE_SUPER:
      return invoke_instruct("INVOKE_SUPER", chunk, offset);
    case OP_CLOSURE: {
      offset++;

      uint8_t constant = chunk->code[offset++];

      printf("%-16s %4d ", "CLOSURE", constant);
      print_val(chunk->constants.values[constant]);
      printf("\n");

      ObjFunc* function = AS_FUNC(chunk->constants.values[constant]);

      for (int j = 0; j < function->upval_count; j++) {
        int is_local = chunk->code[offset++];
        int index = chunk->code[offset++];
        
        printf("%04d    |           %s %d\n", offset - 2, is_local ? "local" : "upval", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVAL:
      return simple_instruct("CLOSE_UPVAL", offset);
    case OP_RETURN:
      return simple_instruct("RETURN", offset);
    case OP_CLASS:
      return const_instruct("CLASS", chunk, offset);
    case OP_INHERIT:
      return simple_instruct("INHERIT", offset);
    case OP_METHOD:
      return const_instruct("METHOD", chunk, offset);
    default:
      printf("Unknown or invalid opcode `%d`.\n", instruct);
      return offset + 1;
  }
}
