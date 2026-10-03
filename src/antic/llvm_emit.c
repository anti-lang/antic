#include "llvm_emit.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abi.h"
#include "alloc.h"
#include "llvm_target.h"
#include "rt_abi.h"

/* DESIGN: the IR is not in SSA form, so every temporary gets an alloca in
   the entry block. A definition stores into it and each use loads from
   it. The sroa and mem2reg passes of opt build the SSA form, as they do
   for every local clang writes. Each block of the IR is one LLVM block
   named b<index>, and the entry block of the IR is the entry of the
   function. The section "Instruction mapping" of
   docs/work-order-llvm-back-end.md gives the form of each operation.

   The step emit-core translates copy, jump, branch and ret, and the
   signatures of scalars. The step emit-arith adds the scalar operations,
   the entry of the runtime and the globals that hold no address. Every
   other operation is refused with the step that adds it. A global that
   holds an address and every instruction that names a global wait for the
   step emit-memory. */

/* The metadata of a branch into a cold block, one id for each side. */
enum { COLD_THEN, COLD_ELSE, COLD_COUNT };

struct emitter {
    const struct llvm_emit_options *o;
    struct layouts *l;
    struct text *out;
    char *error;
    size_t error_size;
    bool failed;
    const struct ir_function *f;
    uint32_t value;                 /* the next %v<n> of the function */
    uint32_t cold_id;               /* the metadata id of COLD_THEN */
    bool cold_used[COLD_COUNT];
    struct text intrinsics;         /* the declarations of the intrinsics */
    struct text entry;              /* the type of main, for its alias */
};

static void refuse(struct emitter *e, const char *what, const char *step)
{
    struct text name = {0};

    if (e->failed) {
        return;
    }
    e->failed = true;
    ir_name_append(&name, e->f->module, e->f->name);
    text_format(e->error, e->error_size,
                "the LLVM back end does not translate %s before the step "
                "%s, in %s",
                what, step, text_cstr(&name));
    text_free(&name);
}

/* The step of the work order that translates op, or NULL for the
   operations of the steps emit-core and emit-arith. */
static const char *step_of(enum ir_op op)
{
    switch (op) {
    case IR_COPY:
    case IR_JUMP:
    case IR_BRANCH:
    case IR_RET:
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_SDIV: case IR_UDIV:
    case IR_SREM: case IR_UREM: case IR_AND: case IR_OR: case IR_XOR:
    case IR_SHL: case IR_SHR_S: case IR_SHR_U:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_NEG: case IR_NOT: case IR_FNEG:
    case IR_EQ: case IR_NE: case IR_SLT: case IR_SLE: case IR_SGT:
    case IR_SGE: case IR_ULT: case IR_ULE: case IR_UGT: case IR_UGE:
    case IR_FEQ: case IR_FNE: case IR_FLT: case IR_FLE: case IR_FGT:
    case IR_FGE:
    case IR_TRUNC: case IR_SEXT: case IR_ZEXT: case IR_SITOF:
    case IR_UITOF: case IR_FTOSI: case IR_FTOUI: case IR_FEXT:
    case IR_FTRUNC: case IR_HEXT: case IR_HTRUNC:
        return NULL;
    case IR_ADD_OV: case IR_SUB_OV: case IR_MUL_OV: case IR_BRANCH_OV:
    case IR_MULH_S: case IR_MULH_U:
    case IR_ADD_SAT_S: case IR_ADD_SAT_U: case IR_SUB_SAT_S:
    case IR_SUB_SAT_U: case IR_MUL_SAT_S: case IR_MUL_SAT_U:
    case IR_ADD_FL: case IR_SUB_FL: case IR_MUL_FL: case IR_SHL_FL:
    case IR_SHR_S_FL: case IR_SHR_U_FL: case IR_NEG_FL: case IR_FLAG:
    case IR_VBINARY: case IR_VUNARY: case IR_VSPLAT: case IR_VSELECT:
    case IR_VSHUFFLE: case IR_VREDUCE:
        return "emit-wide";
    case IR_SLOT: case IR_LOAD: case IR_STORE: case IR_PTRADD:
    case IR_MEMCOPY: case IR_ADDR: case IR_BITLOAD: case IR_BITSTORE:
    case IR_CALL:
        return "emit-memory";
    }
    return "emit-memory";
}

/* The LLVM type of a scalar after layout_resolve, or NULL for an
   aggregate and the types the layouts replace. */
static const char *scalar_type(enum ir_type type)
{
    switch (type) {
    case IR_VOID: return "void";
    case IR_I8: return "i8";
    case IR_I16: return "i16";
    case IR_I32: return "i32";
    case IR_I64: return "i64";
    case IR_F32: return "float";
    case IR_F64: return "double";
    case IR_PTR: return "ptr";
    case IR_AGG:
    case IR_CLONG:
    case IR_CWCHAR:
    case IR_LOCK:
        break;
    }
    return NULL;
}

