exec = nvmbrc
CC = gcc
UNAME_S = uname -s
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
FLAGS = -Wall -fPIC -O2

ifeq ($(OS), Windows_NT)
	RM_COM = del
	DELSRC = del src\*.o
	INST = echo "Cannot run `make install` on Windows."
	UNINST = echo "Run `del nvmbrc.exe` instead."
else
	RM_COM = rm -v
	DELSRC = rm -v src/*.o
	INST = cp nvmbrc /usr/bin
	UNINST = rm -v /usr/bin/nvmbrc
endif

$(exec): $(OBJ)
	$(CC) $(OBJ) $(FLAGS) -o $(exec)

%.o: %.c include/%.h
	$(CC) -c $(FLAGS) $< -o $@

clean:
	$(RM_COM) nvmbrc*
	$(DELSRC)

install:	
	$(INST)

uninstall:	
	$(RM_COM) nvmbrc*
	$(DELSRC)
	$(UNINST)
