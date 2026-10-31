CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab2

lab2: lab2.c
	$(CC) $(CFLAGS) lab2.c -o lab2 $(LDLIBS)

run: lab2
	./lab2 $(ARGS)

valgrind: lab2
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab2 $(ARGS)

clean:
	rm -f lab2

