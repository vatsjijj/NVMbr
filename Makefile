exec = nvmbrc
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -Wall -lm -ldl -fPIC -rdynamic -O2

$(exec): $(objects)
	gcc $(objects) $(flags) -o $(exec)

%.o: %.c include/%.h
	gcc -c $(flags) $< -o $@

clean:
	-rm -v nvmbrc
	-rm -v *.o
	-rm -v *.a
	-rm -v src/*.o

lint:
	clang-tidy src/*.c src/include/*.h
