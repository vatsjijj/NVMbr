exec = nvmbrc
CC = gcc
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -Wall -fPIC -O2

ifeq ($(OS), Windows_NT)
	RM_COM = del
	delsrc = del src\*.o
else
	RM_COM = rm -v
	delsrc = rm -v src/*.o
endif

$(exec): $(objects)
	$(CC) $(objects) $(flags) -o $(exec)

%.o: %.c include/%.h
	$(CC) -c $(flags) $< -o $@

clean:
	$(RM_COM) nvmbrc*
	$(delsrc)
