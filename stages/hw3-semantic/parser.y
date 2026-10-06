%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"

#define dbg(fmt, args...)           \
  do {                              \
    if (debug_parsing)              \
      fprintf(stderr, fmt, ##args); \
  } while (0)

ASTNode *ast_root;

int yylex (void);
int yyerror(char *s);

/* Append siblings (b) to the siblings (a).
   Return the leftmost sibling. */
static ASTNode *makeSiblings(ASTNode *a, ASTNode *b)
{
  while (a->rightSib)
    a = a->rightSib;

  b = b->leftmostSib;
  a->rightSib = b;

  while (b) {
    b->parent = a->parent;
    b->leftmostSib = a->leftmostSib;
    b = b->rightSib;
  }

  return a->leftmostSib;
}

/* Add a child (or a list of children) to the parent node. */
static void makeChildren(ASTNode *parent, ASTNode *child)
{
  if (parent->child)
    makeSiblings(parent->child, child);
  else {
    child = child->leftmostSib;
    parent->child = child;
    while (child) {
      child->parent = parent;
      child = child->rightSib;
    }
  }
}

/* Add a family of nodes to the parent. */
static ASTNode *makeFamily(ASTNode *parent, int count, ...)
{
  int i;
  va_list ap;

  va_start(ap, count);
  for (i = 0; i < count; ++i) {
    ASTNode *child = va_arg(ap, ASTNode*);
    makeChildren(parent, child);
  }
  va_end(ap);
  return parent;
}

/* Create a AST node. */
static ASTNode *makeNode(int kind)
{
  ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
  node->kind = kind; 
  node->parent = node;
  node->child = NULL;
  node->rightSib = NULL;
  node->leftmostSib = node;
  return node;
}

/* Create a NULL node. */
static inline ASTNode *makeNullNode()
{
  return makeNode(K_NULL);
}

/* Create a constant node. */
static ASTNode *makeConstNode(const_val *constant)
{
  ASTNode *node = makeNode(K_CONST);
  node->semantic.constant = constant;
  return node;
}

/* Create a constant node with integer value `val'. */
static ASTNode *makeConstIntNode(int val)
{
  ASTNode *node = makeNode(K_CONST);
  const_val *c = (const_val *)malloc(sizeof(const_val));
  c->type = TYPE_INT;
  c->u.const_int = val;
  node->semantic.constant = c;
  return node;
}

/* Create a type node. */
static ASTNode *makeTypeNode(char *type_name)
{
  ASTNode *node = makeNode(K_TYPE);
  node->semantic.type.name = type_name;
  return node;
}

/* Create a typical ID (i.e., non-array) node. */
static ASTNode *makeTypicalID(char *name, ASTNode *assign)
{
  ASTNode *node = makeNode(K_ID);
  if (assign) {
    node->semantic.id.type = ID_TYPICAL_ASSIGN;
    makeChildren(node, assign);
  } else
    node->semantic.id.type = ID_TYPICAL;
  node->semantic.id.name = name;
  return node;
}

/* Create an array ID node. */
static ASTNode *makeArrayID(char *name, ASTNode *dim)
{
  ASTNode *node = makeNode(K_ID);
  makeChildren(node, dim);
  node->semantic.id.type = ID_ARRAY;
  node->semantic.id.name = name;
  return node;
}

/* Create a program node. */
static ASTNode *makeProgramNode(ASTNode *decl_list, ASTNode *func_list)
{
  ASTNode *node = makeNode(K_PROGRAM);
  if (decl_list)
    makeChildren(node, decl_list);
  if (func_list)
    makeChildren(node, func_list);
  node->semantic.program.has_decl = (decl_list != NULL);
  node->semantic.program.has_func = (func_list != NULL);
  return node;
}

/* Create a function node. */
static ASTNode *makeFunctionNode(ASTNode *type, ASTNode *name,
                                 ASTNode *param, ASTNode *body)
{
  ASTNode *node = makeNode(K_FUNCTION);
  makeFamily(node, 4, type, name, param, body);
  return node;
}

/* Create a block node. */
static ASTNode *makeBlockNode(ASTNode *decl_list, ASTNode *stmt_list)
{
  ASTNode *node = makeNode(K_BLOCK);
  if (decl_list)
    makeChildren(node, decl_list);
  if (stmt_list)
    makeChildren(node, stmt_list);
  node->semantic.block.has_decl = (decl_list != NULL);
  node->semantic.block.has_stmt = (stmt_list != NULL);
  return node;
}

/* Create a list node. */
static inline ASTNode *makeListNode(ASTNode *child, int type)
{
  ASTNode *node = makeNode(K_LIST);
  makeChildren(node, child);
  node->semantic.list.type = type;
  return node;
}

/* Create a decl node. */
static ASTNode *makeDeclNode(ASTNode *type, ASTNode *var, int is_typedef)
{
  ASTNode *node = makeNode(K_DECL);
  node->semantic.decl.type = (is_typedef) ? DECL_TYPEDEF : DECL_VAR;
  makeChildren(node, type);
  makeChildren(node, var);
  return node;
}

/* Create a stmt node. */
static ASTNode *makeStmtNode(int type)
{
  ASTNode *node = makeNode(K_STMT);
  node->semantic.stmt.type = type;
  return node;
}

/* Create an expr node. */
static ASTNode *makeExprNode(int type, ASTNode *operand1, ASTNode *operand2)
{
  ASTNode *node = makeNode(K_EXPR);
  node->semantic.expr.type = type;
  makeChildren(node, operand1);
  if (operand2)
    makeChildren(node, operand2);
  return node;
}

%}


