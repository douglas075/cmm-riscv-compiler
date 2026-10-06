#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "symbol.h"

#define HASH_TABLE_SIZE (0x1U << 10)
#define MAX_DEPTH	(1024)

#define dbg(fmt, args...)           \
  do {                              \
    if (debug_semantic)             \
      fprintf(stderr, fmt, ##args); \
  } while (0)

static SymbolTableEntry SymbolTable[HASH_TABLE_SIZE];
static SymbolTableEntry *ScopeList[MAX_DEPTH];
static unsigned int CurrentScope; /* 0 for global scope */

static inline unsigned int Hash(char *name)
{
  unsigned i, n, val = 0;
  for (i = 0, n = strlen(name); i != n; ++i)
    val += (name[i] << (i % 32));
  return val & (HASH_TABLE_SIZE - 1);
}

int open_scope()
{
  if (CurrentScope == MAX_DEPTH - 1)
    error("%s: max scope depth is %d ", __func__, MAX_DEPTH);

  ++CurrentScope;
  return 0;
}

int close_scope()
{
  SymbolTableEntry *symbol = ScopeList[CurrentScope];
  while (symbol) {
    dbg("%s: remove symbol %s\n", __func__, symbol->name);

    SymbolTableEntry *current = symbol;
    current->prev->next = current->next;
    if (current->next)
      current->next->prev = current->prev;

    symbol = current->level;

    if (current->type.is_function) {
      if (current->type.u.func.num_params)
        free(current->type.u.func.param_types);
    } else {
      if (current->type.u.var.num_dimensions)
        free(current->type.u.var.dimensions);
    }
    free(current);
  }

  ScopeList[CurrentScope] = NULL;
  --CurrentScope;
  return 0;
}

static void copy_var_type(VarDesc *to, VarDesc *from)
{
  to->is_typedef = from->is_typedef;
  to->type = from->type;
  to->num_dimensions = from->num_dimensions;
  if (to->num_dimensions != 0) {
    to->dimensions = (int *)malloc(to->num_dimensions * sizeof(int));
    memcpy(to->dimensions, from->dimensions, to->num_dimensions * sizeof(int));
  }
}

static void copy_func_type(FuncSignature *to, FuncSignature *from)
{
  to->num_params = from->num_params;
  to->return_type = from->return_type;
  if (to->num_params != 0) {
    int i;
    to->param_types = (VarDesc *)malloc(to->num_params * sizeof(VarDesc));
    for (i = 0; i < to->num_params; ++i)
      copy_var_type(&to->param_types[i], &from->param_types[i]);
  }
}

SymbolTableEntry *enter_symbol(char *name, SymbolType *type)
{
  if (type == NULL)
    error("%s: internal error\n", __func__);

  dbg("%s: name=%s depth=%d\n", __func__, name, CurrentScope);

  SymbolTableEntry *found = retrieve_symbol(name);
  if (found && found->depth == CurrentScope) {
    sprintf(error_msg, "symbol redeclaration `%s'", name);
    return NULL;
  }

  SymbolTableEntry *symbol;
  symbol = (SymbolTableEntry *)malloc(sizeof(SymbolTableEntry));
  symbol->name = strdup(name);
  symbol->type.is_function = type->is_function;
  if (type->is_function)
    copy_func_type(&symbol->type.u.func, &type->u.func);
  else
    copy_var_type(&symbol->type.u.var, &type->u.var);

  symbol->depth = CurrentScope;

  int index = Hash(name);
  symbol->next = SymbolTable[index].next;
  symbol->prev = &SymbolTable[index];
  symbol->var = found;
  symbol->level = ScopeList[CurrentScope];
  if (SymbolTable[index].next)
    SymbolTable[index].next->prev = symbol;
  SymbolTable[index].next = symbol;
  ScopeList[CurrentScope] = symbol;

  return symbol;
}

SymbolTableEntry *retrieve_symbol(char *name)
{
  SymbolTableEntry *entry = SymbolTable[Hash(name)].next;
  while (entry) {
    if (strcmp(name, entry->name) == 0)
      break;
    entry = entry->next;
  }
  return entry;
}

int declare_locally(char *name)
{
  SymbolTableEntry *entry = retrieve_symbol(name);
  if (entry && entry->depth == CurrentScope)
    return 1;
  return 0;
}

void init_library_function()
{
  dbg("initialize library functions.\n");

  SymbolType func_type;
  VarDesc *param = (VarDesc *)malloc(sizeof(VarDesc));

  func_type.is_function = 1;
  func_type.u.func.num_params = 0;
  func_type.u.func.return_type = TYPE_INT;
  enter_symbol("read_int", &func_type);

  func_type.u.func.return_type = TYPE_FLOAT;
  enter_symbol("read_fp", &func_type);

  func_type.u.func.return_type = TYPE_VOID;
  func_type.u.func.num_params = 1;
  func_type.u.func.param_types = param;
  param[0].is_typedef = 0;
  param[0].num_dimensions = 0;
  param[0].type = TYPE_INT;
  enter_symbol("write_int", &func_type);
  param[0].type = TYPE_FLOAT;
  enter_symbol("write_fp", &func_type);
  param[0].type = TYPE_STRING;
  enter_symbol("write_str", &func_type);
}

void init_symbol_table()
{
  dbg("initialize symbol table.\n");

  int i;
  CurrentScope = 0;
  for (i = 0; i < HASH_TABLE_SIZE; ++i) {
    SymbolTable[i].next = NULL;
    SymbolTable[i].prev = NULL;
    SymbolTable[i].var = NULL;
    SymbolTable[i].level = NULL;
  }
  for (i = 0; i < MAX_DEPTH; ++i)
    ScopeList[i] = NULL;

  SymbolType type;
  type.is_function = 0;
  type.u.var.is_typedef = 1;
  type.u.var.type = TYPE_INT;
  type.u.var.num_dimensions = 0;
  enter_symbol("int", &type);

  type.u.var.type = TYPE_FLOAT;
  enter_symbol("float", &type);

  type.u.var.type = TYPE_VOID;
  enter_symbol("void", &type);

  init_library_function();
}

