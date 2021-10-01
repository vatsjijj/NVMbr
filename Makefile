exec = nvmbrc
CC = gcc
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -Wall -fPIC -rdynamic -O2

ifeq ($(OS), Windows_NT)
	RM_COM = del
else
	RM_COM = rm -v
endif

$(exec): $(objects)
	$(CC) $(objects) $(flags) -o $(exec)

%.o: %.c include/%.h
	$(CC) -c $(flags) $< -o $@

clean:
	$(RM_COM) nvmbrc*
	$(RM_COM) src/*.o
