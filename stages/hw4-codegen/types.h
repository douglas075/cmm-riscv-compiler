#ifndef __TYPES_H
#define __TYPES_H

#define error(fmt, args...)                 \
  do {                                      \
    fprintf(stderr, "Error: " fmt, ##args); \
    exit(0);                                \
  } while (0)

typedef enum DataType {
  TYPE_INVALID = 0,
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_VOID,
  TYPE_STRING,
  TYPE_INT_PTR,  /* array of integer in function parameter */
  TYPE_FLOAT_PTR /* array of float in function parameter */
} DataType;

enum {
  K_NULL = 0,
  K_PROGRAM,
  K_FUNCTION,
  K_BLOCK,
  K_LIST,
  K_DECL,
  K_STMT,
  K_EXPR,
  K_CONST,
  K_ID,
  K_TYPE,
};

typedef struct const_val {
  DataType type;
  union {
    int   const_int;
    float const_fp;
    char  *const_str;
  } u;
} const_val;

/* semantic of const node */
typedef struct const_val SemanticConst;

/* semantic of type node */
struct SemanticType {
  char *name;
};

/* semantic of ID node */
struct SemanticID {
  enum {
    ID_TYPICAL,
    ID_TYPICAL_ASSIGN,
    ID_ARRAY
  } type;
  char *name;
};

/* semantic of block node */
struct SemanticProgram {
  unsigned has_decl;
  unsigned has_func;
};

/* semantic of block node */
struct SemanticBlock {
  unsigned has_decl;
  unsigned has_stmt;
};

/* semantic of list node */
struct SemanticList {
  enum {
    LIST_FUNC,
    LIST_PARAM,
    LIST_DECL,
    LIST_STMT,
    LIST_ASSIGN,
    LIST_ARGUMENT
  } type;
};

/* semantic of decl node */
struct SemanticDecl {
  enum {
    DECL_VAR,
    DECL_TYPEDEF
  } type;
};

/* semantic of stmt node */
struct SemanticStmt {
  enum {
    STMT_ASSIGN,
    STMT_WHILE,
    STMT_IF,
    STMT_FOR,
    STMT_RETURN,
    STMT_CALL
  } type;
};

/* semantic of expr node */
struct SemanticExpr {
  enum  {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_AND, OP_OR,
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    OP_NOT, OP_NEG
  } type;
};

/* AST node type */
typedef struct ASTNode {
  int kind; /* node type */
  DataType type;
  struct ASTNode *parent;
  struct ASTNode *child;
  struct ASTNode *rightSib;
  struct ASTNode *leftmostSib;
  int place;	/* mapped register */

  union {
    SemanticConst *constant;
    struct SemanticType type;
    struct SemanticID id;
    struct SemanticProgram program;
    struct SemanticBlock block;
    struct SemanticList list;
    struct SemanticDecl decl;
    struct SemanticStmt stmt;
    struct SemanticExpr expr;
  } semantic; 
} ASTNode;

extern ASTNode *ast_root;
extern int lineno;
extern int debug_parsing;
extern int debug_ast;
extern int debug_semantic;
extern char error_msg[];

int yyparse(void);
int yyerror(char *s);
void print_ast();
void semantic_analysis();
void code_generation();

#endif
