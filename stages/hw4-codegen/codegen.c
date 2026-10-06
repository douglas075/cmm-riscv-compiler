#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "symbol.h"

#define dbg(fmt, args...)           \
  do {                              \
    if (debug_codegen)              \
      fprintf(stderr, fmt, ##args); \
  } while (0)

#define emit(fmt, args...)          \
  do {                              \
      fprintf(file_p, fmt, ##args); \
  } while (0)

#define ALIGN_UP(x,align) ((x + (align-1)) & ~(align-1))
#define REG_NUM 32
#define ALIGNMENT 8
#define BASE_SIZE 4    // sizeof(int) = sizeof(float) = 4
#define OFFSET_FIX 8   // 0(sp) is for next function
#define FP_TO_PARAM 16 // &param0 = fp + 16
#define VAR_GLOBAL 1

static void visitProgram(ASTNode *node);
static void visitBlock(ASTNode *node);
static void visitGlobalDeclList(ASTNode *node);
static void visitDeclList(ASTNode *node);
static void visitFuncList(ASTNode *node);
static void visitStmtList(ASTNode *node);
static void visitGlobalDecl(ASTNode *node);
static void visitDecl(ASTNode *node);
static void visitFunc(ASTNode *node);
static void visitStmt(ASTNode *node);
static void visitCall(ASTNode *node);
static int visitExpr(ASTNode *node);
void gen_head(char *name);
void gen_prologue(char *name);
void gen_epilogue(char *name, int frame_size);

static FILE *file_p;
static SymbolTableEntry *CurrentFunc;
static int Label_serial = 0; // for const/jump label
static int ARoffset; // init to (size saved reg)
static int reg_id = 0;
static int freg_id = 0;
static int debug_codegen;
char *str_const[20];
int str_const_count = 0;

typedef enum RegType {
  REG_UNAVAILABLE = 0,
  REG_CALLEE_SAVED,
  REG_CALLER_SAVED,
} RegType;

// x1(ra), x10(a0) can be used as temporary register in instruction like li, la. discarded when call function
// x10(a0) store function return
// exclude x0(zero), x1(ra), x2(sp), x3(gp), x8(fp), x10(a0)
// x3(gp) is used by lib function, so unavailable
static const RegType reg_type[REG_NUM] = {
  REG_UNAVAILABLE,  REG_UNAVAILABLE,  REG_UNAVAILABLE,  REG_UNAVAILABLE,
  REG_CALLEE_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
  REG_UNAVAILABLE,  REG_CALLEE_SAVED, REG_UNAVAILABLE,  REG_CALLER_SAVED, // x9
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x18, x19
  REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x20~23
  REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x24~27
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
};

// exclude f10(fa0)
static const RegType freg_type[REG_NUM] = {
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
  REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_UNAVAILABLE,  REG_CALLER_SAVED, // f8, f9
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x18, x19
  REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x20~23
  REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, REG_CALLEE_SAVED, // x24~27
  REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED, REG_CALLER_SAVED,
};

#define NUM_CALLEE_SAVED   12
#define NUM_CALLER_SAVED   14
#define NUM_F_CALLEE_SAVED 12
#define NUM_F_CALLER_SAVED 19
#define TOTAL_REG_SIZE ( (NUM_CALLEE_SAVED + NUM_CALLER_SAVED) * 8 \
                       + (NUM_F_CALLEE_SAVED + NUM_F_CALLER_SAVED) * 4 )

static inline int get_serial(){
  return Label_serial++;
}

static inline int get_reg() {
  do {
    reg_id = (reg_id + 1) % REG_NUM;
  } while(reg_type[reg_id] == REG_UNAVAILABLE);
  return reg_id;
}

static inline int get_freg() {
  do {
    freg_id = (freg_id + 1) % REG_NUM;
  } while(freg_type[freg_id] == REG_UNAVAILABLE);
  return freg_id;
}

#define zero     0
#define sp       2
#define fp       8
#define a0      10
#define tmp_reg  1 // use x1(ra) as tmp_reg
#define fa0     10
// used instruction
static inline void emit_lw(int rd, int rs1, int offset) { emit("\tlw x%d, %d(x%d)\n", rd, offset, rs1); }
static inline void emit_sw(int rd, int rs1, int offset) { emit("\tsw x%d, %d(x%d)\n", rd, offset, rs1); }
static inline void emit_ld(int rd, int rs1, int offset) { emit("\tld x%d, %d(x%d)\n", rd, offset, rs1); }
static inline void emit_sd(int rd, int rs1, int offset) { emit("\tsd x%d, %d(x%d)\n", rd, offset, rs1); }
static inline void emit_li(int rd, int imm) { emit("\tli x%d, %d\n", rd, imm); }
#define emit_la(rd, fmt, args...) emit("\tla x%d, " fmt "\n", rd, ##args)

static inline void emit_flw(int frd, int rs1, int offset) { emit("\tflw f%d, %d(x%d)\n", frd, offset, rs1); }
static inline void emit_fsw(int frd, int rs1, int offset) { emit("\tfsw f%d, %d(x%d)\n", frd, offset, rs1); }
static inline void emit_fcvt_s_w(int frd, int rs1) { emit("\tfcvt.s.w f%d, x%d\n", frd, rs1); }
static inline void emit_fmv_w_x(int frd, int rs1) { emit("\tfmv.w.x f%d, x%d\n", frd, rs1); }
static inline void emit_fmv_s(int frd, int frs1) { emit("\tfmv.s f%d, f%d\n", frd, frs1); }

static inline void emit_add(int rd, int rs1, int rs2) { emit("\tadd x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_addi(int rd, int rs1, int imm) { emit("\taddi x%d, x%d, %d\n", rd, rs1, imm); }
static inline void emit_sub(int rd, int rs1, int rs2) { emit("\tsub x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_mul(int rd, int rs1, int rs2) { emit("\tmul x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_div(int rd, int rs1, int rs2) { emit("\tdiv x%d, x%d, x%d\n", rd, rs1, rs2); }

static inline void emit_and(int rd, int rs1, int rs2) { emit("\tand x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_or(int rd, int rs1, int rs2) { emit("\tor x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_slt(int rd, int rs1, int rs2) { emit("\tslt x%d, x%d, x%d\n", rd, rs1, rs2); }
static inline void emit_snez(int rd, int rs1) { emit("\tsnez x%d, x%d\n", rd, rs1); }
static inline void emit_seqz(int rd, int rs1) { emit("\tseqz x%d, x%d\n", rd, rs1); }

static inline void emit_fadd_s(int frd, int frs1, int frs2) { emit("\tfadd.s f%d, f%d, f%d\n", frd, frs1, frs2); }
static inline void emit_fsub_s(int frd, int frs1, int frs2) { emit("\tfsub.s f%d, f%d, f%d\n", frd, frs1, frs2); }
static inline void emit_fmul_s(int frd, int frs1, int frs2) { emit("\tfmul.s f%d, f%d, f%d\n", frd, frs1, frs2); }
static inline void emit_fdiv_s(int frd, int frs1, int frs2) { emit("\tfdiv.s f%d, f%d, f%d\n", frd, frs1, frs2); }
static inline void emit_fneg_s(int frd, int frs1) { emit("\tfneg.s f%d, f%d\n", frd, frs1); }

static inline void emit_flt_s(int rd, int frs1, int frs2) { emit("\tflt.s x%d, f%d, f%d\n", rd, frs1, frs2); }
static inline void emit_fle_s(int rd, int frs1, int frs2) { emit("\tfle.s x%d, f%d, f%d\n", rd, frs1, frs2); }
static inline void emit_feq_s(int rd, int frs1, int frs2) { emit("\tfeq.s x%d, f%d, f%d\n", rd, frs1, frs2); }

static inline void emit_callee_save() {
  int offset = -8;
  for (int i = 0; i < REG_NUM; i++) {
    if (reg_type[i] == REG_CALLEE_SAVED) {
      emit_sd(i, fp, offset);
      offset -= 8;
    }
  }
  offset = -4 - (NUM_CALLEE_SAVED + NUM_CALLER_SAVED) * 8;
  for (int i = 0; i < REG_NUM; i++) {
    if (freg_type[i] == REG_CALLEE_SAVED) {
      emit_fsw(i, fp, offset);
      offset -= 4;
    }
  }
}

static inline void emit_callee_restore() {
  int offset = -8;
  for (int i = 0; i < REG_NUM; i++) {
    if (reg_type[i] == REG_CALLEE_SAVED) {
      emit_ld(i, fp, offset);
      offset -= 8;
    }
  }
  offset = -4 - (NUM_CALLEE_SAVED + NUM_CALLER_SAVED) * 8;
  for (int i = 0; i < REG_NUM; i++) {
    if (freg_type[i] == REG_CALLEE_SAVED) {
      emit_flw(i, fp, offset);
      offset -= 4;
    }
  }
}

static inline void emit_caller_save() {
  int offset = -8 - NUM_CALLEE_SAVED * 8;
  for (int i = 0; i < REG_NUM; i++) {
    if (reg_type[i] == REG_CALLER_SAVED) {
      emit_sd(i, fp, offset);
      offset -= 8;
    }
  }
  offset = -4 - (NUM_CALLEE_SAVED + NUM_CALLER_SAVED) * 8 - NUM_F_CALLEE_SAVED * 4;
  for (int i = 0; i < REG_NUM; i++) {
    if (freg_type[i] == REG_CALLER_SAVED) {
      emit_fsw(i, fp, offset);
      offset -= 4;
    }
  }
}

static inline void emit_caller_restore() {
  int offset = -8 - NUM_CALLEE_SAVED * 8;
  for (int i = 0; i < REG_NUM; i++) {
    if (reg_type[i] == REG_CALLER_SAVED) {
      emit_ld(i, fp, offset);
      offset -= 8;
    }
  }
  offset = -4 - (NUM_CALLEE_SAVED + NUM_CALLER_SAVED) * 8 - NUM_F_CALLEE_SAVED * 4;
  for (int i = 0; i < REG_NUM; i++) {
    if (freg_type[i] == REG_CALLER_SAVED) {
      emit_flw(i, fp, offset);
      offset -= 4;
    }
  }
}

/*
 * Code generation
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

static int get_var_size(VarDesc *Var){
  if (Var->is_typedef)
    error("call %s with typedef", __func__);
  if (Var->type != TYPE_INT && Var->type != TYPE_FLOAT)
    error("invalid type with id %d", Var->type);
  int size = BASE_SIZE;
  for(int i = 0; i < Var->num_dimensions; i++) {
    size *= Var->dimensions[i];
  }
  return size;
}

static void gen_str_const_section()
{
  if (str_const_count == 0)
    return;

  emit(".section .rodata\n");
  for (int i = 0; i < str_const_count; i++) {
    emit("_const_%d:\n", i);
    emit("\t.string %s\n", str_const[i]);
  }
}

// return reg offset stored in
static int emit_array_subscript(VarDesc *Var, ASTNode *ID) {
  if (ID->semantic.id.type != ID_ARRAY)
    error("call %s with param 'ID' not an array!", __func__);

  ASTNode *child = ID->child;
  int offset = get_reg();
  emit_addi(offset, zero, 0); // accumulated byte offset

  int stride_val = BASE_SIZE; // size of one element
  for (int i = 0; i < Var->num_dimensions; i++) {
    visitExpr(child);                // compute index expression
    int idx = child->place;          // index register

    emit_li(tmp_reg, stride_val);    // tmp_reg = current stride (bytes)
    emit_mul(idx, idx, tmp_reg);     // idx *= stride
    emit_add(offset, offset, idx);   // offset += idx

    stride_val *= Var->dimensions[i]; // next dimension stride
    child = child->rightSib;
  }

  return offset;
}

static void emit_expr_op(ASTNode *Expr){
  int op = Expr->semantic.expr.type;
  int type1 = visitExpr(Expr->child);
  int type2 = TYPE_INVALID;
  int type_processing = type1;
  int type_final = TYPE_INVALID; // 1.0 < 2.0 => 1(int)

  int rs1 = Expr->child->place;
  int rd = rs1; // set latest allocated
  int rs2;

  if (Expr->child->rightSib) {
    type2 = visitExpr(Expr->child->rightSib);
    rs2 = Expr->child->rightSib->place;
    rd = rs2;
    if (type2 == TYPE_FLOAT)
      type_processing = TYPE_FLOAT;
  }

  // convert int to float
  if (type1 == TYPE_INT && type2 == TYPE_FLOAT) {
    int frs = get_freg();
    emit_fcvt_s_w(frs, rs1);
    rs1 = frs;
    rd = rs1;
  } else if (type1 == TYPE_FLOAT && type2 == TYPE_INT) {
    int frs = get_freg();
    emit_fcvt_s_w(frs, rs2);
    rs2 = frs;
    rd = rs2;
  }

  type_final = type_processing;
  switch (op) {
  case OP_ADD:
    if (type_processing == TYPE_INT) {
      emit_add(rd, rs1, rs2);
    } else {
      emit_fadd_s(rd, rs1, rs2);
    }
    break;

  case OP_SUB:
    if (type_processing == TYPE_INT) {
      emit_sub(rd, rs1, rs2);
    } else {
      emit_fsub_s(rd, rs1, rs2);
    }
    break;

  case OP_MUL:
    if (type_processing == TYPE_INT) {
      emit_mul(rd, rs1, rs2);
    } else {
      emit_fmul_s(rd, rs1, rs2);
    }
    break;

  case OP_DIV:
    if (type_processing == TYPE_INT) {
      emit_div(rd, rs1, rs2);
    } else {
      emit_fdiv_s(rd, rs1, rs2);
    }
    break;

  case OP_AND:
    if (type_processing != TYPE_INT)
      error("Invalid expression for '&&' in %s\n", __func__);
    emit_snez(rs1, rs1); // rs1 = rs1 != 0 ? 1 : 0
    emit_snez(rs2, rs2);
    emit_and(rd, rs1, rs2);
    break;

  case OP_OR:
    if (type_processing != TYPE_INT)
      error("Invalid expression for '||' in %s\n", __func__);
    emit_or(rd, rs1, rs2);     // rd = rs1 | rs2
    emit_snez(rd, rd);         // rd = rd != 0 ? 1 : 0
    break;

  case OP_LT:
    if (type_processing == TYPE_INT) {
      emit_slt(rd, rs1, rs2);   // rs1 < rs2
    } else {
      rd = get_reg();
      emit_flt_s(rd, rs1, rs2); // rs1 < rs2
    }
    type_final = TYPE_INT;
    break;

  case OP_GT:
    if (type_processing == TYPE_INT) {
      emit_slt(rd, rs2, rs1);   // rs2 < rs1, rs1 > rs2
    } else {
      rd = get_reg();
      emit_flt_s(rd, rs2, rs1); // rs2 < rs1, rs1 > rs2
    }
    type_final = TYPE_INT;
    break;

  case OP_LE:
    if (type_processing == TYPE_INT) {
      emit_slt(rd, rs2, rs1);   // rs2 < rs1
      emit_seqz(rd, rd);        // rs2 >= rs1, rs1 <= rs2
    } else {
      rd = get_reg();
      emit_fle_s(rd, rs1, rs2); // rs1 <= rs2
    }
    type_final = TYPE_INT;
    break;

  case OP_GE:
    if (type_processing == TYPE_INT) {
      emit_slt(rd, rs1, rs2);   // rs1 < rs2
      emit_seqz(rd, rd);        // rs1 >= rs2
    } else {
      rd = get_reg();
      emit_fle_s(rd, rs2, rs1); // rs2 <= rs1, rs1 >= rs2
    }
    type_final = TYPE_INT;
    break;

  case OP_EQ:
    if (type_processing == TYPE_INT) {
      emit_sub(rd, rs1, rs2);   // rd = rs1 - rs2
      emit_seqz(rd, rd);        // rd = rd == 0 ? 1 : 0
    } else {
      rd = get_reg();
      emit_feq_s(rd, rs1, rs2); // rd = rs1 == rs2 ? 1 : 0
    }
    type_final = TYPE_INT;
    break;

  case OP_NE:
    if (type_processing == TYPE_INT) {
      emit_sub(rd, rs1, rs2);   // rd = rs1 - rs2
      emit_snez(rd, rd);        // rd = rd != 0 ? 1 : 0
    } else {
      rd = get_reg();
      emit_feq_s(rd, rs1, rs2); // rd = rs1 == rs2 ? 1 : 0
      emit_seqz(rd, rd);        // rd = rd == 0 ? 1 : 0
    }
    type_final = TYPE_INT;
    break;

  case OP_NOT:
    if (type_processing != TYPE_INT)
      error("Invalid expression for '!' in %s\n", __func__);
    emit_seqz(rd, rs1);
    break;

  case OP_NEG:
    if (type_processing == TYPE_INT) {
      emit_sub(rd, zero, rs1);
    } else {
      emit_fneg_s(rd, rs1);
    }
    break;
  }

  Expr->type = type_final;
  Expr->place = rd;
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
    ARoffset += get_var_size(&type.u.var);
    symbol->offset = -ARoffset;
    if (ID->semantic.id.type == ID_TYPICAL_ASSIGN) {
      int rhs_type = visitExpr(ID->child);
      int rd = ID->child->place;
      if (type.u.var.type == TYPE_INT && rhs_type == TYPE_FLOAT)
        error("cannot convert float value to integer symbol `%s'.\n", get_id(ID));

      if (type.u.var.type == TYPE_FLOAT && rhs_type == TYPE_INT) {
        int frd = get_freg();
        emit_fcvt_s_w(frd, rd);
        rd = frd;
      }

      if (type.u.var.type == TYPE_INT)
        emit_sw(rd, fp, symbol->offset);
      else
        emit_fsw(rd, fp, symbol->offset);
    }

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

    DataType entry_type = entry->type.u.var.type;
    if (num_dimensions == 0) {
      if (entry_type == TYPE_INT) {
        Expr->place = get_reg();
        if (entry->depth == VAR_GLOBAL) {
          emit_la(tmp_reg, "%s", entry->name);
          emit_lw(Expr->place, tmp_reg, 0);
        } else {
          emit_lw(Expr->place, fp, entry->offset);
        }
      } else {
        Expr->place = get_freg();
        if (entry->depth == VAR_GLOBAL) {
          emit_la(tmp_reg, "%s", entry->name);
          emit_flw(Expr->place, tmp_reg, 0);
        } else {
          emit_flw(Expr->place, fp, entry->offset);
        }
      }
    } else {
      int rs1 = emit_array_subscript(&entry->type.u.var, Expr);
      if (entry->depth == VAR_GLOBAL) {
        emit_la(tmp_reg, "%s", entry->name);
        emit_add(rs1, rs1, tmp_reg);
      } else {
        emit_addi(rs1, rs1, entry->offset);
        emit_add(rs1, rs1, fp);
      }

      if (entry_type == TYPE_INT) {
        Expr->place = rs1;
        emit_lw(Expr->place, rs1, 0);
      } else {
        Expr->place = get_freg();
        emit_flw(Expr->place, rs1, 0);
      }
    }

    Expr->type = entry->type.u.var.type;
    return Expr->type;
  }

  if (Expr->kind == K_CONST) {
    Expr->type = Expr->semantic.constant->type;
    if (Expr->type == TYPE_INT) {
      Expr->place = get_reg();
      emit_li(Expr->place, Expr->semantic.constant->u.const_int);
    } else if (Expr->type == TYPE_FLOAT) {
      Expr->place = get_freg();
      // if value is float, reinterpret as int
      emit_li(tmp_reg, Expr->semantic.constant->u.const_int);
      emit_fmv_w_x(Expr->place, tmp_reg);
    } else { // TYPE_STRING
      // not sure should we use get_reg for string constant
      // because string is always the argument for write calls
      // such an argument should be placed at a0 before called
      str_const[str_const_count] = Expr->semantic.constant->u.const_str;
      emit_la(a0, "_const_%d", str_const_count);
      str_const_count++;
      Expr->place = a0;
      // .section .rodata
      // .LC0:
      //     .string "hello"
      // will be emitted in the end of program traversal
    }

    return Expr->type;
  }

  if (Expr->kind == K_STMT && Expr->semantic.stmt.type == STMT_CALL) {
    visitCall(Expr);
    return Expr->type; // set in visitCall
  }

  /* K_EXPR */
  emit_expr_op(Expr);
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

  int rhs_type = visitExpr(Expr);
  int rd = Expr->place;
  int lhs_type = entry->type.u.var.type;
  if (lhs_type == TYPE_INT && rhs_type == TYPE_FLOAT)
    error("cannot convert float value to integer symbol `%s'.\n", id);

  // convert int to float
  if (lhs_type == TYPE_FLOAT && rhs_type == TYPE_INT) {
    int frd = get_freg();
    emit_fcvt_s_w(frd, rd);
    rd = frd;
  }

  // store
  DataType entry_type = entry->type.u.var.type;
  if (ID->semantic.id.type == ID_TYPICAL) {
    if (entry_type == TYPE_INT) {
      if (entry->depth == VAR_GLOBAL) {
        emit_la(tmp_reg, "%s", entry->name);
        emit_sw(rd, tmp_reg, 0);
      } else {
        emit_sw(rd, fp, entry->offset);
      }
    } else {
      if (entry->depth == VAR_GLOBAL) {
        emit_la(tmp_reg, "%s", entry->name);
        emit_fsw(rd, tmp_reg, 0);
      } else {
        emit_fsw(rd, fp, entry->offset);
      }
    }
  } else { // ID->semantic.id.type == ID_ARRAY
    int rs1 = emit_array_subscript(&entry->type.u.var, ID);
    if (entry->depth == VAR_GLOBAL) {
      emit_la(tmp_reg, "%s", entry->name);
      emit_add(rs1, rs1, tmp_reg);
    } else {
      emit_addi(rs1, rs1, entry->offset);
      emit_add(rs1, rs1, fp);
    }

    if (entry_type == TYPE_INT) {
      emit_sw(rd, rs1, 0);
    } else {
      emit_fsw(rd, rs1, 0);
    }
  }
  Assign->place = rd;
  Assign->type = lhs_type;
}

static void visitWhile(ASTNode *While)
{
  ASTNode *Expr = While->child;
  ASTNode *Stmt = Expr->rightSib;

  int id = get_serial();

  emit("j _while_end_%d\n", id);
  emit("_while_start_%d:\n", id);
  visitStmt(Stmt);
  emit("_while_end_%d:\n", id);
  visitExpr(Expr);
  emit("bnez x%d, _while_start_%d\n", Expr->place, id);
}

static void visitIf(ASTNode *If)
{
  ASTNode *Expr = If->child;
  ASTNode *IfTrue = Expr->rightSib;
  ASTNode *IfFalse = IfTrue->rightSib;

  int id = get_serial();

  visitExpr(Expr);
  emit("beqz x%d, _else_start_%d\n", Expr->place, id);

  visitStmt(IfTrue);
  emit("j _else_end_%d\n", id);

  emit("_else_start_%d:\n", id);
  visitStmt(IfFalse);
  emit("_else_end_%d:\n", id);
}

static void visitFor(ASTNode *For)
{
  ASTNode *AssignList1 = For->child;
  ASTNode *Expr = AssignList1->rightSib;
  ASTNode *AssignList2 = Expr->rightSib;
  ASTNode *Stmt = AssignList2->rightSib;
  ASTNode *assign;

  int id = get_serial();

  assign = AssignList1->child;
  while(assign && assign->kind != K_NULL) {
    visitAssign(assign);
    assign = assign->rightSib;
  }
  emit("j _for_end_%d\n", id);

  emit("_for_start_%d:\n", id);
  visitStmt(Stmt);
  assign = AssignList2->child;
  while(assign && assign->kind != K_NULL) {
    visitAssign(assign);
    assign = assign->rightSib;
  }

  emit("_for_end_%d:\n", id);
  if (Expr->kind == K_NULL) {
    emit("j _for_start_%d\n", id);
  } else {
    visitExpr(Expr);
    emit("bnez x%d, _for_start_%d\n", Expr->place, id);
  }
}

static void visitReturn(ASTNode *Return)
{
  ASTNode *node = Return->child;
  int return_type = CurrentFunc->type.u.func.return_type;

  if (node->kind == K_NULL) {
    if (return_type != TYPE_VOID)
      fprintf(stderr, "Warning: return with no value in function `%s'.\n",
              CurrentFunc->name);
    emit("j _end_%s\n", CurrentFunc->name);
    return;
  }

  int type = visitExpr(node);
  if (return_type != type)
    error("return type mismatch in function `%s'.\n", CurrentFunc->name);
  if (return_type == TYPE_INT)
    emit_addi(a0, node->place, 0);
  else
    emit_fmv_s(fa0, node->place);
  emit("j _end_%s\n", CurrentFunc->name);
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
  int arg_size = ALIGN_UP(num_arguments * BASE_SIZE, ALIGNMENT);
  emit_addi(sp, sp, -arg_size);
  if (num_arguments != 0) {
    ASTNode *Expr = ArgumentList->child;
    int rs;
    while (Expr) {
      int type = visitExpr(Expr);
      rs = Expr->place;
      if (params[i].type == type) {
        // string does not matter since only library function use it
        if (params[i].type == TYPE_FLOAT)
          emit_fsw(rs, sp, i * BASE_SIZE + OFFSET_FIX);
        else
          emit_sw(rs, sp, i * BASE_SIZE + OFFSET_FIX);
      } else if (params[i].type == TYPE_FLOAT && type == TYPE_INT) {
        int frd = get_freg();
        emit_fcvt_s_w(frd, rs);
        emit_fsw(frd, sp, i * BASE_SIZE + OFFSET_FIX);
      } else
        error("argument type mismatch.\n");
      ++i;
      Expr = Expr->rightSib;
    }
    if (num_arguments == 1) { // for library function
      if (params[0].type == TYPE_FLOAT)
        emit_fmv_s(fa0, rs);
      else
        emit_addi(a0, rs, 0);
    }
  }
  emit_caller_save();
  emit("jal %s\n", id);
  emit_addi(sp, sp, arg_size);
  emit_caller_restore();

  int rd;
  Call->type = entry->type.u.func.return_type; 
  if (Call->type == TYPE_INT) {
    rd = get_reg();
    emit_addi(rd, a0, 0);
  } else if (Call->type == TYPE_FLOAT) {
    rd = get_freg();
    emit_fmv_s(rd, fa0);
  } else {
    rd = zero;
  }
  Call->place = rd;
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

static void visitGlobalDecl(ASTNode *node)
{
  if (node->kind != K_DECL)
    error("%s: internal error\n", __func__);

  ASTNode *Type = node->child;
  ASTNode *ID = Type->rightSib;

  while (ID) {
    char *name = get_id(ID); 
    SymbolType type;
    type.is_function = 0;
    build_var_desc(&type.u.var, Type, ID);

    SymbolTableEntry *symbol = enter_symbol(get_id(ID), &type);
    if (!symbol)
      error("%s.\n", error_msg);

    // Check if ID has an initial value (child node)
    if (ID->semantic.id.type == ID_TYPICAL_ASSIGN) { 
      ASTNode *Expr = ID->child; 
      
      if (Expr->kind == K_CONST) { 
        if (Expr->semantic.constant->type == TYPE_INT) { 
          emit("%s: .word %d\n", name, Expr->semantic.constant->u.const_int);
        } else if (Expr->semantic.constant->type == TYPE_FLOAT) { 
          emit("%s: .float %f\n", name, Expr->semantic.constant->u.const_fp);
        }
      } else {
        error("Global variable `%s` must be initialized with a constant.\n", name);
      }
    } 
    else if (ID->semantic.id.type == ID_ARRAY) { 
      int total_elements = 1;
      ASTNode *dim = ID->child;
      while (dim) {
        total_elements *= dim->semantic.constant->u.const_int; 
        dim = dim->rightSib;
      }
      emit("%s: .zero %d\n", name, total_elements * 4);
    } else {
      // Uninitialized scalar [cite: 125]
      emit("%s: .word 0\n", name);
    }
    ID = ID->rightSib;
  }
}

static void visitGlobalDeclList(ASTNode *node)
{
  if (node->semantic.list.type != LIST_DECL)
    error("%s: internal error\n", __func__);

  emit(".data\n");
  ASTNode *Decl = node->child;
  while (Decl) {
    visitGlobalDecl(Decl);
    Decl = Decl->rightSib;
  }
}

static int cal_ARoffset(ASTNode *block)
{
  int total_offset = 0;
  ASTNode *ListNode = block->child;

  while (ListNode) {
    if (ListNode->kind == K_LIST && ListNode->semantic.list.type == LIST_DECL) {
      ASTNode *Decl = ListNode->child;
      while (Decl) {
        ASTNode *Type = Decl->child;
        ASTNode *ID = Type->rightSib;

        while (ID) {
          // Build a temporary VarDesc to calculate size
          VarDesc v;
          build_var_desc(&v, Type, ID);
          int var_size = get_var_size(&v);
          
          // Align each variable to 8 bytes for 64-bit stack alignment
          // brutal but safe
          total_offset += ALIGN_UP(var_size, 8);
          
          free(v.dimensions);
          ID = ID->rightSib;
        }
        Decl = Decl->rightSib;
      }
    }
    ListNode = ListNode->rightSib;
  }
  return total_offset;
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

  // initialize some global var
  ARoffset = TOTAL_REG_SIZE;
  reg_id = 0;
  freg_id = 0;

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
  
  char *func_name = get_id(ID);
  gen_head(func_name);
  // ARoffset = cal_ARoffset(Block);
  // int reg_space = 24 * 8;
  // int total_frame_size = ALIGN_UP(ARoffset + reg_space, 16);
  gen_prologue(func_name);

  open_scope();

  /* insert function parameters into the symbol table */
  int offset = FP_TO_PARAM;
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
      symbol->offset = offset;
      offset += BASE_SIZE;
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

  gen_epilogue(func_name, ALIGN_UP(ARoffset, ALIGNMENT));
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
      visitGlobalDeclList(ListNode);
    else
      visitFuncList(ListNode);

    ListNode = ListNode->rightSib;
  }

  close_scope();
  gen_str_const_section();
}

void gen_head(char *name)
{
  emit(".text\n");
  if (strcmp(name, "main") == 0) 
    emit("__start_main:\n");
  else
    emit("%s:\n", name);
}

void gen_prologue(char *name)
{
  emit("sd ra, 0(sp)\n" 		    // store return address
       "sd fp, -8(sp)\n"      	// save old fp
       "addi fp, sp, -8\n"    	// new fp
       "addi sp, sp, -16\n" 	    // new sp
       "la ra, _frameSize_%s\n"
       "lw ra, 0(ra)\n"
       "sub sp, sp, ra\n", 		// push new AR
        name);
  // emit_dynamic_save(ARoffset);
  emit_callee_save();
  emit("_begin_%s:\n", name);
}

void gen_epilogue(char *name, int frame_size)
{
  emit("_end_%s:\n", name);
  // emit_dynamic_restore(ARoffset);
  emit_callee_restore();
  emit("ld ra, 8(fp)\n"      // restore return address
       "addi sp, fp, 8\n"    // pop AR
       "ld fp, 0(fp)\n"      // restore caller (old) fp
       "jr ra\n"
       ".data\n"
       "_frameSize_%s: .word %d\n",
       name, frame_size);
}

void code_generation()
{
  char *filename = "output.s";
  file_p = fopen(filename, "w+");
  if (!file_p)
    error("cannot open file %s\n", filename);

  dbg("===== Code Generation Start =====\n");
  visitProgram(ast_root);

  // example();

  fclose(file_p);
}