CC ?= gcc

.PHONY: all check clean

all: lib.so test

lib.so: lib.c
	$(CC) -shared -fPIC -o $@ $<

test: test.c
	$(CC) -o $@ $< -ldl

check: lib.so test
	./test ./lib.so

clean:
	rm -f lib.so test
