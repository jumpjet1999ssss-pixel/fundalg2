CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab6

lab6: lab6.c
	$(CC) $(CFLAGS) lab6.c -o lab6 $(LDLIBS)

run: lab6
	./lab6 $(ARGS)

valgrind: lab6
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab6 $(ARGS)

clean:
	rm -f lab6

