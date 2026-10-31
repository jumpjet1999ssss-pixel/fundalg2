CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab7

lab7: lab7.c
	$(CC) $(CFLAGS) lab7.c -o lab7 $(LDLIBS)

run: lab7
	./lab7 $(ARGS)

valgrind: lab7
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab7 $(ARGS)

clean:
	rm -f lab7