/* The LLVM type of a value of type, or NULL after a refusal. */
static const char *value_type(struct emitter *e, enum ir_type type)
{
    const char *name = scalar_type(type);

    if (name == NULL) {
        refuse(e, type == IR_AGG ? "an aggregate value" : "this type",
               "emit-memory");
    }
    return name;
}

static uint64_t align_of(struct emitter *e, enum ir_type type)
{
    return layout_align(e->l, ir_scalar(type));
}

/* Whether name is an LLVM identifier that needs no quotes. */
static bool plain_name(const char *name)
{
    const char *p;

    if (*name == '\0' || (*name >= '0' && *name <= '9')) {
        return false;
    }
    for (p = name; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '$' ||
              ch == '.' || ch == '_')) {
            return false;
        }
    }
    return true;
}

/* Append name after sigil, in quotes with \XX escapes where it is no
   plain identifier. */
static void llvm_name(struct text *out, char sigil, const char *name)
{
    const char *p;

    text_appendf(out, "%c", sigil);
    if (plain_name(name)) {
        text_append(out, name);
        return;
    }
    text_append(out, "\"");
    for (p = name; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\') {
            text_appendf(out, "\\%02X", ch);
        } else {
            text_appendf(out, "%c", ch);
        }
    }
    text_append(out, "\"");
}

/* DESIGN: a function takes the symbol the native back end gives it, so
   objects of the two back ends link together until the step switch. llc
   applies the mangling of the triple, so the text drops the leading _ of
   Mach-O, which llc writes again. */
static void function_symbol(struct text *out, enum target t,
                            const struct ir_function *f)
{
    struct text symbol = {0};
    const char *name;

    if (f->module == NULL || f->exported) {
        target_c_symbol(&symbol, t, f->name);
    } else {
        target_mangle(&symbol, t, f->module, f->name);
    }
    name = text_cstr(&symbol);
    if (target_info(t)->format == FORMAT_MACHO && name[0] == '_') {
        name++;
    }
    text_append(out, name);
    text_free(&symbol);
}

static void function_name(struct text *out, enum target t,
                          const struct ir_function *f)
{
    struct text symbol = {0};

    function_symbol(&symbol, t, f);
    llvm_name(out, '@', text_cstr(&symbol));
    text_free(&symbol);
}

/* The constant of an integer operand of type: the signed value of its
   bits, or null for a pointer. */
static void int_constant(struct text *out, enum ir_type type, uint64_t value)
{
    switch (type) {
    case IR_I8:
        text_appendf(out, "%d", (int)(int8_t)(uint8_t)value);
        return;
    case IR_I16:
        text_appendf(out, "%d", (int)(int16_t)(uint16_t)value);
        return;
    case IR_I32:
        text_appendf(out, "%" PRId32, (int32_t)(uint32_t)value);
        return;
    case IR_PTR:
        if (value == 0) {
            text_append(out, "null");
        } else {
            text_appendf(out, "inttoptr (i64 %" PRIu64 " to ptr)", value);
        }
        return;
    default:
        text_appendf(out, "%" PRId64, (int64_t)value);
        return;
    }
}

/* The constant of a float operand: the hexadecimal bits of its double,
   which LLVM takes for both float types. A float holds the value rounded
   to its width, so its double is exact. */
static void float_constant(struct text *out, enum ir_type type, double value)
{
    double exact = type == IR_F32 ? (double)(float)value : value;
    uint64_t bits;

    memcpy(&bits, &exact, sizeof bits);
    text_appendf(out, "0x%016" PRIX64, bits);
}

/* Append to value the LLVM operand of o as a value of type, after the
   load that reads a temporary. */
static void operand(struct emitter *e, const struct ir_operand *o,
                    enum ir_type type, struct text *value)
{
    const char *name = value_type(e, type);

    if (name == NULL) {
        return;
    }
    switch (o->kind) {
    case IR_TEMP:
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %%t%" PRIu32
                             ", align %" PRIu64 "\n",
                     e->value, name, o->as.temp, align_of(e, type));
        text_appendf(value, "%%v%" PRIu32, e->value++);
        return;
    case IR_INT:
        int_constant(value, type, o->as.integer);
        return;
    case IR_FLOAT:
        float_constant(value, type, o->as.floating);
        return;
    case IR_GLOBAL:
    case IR_FUNC:
        refuse(e, "the address of a global or a function", "emit-memory");
        return;
    case IR_NONE:
    case IR_BLOCK:
    case IR_SYM:
        break;
    }
    refuse(e, "this operand", "emit-memory");
}

