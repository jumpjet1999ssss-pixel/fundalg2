CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O0
LDLIBS = -lm

.PHONY: all run valgrind clean

all: lab4

lab4: lab4.c
	$(CC) $(CFLAGS) lab4.c -o lab4 $(LDLIBS)

run: lab4
	./lab4 $(ARGS)

valgrind: lab4
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./lab4 $(ARGS)

clean:
	rm -f lab4

