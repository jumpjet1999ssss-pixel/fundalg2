CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab8

lab8: lab8.c
	$(CC) $(CFLAGS) lab8.c -o lab8 $(LDLIBS)

run: lab8
	./lab8 $(ARGS)

valgrind: lab8
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab8 $(ARGS)

clean:
	rm -f lab8

