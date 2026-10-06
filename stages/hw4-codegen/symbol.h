#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "types.h"

/* type descriptor for scalar/typedef/array */
typedef struct VarDesc {
  int is_typedef;
  DataType type;
  int num_dimensions; /* 0 for scalar type */
  int *dimensions;    /* used only for array type */
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
  int offset;	/* offset to the frame or stack pointer */

  struct SymbolTableEntry* next;  /* next symbol in the hash list */
  struct SymbolTableEntry* prev;  /* prev symbol in the hash list */
  struct SymbolTableEntry* var;   /* list of symbols of the same name */
  struct SymbolTableEntry* level; /* list of symbols in the same scope */
} SymbolTableEntry;

int open_scope();
int close_scope();
SymbolTableEntry *enter_symbol(char *name, SymbolType *type);
SymbolTableEntry *retrieve_symbol(char *name);
int declare_locally(char *name);
void init_symbol_table();

#endif
