#include <stdlib.h>
#include "include/chunk.h"
#include "include/memory.h"
#include "include/vm.h"

void init_chunk(Chunk* chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
  chunk->line_count = 0;
  chunk->line_capacity = 0;
	chunk->lines = NULL;

	init_val_arr(&chunk->constants);
}

void free_chunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(LineStart, chunk->lines, chunk->line_capacity);
	free_val_arr(&chunk->constants);

	init_chunk(chunk);
}

void write_chunk(Chunk* chunk, uint8_t byte, int line) {
	if (chunk->capacity < chunk->count + 1) {
		int old_capacity = chunk->capacity;

		chunk->capacity = GROW_CAPACITY(old_capacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->count++;

  if (chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line) {
		return;
	}

	if (chunk->line_capacity < chunk->line_count + 1) {
		int old_capacity = chunk->line_capacity;
		chunk->line_capacity = GROW_CAPACITY(old_capacity);
		chunk->lines = GROW_ARRAY(LineStart, chunk->lines, old_capacity, chunk->line_capacity);
	}

	LineStart* line_start = &chunk->lines[chunk->line_count++];
	line_start->offset = chunk->count - 1;
	line_start->line = line;
}

void write_const(Chunk* chunk, Value value, int line) {
	int index = add_const(chunk, value);
	if (index < 256) {
		write_chunk(chunk, OP_CONSTANT, line);
		write_chunk(chunk, (uint8_t)index, line);
	}
	else {
		write_chunk(chunk, OP_CONSTANT_LONG, line);
		write_chunk(chunk, (uint8_t)(index & 0xff), line);
		write_chunk(chunk, (uint8_t)((index >> 8) & 0xff), line);
		write_chunk(chunk, (uint8_t)((index >> 16) & 0xff), line);
	}
}

int add_const(Chunk* chunk, Value value) {
	push(value);
	write_val_arr(&chunk->constants, value);
	pop();

	return chunk->constants.count - 1;
}

int get_line(Chunk* chunk, int instruct) {
	int start = 0;
	int end = chunk->line_count - 1;

	for (;;) {
		int mid = (start + end) / 2;
		LineStart* line = &chunk->lines[mid];
		if (instruct < line->offset) {
			end = mid - 1;
		}
		else if (mid == chunk->line_count - 1 || instruct < chunk->lines[mid + 1].offset) {
			return line->line;
		}
		else {
			start = mid + 1;
		}
	}
}
