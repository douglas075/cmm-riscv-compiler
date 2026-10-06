%{
#include <stdio.h>
#include "types.h"
#include "y.tab.h"
int lineno = 1;
%}

letter          [A-Za-z]
digit           [0-9]
identifier      ({letter}|_)({letter}|{digit}|_)*
const_int       {digit}+
const_fp        (({digit}*\.{digit}+|{digit}+\.)([eE][+-]?{digit}+)?)|({digit}+[eE][+-]?{digit}+)
const_str       \"([^"\n])*\"
op_add          "+"
op_sub          "-"
op_mul          "*"
op_div          "/"
op_and          "&&"
op_or           "||"
op_not          "!"
op_lt           "<"
op_gt           ">"
op_le           "<="
op_ge           ">="
op_eq           "=="
op_ne           "!="
op_assign       "="
lbrace          "{"
rbrace          "}"
lbracket        "["
rbracket        "]"
lparenth        "("
rparenth        ")"
semicolon       ";"
comma           ","
dot             "."
comment	        "/*"(([^*])|([*]+[^/*]))*("*")+"/"
scomment        "//"[^\n]*
ws              [ \t]+
newline         "\n"
error            .

%%

{ws}            ;   /* do nothing with whitespace */
{comment}       {
                  char *p = yytext;
                  while (*p != '\0') {
                    if (*p == '\n')
                      ++lineno;
                    ++p;
                  }
                }	
{scomment}      {}
"int"           { return INT;       }
"float"         { return FLOAT;     }
"void"          { return VOID;      }
"struct"        { return STRUCT;    }
"union"         { return UNION;     }
"if"            { return IF;        }
"else"          { return ELSE;      }
"while"         { return WHILE;     }
"for"           { return FOR;       }
"typedef"       { return TYPEDEF;   }
"return"        { return RETURN;    }
{op_add}        { return '+';       }
{op_sub}        { return '-';       }
{op_mul}        { return '*';       }
{op_div}        { return '/';       }
{op_and}        { return AND;       }
{op_or}         { return OR;        }
{op_not}        { return '!';       }
{op_lt}         { return LT;        }
{op_gt}         { return GT;        }
{op_le}         { return LE;        }
{op_ge}         { return GE;        }
{op_eq}         { return EQ;        }
{op_ne}         { return NE;        }
{op_assign}     { return '=';       }
{lbrace}        { return '{';       }
{rbrace}        { return '}';       }
{lbracket}      { return '[';       }
{rbracket}      { return ']';       }
{lparenth}      { return '(';       }
{rparenth}      { return ')';       }
{semicolon}     { return ';';       }
{comma}         { return ',';       }
{dot}           { return '.';       }
{const_int}     {
                  const_val *c;
                  c = (const_val *)malloc(sizeof(const_val));
                  c->type = TYPE_INT;
                  c->u.const_int = atoi(yytext);
                  yylval.constant = c;
                  return CONST_INT;
                }
{const_fp}      {
                  const_val *c;
                  c = (const_val *)malloc(sizeof(const_val));
                  c->type = TYPE_FLOAT;
                  c->u.const_fp = atof(yytext);
                  yylval.constant = c;
                  return CONST_FP;
                }
{const_str}     {
                  const_val *c;
                  c = (const_val *)malloc(sizeof(const_val));
                  c->type = TYPE_STRING;
                  c->u.const_str = strdup(yytext);
                  yylval.constant = c;
                  return CONST_STR;
                }
{identifier}    {
                  yylval.lexeme = strdup(yytext);
                  return ID;
                }
{newline}       { ++lineno;         }
{error}         { return ERROR;     }

%%