%union {
  int integer;
  char *lexeme;
  const_val *constant;
  ASTNode *node;
};

%token <lexeme> ID
%token <constant> CONST_INT CONST_FP CONST_STR
%token INT FLOAT VOID STRUCT UNION TYPEDEF
%token IF ELSE WHILE FOR RETURN
%token AND OR LT GT LE GE EQ NE
%token ERROR

%type <node> program global_decl_list function_list function_def param_list param block decl_list var_decl var_list var dim_decl dim_ref stmt_list stmt expr assign_list assign_expr argument_list type param_dim_decl param_first_dim_decl empty_or_cexpr
%type <integer> cexpr

%start program

// TODO: fix precedence and associativity of operators.
//%left OR AND EQ NE LT GT LE GE '+' '-' '*' '/' '!'

/////////////////////////
// https://en.cppreference.com/w/c/language/operator_precedence.html
%right '='
%left  OR
%left  AND
%left EQ NE
%left LT LE GT GE
%left  '+' '-'
%left  '*' '/'
%right '!'

/////////////////////////

%nonassoc IFX
%nonassoc ELSE

%%

program:
      global_decl_list {
        dbg("[program] done\n");
        $$ = ast_root = $1;
      }
    ;

global_decl_list:
      { $$ = makeProgramNode(NULL, NULL); /* empty */ }
    | decl_list {
        ASTNode *decl_list = makeListNode($1, LIST_DECL);
        $$ = makeProgramNode(decl_list, NULL);
      }
    | function_list {
        ASTNode *func_list = makeListNode($1, LIST_FUNC);
        $$ = makeProgramNode(NULL, func_list);
      }
    | decl_list function_list {
        ASTNode *decl_list = makeListNode($1, LIST_DECL);
        ASTNode *func_list = makeListNode($2, LIST_FUNC);
        $$ = makeProgramNode(decl_list, func_list);
      }
    ; 

function_list:
      function_def               { $$ = $1;                   }
    | function_list function_def { $$ = makeSiblings($1, $2); }
    ;