static void store_result(struct emitter *e, const struct ir_inst *inst,
                         const char *value)
{
    text_appendf(e->out, "  store %s %s, ptr %%t%" PRIu32 ", align %" PRIu64
                         "\n",
                 scalar_type(inst->type), value, inst->result,
                 align_of(e, inst->type));
}

/* The type of operand o: the type of its temporary, or the type a
   constant carries. */
static enum ir_type operand_type(const struct emitter *e,
                                 const struct ir_operand *o)
{
    return o->kind == IR_TEMP ? e->f->temps[o->as.temp] : o->type;
}

/* The next value of the function, %v<n>, which the caller defines. */
static uint32_t fresh(struct emitter *e)
{
    return e->value++;
}

static unsigned width_of(enum ir_type type)
{
    return type == IR_I8 ? 8 : type == IR_I16 ? 16 : type == IR_I32 ? 32 : 64;
}

/* The LLVM instruction of an operation that maps to one instruction on
   the same type, or NULL. */
static const char *plain_binary(enum ir_op op)
{
    switch (op) {
    case IR_ADD: return "add";
    case IR_SUB: return "sub";
    case IR_MUL: return "mul";
    case IR_AND: return "and";
    case IR_OR: return "or";
    case IR_XOR: return "xor";
    case IR_FADD: return "fadd";
    case IR_FSUB: return "fsub";
    case IR_FMUL: return "fmul";
    case IR_FDIV: return "fdiv";
    default: return NULL;
    }
}

/* The instruction and the predicate of a comparison. */
static const char *comparison(enum ir_op op)
{
    switch (op) {
    case IR_EQ: return "icmp eq";
    case IR_NE: return "icmp ne";
    case IR_SLT: return "icmp slt";
    case IR_SLE: return "icmp sle";
    case IR_SGT: return "icmp sgt";
    case IR_SGE: return "icmp sge";
    case IR_ULT: return "icmp ult";
    case IR_ULE: return "icmp ule";
    case IR_UGT: return "icmp ugt";
    case IR_UGE: return "icmp uge";
    case IR_FEQ: return "fcmp oeq";
    case IR_FNE: return "fcmp une";
    case IR_FLT: return "fcmp olt";
    case IR_FLE: return "fcmp ole";
    case IR_FGT: return "fcmp ogt";
    case IR_FGE: return "fcmp oge";
    default: return NULL;
    }
}

/* DESIGN: a division by zero gives 0, and the least value divided by
   minus one gives the least value, with a remainder of 0, on every
   target. The section "Defined results in release mode" of
   docs/work-order-llvm-back-end.md decides it. The divisor the
   instruction sees is 1 where the divisor is 0 or minus one, so no sdiv
   or srem of the text is undefined, and selects put in the defined
   result. A division by minus one is the negation, which wraps at the
   least value. A remainder by 1 is 0, which is the result of both cases.
   opt folds each select whose condition it can decide. */
static void division(struct emitter *e, enum ir_op op, const char *type,
                     const char *a, const char *b)
{
    bool is_signed = op == IR_SDIV || op == IR_SREM;
    bool quotient = op == IR_SDIV || op == IR_UDIV;
    uint32_t zero = fresh(e);
    uint32_t guard = zero;
    uint32_t minus = 0;
    uint32_t safe;
    uint32_t raw;

    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, 0\n", zero, type,
                 b);
    if (is_signed) {
        minus = fresh(e);
        guard = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, -1\n", minus,
                     type, b);
        text_appendf(e->out, "  %%v%" PRIu32 " = or i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     guard, zero, minus);
    }
    safe = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32 ", %s 1, "
                         "%s %s\n",
                 safe, guard, type, type, b);
    raw = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %%v%" PRIu32 "\n", raw,
                 op == IR_SDIV   ? "sdiv"
                 : op == IR_UDIV ? "udiv"
                 : op == IR_SREM ? "srem"
                                 : "urem",
                 type, a, safe);
    if (!quotient) {
        return;
    }
    if (is_signed) {
        uint32_t negated = fresh(e);
        uint32_t chosen = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n", negated,
                     type, a);
        text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32
                             ", %s %%v%" PRIu32 ", %s %%v%" PRIu32 "\n",
                     chosen, minus, type, negated, type, raw);
        raw = chosen;
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32 ", %s 0, "
                         "%s %%v%" PRIu32 "\n",
                 fresh(e), zero, type, type, raw);
}

