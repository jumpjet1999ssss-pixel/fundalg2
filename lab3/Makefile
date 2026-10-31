CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab3

lab3: lab3.c
	$(CC) $(CFLAGS) lab3.c -o lab3 $(LDLIBS)

run: lab3
	./lab3 $(ARGS)

valgrind: lab3
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab3 $(ARGS)

clean:
	rm -f lab3

