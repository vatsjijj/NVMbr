exec = nvmbrc
sources = src/*.c
flags = -Wall -fPIC -rdynamic -O2

$(exec):
	gcc $(sources) $(flags) -o $(exec)

clean:
	-rm -v nvmbrc

