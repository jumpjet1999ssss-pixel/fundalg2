CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab1

lab1: lab1.c
	$(CC) $(CFLAGS) lab1.c -o lab1 $(LDLIBS)

run: lab1
	./lab1 $(ARGS)

valgrind: lab1
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab1 $(ARGS)

clean:
	rm -f lab1

