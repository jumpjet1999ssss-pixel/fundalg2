CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab5

lab5: lab5.c
	$(CC) $(CFLAGS) lab5.c -o lab5 $(LDLIBS)

run: lab5
	./lab5 $(ARGS)

valgrind: lab5
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab5 $(ARGS)

clean:
	rm -f lab5

