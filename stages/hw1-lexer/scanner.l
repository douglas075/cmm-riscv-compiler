%{
#include <stdio.h>
#include "token_types.h"

void found(const char *type);
int num_lines = 0;

%}
letter          [A-Za-z]
digit           [0-9]

integer         [0-9]+
real            ([0-9]+e(\+|-)?[0-9]+)|([0-9]+\.[0-9]+(e(\+|-)?[0-9]+)?)|(([0-9]+)?\.[0-9]+(e(\+|-)?[0-9]+)?)
string          \"([^"\n]|\\.)*\"

ws              [ \t]+
newline         "\n"
comment         \/\*([^*]|\*+[^*\/])*\*+\/
arithmetic      \+|-|\/|\*
relational      <=|>=|<|>|!=|==
logical         \|\||&&|!
assignment      =
constant        {integer}|{real}|{string}
id              {letter}({letter}|{digit}|_)*
error            .

%%

{ws}            { ;   /* do nothing with whitespace */ }
{newline}       { ++num_lines; }
{comment}       { for(int i = 0; i < yyleng; ++i) if (yytext[i] == '\n') ++num_lines; }
"int"           { found("INT"); }
"float"         { found("FLOAT"); }
"void"          { found("VOID"); }
"if"            { found("IF"); }
"else"          { found("ELSE"); }
"while"         { found("WHILE"); }
"for"           { found("FOR"); }
"return"        { found("RETURN"); }
"typedef"       { found("TYPEDEF"); }
"struct"        { found("STRUCT"); }
"union"         { found("UNION"); }
"{"             { found("LBRACE"); }
"}"             { found("RBRACE"); }
"["             { found("LBRACKET"); }
"]"             { found("RBRACKET"); }
"("             { found("LPAREN"); }
")"             { found("RPAREN"); }
";"             { found("SEMICOLON"); }
","             { found("COMMA"); }
"."             { found("DOT"); }
{arithmetic}    { found("ARITHMETIC"); }
{relational}    { found("RELATIONAL"); }
{logical}       { found("LOGICAL"); }
{assignment}    { found("ASSIGNMENT"); }
{constant}      { found("CONST"); }
{id}            { found("ID"); }
{error}         { return ERROR; }

%%

void
found(const char *type) {
    printf("Found a %s: %s\n", type, yytext);
}