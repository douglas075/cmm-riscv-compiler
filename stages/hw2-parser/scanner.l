%{
#include <stdio.h>
#include "types.h"
#include "y.tab.h"
#include <string.h>
#include <stdlib.h>

int num_lines = 0;

#define MAX_TYPEDEFS 256
static char *typedef_names[MAX_TYPEDEFS];
static int typedef_count = 0;

/* --- typedef name helper --- */
static int is_typedef_name(const char *s) {
    for (int i = 0; i < typedef_count; i++) {
        if (strcmp(typedef_names[i], s) == 0)
            return 1;
    }
    return 0;
}

static void add_typedef_name(const char *s) {
    if (typedef_count < MAX_TYPEDEFS) {
        typedef_names[typedef_count++] = strdup(s);
    }
}
%}

letter          [A-Za-z]
digit           [0-9]
ID              ({letter})({letter}|{digit}|"_")*
op_assign       "="
op_or           "||"
op_and          "&&"
op_not          "!"
op_eq           "=="
op_ne           "!="
op_lt           "<"
op_gt           ">"
op_le           "<="
op_ge           ">="
op_add          "+"
op_sub          "-"
op_mul          "*"
op_div          "/"
int_const       {digit}+
fp_const       (({digit}*\.{digit}+|{digit}+\.)([eE][+-]?{digit}+)?)|({digit}+[eE][+-]?{digit}+)
str_const       \"([^"\n])*\"
comment	        "/*"(([^*])|([*]+[^/*]))*("*")+"/"
ws              [ \t]+
newline         "\n"
mk_lparen       "("
mk_rparen       ")"
mk_lbrace       "{"
mk_rbrace       "}"
mk_lsqbrace     "["
mk_rsqbrace     "]"
mk_comma        ","
mk_semicolon    ";"
mk_dot          "."
error            .

%%

{ws}            ;   /* do nothing with whitespace */
{comment}       {
                    int i = 0;
                    while (yytext[i]!='\0') {
                        if (yytext[i]=='\n')
                            ++num_lines;
                        ++i;
                    }
                }	
"int"           { return INT;     }
"float"         { return FLOAT;   }
"void"          { return VOID;    }
"if"            { return IF;      }
"else"          { return ELSE;    }
"while"         { return WHILE;   }
"for"           { return FOR;     }
"typedef"       { return TYPEDEF; }
"return"        { return RETURN;  }
{ID}            {
                  yylval.lexeme = strdup(yytext);
                  if (is_typedef_name(yytext))
                      return TYPE_NAME;
                  else
                      return ID;
                }
{op_assign}     { return OP_ASSIGN; }
{op_and}        { return OP_AND;    }
{op_or}         { return OP_OR;     }
{op_not}        { return OP_NOT;    }
{op_eq}         { return OP_EQ;     }
{op_ne}         { return OP_NE;     }
{op_lt}         { return OP_LT;     }
{op_gt}         { return OP_GT;     }
{op_le}         { return OP_LE;     }
{op_ge}         { return OP_GE;     }
{op_add}        { return OP_ADD;    }
{op_sub}        { return OP_SUB;    }
{op_mul}        { return OP_MUL;    }
{op_div}        { return OP_DIV;    }
{int_const}     {
                  const_type *t;
                  t = (const_type *)malloc(sizeof(const_type));
                  t->type = TYPE_INTEGER;
                  t->u.int_val = atoi(yytext);
                  yylval.constant = t;
                  return CONSTANT;
                }
{fp_const}      {
                  const_type *t;
                  t = (const_type *)malloc(sizeof(const_type));
                  t->type = TYPE_FLOAT;
                  t->u.fp_val = atof(yytext);
                  yylval.constant = t;
                  return CONSTANT;
                }
{str_const}     {
                  const_type *t;
                  t = (const_type *)malloc(sizeof(const_type));
                  t->type = TYPE_STRING;
                  t->u.str_val = strdup(yytext);
                  yylval.constant = t;
                  return CONSTANT;
                }
{mk_lparen}     { return MK_LPAREN;    }
{mk_rparen}     { return MK_RPAREN;    }
{mk_lbrace}     { return MK_LBRACE;    }
{mk_rbrace}     { return MK_RBRACE;    }
{mk_lsqbrace}   { return MK_LB;        }
{mk_rsqbrace}   { return MK_RB;        }
{mk_comma}      { return MK_COMMA;     }
{mk_semicolon}  { return MK_SEMICOLON; }
{mk_dot}        { return MK_DOT;       }
{newline}       { ++num_lines;         }
{error}         { return ERROR;        }

%%

void register_typedef(const char *s) {
    add_typedef_name(s);
}