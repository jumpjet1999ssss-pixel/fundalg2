CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab9

lab9: lab9.c
	$(CC) $(CFLAGS) lab9.c -o lab9 $(LDLIBS)

run: lab9
	./lab9 $(ARGS)

valgrind: lab9
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab9 $(ARGS)

clean:
	rm -f lab9