/* Declare the intrinsic name of type result (parameters) once. */
static void intrinsic(struct emitter *e, const char *result, const char *name,
                      const char *parameters)
{
    char line[160];

    text_format(line, sizeof line, "declare %s @%s(%s)\n", result, name,
                parameters);
    if (strstr(text_cstr(&e->intrinsics), line) == NULL) {
        text_append(&e->intrinsics, line);
    }
}

/* An operation of two operands of one type: the plain instructions, the
   shifts with the count modulo the width, the guarded divisions and the
   comparisons, which widen their i1 to the i8 of a bool. */
static void binary(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_type type = comparison(inst->op) != NULL
                            ? operand_type(e, &inst->a)
                            : inst->type;
    const char *name = value_type(e, type);
    struct text a = {0};
    struct text b = {0};
    char value[24];

    if (name == NULL) {
        return;
    }
    operand(e, &inst->a, type, &a);
    operand(e, &inst->b, type, &b);
    if (e->failed) {
        text_free(&a);
        text_free(&b);
        return;
    }
    switch (inst->op) {
    case IR_SHL:
    case IR_SHR_S:
    case IR_SHR_U: {
        uint32_t count = fresh(e);
        uint32_t shifted = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = and %s %s, %u\n", count, name,
                     text_cstr(&b), width_of(type) - 1);
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %%v%" PRIu32 "\n",
                     shifted,
                     inst->op == IR_SHL     ? "shl"
                     : inst->op == IR_SHR_S ? "ashr"
                                            : "lshr",
                     name, text_cstr(&a), count);
        break;
    }
    case IR_SDIV:
    case IR_UDIV:
    case IR_SREM:
    case IR_UREM:
        division(e, inst->op, name, text_cstr(&a), text_cstr(&b));
        break;
    default:
        if (comparison(inst->op) != NULL) {
            uint32_t bit = fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %s\n", bit,
                         comparison(inst->op), name, text_cstr(&a),
                         text_cstr(&b));
            text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %%v%" PRIu32
                                 " to i8\n",
                         fresh(e), bit);
        } else {
            text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %s\n",
                         fresh(e), plain_binary(inst->op), name,
                         text_cstr(&a), text_cstr(&b));
        }
        break;
    }
    text_format(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    store_result(e, inst, value);
    text_free(&a);
    text_free(&b);
}

/* The name of a float type in the name of an intrinsic. */
static const char *float_suffix(enum ir_type type)
{
    return type == IR_F32 ? "f32" : "f64";
}

/* DESIGN: a float out of the range of the integer type saturates, and NaN
   gives 0, on every target. llvm.fptosi.sat and llvm.fptoui.sat define
   exactly that, where fptosi and fptoui give poison. An f16 is its bits
   in an i16, so hext and htrunc pass through half, and llc calls the
   conversions of compiler-rt where the processor level has no
   instruction for them. */
static void conversion(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_type from = operand_type(e, &inst->a);
    const char *source = value_type(e, from);
    const char *target = value_type(e, inst->type);
    struct text a = {0};
    char value[24];

    if (source == NULL || target == NULL) {
        return;
    }
    operand(e, &inst->a, from, &a);
    if (e->failed) {
        text_free(&a);
        return;
    }
    switch (inst->op) {
    case IR_FTOSI:
    case IR_FTOUI: {
        char name[48];
        text_format(name, sizeof name, "llvm.fpto%s.sat.i%u.%s",
                    inst->op == IR_FTOSI ? "si" : "ui", width_of(inst->type),
                    float_suffix(from));
        intrinsic(e, target, name, source);
        text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s)\n",
                     fresh(e), target, name, source, text_cstr(&a));
        break;
    }
    case IR_HEXT: {
        uint32_t half = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = bitcast %s %s to half\n",
                     half, source, text_cstr(&a));
        text_appendf(e->out, "  %%v%" PRIu32 " = fpext half %%v%" PRIu32
                             " to %s\n",
                     fresh(e), half, target);
        break;
    }
    case IR_HTRUNC: {
        uint32_t half = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = fptrunc %s %s to half\n",
                     half, source, text_cstr(&a));
        text_appendf(e->out, "  %%v%" PRIu32 " = bitcast half %%v%" PRIu32
                             " to %s\n",
                     fresh(e), half, target);
        break;
    }
    default:
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n", fresh(e),
                     inst->op == IR_TRUNC   ? "trunc"
                     : inst->op == IR_SEXT  ? "sext"
                     : inst->op == IR_ZEXT  ? "zext"
                     : inst->op == IR_SITOF ? "sitofp"
                     : inst->op == IR_UITOF ? "uitofp"
                     : inst->op == IR_FEXT  ? "fpext"
                                            : "fptrunc",
                     source, text_cstr(&a), target);
        break;
    }
    text_format(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    store_result(e, inst, value);
    text_free(&a);
}

