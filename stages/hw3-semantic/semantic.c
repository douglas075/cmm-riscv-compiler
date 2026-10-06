#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "types.h"
#include "symbol.h"

static void visitProgramNode(ASTNode *node);
static void visitBlockNode(ASTNode *node);
static void visitDeclListNode(ASTNode *node);
static void visitFuncListNode(ASTNode *node);
static void visitStmtListNode(ASTNode *node);
static void visitDeclNode(ASTNode *node);
static void visitFuncNode(ASTNode *node);
static void visitStmtNode(ASTNode *node);

static DataType visitIDNode(ASTNode *node);
static DataType visitExprNode(ASTNode *node);
static void visitAssignStmtNode(ASTNode *node);

inline static DataType type_name_to_datatype(char *type_name);
// inline static DataType datatype_to_pointer_datatype(DataType type);
static VarDesc idnode_to_vardesc(char *type_name, ASTNode *IDNode);

// Datatype may be TYPE_VOID, TYPE_STRING
inline static DataType type_name_to_datatype(char *type_name)
{
  if (strcmp(type_name, "int") == 0)
    return TYPE_INT;
  if (strcmp(type_name, "float") == 0)
    return TYPE_FLOAT;
  if (strcmp(type_name, "void") == 0)
    return TYPE_VOID;
  if (strcmp(type_name, "string") == 0)
    return TYPE_STRING;
  return TYPE_INVALID;
}

