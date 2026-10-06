#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "symbol.h"

/* Handled syntax/semantic errors include:
 *  symbol redeclaration
 *  symbol undeclared
 *  use subscript in not-array variable
 *  pointer type not supported
 *  invalid expression
 *  subscript of array is not an integer
 *  cannot convert float value to integer symbol
 *  return type mismatch
 *  too many/few arguments to function
 *  argument type mismatch
 *  cannot find main function */

#define dbg(fmt, args...)           \
  do {                              \
    if (debug_semantic)             \
      fprintf(stderr, fmt, ##args); \
  } while (0)

static void visitProgram(ASTNode *node);
static void visitBlock(ASTNode *node);
static void visitDeclList(ASTNode *node);
static void visitFuncList(ASTNode *node);
static void visitStmtList(ASTNode *node);
static void visitDecl(ASTNode *node);
static void visitFunc(ASTNode *node);
static void visitStmt(ASTNode *node);
static void visitCall(ASTNode *Call);

static SymbolTableEntry *CurrentFunc;

char error_msg[256];

/*
 * Print AST
 */
static void printList(ASTNode *node)
{
  char *msg;
  switch (node->semantic.list.type) {
    case LIST_FUNC:     msg = "func";     break;
    case LIST_PARAM:    msg = "param";    break;
    case LIST_DECL:     msg = "decl";     break;
    case LIST_STMT:     msg = "stmt";     break;
    case LIST_ASSIGN:   msg = "assign";   break;
    case LIST_ARGUMENT: msg = "argument"; break;
    default: break;
  }
  fprintf(stdout, "%s_list\n", msg);
}

static void printDecl(ASTNode *node)
{
  if (node->semantic.decl.type == DECL_VAR)
    fprintf(stdout, "decl (variable)\n");
  else if (node->semantic.decl.type == DECL_TYPEDEF)
    fprintf(stdout, "decl (typedef)\n");
  else
    error("%s: internal error\n", __func__);
}

static void printStmt(ASTNode *node)
{
  char *stmt = "invalid";
  switch (node->semantic.stmt.type) {
    case STMT_ASSIGN: stmt = "="; break;
    case STMT_WHILE:  stmt = "while"; break;
    case STMT_IF:     stmt = "if"; break;
    case STMT_FOR:    stmt = "for"; break;
    case STMT_RETURN: stmt = "return"; break;
    case STMT_CALL:   stmt = "call"; break;
    default: break;
  }
  fprintf(stdout, "stmt (%s)\n", stmt);
}

static void printExpr(ASTNode *node)
{
  char *op = "invalid";
  switch (node->semantic.expr.type) {
    case OP_ADD: op = "+"; break;
    case OP_SUB: op = "-"; break;
    case OP_MUL: op = "*"; break;
    case OP_DIV: op = "/"; break;
    case OP_AND: op = "&&"; break;
    case OP_OR:  op = "||"; break;
    case OP_LT:  op = "<"; break;
    case OP_GT:  op = ">"; break;
    case OP_LE:  op = "<="; break;
    case OP_GE:  op = ">="; break;
    case OP_EQ:  op = "=="; break; 
    case OP_NE:  op = "!="; break;
    case OP_NOT: op = "!"; break;
    case OP_NEG: op = "-"; break;
    default: break;
  }
  fprintf(stdout, "expr (%s)\n", op);
}

static void printConst(ASTNode *node)
{
  SemanticConst *c = node->semantic.constant;
  if (c->type == TYPE_INT)
    fprintf(stdout, "const (%d)\n", c->u.const_int);
  else if (c->type == TYPE_FLOAT)
    fprintf(stdout, "const (%f)\n", c->u.const_fp);
  else if (c->type == TYPE_STRING)
    fprintf(stdout, "const (%s)\n", c->u.const_str);
  else
    error("%s: internal error\n", __func__);
}

static void printID(ASTNode *node)
{
  struct SemanticID *id = &node->semantic.id;
  if (id->type == ID_TYPICAL)
    fprintf(stdout, "id (%s)\n", id->name);
  else if (id->type == ID_TYPICAL_ASSIGN)
    fprintf(stdout, "id (%s) = \n", id->name);
  else if (id->type == ID_ARRAY)
    fprintf(stdout, "id (%s[])\n", id->name);
  else
    error("%s: internal error\n", __func__);
}

/* Print the abstract syntax tree (AST). */
void printAST(ASTNode *node, int indent)
{
  int i;
  for (i = 0; i < indent; ++i) fprintf(stdout, " ");

  switch (node->kind)
  {
    case K_NULL:     fprintf(stdout, "null\n"); break;
    case K_PROGRAM:  fprintf(stdout, "program\n"); break;
    case K_FUNCTION: fprintf(stdout, "function\n"); break;
    case K_BLOCK:    fprintf(stdout, "block\n"); break;
    case K_LIST:     printList(node); break;
    case K_DECL:     printDecl(node); break;
    case K_STMT:     printStmt(node); break;
    case K_EXPR:     printExpr(node); break;
    case K_CONST:    printConst(node); break;
    case K_ID:       printID(node); break;
    case K_TYPE:
      fprintf(stdout, "type (%s)\n", node->semantic.type.name);
      break;
    default:
      break;
  }

  ASTNode *next = node->child;
  while (next) {
    printAST(next, indent + 4);
    next = next->rightSib;
  }
}

void print_ast()
{
  if (!debug_ast)
    return;

  fprintf(stdout, "--------------------\n\tAST\n--------------------\n");
  printAST(ast_root, 0);
}


/*
 * Semantic analysis
 */
static inline char *get_typename(ASTNode *Type)
{
  if (Type->kind == K_TYPE)
    return Type->semantic.type.name;
  return Type->semantic.id.name; /* typedef */
}

static inline char *get_id(ASTNode *ID)
{
  return ID->semantic.id.name;
}

static int get_num_dimensions(ASTNode *ID)
{
  int num_dimensions = 0;
  if (ID->semantic.id.type == ID_ARRAY) {
    ASTNode *child = ID->child;
    while (child) {
      ++num_dimensions;
      child = child->rightSib;
    }
  }
  return num_dimensions;
}

static int get_num_params(ASTNode *ParamList)
{
  int num_params = 0;
  ASTNode *child = ParamList->child;
  if (child->kind == K_NULL)
    num_params = 0;
  else {
    while (child) {
      ++num_params;
      child = child->rightSib;
    }
  }
  return num_params;
}

static int get_num_arguments(ASTNode *ArgumentList)
{
  int num_arguments = 0;
  ASTNode *child = ArgumentList->child;
  if (child->kind == K_NULL)
    num_arguments = 0;
  else {
    while (child) {
      ++num_arguments;
      child = child->rightSib;
    }
  }
  return num_arguments;
}

static void build_var_desc(VarDesc *Var, ASTNode *Type, ASTNode *ID)
{
  int i;
  char *TypeName = get_typename(Type);

  SymbolTableEntry *entry = retrieve_symbol(TypeName);
  if (!entry || entry->type.is_function)
    error("unknown type name `%s'.\n", TypeName);

  /* num_dimensions =
   *   dimensions from typedef/primitive + dimensions of this var */
  Var->is_typedef = (Type->kind == K_ID);
  Var->type = entry->type.u.var.type;
  Var->num_dimensions = 
    entry->type.u.var.num_dimensions + get_num_dimensions(ID);
  Var->dimensions = (int *)malloc(Var->num_dimensions * sizeof(int));

  /* set dimensions from typedef/primitive */
  for (i = 0; i < entry->type.u.var.num_dimensions; ++i)
    Var->dimensions[i] = entry->type.u.var.dimensions[i];

  /* set dimensions from this var */
  if (ID->semantic.id.type == ID_ARRAY) {
    ASTNode *child = ID->child;
    while (child) {
      Var->dimensions[i++] = 
        child->semantic.constant->u.const_int;
      child = child->rightSib;
    }
  }
}

static void visitDecl(ASTNode *node)
{
  if (node->kind != K_DECL)
    error("%s: internal error\n", __func__);

  /* DeclNode format: type, id, id, ... */
  ASTNode *Type = node->child;
  ASTNode *ID = Type->rightSib;

  while (ID) {
    /* build the type description */
    SymbolType type;
    type.is_function = 0;
    build_var_desc(&type.u.var, Type, ID);

    /* insert type into the symbol table */
    SymbolTableEntry *symbol = enter_symbol(get_id(ID), &type);
    if (!symbol)
      error("%s.\n", error_msg);

    ID = ID->rightSib;
    free(type.u.var.dimensions);
  }
}

static void visitDeclList(ASTNode *node)
{
  if (node->semantic.list.type != LIST_DECL)
    error("%s: internal error\n", __func__);

  ASTNode *Decl = node->child;
  while (Decl) {
    visitDecl(Decl);
    Decl = Decl->rightSib;
  }
}

static int visitExpr(ASTNode *Expr)
{
  if (Expr->type != TYPE_INVALID)
    return Expr->type;

  if (Expr->kind == K_ID) {
    char *id = get_id(Expr);
    SymbolTableEntry *entry = retrieve_symbol(id);
    if (!entry || entry->type.is_function)
      error("symbol undeclared `%s'.\n", id);

    int num_dimensions = entry->type.u.var.num_dimensions;
    if (num_dimensions == 0) {
      if (get_num_dimensions(Expr) != 0)
        error("subscripted value is not an array.\n");
    } else {
      /* array type */
      if (num_dimensions != get_num_dimensions(Expr))
        error("pointer type not supported.\n");
    }

    Expr->type = entry->type.u.var.type;
    return Expr->type;
  }

  if (Expr->kind == K_CONST) {
    Expr->type = Expr->semantic.constant->type;
    return Expr->type;
  }

  if (Expr->kind == K_STMT && Expr->semantic.stmt.type == STMT_CALL) {
    visitCall(Expr);
    char *id = get_id(Expr->child);
    SymbolTableEntry *entry = retrieve_symbol(id);
    if (!entry || !entry->type.is_function)
      error("symbol undeclared `%s'.\n", id);

    Expr->type = entry->type.u.func.return_type;
    return Expr->type;
  }

  /* K_EXPR */
  Expr->type = visitExpr(Expr->child);
  if (Expr->child->rightSib) {
    int type2 = visitExpr(Expr->child->rightSib);
    if (Expr->type == TYPE_INT && type2 == TYPE_INT) ;
    else if (Expr->type == TYPE_INT && type2 == TYPE_FLOAT)
      Expr->type = TYPE_FLOAT;
    else if (Expr->type == TYPE_FLOAT && type2 == TYPE_INT) ;
    else if (Expr->type == TYPE_FLOAT && type2 == TYPE_FLOAT) ;
    else
      error("invalid expression.\n");
  } 
  return Expr->type;
}

static void visitAssign(ASTNode *Assign)
{
  ASTNode *ID = Assign->child;
  ASTNode *Expr = ID->rightSib;
  char *id = get_id(ID);

  SymbolTableEntry *entry = retrieve_symbol(id);
  if (!entry || entry->type.is_function)
    error("symbol undeclared `%s'.\n", id);

  if (ID->semantic.id.type == ID_ARRAY) {
    ASTNode *child = ID->child; /* dim_ref */
    while (child) {
      int type = visitExpr(child);
      if (type != TYPE_INT)
        error("subscript of array `%s' is not an integer.\n", id);
      child = child->rightSib;
    }
  }

  int rhs_type = visitExpr(Expr);
  int lhs_type = entry->type.u.var.type;
  if (lhs_type == TYPE_INT && rhs_type == TYPE_FLOAT)
    error("cannot convert float value to integer symbol `%s'.\n", id);

  Assign->type = lhs_type;
}

static void visitWhile(ASTNode *While)
{
  ASTNode *Expr = While->child;
  ASTNode *Stmt = Expr->rightSib;

  visitExpr(Expr);
  visitStmt(Stmt);
}

static void visitIf(ASTNode *If)
{
  ASTNode *Expr = If->child;
  ASTNode *IfTrue = Expr->rightSib;
  ASTNode *IfFalse = IfTrue->rightSib;

  visitExpr(Expr);
  visitStmt(IfTrue);
  visitStmt(IfFalse);
}

static void visitFor(ASTNode *For)
{
  ASTNode *AssignList1 = For->child;
  ASTNode *Expr = AssignList1->rightSib;
  ASTNode *AssignList2 = Expr->rightSib;
  ASTNode *Stmt = AssignList2->rightSib;

  if (Expr->kind != K_NULL)
    visitExpr(Expr);
  visitStmt(Stmt);
}

static void visitReturn(ASTNode *Return)
{
  ASTNode *node = Return->child;
  int return_type = CurrentFunc->type.u.func.return_type;

  if (node->kind == K_NULL) {
    if (return_type != TYPE_VOID)
      fprintf(stderr, "Warning: return with no value in function `%s'.\n",
              CurrentFunc->name);
    return;
  }

  int type = visitExpr(node);
  if (return_type != type)
    error("return type mismatch in function `%s'.\n", CurrentFunc->name);
}

static void replace_library_function(ASTNode *Call)
{
  ASTNode *ID = Call->child;
  ASTNode *ArgumentList = ID->rightSib;
  char *id = get_id(ID);

  /* skip if this is not the library function */
  if (strcmp(id, "read") && strcmp(id, "fread") && strcmp(id, "write"))
    return;

  int num_arguments = get_num_arguments(ArgumentList);
  if (strcmp(id, "read") == 0)
    ID->semantic.id.name = "read_int";
  else if (strcmp(id, "fread") == 0)
    ID->semantic.id.name = "read_fp";
  else if (strcmp(id, "write") == 0) {
    if (num_arguments != 1)
      error("library function `%s' must have one argument\n", id);

    ASTNode *Expr = ArgumentList->child;
    int type = visitExpr(Expr);
    if (type == TYPE_INT)
      ID->semantic.id.name = "write_int";
    else if (type == TYPE_FLOAT)
      ID->semantic.id.name = "write_fp";
    else if (type == TYPE_STRING)
      ID->semantic.id.name = "write_str";
  }
}

static void visitCall(ASTNode *Call)
{
  ASTNode *ID = Call->child;
  ASTNode *ArgumentList = ID->rightSib;

  /* We first rewrite the names of library functions. */
  replace_library_function(Call);

  char *id = get_id(ID);
  int num_arguments = get_num_arguments(ArgumentList);

  SymbolTableEntry *entry = retrieve_symbol(id);
  if (!entry || !entry->type.is_function)
    error("symbol undeclared `%s'.\n", id);

  if (num_arguments > entry->type.u.func.num_params)
    error("too many arguments to function `%s'.\n", entry->name);
  else if (num_arguments < entry->type.u.func.num_params)
    error("too few arguments to function `%s'.\n", entry->name);

  int i = 0;
  VarDesc *params = entry->type.u.func.param_types;
  if (num_arguments != 0) {
    ASTNode *Expr = ArgumentList->child;
    while (Expr) {
      int type = visitExpr(Expr);
      if (params[i].type == type) ;
      else if (params[i].type == TYPE_FLOAT && type == TYPE_INT) ;
      else
        error("argument type mismatch.\n");
      ++i;
      Expr = Expr->rightSib;
    }
  }
}

static void visitStmt(ASTNode *node)
{
  if (node->kind == K_BLOCK) {
    visitBlock(node);
    return;
  }
  if (node->kind == K_NULL)
    return;

  if (node->kind != K_STMT)
    error("%s: internal error\n", __func__);

  int type = node->semantic.stmt.type;
  if (type == STMT_ASSIGN) {
    /* Format: ID = expr or ID dim_ref = expr */
    visitAssign(node);
  } else if (type == STMT_WHILE) {
    /* Format: WHILE expr stmt */
    visitWhile(node);
  } else if (type == STMT_IF) {
    /* Format: IF expr stmt [ELSE stmt] */
    visitIf(node);
  } else if (type == STMT_FOR) {
    /* Format: FOR assign_list [expr] assign_list stmt */
    visitFor(node);
  } else if (type == STMT_RETURN) {
    /* Format: RETURN or RETURN expr */
    visitReturn(node);
  } else if (type == STMT_CALL) {
    /* Format: ID argument_list */
    visitCall(node);
  }
}

static void visitStmtList(ASTNode *node)
{
  if (node->semantic.list.type != LIST_STMT)
    error("%s: internal error\n", __func__);

  ASTNode *Stmt = node->child;
  while (Stmt) {
    if (Stmt->kind == K_BLOCK)
      visitBlock(Stmt);
    else if (Stmt->kind == K_STMT)
      visitStmt(Stmt);
    else
      ; /* Null node. Do nothing. */

    Stmt = Stmt->rightSib;
  }
}

static void visitBlock(ASTNode *node)
{
  if (node->kind != K_BLOCK)
    error("%s: internal error\n", __func__);

  open_scope();

  ASTNode *ListNode = node->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclList(ListNode);
    else
      visitStmtList(ListNode);

    ListNode = ListNode->rightSib;
  }

  close_scope();
}

