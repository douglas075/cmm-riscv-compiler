OS := $(shell uname -s)

CC = gcc
CFLAGS = -O3
LEX = flex
YACC = bison -y -v 

all: compiler

lex.yy.o: scanner.l
	$(LEX) $<
	$(CC) $(CFLAGS) -c lex.yy.c

y.tab.o: parser.y
	$(YACC) -d $<
	$(CC) $(CFLAGS) -c y.tab.c

compiler: y.tab.o lex.yy.o semantic.o symbol.o codegen.o main.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o compiler y.tab.c y.tab.h y.output lex.yy.c lex.yy.c main.S output.s a.out