static inline char *op_to_str(int type){
  char *op = "invalid";
  switch (type) {
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
  return op;
}

// inline static DataType datatype_to_pointer_datatype(DataType type)
// {
//     switch (type) {
//     case TYPE_INT:   return TYPE_INT_PTR;
//     case TYPE_FLOAT: return TYPE_FLOAT_PTR;
//     default:         return TYPE_INVALID;
//   }
// }

// Convert an ID node to VarDesc
// VarDesc.type may be TYPE_VOID, TYPE_STRING
static VarDesc idnode_to_vardesc(char *type_name, ASTNode *IDNode)
{
  DataType base_type = type_name_to_datatype(type_name);
  SymbolTableEntry *entry = NULL;
  int base_type_arr_dimensions = 0;
  if (base_type == TYPE_INVALID) {
    entry = retrieve_symbol(type_name);
    if (!entry || entry->type.is_function || entry->type.u.var.kind != SYM_TYPEDEF) {
      error("Unknown type name: %s\n", type_name);
    } else {
      base_type = entry->type.u.var.type;
      base_type_arr_dimensions = entry->type.u.var.num_dimensions;
    }
  }

  VarDesc var;
  if (IDNode->semantic.id.type == ID_ARRAY) {
    var.kind = SYM_ARRAY;
    var.type = base_type;
    var.num_dimensions = 0;
    ASTNode *DimNode = IDNode->child;
    while (DimNode) {
      var.num_dimensions++;
      DimNode = DimNode->rightSib;
    }
    var.dimension = malloc(sizeof(int) * (var.num_dimensions + base_type_arr_dimensions));
    DimNode = IDNode->child;
    for (int i = 0; i < var.num_dimensions; ++i) {
      if (DimNode->kind == K_NULL) { // is null node -> first dim empty (in func param) 
        if (i != 0) {
          error("the dimension %d cannot be empty\n", i + 1);
        }
        var.dimension[0] = INT_MAX;
      }
      else if (DimNode->semantic.constant->type != TYPE_INT) {
        error("Array dimension is not an integer\n");
      }
      else
        var.dimension[i] = DimNode->semantic.constant->u.const_int;
      DimNode = DimNode->rightSib;
    }
    for (int i = 0; i < base_type_arr_dimensions; i++) {
      var.dimension[var.num_dimensions + i] = entry->type.u.var.dimension[i];
    }
    var.num_dimensions += base_type_arr_dimensions;
    
  } else { // ID_TYPICAL and ID_TYPICAL_ASSIGN
    if (base_type_arr_dimensions > 0) { // refer to a typedef array
      var.kind = SYM_ARRAY;
      var.type = base_type;
      var.num_dimensions = base_type_arr_dimensions;
      var.dimension = malloc(sizeof(int) * base_type_arr_dimensions);
      for (int i = 0; i < base_type_arr_dimensions; i++) {
        var.dimension[i] = entry->type.u.var.dimension[i];
      }
    } else {
      var.kind = SYM_TYPICAL;
      var.type = base_type;
      var.num_dimensions = 0;
      var.dimension = NULL;
    }
  }
  return var;
}

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
    fprintf(stdout, "decl (typdef)\n");
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
    case OP_CALL: op = "func call"; break;
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
static void visitDeclNode(ASTNode *node)
{
  if (node->kind != K_DECL)
    error("%s: internal error\n", __func__);

  /* DeclNode format: type, id, id, ... */
  ASTNode *TypeNode = node->child;
  ASTNode *IDNode = TypeNode->rightSib;

  SymbolType type;
  char *type_name;
  if (TypeNode->kind == K_TYPE) {
    type_name = TypeNode->semantic.type.name;
  } else { // K_ID (typedef)
    type_name = TypeNode->semantic.id.name;
  }

  // typedef declaration
  if (node->semantic.decl.type == DECL_TYPEDEF){
    type.is_function = 0;
    type.u.var = idnode_to_vardesc(type_name, IDNode);
    type.u.var.kind = SYM_TYPEDEF;
    enter_symbol(IDNode->semantic.id.name, &type);
    free_symboltype_things(&type);
    return;
  };

  // variable declaration
  while (IDNode) {
    type.is_function = 0;
    type.u.var = idnode_to_vardesc(type_name, IDNode);
    if (type.u.var.type == TYPE_VOID) {
      error("Variable %s declared with void type\n", IDNode->semantic.id.name);
    }
    enter_symbol(IDNode->semantic.id.name, &type);
    free_symboltype_things(&type);

    IDNode = IDNode->rightSib;
  }
}

static void visitDeclListNode(ASTNode *node)
{
  if (node->semantic.list.type != LIST_DECL)
    error("%s: internal error\n", __func__);

  ASTNode *DeclNode = node->child;
  while (DeclNode) {
    visitDeclNode(DeclNode);
    DeclNode = DeclNode->rightSib;
  }
}

static DataType visitIDNode(ASTNode *node){
  if (node->kind != K_ID)
    error("%s: internal error\n", __func__);

  SymbolTableEntry *entry = retrieve_symbol(node->semantic.id.name);
  if (!entry || entry->type.is_function || entry->type.u.var.kind == SYM_TYPEDEF)
    error("Undeclared identifier: %s\n", node->semantic.id.name);

  if (node->semantic.id.type == ID_ARRAY && entry->type.u.var.kind == SYM_ARRAY) {
    ASTNode *DimNode = node->child;
    int num_dimentions = 0;
    while (DimNode) {
      if (visitExprNode(DimNode) != TYPE_INT) {
        error("Array subscript is not an integer for array %s\n", node->semantic.id.name);
      }
      num_dimentions++;
      DimNode = DimNode->rightSib;
    }
    if (num_dimentions != entry->type.u.var.num_dimensions) {
      error("Array subscript number mismatch for array %s\n", node->semantic.id.name);
    }
    return entry->type.u.var.type;
  }

  if (node->semantic.id.type == ID_TYPICAL && entry->type.u.var.kind == SYM_TYPICAL) {
    return entry->type.u.var.type;
  }

  if (node->semantic.id.type == ID_TYPICAL && entry->type.u.var.kind == SYM_ARRAY) {
    // Using an array name as an expression (e.g., passing it to a function)
    // is valid — treat it as the array's base type (decayed pointer).
    return entry->type.u.var.type;
  }


  error("Invalid use of identifier: %s\n", node->semantic.id.name);
}

// TODO: partial array subscript support
static DataType visitExprNode(ASTNode *node)
{
  if (node->kind != K_EXPR && node->kind != K_CONST && node->kind != K_ID)
    error("%s: internal error\n", __func__);

  if (node->kind == K_CONST) {
    return node->semantic.constant->type;
  }
  
  if (node->kind == K_ID) {
    return visitIDNode(node);
  }

  ASTNode *left_node, *right_node;
  DataType left_type, right_type;
  int type = node->semantic.expr.type;
  if (type == OP_ADD || type == OP_SUB || type == OP_MUL || type == OP_DIV) {
    /* Format: expr op expr */
    left_node = node->child;
    right_node = left_node->rightSib;
    left_type = visitExprNode(left_node);
    right_type = visitExprNode(right_node);
    if ((left_type == TYPE_INT && right_type == TYPE_INT)
        || (left_type == TYPE_FLOAT && right_type == TYPE_FLOAT))
      return left_type;

    if ((left_type == TYPE_INT && right_type == TYPE_FLOAT)
        || (left_type == TYPE_FLOAT && right_type == TYPE_INT))
      return TYPE_FLOAT;
  
    error("Invalid expression %s %s %s\n", data_type_to_string(left_type), op_to_str(type), data_type_to_string(right_type));
  
  } else if (type == OP_AND || type == OP_OR) {
    /* Format: expr op expr */
    left_node = node->child;
    right_node = left_node->rightSib;
    left_type = visitExprNode(left_node);
    right_type = visitExprNode(right_node);
    if (left_type == TYPE_INT && right_type == TYPE_INT)
      return TYPE_INT;
    error("Invalid expression %s %s %s\n", data_type_to_string(left_type), op_to_str(type), data_type_to_string(right_type));

  } else if (type == OP_LT || type == OP_GT || type == OP_LE ||
             type == OP_GE || type == OP_EQ || type == OP_NE) {
    /* Format: expr op expr */
    left_node = node->child;
    right_node = left_node->rightSib;
    left_type = visitExprNode(left_node);
    right_type = visitExprNode(right_node);
    if ((left_type == TYPE_INT && right_type == TYPE_INT)
        || (left_type == TYPE_FLOAT && right_type == TYPE_FLOAT))
      return TYPE_INT;
    if ((left_type == TYPE_INT && right_type == TYPE_FLOAT)
        || (left_type == TYPE_FLOAT && right_type == TYPE_INT))
    {
      warning("Implicit type conversion in expression %s %s %s\n", data_type_to_string(left_type), op_to_str(type), data_type_to_string(right_type));
      return TYPE_INT;
    }
    error("Invalid expression %s %s %s\n", data_type_to_string(left_type), op_to_str(type), data_type_to_string(right_type));

  } else if (type == OP_NOT) {
    /* Format: op expr */
    left_node = node->child;
    left_type = visitExprNode(left_node);
    if (left_type == TYPE_INT)
      return left_type;
    error("Invalid expression %s %s\n", op_to_str(type), data_type_to_string(left_type));

  } else if (type == OP_NEG) {
    /* Format: op expr */
    left_node = node->child;
    left_type = visitExprNode(left_node);
    if (left_type == TYPE_INT || left_type == TYPE_FLOAT)
      return left_type;
    error("Invalid expression %s %s\n", op_to_str(type), data_type_to_string(left_type));
  } else if (type == OP_CALL) {
    ASTNode *IDNode = node->child;
    ASTNode *ArgumentListNode = IDNode->rightSib;
    SymbolTableEntry *entry = retrieve_symbol(IDNode->semantic.id.name);
    if (!entry || !entry->type.is_function) {
      error("Undeclared function: %s\n", IDNode->semantic.id.name);
    }
    int num_arguments = 0;
    ASTNode *ArgumentNode = ArgumentListNode->child;
    while (ArgumentNode && ArgumentNode->kind != K_NULL) {
      num_arguments++;
      ArgumentNode = ArgumentNode->rightSib;
    }
    if (num_arguments != entry->type.u.func.num_params) {
      error("Function %s called with incorrect number of arguments: expected %d, got %d\n",IDNode->semantic.id.name, entry->type.u.func.num_params, num_arguments);
    }
    ArgumentNode = ArgumentListNode->child;
    for (int i = 0; i < num_arguments; ++i) {
      DataType arg_type = visitExprNode(ArgumentNode);
      VarDesc param_desc = entry->type.u.func.param_types[i];
      DataType param_type = param_desc.type;

      if ((arg_type == TYPE_INT && param_type == TYPE_FLOAT)
               || (arg_type == TYPE_FLOAT && param_type == TYPE_INT)) {
        warning("Implicit type conversion in function call %s for parameter %d: expected %s, got %s\n", IDNode->semantic.id.name, i + 1, data_type_to_string(param_type), data_type_to_string(arg_type));
      } else if (arg_type != param_type) {
        error("Type mismatch in function call %s for parameter %d: expected %s, got %s\n", IDNode->semantic.id.name, i + 1, data_type_to_string(param_type), data_type_to_string(arg_type));
      }

      // param is arr
      if (param_desc.kind == SYM_ARRAY) {
        if (ArgumentNode->kind != K_ID) 
          error("parameter is array type but argument is not a id and not an array\n");
        SymbolTableEntry *arg_entry = retrieve_symbol(ArgumentNode->semantic.id.name);
        VarDesc *arg_desc = &arg_entry->type.u.var;
        if (arg_desc->kind != SYM_ARRAY) 
          error("parameter is array type but argument is a id but not an array\n");
      
        // num of dimension
        if (arg_desc->num_dimensions != param_desc.num_dimensions)
          error("number of dimension of array argument %s does not match the that of parameter\n", arg_entry->name);

        for (int d = 0; d < param_desc.num_dimensions; ++d) {
            if (d == 0 && param_desc.dimension[0] == INT_MAX)
              continue;
            if (arg_desc->dimension[d] != param_desc.dimension[d]) {
              
                error("Array dimension mismatch for parameter %d of %s: expected %d in dimension %d, got %d\n",
                      i + 1, IDNode->semantic.id.name,
                      param_desc.dimension[d], d + 1, arg_desc->dimension[d]);
            }
        }
      }

      // arg is arr but param is not
      if (ArgumentNode->kind == K_ID && retrieve_symbol(ArgumentNode->semantic.id.name)->type.u.var.kind == SYM_ARRAY)
        if (param_desc.kind != SYM_ARRAY)
          error("parameter is not array type but argument is an array\n");

      ArgumentNode = ArgumentNode->rightSib;
    }
    return entry->type.u.func.return_type;
  }
  error("Internal error in visitExprNode\n");
}

static void visitAssignStmtNode(ASTNode *node)
{
  ASTNode *IDNode = node->child;
  ASTNode *ExprNode = IDNode->rightSib;
  DataType left_type = visitIDNode(IDNode);
  DataType right_type = visitExprNode(ExprNode);
  if ((left_type == TYPE_INT && right_type == TYPE_INT)
      || (left_type == TYPE_FLOAT && right_type == TYPE_FLOAT))
    return;
  if ((left_type == TYPE_INT && right_type == TYPE_FLOAT)
      || (left_type == TYPE_FLOAT && right_type == TYPE_INT)) {
    warning("Implicit type conversion in assignment: %s = %s\n", data_type_to_string(left_type), data_type_to_string(right_type));
    return;
  }
  error("Type mismatch in assignment: %s = %s\n", data_type_to_string(left_type), data_type_to_string(right_type));
}

static void visitStmtNode(ASTNode *node)
{
  if (node->kind == K_NULL)
    return;
  if (node->kind == K_BLOCK) {
    visitBlockNode(node);
    return;
  }
  // printAST(node, 0);
  if (node->kind != K_STMT)
    error("%s: internal error\n", __func__);

  SymbolTableEntry *entry;
  //= retrieve_symbol(node->semantic.id.name);
  DataType left_type, right_type;
  int type = node->semantic.stmt.type;
  if (type == STMT_ASSIGN) {
    /* Format: ID = expr or ID dim_ref = expr */
    visitAssignStmtNode(node);
    return;
  } else if (type == STMT_WHILE) {
    /* Format: WHILE expr stmt */
    ASTNode *ExprNode = node->child;
    ASTNode *StmtNode = ExprNode->rightSib;
    left_type = visitExprNode(ExprNode);
    if (left_type != TYPE_INT && left_type != TYPE_FLOAT) {
      error("Non-numeric expression in while statement\n");
    }
    open_scope();
    visitStmtNode(StmtNode);
    close_scope();
    return;

  } else if (type == STMT_IF) {
    /* Format: IF expr stmt [ELSE stmt] */
    ASTNode *ExprNode = node->child;
    ASTNode *ThenStmtNode = ExprNode->rightSib;
    ASTNode *ElseStmtNode = ThenStmtNode->rightSib;
    left_type = visitExprNode(ExprNode);
    if (left_type != TYPE_INT && left_type != TYPE_FLOAT) {
      error("Non-numeric expression in if statement\n");
    }
    open_scope();
    visitStmtNode(ThenStmtNode);
    close_scope();
    if (ElseStmtNode->kind == K_STMT) {
      open_scope();
      visitStmtNode(ElseStmtNode);
      close_scope();
    }
    return;

  } else if (type == STMT_FOR) {
    /* Format: FOR assign_list [expr] assign_list stmt */
    ASTNode *InitAssignListNode = node->child;
    ASTNode *CondExprNode = InitAssignListNode->rightSib;
    ASTNode *LoopAssignListNode = CondExprNode->rightSib;
    ASTNode *StmtNode = LoopAssignListNode->rightSib;

    open_scope();
    ASTNode *AssignNode = InitAssignListNode->child;
    while (AssignNode && AssignNode->kind != K_NULL) {
      visitAssignStmtNode(AssignNode);
      AssignNode = AssignNode->rightSib;
    }
    if (CondExprNode->kind != K_NULL) {
      left_type = visitExprNode(CondExprNode);
      if (left_type != TYPE_INT && left_type != TYPE_FLOAT) {
        error("Non-numeric expression in for statement\n");
      }
    }
    AssignNode = LoopAssignListNode->child;
    while (AssignNode && AssignNode->kind != K_NULL) {
      visitAssignStmtNode(AssignNode);
      AssignNode = AssignNode->rightSib;
    }
    visitStmtNode(StmtNode);
    close_scope();
    return;
  } else if (type == STMT_RETURN) {
    /* Format: RETURN or RETURN expr */
    entry = retrieve_symbol(CURRENT_FUNCTION);
    if (!entry || !entry->type.is_function) {
      error("Internal error: return statement not in a function\n");
    }
    if (node->child->kind != K_NULL) {
      left_type = visitExprNode(node->child);
      if (left_type != entry->type.u.func.return_type) {
        if ((left_type == TYPE_INT && entry->type.u.func.return_type == TYPE_FLOAT)
            || (left_type == TYPE_FLOAT && entry->type.u.func.return_type == TYPE_INT)) {
          warning("Implicit type conversion in return statement: expected %s, got %s\n", data_type_to_string(entry->type.u.func.return_type), data_type_to_string(left_type));
        } else {
          error("Type mismatch in return statement: expected %s, got %s\n", data_type_to_string(entry->type.u.func.return_type), data_type_to_string(left_type));
        }
      }
    } else {
      if (entry->type.u.func.return_type != TYPE_VOID) {
        error("Non-void function must return a value\n");
      }
    }
    return;
  } else if (type == STMT_CALL) {
    /* Format: ID argument_list */
    ASTNode *IDNode = node->child;
    ASTNode *ArgumentListNode = IDNode->rightSib;
    SymbolTableEntry *entry = retrieve_symbol(IDNode->semantic.id.name);
    if (!entry || !entry->type.is_function) {
      error("Undeclared function: %s\n", IDNode->semantic.id.name);
    }
    int num_arguments = 0;
    ASTNode *ArgumentNode = ArgumentListNode->child;
    while (ArgumentNode && ArgumentNode->kind != K_NULL) {
      num_arguments++;
      ArgumentNode = ArgumentNode->rightSib;
    }
    if (num_arguments != entry->type.u.func.num_params) {
      error("Function %s called with incorrect number of arguments: expected %d, got %d\n",IDNode->semantic.id.name, entry->type.u.func.num_params, num_arguments);
    }
    ArgumentNode = ArgumentListNode->child;
    for (int i = 0; i < num_arguments; ++i) {
      DataType arg_type = visitExprNode(ArgumentNode);
      VarDesc param_desc = entry->type.u.func.param_types[i];
      DataType param_type = param_desc.type;

      if ((arg_type == TYPE_INT && param_type == TYPE_FLOAT)
               || (arg_type == TYPE_FLOAT && param_type == TYPE_INT)) {
        warning("Implicit type conversion in function call %s for parameter %d: expected %s, got %s\n", IDNode->semantic.id.name, i + 1, data_type_to_string(param_type), data_type_to_string(arg_type));
      } else if (arg_type != param_type) {
        error("Type mismatch in function call %s for parameter %d: expected %s, got %s\n", IDNode->semantic.id.name, i + 1, data_type_to_string(param_type), data_type_to_string(arg_type));
      }

      // param is arr
      if (param_desc.kind == SYM_ARRAY) {
        if (ArgumentNode->kind != K_ID) 
          error("parameter is array type but argument is not a id and not an array\n");
        SymbolTableEntry *arg_entry = retrieve_symbol(ArgumentNode->semantic.id.name);
        VarDesc *arg_desc = &arg_entry->type.u.var;
        if (arg_desc->kind != SYM_ARRAY) 
          error("parameter is array type but argument is a id but not an array\n");
      
        // num of dimension
        if (arg_desc->num_dimensions != param_desc.num_dimensions)
          error("number of dimension of array argument %s does not match the that of parameter\n", arg_entry->name);

        for (int d = 0; d < param_desc.num_dimensions; ++d) {
            if (d == 0 && param_desc.dimension[0] == INT_MAX)
              continue;
            if (arg_desc->dimension[d] != param_desc.dimension[d]) {
              
                error("Array dimension mismatch for parameter %d of %s: expected %d in dimension %d, got %d\n",
                      i + 1, IDNode->semantic.id.name,
                      param_desc.dimension[d], d + 1, arg_desc->dimension[d]);
            }
        }
      }

      // arg is arr but param is not
      if (ArgumentNode->kind == K_ID && retrieve_symbol(ArgumentNode->semantic.id.name)->type.u.var.kind == SYM_ARRAY)
        if (param_desc.kind != SYM_ARRAY)
          error("parameter is not array type but argument is an array\n");

      ArgumentNode = ArgumentNode->rightSib;
    }
    return;
  }
  error("Internal error in visitStmtNode\n");
}

static void visitStmtListNode(ASTNode *node)
{
  if (node->semantic.list.type != LIST_STMT)
    error("%s: internal error\n", __func__);

  ASTNode *child = node->child;
  while (child) {
    if (child->kind == K_BLOCK)
      visitBlockNode(child);
    else if (child->kind == K_STMT)
      visitStmtNode(child);
    else
      ; /* Null node. Do nothing. */

    child = child->rightSib;
  }
}

static void visitBlockNode(ASTNode *node)
{
  if (node->kind != K_BLOCK)
    error("%s: internal error\n", __func__);

  open_scope();

  ASTNode *ListNode = node->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: internal error: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclListNode(ListNode);
    else
      visitStmtListNode(ListNode);

    ListNode = ListNode->rightSib;
  }

  close_scope();
}