/* neg, not and fneg: sub from 0, xor with all ones and fneg. */
static void unary(struct emitter *e, const struct ir_inst *inst)
{
    const char *name = value_type(e, inst->type);
    struct text a = {0};
    char value[24];

    if (name == NULL) {
        return;
    }
    operand(e, &inst->a, inst->type, &a);
    if (e->failed) {
        text_free(&a);
        return;
    }
    if (inst->op == IR_NEG) {
        text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n", fresh(e),
                     name, text_cstr(&a));
    } else if (inst->op == IR_NOT) {
        text_appendf(e->out, "  %%v%" PRIu32 " = xor %s %s, -1\n", fresh(e),
                     name, text_cstr(&a));
    } else {
        text_appendf(e->out, "  %%v%" PRIu32 " = fneg %s %s\n", fresh(e),
                     name, text_cstr(&a));
    }
    text_format(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    store_result(e, inst, value);
    text_free(&a);
}

/* The metadata of a branch whose one side is the failure arm of an
   assertion or a check, which marks that side cold. */
static void branch_weights(struct emitter *e, uint32_t then_block,
                           uint32_t else_block)
{
    bool then_cold = e->f->blocks[then_block]->fail != IR_FAIL_NONE;
    bool else_cold = e->f->blocks[else_block]->fail != IR_FAIL_NONE;

    if (then_cold == else_cold) {
        return;
    }
    text_appendf(e->out, ", !prof !%" PRIu32,
                 e->cold_id + (then_cold ? COLD_THEN : COLD_ELSE));
    e->cold_used[then_cold ? COLD_THEN : COLD_ELSE] = true;
}

static void instruction(struct emitter *e, const struct ir_inst *inst)
{
    struct text value = {0};
    const char *step = step_of(inst->op);

    if (step != NULL) {
        char what[40];
        text_format(what, sizeof what, "`%s`", ir_op_name(inst->op));
        refuse(e, what, step);
        return;
    }
    switch (inst->op) {
    case IR_COPY:
        operand(e, &inst->a, inst->type, &value);
        if (!e->failed) {
            store_result(e, inst, text_cstr(&value));
        }
        break;
    case IR_JUMP:
        text_appendf(e->out, "  br label %%b%" PRIu32 "\n", inst->a.as.index);
        break;
    case IR_BRANCH:
        operand(e, &inst->a, IR_I8, &value);
        if (e->failed) {
            break;
        }
        text_appendf(e->out, "  %%v%" PRIu32 " = trunc i8 %s to i1\n",
                     e->value, text_cstr(&value));
        text_appendf(e->out, "  br i1 %%v%" PRIu32 ", label %%b%" PRIu32
                             ", label %%b%" PRIu32,
                     e->value++, inst->b.as.index, inst->c.as.index);
        branch_weights(e, inst->b.as.index, inst->c.as.index);
        text_append(e->out, "\n");
        break;
    case IR_RET:
        if (inst->type == IR_VOID) {
            text_append(e->out, "  ret void\n");
            break;
        }
        operand(e, &inst->a, inst->type, &value);
        if (!e->failed) {
            text_appendf(e->out, "  ret %s %s\n", scalar_type(inst->type),
                         text_cstr(&value));
        }
        break;
    case IR_NEG:
    case IR_NOT:
    case IR_FNEG:
        unary(e, inst);
        break;
    case IR_TRUNC: case IR_SEXT: case IR_ZEXT: case IR_SITOF: case IR_UITOF:
    case IR_FTOSI: case IR_FTOUI: case IR_FEXT: case IR_FTRUNC:
    case IR_HEXT: case IR_HTRUNC:
        conversion(e, inst);
        break;
    default:
        binary(e, inst);
        break;
    }
    text_free(&value);
}

/* The parameters of f in parentheses, with the names %p<i> when names is
   set, as abi_classify gives them. The function type goes to type unless
   it is NULL. Returns false after a refusal. */
