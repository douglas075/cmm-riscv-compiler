LEX = flex
CFLAGS = -O3
LDFLAGS = 
CC = gcc

.PHONY: all clean test

all: scanner

scanner: scanner.c main.c
	$(CC) $(CFLAGS) -o scanner scanner.c main.c $(LDFLAGS)

scanner.c: scanner.l
	$(LEX) -o scanner.c scanner.l

clean:
	rm -f *.o scanner lex.yy.c scanner.c

test: scanner
	./scanner < test.c
