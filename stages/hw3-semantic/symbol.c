#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "symbol.h"

#define HASH_TABLE_SIZE (0x1U << 10)
#define MAX_DEPTH	(1024)

static SymbolTableEntry SymbolTable[HASH_TABLE_SIZE]; // dummy heads
static SymbolTableEntry *ScopeList[MAX_DEPTH];
static int CurrentScope = -1; /* 0 for global scope */

static inline unsigned int Hash(char *name)
{
  unsigned i, n, val = 0;
  for (i = 0, n = strlen(name); i != n; ++i)
    val += (name[i] << (i % 32));
  return val & (HASH_TABLE_SIZE - 1);
}

const char *data_type_to_string(DataType type)
{
  switch (type) {
    case TYPE_INVALID:   return "INVALID";
    case TYPE_INT:       return "int";
    case TYPE_FLOAT:     return "float";
    case TYPE_VOID:      return "void";
    case TYPE_STRING:    return "str";
    // case TYPE_INT_PTR:   return "int";   //"INT_PTR";
    // case TYPE_FLOAT_PTR: return "float"; //"FLOAT_PTR";
    default:             return "UNKNOWN";
  }
}

static inline void print_arr(VarDesc *var)
{
  for (int i = 0; i < var->num_dimensions; i++) {
    printf("[%d]", var->dimension[i]);
  }
}

static void print_symbol_table_entry(SymbolTableEntry *entry)
{
  // int foo;
  // void bar(int a, float b);
  // float baz[10][20];
  // typedef int foo[3];
  printf("Depth:%d, ", entry->depth);
  if (entry->type.is_function) {
    printf("%s %s(", data_type_to_string(entry->type.u.func.return_type) ,entry->name);
    for (int i = 0; i < entry->type.u.func.num_params; i++) {
      printf("%s", data_type_to_string(entry->type.u.func.param_types[i].type));
      print_arr(&entry->type.u.func.param_types[i]);
      if (i != entry->type.u.func.num_params - 1)
        printf(", ");
    }
    printf(")");
  } else {
    if (entry->type.u.var.kind == SYM_TYPEDEF)
      printf("typedef ");
    printf("%s %s", data_type_to_string(entry->type.u.var.type), entry->name);
    print_arr(&entry->type.u.var);
  }
  printf(";\n");
}

// return current scope depth after opening a new scope
int open_scope()
{
  CurrentScope++;
  return CurrentScope;
}

// return current scope depth after closing the current scope
int close_scope()
{
  SymbolTableEntry *entry = ScopeList[CurrentScope];
  SymbolTableEntry *next_entry;
  while (entry) {
    if (debug_semantic){
      printf("Close symbol: ");
      print_symbol_table_entry(entry);
    }
    entry->prev->next = entry->next;
    if (entry->next)
      entry->next->prev = entry->prev;
    next_entry = entry->level;
    free_symboltype_things(&entry->type);
    free(entry->name);
    free(entry);
    entry = next_entry;
  }
  ScopeList[CurrentScope] = NULL;
  CurrentScope--;
  return CurrentScope;
}


/// @brief Enter a new symbol into the symbol table.
/// @brief Both name and type are copied, so the caller can free them afterward.
/// @param name Symbol name
/// @param type Symbol type
/// @return Pointer to the new SymbolTableEntry
SymbolTableEntry *enter_symbol(char *name, SymbolType *type)
{
  // creating new symbol table entry
  SymbolTableEntry *new_entry = malloc(sizeof(SymbolTableEntry));
  new_entry->name = strdup(name);
  new_entry->type = *type;
  if (new_entry->type.is_function){
    new_entry->type.u.func.param_types = malloc(sizeof(VarDesc) * new_entry->type.u.func.num_params);
    for (int i = 0; i < new_entry->type.u.func.num_params; i++) {
      new_entry->type.u.func.param_types[i] = type->u.func.param_types[i];
      if (new_entry->type.u.func.param_types[i].num_dimensions > 0) {
        new_entry->type.u.func.param_types[i].dimension = malloc(sizeof(int) * new_entry->type.u.func.param_types[i].num_dimensions);
        memcpy(new_entry->type.u.func.param_types[i].dimension, type->u.func.param_types[i].dimension, sizeof(int) * new_entry->type.u.func.param_types[i].num_dimensions);
      }
    }
  } else {
    if (new_entry->type.u.var.num_dimensions > 0) {
      new_entry->type.u.var.dimension = malloc(sizeof(int) * new_entry->type.u.var.num_dimensions);
      memcpy(new_entry->type.u.var.dimension, type->u.var.dimension, sizeof(int) * new_entry->type.u.var.num_dimensions);
    }
  }
  new_entry->depth = CurrentScope;
  new_entry->next = NULL;
  new_entry->level = NULL;

  // setup hash list
  SymbolTableEntry *entry = &SymbolTable[Hash(name)];
  while (entry->next) {
    entry = entry->next;
    if (strcmp(entry->name, name) == 0) {
      if (entry->depth == CurrentScope) {
        free(new_entry->name);
        free(new_entry);
        error("Symbol redeclaration: %s\n", name);
      }
      warning("Symbol shadowing: %s\n", name);
      break;
    }
  }
  entry->next = new_entry;
  new_entry->prev = entry;

  // setup scope list
  entry = ScopeList[CurrentScope];
  if (entry == NULL) {
    ScopeList[CurrentScope] = new_entry;
  } else {
    while (entry->level) {
      entry = entry->level;
    }
    entry->level = new_entry;
  }
  if (debug_semantic){
    printf("Enter symbol: ");
    print_symbol_table_entry(new_entry);
  }
  return new_entry;
}

SymbolTableEntry *retrieve_symbol(char *name)
{
  SymbolTableEntry *entry = &SymbolTable[Hash(name)];
  SymbolTableEntry *entry_in_closest_scope = NULL;
  
  while (entry->next) {
    entry = entry->next;
    if (strcmp(entry->name, name) == 0) {
      entry_in_closest_scope = entry;
    }
  }

  return entry_in_closest_scope;
}

int declare_locally(char *name)
{
  return 0;
}

static inline void free_var_desc_things(VarDesc *var){
  if (var->num_dimensions > 0){
    free(var->dimension);
    var->num_dimensions = 0;
    var->dimension = NULL;
  }
}

void free_symboltype_things(SymbolType *type){
  if (type->is_function){
    while(type->u.func.num_params--){
      free_var_desc_things(&type->u.func.param_types[type->u.func.num_params]);
    }
    free(type->u.func.param_types);
  } else {
    free_var_desc_things(&type->u.var);
  }
}
