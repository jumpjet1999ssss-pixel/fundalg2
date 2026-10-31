CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab10

lab10: lab10.c
	$(CC) $(CFLAGS) lab10.c -o lab10 $(LDLIBS)

run: lab10
	./lab10 $(ARGS)

valgrind: lab10
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab10 $(ARGS)

clean:
	rm -f lab10