function_def:
      type ID '(' param_list ')' '{' block '}' {
        dbg("[function_def] name=%s\n", $2);
        $$ = makeFunctionNode($1,
                              makeTypicalID($2, NULL),
                              makeListNode($4, LIST_PARAM),
                              $7);
      }
    | ID ID '(' param_list ')' '{' block '}' {
        dbg("[function_def] name=%s\n", $2);
        $$ = makeFunctionNode(makeTypicalID($1, NULL),
                              makeTypicalID($2, NULL),
                              makeListNode($4, LIST_PARAM),
                              $7);
      }

    | VOID ID '(' param_list ')' '{' block '}' {
        dbg("[function_def] name=%s\n", $2);
        $$ = makeFunctionNode(makeTypeNode("void"),
                              makeTypicalID($2, NULL),
                              makeListNode($4, LIST_PARAM),
                              $7);
      }
    ;

param_list:
      {
        dbg("[param_list] empty parameter list\n"); /* empty */
        $$ = makeNullNode();
      }
    | param	               { $$ = $1;                   }
    | param_list ',' param { $$ = makeSiblings($1, $3); }
    ;

param:
      type ID {
        dbg("[param] name=%s\n", $2);
        $$ = makeDeclNode($1, makeTypicalID($2, NULL), 0);
      }
    // TODO: array param
    | type ID param_dim_decl {
         dbg("[param] array param name=%s\n", $2);
         $$ = makeDeclNode($1, makeArrayID($2, $3), 0);
       }
    | ID ID {
        dbg("[param] typedef param name=%s\n", $2);
        $$ = makeDeclNode(makeTypicalID($1, NULL),
                          makeTypicalID($2, NULL), 0);
      }
    | ID ID param_dim_decl {
         dbg("[param] array param name=%s\n", $2);
         $$ = makeDeclNode(makeTypicalID($1, NULL), makeArrayID($2, $3), 0);
       }

    ;

block:
      { $$ = makeBlockNode(NULL, NULL); /* empty block */ }
    | decl_list {
        ASTNode *decl_list = makeListNode($1, LIST_DECL);
        $$ = makeBlockNode(decl_list, NULL);
      }
    | stmt_list {
        ASTNode *stmt_list = makeListNode($1, LIST_STMT);
        $$ = makeBlockNode(NULL, stmt_list);
      }
    | decl_list stmt_list {
        ASTNode *decl_list = makeListNode($1, LIST_DECL);
        ASTNode *stmt_list = makeListNode($2, LIST_STMT);
        $$ = makeBlockNode(decl_list, stmt_list);
      }
    ;
 
decl_list:
      var_decl ';'           { $$ = $1;                   }
    | decl_list var_decl ';' { $$ = makeSiblings($1, $2); }
    ;

var_decl:
      type var_list {
        $$ = makeDeclNode($1, $2, 0);
      }
    | ID var_list {
        $$ = makeDeclNode(makeTypicalID($1, NULL), $2, 0);
      }
    | TYPEDEF type ID {
        dbg("[var_decl] typedef name=%s\n", $3);
        $$ = makeDeclNode($2, makeTypicalID($3, NULL), 1);
      }
    | TYPEDEF type ID dim_decl {
        dbg("[var_decl] typedef name=%s\n", $3);
        $$ = makeDeclNode($2, makeArrayID($3, $4), 1);
      }
    ;

var_list:
      var              { $$ = $1;                   }
    | var_list ',' var { $$ = makeSiblings($1, $3); }
    ;

var:
      ID {
        dbg("[var] scalar %s\n", $1);
        $$ = makeTypicalID($1, NULL);
      }
    | ID '=' expr {
        dbg("[var] scalar %s with assign\n", $1);
        $$ = makeTypicalID($1, $3);
      }
    | ID dim_decl {
        dbg("[var] array %s\n", $1);
        $$ = makeArrayID($1, $2);
      }
    ;

stmt_list:
      stmt           { $$ = $1;                   }
    | stmt_list stmt { $$ = makeSiblings($1, $2); }
    ;

