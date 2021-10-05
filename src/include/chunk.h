#ifndef nvmbr_chunk_h
#define nvmbr_chunk_h
#include "common.h"
#include "value.h"

typedef enum {
	OP_CONSTANT,
  OP_CONSTANT_LONG,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_POP,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_GET_GLOBAL,
	OP_DEF_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_UPVAL,
	OP_SET_UPVAL,
	OP_GET_PROP,
	OP_SET_PROP,
	OP_GET_SUPER,
	OP_EQU,
	OP_LESS,
	OP_LARROW,
	OP_GREATER,
	OP_DUP,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_CALL,
	OP_INVOKE,
	OP_INVOKE_SUPER,
	OP_CLOSURE,
	OP_CLOSE_UPVAL,
	OP_RETURN,
	OP_CLASS,
	OP_INHERIT,
	OP_METHOD,
} OpCode;

typedef struct {
	int offset;
	int line;
} LineStart;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
  int line_count;
  int line_capacity;
	LineStart* lines;
	ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);
void write_const(Chunk* chunk, Value value, int line);
int add_const(Chunk* chunk, Value value);
int get_line(Chunk* chunk, int instruct);

#endif
