#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "types.h"

/* type descriptor for scalar/typedef/array */
typedef struct VarDesc {
  enum { SYM_TYPICAL, SYM_TYPEDEF, SYM_ARRAY } kind;

  /* Scalar/typedef type, and element type for array. The value can be
     TYPE_INT, TYPE_FLOAT, TYPE_VOID for scalar/typedef/array, and
     TYPE_STRING, TYPE_INT_PTR, TYPE_FLOAT_PTR for function parameters. */
  DataType type;

  int num_dimensions; /* used only for array type */
  int *dimension;     /* used only for array type */
} VarDesc;

/* type descriptor for function */
typedef struct FuncSignature {
  int num_params;
  VarDesc *param_types;
  DataType return_type; /* int/float/void */
} FuncSignature;

typedef struct SymbolType {
  int is_function;
  union {
    VarDesc var;
    FuncSignature func;
  } u;
} SymbolType;

typedef struct SymbolTableEntry {
  char *name;
  SymbolType type;
  int depth;

  struct SymbolTableEntry* next;  /* next symbol in the hash list */
  struct SymbolTableEntry* prev;  /* prev symbol in the hash list */
  struct SymbolTableEntry* var;   /* list of symbols of the same name */ // not implement
  struct SymbolTableEntry* level; /* list of symbols in the same scope */
} SymbolTableEntry;

int open_scope();
int close_scope();
SymbolTableEntry *enter_symbol(char *name, SymbolType *type);
SymbolTableEntry *retrieve_symbol(char *name);
int declare_locally(char *name);

#define CURRENT_FUNCTION "__CURRENT_FUNCTION__"

void free_symboltype_things(SymbolType *type);
const char *data_type_to_string(DataType type);

#endif