static void visitFunc(ASTNode *node)
{
  if (node->kind != K_FUNCTION)
    error("%s: internal error\n", __func__);

  /* FuncNode format: type, id, param_list, block */
  ASTNode *Type = node->child;
  ASTNode *ID = Type->rightSib;
  ASTNode *ParamList = ID->rightSib;
  ASTNode *Block = ParamList->rightSib;

  char *ReturnTypeName = get_typename(Type);
  SymbolTableEntry *entry = retrieve_symbol(ReturnTypeName);
  if (!entry || entry->type.is_function)
    error("unknown type name `%s'.\n", ReturnTypeName);

  int i = 0;
  int num_params = get_num_params(ParamList);

  /* build function signature */
  SymbolType func_type;
  func_type.is_function = 1;
  func_type.u.func.return_type = entry->type.u.var.type;
  func_type.u.func.num_params = num_params;
  if (num_params != 0) {
    func_type.u.func.param_types =
      (VarDesc *)malloc(num_params * sizeof(VarDesc));

    ASTNode *param = ParamList->child;
    while (param) {
      build_var_desc(&func_type.u.func.param_types[i++],
                     param->child,
                     param->child->rightSib);
      param = param->rightSib;
    }
  }

  /* insert function signature into the symbol table */
  SymbolTableEntry *symbol = enter_symbol(get_id(ID), &func_type);
  if (!symbol)
    error("%s.\n", error_msg);

  CurrentFunc = symbol;

  open_scope();

  /* insert function parameters into the symbol table */
  if (num_params != 0) {
    ASTNode *param = ParamList->child;
    while (param) {
      ASTNode *Type = param->child;
      ASTNode *ID = Type->rightSib;

      SymbolType param_type;
      param_type.is_function = 0;
      build_var_desc(&param_type.u.var, Type, ID);

      SymbolTableEntry *symbol = enter_symbol(get_id(ID), &param_type);
      if (!symbol)
        error("%s.\n", error_msg);
      param = param->rightSib;
      free(param_type.u.var.dimensions);
    }
  }

  ASTNode *ListNode = Block->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclList(ListNode);
    else
      visitStmtList(ListNode);

    ListNode = ListNode->rightSib;
  }

  close_scope();
}

static void visitFuncList(ASTNode *node)
{
  if (node->semantic.list.type != LIST_FUNC)
    error("%s: internal error\n", __func__);

  ASTNode *Func = node->child;
  while (Func) {
    visitFunc(Func);
    Func = Func->rightSib;
  }
}

static void visitProgram(ASTNode *node)
{
  if (node->kind != K_PROGRAM)
    error("%s: internal error\n", __func__);

  open_scope();

  ASTNode *ListNode = node->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclList(ListNode);
    else
      visitFuncList(ListNode);

    ListNode = ListNode->rightSib;
  }

  SymbolTableEntry *entry = retrieve_symbol("main");
  if (!entry || !entry->type.is_function)
    error("cannot find main function.\n");

  close_scope();

}

void semantic_analysis()
{
  visitProgram(ast_root);
}