static bool signature(struct emitter *e, const struct ir_function *f,
                      bool names, struct text *type)
{
    struct abi_param *params =
        alloc_zeroed(f->param_count + 1, sizeof *params);
    struct abi_param result;
    struct text head = {0};
    struct text types = {0};
    size_t i;

    abi_classify(e->o->target, e->l, f, params, &result);
    if (result.kind != ABI_DIRECT) {
        refuse(e, "an aggregate parameter or result", "emit-memory");
    }
    for (i = 0; i < f->param_count && !e->failed; i++) {
        const struct ir_param *p = &f->params[i];
        if (params[i].kind != ABI_DIRECT) {
            refuse(e, "an aggregate parameter or result", "emit-memory");
            break;
        }
        text_appendf(&head, "%s%s", i > 0 ? ", " : "", params[i].types[0]);
        text_appendf(&types, "%s%s", i > 0 ? ", " : "", params[i].types[0]);
        if (p->type == IR_I8 || p->type == IR_I16) {
            text_append(&head, p->ext == IR_EXT_SIGN   ? " signext"
                               : p->ext == IR_EXT_ZERO ? " zeroext"
                                                       : "");
        }
        if (names) {
            text_appendf(&head, " %%p%zu", i);
        }
    }
    if (f->variadic) {
        text_append(&head, f->param_count > 0 ? ", ..." : "...");
        text_append(&types, f->param_count > 0 ? ", ..." : "...");
    }
    if (!e->failed) {
        text_appendf(e->out, "%s ", result.types[0]);
        function_name(e->out, e->o->target, f);
        text_appendf(e->out, "(%s)", text_cstr(&head));
        if (type != NULL) {
            text_appendf(type, "%s (%s)", result.types[0], text_cstr(&types));
        }
    }
    text_free(&head);
    text_free(&types);
    free(params);
    return !e->failed;
}

/* Whether f is a copy of a generic in an object of one module, which
   every module that uses it defines. */
static bool link_once(const struct emitter *e, const struct ir_function *f)
{
    return e->o->one_module && ir_is_copy_name(f->name);
}

/* DESIGN: the linkage follows the symbols emit.c writes. A whole program
   keeps every function internal, unless it is an export fn or the
   program hosts plugins. An object of one module makes every function
   global and hidden for the other objects, and a copy of a generic weak,
   in a COMDAT on COFF, which has no hidden symbols. An export fn is
   dso_local with default visibility. */
static void linkage(struct emitter *e, const struct ir_function *f)
{
    bool coff = target_info(e->o->target)->format == FORMAT_COFF;

    if (!f->exported && !e->o->one_module && !e->o->exports) {
        text_append(e->out, "internal ");
        return;
    }
    if (link_once(e, f)) {
        text_append(e->out, "weak_odr ");
    }
    if (f->exported) {
        text_append(e->out, "dso_local ");
    } else if (e->o->one_module && !e->o->exports && !coff) {
        text_append(e->out, "hidden ");
    }
}

/* Whether f is the main of the module that is compiled, which the
   runtime calls through its entry. */
static bool is_main(const struct emitter *e, const struct ir_function *f)
{
    return !f->is_extern && f->module != NULL &&
           strcmp(f->module, e->o->module) == 0 && strcmp(f->name, "main") == 0;
}

static void definition(struct emitter *e, const struct ir_function *f)
{
    size_t b;
    size_t i;
    uint32_t t;

    e->f = f;
    e->value = 0;
    text_append(e->out, "define ");
    linkage(e, f);
    if (!signature(e, f, true, is_main(e, f) ? &e->entry : NULL)) {
        return;
    }
    text_append(e->out, " #0");
    if (link_once(e, f) &&
        target_info(e->o->target)->format == FORMAT_COFF) {
        text_append(e->out, " comdat");
    }
    text_append(e->out, " {\n");
    for (b = 0; b < f->block_count && !e->failed; b++) {
        const struct ir_block *block = f->blocks[b];
        text_appendf(e->out, "%sb%zu:\n", b > 0 ? "\n" : "", b);
        if (b == 0) {
            for (t = 0; t < f->temp_count && !e->failed; t++) {
                const char *name = value_type(e, f->temps[t]);
                if (name != NULL) {
                    text_appendf(e->out, "  %%t%" PRIu32 " = alloca %s, align "
                                         "%" PRIu64 "\n",
                                 t, name, align_of(e, f->temps[t]));
                }
            }
            for (i = 0; i < f->param_count && !e->failed; i++) {
                enum ir_type type = f->params[i].type;
                text_appendf(e->out, "  store %s %%p%zu, ptr %%t%" PRIu32
                                     ", align %" PRIu64 "\n",
                             scalar_type(type), i, f->params[i].temp,
                             align_of(e, type));
            }
        }
        for (i = 0; i < block->count && !e->failed; i++) {
            instruction(e, &block->insts[i]);
        }
    }
    text_append(e->out, "}\n\n");
}

/* The symbol of a global, without the _ of Mach-O, which llc writes. */
static void global_symbol(struct text *out, enum target t,
                          const struct ir_global *g)
{
    struct text symbol = {0};
    const char *name;

    if (g->exported) {
        target_c_symbol(&symbol, t, g->name);
    } else {
        target_mangle(&symbol, t, g->module, g->name);
    }
    name = text_cstr(&symbol);
    if (target_info(t)->format == FORMAT_MACHO && name[0] == '_') {
        name++;
    }
    llvm_name(out, '@', name);
    text_free(&symbol);
}