stmt:
      '{' block '}' {
        dbg("[stmt] block\n");
        $$ = $2;
      }
    | ';' {
        dbg("[stmt] ;");
        $$ = makeNullNode();
      }
    | ID '=' expr ';' {
        dbg("[stmt] assign scalar %s\n", $1);
        $$ = makeStmtNode(STMT_ASSIGN);
        makeFamily($$, 2, makeTypicalID($1, NULL), $3);
      }
    | ID dim_ref '=' expr ';' {
        dbg("[stmt] assign array %s\n", $1);
        $$ = makeStmtNode(STMT_ASSIGN);
        makeFamily($$, 2, makeArrayID($1, $2), $4);
      }
    | WHILE '(' expr ')' stmt {
        dbg("[stmt] while\n");
        $$ = makeStmtNode(STMT_WHILE);
        makeFamily($$, 2, $3, $5);
      }
    | IF '(' expr ')' stmt %prec IFX {
        dbg("[stmt] if\n");
        $$ = makeStmtNode(STMT_IF);
        makeFamily($$, 3, $3, $5, makeNullNode());
      }
    | IF '(' expr ')' stmt ELSE stmt {
        dbg("[stmt] if-else\n");
        $$ = makeStmtNode(STMT_IF);
        makeFamily($$, 3, $3, $5, $7);
      }
    | FOR '(' assign_list ';' ';' assign_list ')' stmt {
        dbg("[stmt] for\n");
        $$ = makeStmtNode(STMT_FOR);
        makeFamily($$, 4,
                   makeListNode($3, LIST_ASSIGN),
                   makeNullNode(),
                   makeListNode($6, LIST_ASSIGN),
                   $8);
      }
    | FOR '(' assign_list ';' expr ';' assign_list ')' stmt {
        dbg("[stmt] for\n");
        $$ = makeStmtNode(STMT_FOR);
        makeFamily($$, 4,
                   makeListNode($3, LIST_ASSIGN),
                   $5,
                   makeListNode($7, LIST_ASSIGN),
                   $9);
      }
    | RETURN ';' {
        dbg("[stmt] return without value\n");
        $$ = makeStmtNode(STMT_RETURN);
        makeChildren($$, makeNullNode());
      }
    | RETURN expr ';' {
        dbg("[stmt] return with value\n");
        $$ = makeStmtNode(STMT_RETURN);
        makeChildren($$, $2);
      }
    | ID '(' argument_list ')' ';' {
        dbg("[stmt] function call\n");
        $$ = makeStmtNode(STMT_CALL);
        makeFamily($$, 2,
                   makeTypicalID($1, NULL),
                   makeListNode($3, LIST_ARGUMENT));
      }
    ;

dim_decl:
      '[' cexpr ']' {
        dbg("[dim_decl] %d\n", $2);
        $$ = makeConstIntNode($2);
      }
    | dim_decl '[' cexpr ']' {
        dbg("[dim_decl] %d\n", $3);
        $$ = makeSiblings($1, makeConstIntNode($3));
      }
    ;

param_dim_decl:
      param_first_dim_decl dim_decl {
        $$ = makeSiblings($1, $2);
      }
    | param_first_dim_decl {
        $$ = $1;
      }
    ;

param_first_dim_decl:
    '[' empty_or_cexpr ']' {$$ = $2;}
    ;

empty_or_cexpr:
      {
        dbg("[first dimension empty]\n");
        $$ = makeNullNode();
      }
    | cexpr {
      dbg("[first dimension %d]\n", $1);
      $$ = makeConstIntNode($1);
      }
    ;

dim_ref:
      '[' expr ']'         { $$ = $2; }
    | dim_ref '[' expr ']' {
        $$ = makeSiblings($1, $3);
      }
    ;

cexpr:
      CONST_INT       { $$ = $1->u.const_int; }
    | '(' cexpr ')'   { $$ = $2;      }
    | cexpr '+' cexpr { $$ = $1 + $3; }
    | cexpr '-' cexpr { $$ = $1 - $3; }
    | cexpr '*' cexpr { $$ = $1 * $3; }
    | cexpr '/' cexpr { $$ = $1 / $3; }
    ;

