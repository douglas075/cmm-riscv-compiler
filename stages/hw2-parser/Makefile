LEX = flex
YACC = yacc
CFLAGS = -O3
LDFLAGS = -lfl
CC = gcc

all:
	$(YACC) -d parser.y
	$(LEX) scanner.l
	$(CC) $(CFLAGS) -c y.tab.c
	$(CC) $(CFLAGS) -c lex.yy.c
	$(CC) $(CFLAGS) -c main.c
	$(CC) $(CFLAGS) -o parser main.o y.tab.o lex.yy.o $(LDFLAGS)

clean:
	rm -f *.o parser y.tab.c y.tab.h lex.yy.c lex.yy.c