/* DESIGN: a global is a packed struct of its bytes, so its size and its
   offsets are the ones layout_data computed, whatever the data layout
   string says. The section "Globals, sections and constructors" of
   docs/work-order-llvm-back-end.md gives the form. A global the program
   writes is a global, every other one a constant, which llc puts in the
   read-only section of each format. The linkage follows the one of a
   function. A global that holds an address waits for the step
   emit-memory, which writes it as members of ptr. A global the runtime
   defines is declared by the step that names it. */
static void global(struct emitter *e, const struct ir_global *g)
{
    struct text name = {0};
    bool zero = true;
    uint64_t k;

    if (g->is_extern) {
        return;
    }
    if (g->reloc_count > 0) {
        if (!e->failed) {
            e->failed = true;
            ir_name_append(&name, g->module, g->name);
            text_format(e->error, e->error_size,
                        "the LLVM back end does not translate a global that "
                        "holds an address before the step emit-memory, in "
                        "%s",
                        text_cstr(&name));
            text_free(&name);
        }
        return;
    }
    global_symbol(e->out, e->o->target, g);
    text_append(e->out, " = ");
    if (g->exported) {
        text_append(e->out, "dso_local ");
    } else if (!e->o->one_module && !e->o->exports) {
        text_append(e->out, "internal ");
    } else if (e->o->one_module && !e->o->exports &&
               target_info(e->o->target)->format != FORMAT_COFF) {
        text_append(e->out, "hidden ");
    }
    for (k = 0; k < g->size; k++) {
        zero = zero && g->bytes[k] == 0;
    }
    text_appendf(e->out, "%s <{ [%" PRIu64 " x i8] }> ",
                 g->mutable ? "global" : "constant", g->size);
    if (zero) {
        text_append(e->out, "zeroinitializer");
    } else {
        text_appendf(e->out, "<{ [%" PRIu64 " x i8] c\"", g->size);
        for (k = 0; k < g->size; k++) {
            unsigned char ch = g->bytes[k];
            if (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\') {
                text_appendf(e->out, "\\%02X", ch);
            } else {
                text_appendf(e->out, "%c", ch);
            }
        }
        text_append(e->out, "\" }>");
    }
    text_appendf(e->out, ", align %" PRIu64 "\n", g->align > 0 ? g->align : 1);
}

/* DESIGN: the runtime calls main through its entry, the name RUNTIME_ENTRY
   of RUNTIME_MODULE, as the .set of emit.c does for the native back end.
   An alias gives main the second name, which stays global while main
   itself may be internal. */
static void entry(struct emitter *e, const struct ir_module *m)
{
    struct text symbol = {0};
    const char *name;
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!is_main(e, f)) {
            continue;
        }
        target_mangle(&symbol, e->o->target, RUNTIME_MODULE, RUNTIME_ENTRY);
        name = text_cstr(&symbol);
        if (target_info(e->o->target)->format == FORMAT_MACHO &&
            name[0] == '_') {
            name++;
        }
        llvm_name(e->out, '@', name);
        text_appendf(e->out, " = alias %s, ptr ", text_cstr(&e->entry));
        function_name(e->out, e->o->target, f);
        text_append(e->out, "\n\n");
        text_free(&symbol);
        return;
    }
}

static void declaration(struct emitter *e, const struct ir_function *f)
{
    e->f = f;
    text_append(e->out, "declare ");
    if (signature(e, f, false, NULL)) {
        text_append(e->out, " #1\n");
    }
}

/* DESIGN: every Anti function is nounwind, since Anti has no exceptions
   and fails through a return value. A declaration is a function of
   another module or a function of C, which unwinds through no Anti frame
   either. Windows walks a stack through the unwind tables of each frame,
   and its frames of a page or more call the stack probe. Apple's arm64
   keeps the frame pointer, which the backtraces of crash reports read. */
static void attributes(struct emitter *e)
{
    const struct target_info *info = target_info(e->o->target);
    bool windows = info->os == OS_WINDOWS;
    bool apple_arm64 =
        info->os == OS_MACOS && info->arch == ARCH_ARM64;

    text_appendf(e->out, "attributes #0 = { nounwind%s \"frame-pointer\"="
                         "\"%s\"%s \"target-cpu\"=\"%s\" "
                         "\"target-features\"=\"%s\" }\n",
                 windows ? " uwtable(sync)" : "",
                 apple_arm64 ? "non-leaf" : "none",
                 windows ? " \"stack-probe-size\"=\"4096\"" : "",
                 llvm_target_cpu(e->o->cpu),
                 llvm_target_features(e->o->cpu));
    text_append(e->out, "attributes #1 = { nounwind }\n\n");
}