static void visitFuncNode(ASTNode *node)
{
  if (node->kind != K_FUNCTION)
    error("%s: internal error\n", __func__);

  /* FuncNode format: type, id, param_list, block */
  ASTNode *TypeNode = node->child;
  ASTNode *IDNode = TypeNode->rightSib;
  ASTNode *ParamListNode = IDNode->rightSib;
  ASTNode *BlockNode = ParamListNode->rightSib;

  SymbolType type;
  type.is_function = 1;
  char *type_name;
  if (TypeNode->kind == K_TYPE) {
    type_name = TypeNode->semantic.type.name;
  } else { // K_ID (typedef)
    type_name = TypeNode->semantic.id.name;
  }
  type.u.func.return_type = type_name_to_datatype(type_name);
  if (type.u.func.return_type == TYPE_INVALID){ // may be typedef
    SymbolTableEntry *entry = retrieve_symbol(type_name);
    if (!entry || entry->type.is_function || entry->type.u.var.kind != SYM_TYPEDEF) {
      error("Unknown type name: %s\n", type_name);
    }
    if (entry->type.u.var.num_dimensions > 0) {
      error("Function %s declared with array return type\n", IDNode->semantic.id.name);
    }
    type.u.func.return_type = entry->type.u.var.type;
  }



  ASTNode *ParamNode = ParamListNode->child;
  type.u.func.num_params = 0;
  while (ParamNode && ParamNode->kind != K_NULL) {
    type.u.func.num_params++;
    ParamNode = ParamNode->rightSib;
  }
  type.u.func.param_types = malloc(sizeof(VarDesc) * type.u.func.num_params);
  char **param_names = malloc(sizeof(char*) * type.u.func.num_params);
  ParamNode = ParamListNode->child;
  for (int i = 0; i < type.u.func.num_params; ++i) {
    ASTNode *ParamTypeNode = ParamNode->child;
    ASTNode *ParamIDNode = ParamTypeNode->rightSib;

    type.u.func.param_types[i] = idnode_to_vardesc(ParamTypeNode->semantic.type.name, ParamIDNode);
    param_names[i] = ParamIDNode->semantic.id.name;
    if (type.u.func.param_types[i].type == TYPE_VOID) {
      // may not happen, only function can be parsed as void now
      error("Function %s parameter %s declared with void type\n", IDNode->semantic.id.name, ParamIDNode->semantic.id.name);
    }
    if (type.u.func.param_types[i].num_dimensions > 0) {
      // type.u.func.param_types[i].type = datatype_to_pointer_datatype(type.u.func.param_types[i].type);
      type.u.func.param_types[i].type = type.u.func.param_types[i].type;
    }
    ParamNode = ParamNode->rightSib;
  }

  enter_symbol(IDNode->semantic.id.name, &type);
  // free_symboltype_things(&type); // free later

  open_scope();

  for (int i = 0; i < type.u.func.num_params; ++i) {
    SymbolType param_symbol_type;
    param_symbol_type.is_function = 0;
    param_symbol_type.u.var = type.u.func.param_types[i];
    enter_symbol(param_names[i], &param_symbol_type);
    // free_symboltype_things(&param_symbol_type); // can be used if param is arr
  }

  enter_symbol(CURRENT_FUNCTION, &type); // for return type checking
  // free_symboltype_things(&type);
  // type will be freed by close scope
  /*
  issue here: since we insert type twice, and the two "type"s inserted are 
  referencing to same object, which will at least be free once as CURRENT_FUNCTION
  insertion takes place in a scope. If we manually free it for another time, double
  free error will be trigger. Though one solution is making deep-copy, the overhead
  of refectoring is too large. Hope my modification will not cause other errors.
  */

  ASTNode *ListNode = BlockNode->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclListNode(ListNode);
    else
      visitStmtListNode(ListNode);

    ListNode = ListNode->rightSib;
  }
  close_scope();
}

static void visitFuncListNode(ASTNode *node)
{
  if (node->semantic.list.type != LIST_FUNC)
    error("%s: internal error\n", __func__);

  ASTNode *FuncNode = node->child;
  while (FuncNode) {
    visitFuncNode(FuncNode);
    FuncNode = FuncNode->rightSib;
  }
}

static void visitProgramNode(ASTNode *node)
{
  if (node->kind != K_PROGRAM)
    error("%s: internal error\n", __func__);

  open_scope();

  ASTNode *ListNode = node->child;
  while (ListNode) {
    if (ListNode->kind != K_LIST)
      error("%s: child must be a list node\n", __func__);

    if (ListNode->semantic.list.type == LIST_DECL)
      visitDeclListNode(ListNode);
    else
      visitFuncListNode(ListNode);

    ListNode = ListNode->rightSib;
  }

  SymbolTableEntry *entry = retrieve_symbol("main");
  if (!entry || !entry->type.is_function) {
    error("No main function defined\n");
  }

  close_scope();
}

void semantic_analysis()
{
  visitProgramNode(ast_root);
}

