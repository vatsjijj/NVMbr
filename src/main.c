#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/common.h"
#include "include/chunk.h"
#include "include/debug.h"
#include "include/vm.h"

static void repl() {
	char line[1024];

	printf("Welcome to the NVMbr REPL\n");
	printf("Version 0.0.2 \"Nouveau\"\n\n");

	for (;;) {
		printf("[ repl ] -> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		interp(line);
	}
}

static char* io_read_file(const char* path) {
	FILE* file = fopen(path, "rb");

	if (file == NULL) {
		fprintf(stderr, "Could not open `%s`.\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	char* buffer = (char*)malloc(file_size + 1);

	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory, can't read `%s`.\n", path);
		exit(74);
	}

	size_t read_bytes = fread(buffer, sizeof(char), file_size, file);

	if (read_bytes < file_size) {
		fprintf(stderr, "Could not read `%s`.\n", path);
		exit(74);
	}

	buffer[read_bytes] = '\0';

	fclose(file);
	return buffer;
}

static void io_file_run(const char* path) {
	char* src = io_read_file(path);
	InterpResult result = interp(src);

	free(src);

	if (result == INTERP_COMPILE_ERR) exit(65);
	if (result == INTERP_RUNTIME_ERR) exit(70);
}

int main(int argc, const char* argv[]) {
	init_vm();

	if (argc == 1) {
		repl();
	}
	else if (argc == 2) {
		io_file_run(argv[1]);
	}
	else {
		fprintf(stderr, "Usage: `nvmbrc [path2file]`\n");
		exit(64);
	}

	return 0;
}
