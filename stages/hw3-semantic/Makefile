OS := $(shell uname -s)

CC = gcc
CFLAGS = -O3
LEX = flex
YACC = bison -v 

all:
	$(YACC) -d -y parser.y
	$(LEX) scanner.l
	$(CC) $(CFLAGS) -c y.tab.c
	$(CC) $(CFLAGS) -c lex.yy.c
	$(CC) $(CFLAGS) -c semantic.c
	$(CC) $(CFLAGS) -c symbol.c
	$(CC) $(CFLAGS) -c main.c
	$(CC) $(CFLAGS) -o compiler main.o semantic.o symbol.o y.tab.o lex.yy.o $(LDFLAGS)

clean:
	rm -f *.o compiler y.tab.c y.tab.h y.output lex.yy.c lex.yy.c