expr:
      ID {
        dbg("[expr] %s\n", $1);
        $$ = makeTypicalID($1, NULL);
      }
    | ID dim_ref {
        dbg("[expr] %s[]\n", $1);
        $$ = makeArrayID($1, $2);
      }
    | CONST_INT {
        dbg("[expr] const int=%d\n", $1->u.const_int);
        $$ = makeConstNode($1);
      }
    | CONST_FP {
        dbg("[expr] const fp=%f\n", $1->u.const_fp);
        $$ = makeConstNode($1);
      }
    | CONST_STR {
        dbg("[expr] const str=%s\n", $1->u.const_str);
        $$ = makeConstNode($1);
      }
    | '(' expr ')' {
        dbg("[expr] ()\n");
        $$ = $2;
      }
    | expr '+' expr {
        dbg("[expr] +\n");
        $$ = makeExprNode(OP_ADD, $1, $3);
      }
    | expr '-' expr {
        dbg("[expr] -\n");
        $$ = makeExprNode(OP_SUB, $1, $3);
      }
    | expr '*' expr {
        dbg("[expr] *\n");
        $$ = makeExprNode(OP_MUL, $1, $3);
      }
    | expr '/' expr {
        dbg("[expr] /\n");
        $$ = makeExprNode(OP_DIV, $1, $3);
      }
    | expr AND expr {
        dbg("[expr] &&\n");
        $$ = makeExprNode(OP_AND, $1, $3);
      }
    | expr OR expr {
        dbg("[expr] ||\n");
        $$ = makeExprNode(OP_OR, $1, $3);
      }
    | expr LT expr {
        dbg("[expr] <\n");
        $$ = makeExprNode(OP_LT, $1, $3);
      }
    | expr GT expr {
        dbg("[expr] >\n");
        $$ = makeExprNode(OP_GT, $1, $3);
      }
    | expr LE expr {
        dbg("[expr] <=\n");
        $$ = makeExprNode(OP_LE, $1, $3);
      }
    | expr GE expr {
        dbg("[expr] >=\n");
        $$ = makeExprNode(OP_GE, $1, $3);
      }
    | expr EQ expr {
        dbg("[expr] ==\n");
        $$ = makeExprNode(OP_EQ, $1, $3);
      }
    | expr NE expr {
        dbg("[expr] !=\n");
        $$ = makeExprNode(OP_NE, $1, $3);
      }
    | '!' expr {
        dbg("[expr] !\n");
        $$ = makeExprNode(OP_NOT, $2, NULL);
      }
    | '-' expr {
        dbg("[expr] neg\n");
        $$ = makeExprNode(OP_NEG, $2, NULL);
      }
    | ID '(' argument_list ')' {
        dbg("[expr] func call");
        $$ = makeExprNode(OP_CALL, makeTypicalID($1, NULL), makeListNode($3, LIST_ARGUMENT));
      }

    ;

assign_list:
      { $$ = makeNullNode(); /* empty */ }
    | assign_expr                 { $$ = $1;                   }
    | assign_list ',' assign_expr { $$ = makeSiblings($1, $3); }
    ;

assign_expr:
      ID '=' expr {
        $$ = makeStmtNode(STMT_ASSIGN);
        makeFamily($$, 2, makeTypicalID($1, NULL), $3);
      }
    | ID dim_ref '=' expr {
        $$ = makeStmtNode(STMT_ASSIGN);
        makeFamily($$, 2, makeArrayID($1, $2), $4);
      }
    ;

argument_list:
      { $$ = makeNullNode(); /* empty */ }
    | expr                   { $$ = $1;                   }
    | argument_list ',' expr { $$ = makeSiblings($1, $3); }
    ;

type:
      INT   { $$ = makeTypeNode("int");   }
    | FLOAT { $$ = makeTypeNode("float"); }
    ;

%%