/* The module flags, the identification and the weights of the cold
   branches, in the ids the branches named. */
static void metadata(struct emitter *e, uint32_t flag_count)
{
    const struct target_info *info = target_info(e->o->target);
    uint32_t i;
    uint32_t id = 0;

    text_append(e->out, "!llvm.module.flags = !{");
    for (i = 0; i < flag_count; i++) {
        text_appendf(e->out, "%s!%" PRIu32, i > 0 ? ", " : "", i);
    }
    text_appendf(e->out, "}\n!llvm.ident = !{!%" PRIu32 "}\n", flag_count);
    text_appendf(e->out, "!%" PRIu32 " = !{i32 1, !\"wchar_size\", i32 %"
                         PRIu64 "}\n",
                 id++, layout_size(e->l, ir_scalar(IR_CWCHAR)));
    if (strcmp(llvm_relocation_model(e->o->target), "pic") == 0) {
        text_appendf(e->out, "!%" PRIu32 " = !{i32 8, !\"PIC Level\", "
                             "i32 2}\n",
                     id++);
    }
    if (info->os == OS_WINDOWS) {
        text_appendf(e->out, "!%" PRIu32 " = !{i32 7, !\"uwtable\", i32 2}\n",
                     id++);
    }
    text_appendf(e->out, "!%" PRIu32 " = !{!\"antic %s\"}\n", id,
                 ANTIC_VERSION);
    if (e->cold_used[COLD_THEN]) {
        text_appendf(e->out, "!%" PRIu32 " = !{!\"branch_weights\", i32 1, "
                             "i32 2000}\n",
                     e->cold_id + COLD_THEN);
    }
    if (e->cold_used[COLD_ELSE]) {
        text_appendf(e->out, "!%" PRIu32 " = !{!\"branch_weights\", "
                             "i32 2000, i32 1}\n",
                     e->cold_id + COLD_ELSE);
    }
}

/* The number of module flags of the target: the size of wchar_t, the
   level of position-independent code where the relocation model is pic,
   and the unwind tables of Windows. */
static uint32_t flag_count(enum target t)
{
    return 1 + (strcmp(llvm_relocation_model(t), "pic") == 0) +
           (target_info(t)->os == OS_WINDOWS);
}

bool llvm_emit_module(struct text *out, const struct llvm_emit_options *o,
                      const struct ir_module *m, struct layouts *l,
                      char *error, size_t error_size)
{
    struct emitter e;
    struct text body = {0};
    struct text declarations = {0};
    struct text data = {0};
    uint32_t flags = flag_count(o->target);
    bool comdats = false;
    size_t i;

    memset(&e, 0, sizeof e);
    e.o = o;
    e.l = l;
    e.error = error;
    e.error_size = error_size;
    e.cold_id = flags + 1;
    text_appendf(out, "source_filename = \"%s\"\n", o->module);
    text_appendf(out, "target datalayout = \"%s\"\n",
                 llvm_data_layout(o->target));
    text_append(out, "target triple = \"");
    llvm_triple(out, o->target);
    text_append(out, "\"\n\n");
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!f->is_extern && link_once(&e, f) &&
            target_info(o->target)->format == FORMAT_COFF) {
            struct text symbol = {0};
            function_symbol(&symbol, o->target, f);
            llvm_name(out, '$', text_cstr(&symbol));
            text_append(out, " = comdat any\n");
            text_free(&symbol);
            comdats = true;
        }
    }
    if (comdats) {
        text_append(out, "\n");
    }
    for (i = 0; i < m->function_count && !e.failed; i++) {
        const struct ir_function *f = m->functions[i];
        e.out = f->is_extern ? &declarations : &body;
        if (f->is_extern) {
            declaration(&e, f);
        } else {
            definition(&e, f);
        }
    }
    e.out = &data;
    for (i = 0; i < m->global_count && !e.failed; i++) {
        global(&e, m->globals[i]);
    }
    if (!e.failed) {
        text_append_bytes(out, body.data, body.length);
        e.out = out;
        entry(&e, m);
        if (data.length > 0) {
            text_append_bytes(out, data.data, data.length);
            text_append(out, "\n");
        }
        if (declarations.length > 0 || e.intrinsics.length > 0) {
            text_append_bytes(out, declarations.data, declarations.length);
            text_append_bytes(out, e.intrinsics.data, e.intrinsics.length);
            text_append(out, "\n");
        }
        attributes(&e);
        metadata(&e, flags);
    }
    text_free(&body);
    text_free(&data);
    text_free(&declarations);
    text_free(&e.intrinsics);
    text_free(&e.entry);
    return !e.failed;
}
