%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"
extern int yylex (void);
extern int yyerror(char *s);

static inline node_type *CreateNode() {
  return (node_type *)malloc(sizeof(node_type));
}

void register_typedef(const char *s);
%}


%union {
  char *lexeme;
  const_type *constant;
  node_type *node;
};

%token <lexeme>ID
%token <constant>CONSTANT
%token VOID
%token INT
%token FLOAT
%token TYPE_NAME // by typedef
%token IF
%token ELSE
%token WHILE
%token FOR
%token TYPEDEF
%token RETURN
%token OP_ASSIGN
%token OP_OR
%token OP_AND
%token OP_NOT
%token OP_EQ
%token OP_NE
%token OP_GT
%token OP_LT
%token OP_GE
%token OP_LE
%token OP_ADD
%token OP_SUB
%token OP_MUL
%token OP_DIV
%token MK_LB
%token MK_RB
%token MK_LPAREN
%token MK_RPAREN
%token MK_LBRACE
%token MK_RBRACE
%token MK_COMMA
%token MK_SEMICOLON
%token MK_DOT
%token ERROR

%type <node> program

%right OP_ASSIGN        /* lowest precedence, right-associative   */
%left  OP_OR
%left  OP_AND
%nonassoc OP_EQ OP_NE OP_LT OP_LE OP_GT OP_GE
%left  OP_ADD OP_SUB
%left  OP_MUL OP_DIV
%right OP_NOT 
/* %right OP_SUB */


%start program

%%

/* [] TODO: typedef */
program:
    global_decl_list {
        printf("Reduce -> program\n");
	$$ = CreateNode(); // not used in this homework
    }
    | {}
    ;

global_decl_list:
    global_decl_list global_decl {}
    | global_decl {}
    ; 

global_decl:
    decl_list function_decl {}
    | function_decl {}
    ;

function_decl:
    function_head MK_LPAREN opt_param_list MK_RPAREN stmt_block
    {
      printf("Reduce -> function declaration\n");
    }
    ;

function_head:
    type ID { printf("Parse function: %s\n", $2); }
    | VOID ID { printf("Parse function: %s\n", $2); }
    ;

opt_param_list:
    param_list { printf("Reduce -> param_list\n"); }
    | { printf("Reduce -> empty function param\n"); }
    ;

param_list:
    param_list MK_COMMA param {}
    | param	{}
    ;

/* [v] TODO: array first dimension may be omitted */
param:
    type ID { printf("Parse function variable param: %s\n", $2); }
    | array_param 
    ;

array_param:
    basic_array_param
    | array_param array_dimension
    ;

basic_array_param:
    type ID MK_LB array_param_first_dim MK_RB 
    { printf("Parse function array param: %s\n", $2); }
    ;

array_param_first_dim:
    expr
    |
    ;
 
decl_list:
    decl_list decl {}
    | decl {}
    ;

decl:
    TYPEDEF type ID MK_SEMICOLON {
        printf("typedef %s\n", $3);
        register_typedef($3); 
    }
    | type var_decl_list MK_SEMICOLON
    ;

var_decl_list:
    var_decl_list MK_COMMA var_decl
    | var_decl
    ;
    
var_decl:
    scalar_decl
    | array_decl
    ;

scalar_decl:
    ID optional_init { printf("Parse a scalar declaration: %s\n", $1); }
    ;

optional_init:
    OP_ASSIGN expr
    |
    ;


/* [v] TODO : array a[expr] */
/* [v] TODO: make sure the size is int constant */
/* solved: that is semantic analyzer's job */
array_decl:
    basic_array_decl
    | array_decl array_dimension
    ;

basic_array_decl:
    ID array_dimension { printf("Parse array variable declaration: %s\n", $1); }
    ;

array_dimension:
    MK_LB expr MK_RB
    ;

type:
    INT {}
    | FLOAT {}
    | TYPE_NAME
    ;

stmt_block:
    MK_LBRACE stmt_list MK_RBRACE
    | MK_LBRACE MK_RBRACE

stmt_list:
    stmt_list stmt {}
    | stmt {}
    ;

stmt:
    matched_stmt
    | unmatched_stmt
    ;

matched_stmt:
    IF MK_LPAREN expr MK_RPAREN matched_stmt ELSE matched_stmt { printf("Parse paired if-else\n"); }
    | simple_stmt
    | stmt_block 
    ;

unmatched_stmt:
    IF MK_LPAREN expr MK_RPAREN matched_stmt {  printf("Parse isolated if\n"); }
    | IF MK_LPAREN expr MK_RPAREN matched_stmt ELSE unmatched_stmt { printf("Parse unmatched else\n"); }
    ;

simple_stmt:
    decl {}
    | return MK_SEMICOLON { printf("Parse return\n");}
    | while_loop { printf("Parse while loop\n"); }
    | for_loop { printf("Parse for loop\n"); }
    /* | func_call MK_SEMICOLON { printf("Parse function call\n"); } */
    /* func_call is also an expr */
    | expr MK_SEMICOLON
    | MK_SEMICOLON
    ;

while_loop:
    WHILE MK_LPAREN expr MK_RPAREN stmt
    ;

for_loop:
    FOR MK_LPAREN expr_opt MK_SEMICOLON expr_opt MK_SEMICOLON expr_opt MK_RPAREN stmt
    ;

expr_opt:
    expr
    |
    ;

func_call:
    ID MK_LPAREN opt_arg_list MK_RPAREN
    ;

opt_arg_list:
    arg_list { printf("Reduce -> arg_list\n"); }
    | { printf("Reduce -> empty function arg\n"); }
    ;

arg_list:
    arg_list MK_COMMA arg {}
    | arg {}
    ;

/* [v] TODO: array first dimension may be omitted */
arg:
    expr { printf("Parse function arg\n"); }
    ;

return:
    RETURN expr { printf("Parse return\n"); }
    | RETURN
    ;

/* [v] TODO: expr */
/* cexpr is used for semantic analyzer */
/* recursive def ??? */
array_deref:
    ID MK_LB expr MK_RB
    | array_deref MK_LB expr MK_RB
    ;

primary_expr:
    ID
    | CONSTANT
    | func_call
    | array_deref
    ;

expr:
    expr2 {printf("parse an expr\n");}
    ;

expr2:
    primary_expr 
    | MK_LPAREN expr MK_RPAREN

    /* binary */
    | expr OP_ADD expr 
    | expr OP_SUB expr 
    | expr OP_DIV expr 
    | expr OP_MUL expr 
    | expr OP_EQ expr 
    | expr OP_NE expr 
    | expr OP_GE expr 
    | expr OP_GT expr 
    | expr OP_LE expr 
    | expr OP_LT expr 
    | expr OP_AND expr 
    | expr OP_OR expr 
    | expr OP_ASSIGN expr

    /* unary */
    | OP_NOT expr
    | OP_SUB expr
    ;
%%
