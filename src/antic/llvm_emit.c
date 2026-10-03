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
   signatures of scalars. The step emit-arith adds the scalar operations
   and the entry of the runtime. The step emit-memory adds the operations
   on memory, the calls, the globals, the sections and the constructors.
   Every other operation is refused with the step that adds it. */

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
    const struct ir_module *m;
    struct text *allocas;           /* the allocas of the entry block */
    uint32_t slots;                 /* the next %a<n> of the function */
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
   operations of the steps up to this one. */
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
    case IR_SLOT: case IR_LOAD: case IR_STORE: case IR_PTRADD:
    case IR_MEMCOPY: case IR_ADDR: case IR_BITLOAD: case IR_BITSTORE:
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

/* Fail with a message that names what the IR holds and the function,
   for a form that no step of the work order translates, since the
   lowering never writes it. */
static void unexpected(struct emitter *e, const char *what)
{
    struct text name = {0};

    if (e->failed) {
        return;
    }
    e->failed = true;
    ir_name_append(&name, e->f->module, e->f->name);
    text_format(e->error, e->error_size,
                "the LLVM back end cannot translate %s, in %s", what,
                text_cstr(&name));
    text_free(&name);
}

/* The LLVM type of a value of type, or NULL after a failure. c_long,
   c_wchar and the word of a Mutex take the integer of their width on the
   target, where layout_resolve has left one. */
static const char *value_type(struct emitter *e, enum ir_type type)
{
    const char *name = scalar_type(type);

    if (name == NULL && type != IR_AGG) {
        uint64_t bytes = layout_size(e->l, ir_scalar(type));
        name = bytes == 2 ? "i16" : bytes == 4 ? "i32" : "i64";
    }
    if (name == NULL) {
        unexpected(e, "an aggregate value");
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

static void global_symbol(struct text *out, enum target t,
                          const struct ir_global *g);

/* Append to value the LLVM operand of o as a value of type, after the
   load that reads a temporary. A global or a function is its symbol. */
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
        global_symbol(value, e->o->target, e->m->globals[o->as.index]);
        return;
    case IR_FUNC:
        function_name(value, e->o->target, e->m->functions[o->as.index]);
        return;
    case IR_NONE:
    case IR_BLOCK:
    case IR_SYM:
        break;
    }
    unexpected(e, "this operand");
}

static void store_result(struct emitter *e, const struct ir_inst *inst,
                         const char *value)
{
    text_appendf(e->out, "  store %s %s, ptr %%t%" PRIu32 ", align %" PRIu64
                         "\n",
                 value_type(e, inst->type), value, inst->result,
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

/* Copy size bytes from src to dst, two values of ptr. */
static void copy_bytes(struct emitter *e, const char *dst, const char *src,
                       uint64_t size)
{
    intrinsic(e, "void", "llvm.memcpy.p0.p0.i64", "ptr, ptr, i64, i1");
    text_appendf(e->out, "  call void @llvm.memcpy.p0.p0.i64(ptr %s, ptr %s, "
                         "i64 %" PRIu64 ", i1 false)\n",
                 dst, src, size);
}

/* DESIGN: a slot is an alloca in the entry block, wherever the IR puts
   it, so a slot in a loop is one place in the frame, as the native back
   end makes it. Its temporary holds the address. */
static void slot(struct emitter *e, const struct ir_inst *inst)
{
    char value[24];

    text_appendf(e->allocas, "  %%s%" PRIu32 " = alloca [%" PRIu64
                             " x i8], align %" PRIu64 "\n",
                 inst->result, layout_size(e->l, inst->of),
                 layout_align(e->l, inst->of));
    text_format(value, sizeof value, "%%s%" PRIu32, inst->result);
    store_result(e, inst, value);
}

/* DESIGN: a load and a store take the natural alignment of their type.
   The IR marks no access as unaligned, and the section "Instruction
   mapping" of docs/work-order-llvm-back-end.md gives 1 only to an access
   the IR marks. */
static void load_store(struct emitter *e, const struct ir_inst *inst)
{
    const char *name = value_type(e, inst->type);
    struct text value = {0};
    struct text pointer = {0};

    if (name == NULL) {
        return;
    }
    if (inst->op == IR_STORE) {
        operand(e, &inst->a, inst->type, &value);
        operand(e, &inst->b, IR_PTR, &pointer);
        if (!e->failed) {
            text_appendf(e->out, "  store %s %s, ptr %s, align %" PRIu64 "\n",
                         name, text_cstr(&value), text_cstr(&pointer),
                         align_of(e, inst->type));
        }
    } else {
        operand(e, &inst->a, IR_PTR, &pointer);
        if (!e->failed) {
            text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                                 PRIu64 "\n",
                         e->value, name, text_cstr(&pointer),
                         align_of(e, inst->type));
            text_appendf(&value, "%%v%" PRIu32, e->value++);
            store_result(e, inst, text_cstr(&value));
        }
    }
    text_free(&value);
    text_free(&pointer);
}

/* An offset of bytes, which a getelementptr of i8 adds without inbounds,
   so an address outside its object is no poison. An offset narrower than
   64 bits extends with its sign. */
static void ptradd(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_type type = operand_type(e, &inst->b);
    struct text pointer = {0};
    struct text offset = {0};
    char value[24];

    operand(e, &inst->a, IR_PTR, &pointer);
    operand(e, &inst->b, type, &offset);
    if (!e->failed && type != IR_I64) {
        uint32_t wide = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = sext %s %s to i64\n", wide,
                     value_type(e, type), text_cstr(&offset));
        text_free(&offset);
        text_appendf(&offset, "%%v%" PRIu32, wide);
    }
    if (!e->failed) {
        text_appendf(e->out, "  %%v%" PRIu32 " = getelementptr i8, ptr %s, "
                             "i64 %s\n",
                     e->value, text_cstr(&pointer), text_cstr(&offset));
        text_format(value, sizeof value, "%%v%" PRIu32, e->value++);
        store_result(e, inst, value);
    }
    text_free(&pointer);
    text_free(&offset);
}

static void memcopy(struct emitter *e, const struct ir_inst *inst)
{
    struct text dst = {0};
    struct text src = {0};

    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, IR_PTR, &src);
    if (!e->failed) {
        copy_bytes(e, text_cstr(&dst), text_cstr(&src),
                   layout_size(e->l, inst->of));
    }
    text_free(&dst);
    text_free(&src);
}

static uint64_t field_mask(uint64_t width)
{
    return width >= 64 ? UINT64_MAX : ((uint64_t)1 << width) - 1;
}

/* Convert the value v of type from to type to, with the extension ext
   where to is wider. Returns the value to use, in out. */
static void convert_int(struct emitter *e, struct text *v, enum ir_type from,
                        enum ir_type to, enum ir_ext ext)
{
    unsigned a = width_of(from);
    unsigned b = width_of(to);
    uint32_t n;

    if (a == b) {
        return;
    }
    n = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n", n,
                 a > b                 ? "trunc"
                 : ext == IR_EXT_SIGN ? "sext"
                                      : "zext",
                 value_type(e, from), text_cstr(v), value_type(e, to));
    text_free(v);
    text_appendf(v, "%%v%" PRIu32, n);
}

/* Write v = op type v, k and make v the result. */
static void unit_op(struct emitter *e, struct text *v, const char *op,
                    enum ir_type type, uint64_t k)
{
    uint32_t n = fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, ", n, op,
                 value_type(e, type), text_cstr(v));
    int_constant(e->out, type, k);
    text_append(e->out, "\n");
    text_free(v);
    text_appendf(v, "%%v%" PRIu32, n);
}

/* DESIGN: a bitfield load reads the integer that holds the field, the
   unit of layout_bits. An unsigned field shifts down and masks, and a
   signed one shifts to the top and back with an arithmetic shift, then
   either widens to its type. A store reads the unit, clears the field,
   puts the new bits in and writes the unit back. This is the sequence of
   expand.c for the native back end. A unit may start at any byte, so it
   takes the alignment its offset leaves in the aggregate. */
static void bits(struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_field *field =
        &e->m->aggs[inst->of.agg]->fields[inst->field];
    const struct layout *agg = layout_agg(e->l, inst->of.agg);
    const struct layout_bits *where = &agg->bits[inst->field];
    enum ir_type unit = where->unit_type;
    const char *unit_name = value_type(e, unit);
    unsigned unit_bits = width_of(unit);
    uint64_t width = field->bits;
    uint64_t align = agg->align;
    struct text value = {0};
    struct text pointer = {0};
    struct text word = {0};

    if (where->unit_offset != 0 &&
        (where->unit_offset & (0 - where->unit_offset)) < align) {
        align = where->unit_offset & (0 - where->unit_offset);
    }
    if (unit_bits / 8 < align) {
        align = unit_bits / 8;
    }
    if (inst->op == IR_BITSTORE) {
        operand(e, &inst->a, inst->type, &value);
    }
    operand(e, inst->op == IR_BITLOAD ? &inst->a : &inst->b, IR_PTR,
            &pointer);
    if (e->failed || unit_name == NULL) {
        text_free(&value);
        text_free(&pointer);
        return;
    }
    if (where->unit_offset != 0) {
        uint32_t n = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = getelementptr i8, ptr %s, "
                             "i64 %" PRIu64 "\n",
                     n, text_cstr(&pointer), where->unit_offset);
        text_free(&pointer);
        text_appendf(&pointer, "%%v%" PRIu32, n);
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 e->value, unit_name, text_cstr(&pointer), align);
    text_appendf(&word, "%%v%" PRIu32, e->value++);
    if (inst->op == IR_BITLOAD && field->ext == IR_EXT_SIGN) {
        unsigned up = unit_bits - where->shift - (unsigned)width;
        if (up != 0) {
            unit_op(e, &word, "shl", unit, up);
        }
        if (width < unit_bits) {
            unit_op(e, &word, "ashr", unit, unit_bits - width);
        }
        convert_int(e, &word, unit, inst->type, IR_EXT_SIGN);
        store_result(e, inst, text_cstr(&word));
    } else if (inst->op == IR_BITLOAD) {
        if (where->shift != 0) {
            unit_op(e, &word, "lshr", unit, where->shift);
        }
        if (width < unit_bits) {
            unit_op(e, &word, "and", unit, field_mask(width));
        }
        convert_int(e, &word, unit, inst->type, IR_EXT_ZERO);
        store_result(e, inst, text_cstr(&word));
    } else {
        uint32_t n;
        convert_int(e, &value, inst->type, unit, IR_EXT_ZERO);
        if (width < unit_bits) {
            unit_op(e, &value, "and", unit, field_mask(width));
        }
        if (where->shift != 0) {
            unit_op(e, &value, "shl", unit, where->shift);
        }
        unit_op(e, &word, "and", unit, ~(field_mask(width) << where->shift));
        n = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = or %s %s, %s\n", n,
                     unit_name, text_cstr(&word), text_cstr(&value));
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     unit_name, n, text_cstr(&pointer), align);
    }
    text_free(&value);
    text_free(&pointer);
    text_free(&word);
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
    case IR_SLOT:
        slot(e, inst);
        break;
    case IR_LOAD:
    case IR_STORE:
        load_store(e, inst);
        break;
    case IR_PTRADD:
        ptradd(e, inst);
        break;
    case IR_MEMCOPY:
        memcopy(e, inst);
        break;
    case IR_ADDR:
        operand(e, &inst->a, IR_PTR, &value);
        if (!e->failed) {
            store_result(e, inst, text_cstr(&value));
        }
        break;
    case IR_BITLOAD:
    case IR_BITSTORE:
        bits(e, inst);
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
    struct text *out = e->out;
    struct text body = {0};
    struct text allocas = {0};
    size_t b;
    size_t i;
    uint32_t t;

    e->f = f;
    e->value = 0;
    e->slots = 0;
    text_append(out, "define ");
    linkage(e, f);
    if (!signature(e, f, true, is_main(e, f) ? &e->entry : NULL)) {
        return;
    }
    text_append(out, " #0");
    if (link_once(e, f) &&
        target_info(e->o->target)->format == FORMAT_COFF) {
        text_append(out, " comdat");
    }
    text_append(out, " {\nb0:\n");
    for (t = 0; t < f->temp_count && !e->failed; t++) {
        const char *name = value_type(e, f->temps[t]);
        if (name != NULL) {
            text_appendf(out, "  %%t%" PRIu32 " = alloca %s, align %" PRIu64
                              "\n",
                         t, name, align_of(e, f->temps[t]));
        }
    }
    /* The allocas of the slots and the copies come after the ones of the
       temporaries, and the body after all of them, in the entry block. */
    e->out = &body;
    e->allocas = &allocas;
    for (i = 0; i < f->param_count && !e->failed; i++) {
        enum ir_type type = f->params[i].type;
        text_appendf(&body, "  store %s %%p%zu, ptr %%t%" PRIu32
                            ", align %" PRIu64 "\n",
                     value_type(e, type), i, f->params[i].temp,
                     align_of(e, type));
    }
    for (b = 0; b < f->block_count && !e->failed; b++) {
        const struct ir_block *block = f->blocks[b];
        if (b > 0) {
            text_appendf(&body, "\nb%zu:\n", b);
        }
        for (i = 0; i < block->count && !e->failed; i++) {
            instruction(e, &block->insts[i]);
        }
    }
    e->out = out;
    e->allocas = NULL;
    text_append_bytes(out, allocas.data, allocas.length);
    text_append_bytes(out, body.data, body.length);
    text_append(out, "}\n\n");
    text_free(&body);
    text_free(&allocas);
}

/* The symbol of a global after sigil, without the _ of Mach-O, which llc
   writes. */
static void global_sigil(struct text *out, char sigil, enum target t,
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
    llvm_name(out, sigil, name);
    text_free(&symbol);
}

static void global_symbol(struct text *out, enum target t,
                          const struct ir_global *g)
{
    global_sigil(out, '@', t, g);
}

/* Append c"..." of length bytes, with \XX escapes. */
static void byte_string(struct text *out, const uint8_t *bytes,
                        uint64_t length)
{
    uint64_t k;

    text_append(out, "c\"");
    for (k = 0; k < length; k++) {
        unsigned char ch = bytes[k];
        if (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\') {
            text_appendf(out, "\\%02X", ch);
        } else {
            text_appendf(out, "%c", ch);
        }
    }
    text_append(out, "\"");
}

/* Append the member of the bytes from up to to of g to the types and the
   values of its struct, zeroinitializer for a run of zeros. */
static void byte_member(struct text *types, struct text *values,
                        const struct ir_global *g, uint64_t from, uint64_t to)
{
    bool zero = true;
    uint64_t k;

    for (k = from; k < to; k++) {
        zero = zero && g->bytes[k] == 0;
    }
    text_appendf(types, "%s[%" PRIu64 " x i8]", types->length > 0 ? ", " : "",
                 to - from);
    text_appendf(values, "%s[%" PRIu64 " x i8] ",
                 values->length > 0 ? ", " : "", to - from);
    if (zero) {
        text_append(values, "zeroinitializer");
    } else {
        byte_string(values, g->bytes + from, to - from);
    }
}

static int reloc_order(const void *a, const void *b)
{
    const struct ir_reloc *x = a;
    const struct ir_reloc *y = b;

    return x->offset < y->offset ? -1 : x->offset > y->offset;
}

/* Whether g is the datum of a copy of a generic in an object of one
   module, which every module that uses it defines. */
static bool global_once(const struct emitter *e, const struct ir_global *g)
{
    return e->o->one_module && ir_is_copy_name(g->name);
}

/* DESIGN: a global is a packed struct, so its size and its offsets are
   the ones layout_data computed, whatever the data layout string says.
   The section "Globals, sections and constructors" of
   docs/work-order-llvm-back-end.md gives the form. The bytes between two
   addresses are one member of [N x i8], and each address a member of ptr
   that names the global or the function. A global the program writes is
   a global, every other one a constant, which llc puts in the read-only
   section of each format, or in the one the loader writes once where it
   holds an address. The linkage follows the one of a function. A global
   the runtime defines is a declaration of its bytes. */
static void global(struct emitter *e, const struct ir_global *g)
{
    enum target t = e->o->target;
    bool coff = target_info(t)->format == FORMAT_COFF;
    uint64_t word = layout_size(e->l, ir_scalar(IR_PTR));
    struct text types = {0};
    struct text values = {0};
    struct ir_reloc *relocs;
    uint64_t from = 0;
    bool zero = true;
    size_t i;

    global_symbol(e->out, t, g);
    if (g->is_extern) {
        text_appendf(e->out, " = external global [%" PRIu64 " x i8], align %"
                             PRIu64 "\n",
                     g->size, g->align > 0 ? g->align : 1);
        return;
    }
    text_append(e->out, " = ");
    if (!g->exported && !e->o->one_module && !e->o->exports) {
        text_append(e->out, "internal ");
    } else {
        if (global_once(e, g)) {
            text_append(e->out, "weak_odr ");
        }
        if (g->exported) {
            text_append(e->out, "dso_local ");
        } else if (e->o->one_module && !e->o->exports && !coff) {
            text_append(e->out, "hidden ");
        }
    }
    relocs = alloc_zeroed(g->reloc_count + 1, sizeof *relocs);
    if (g->reloc_count > 0) {
        memcpy(relocs, g->relocs, g->reloc_count * sizeof *relocs);
        qsort(relocs, g->reloc_count, sizeof *relocs, reloc_order);
    }
    for (i = 0; i < g->reloc_count; i++) {
        if (relocs[i].offset > from) {
            byte_member(&types, &values, g, from, relocs[i].offset);
        }
        text_appendf(&types, "%sptr", types.length > 0 ? ", " : "");
        text_appendf(&values, "%sptr ", values.length > 0 ? ", " : "");
        if (relocs[i].fn) {
            function_name(&values, t, e->m->functions[relocs[i].global]);
        } else {
            global_symbol(&values, t, e->m->globals[relocs[i].global]);
        }
        from = relocs[i].offset + word;
    }
    if (from < g->size || g->reloc_count == 0) {
        byte_member(&types, &values, g, from, g->size);
    }
    for (i = 0; i < g->size; i++) {
        zero = zero && g->bytes[i] == 0;
    }
    text_appendf(e->out, "%s <{ %s }> ", g->mutable ? "global" : "constant",
                 text_cstr(&types));
    if (zero && g->reloc_count == 0) {
        text_append(e->out, "zeroinitializer");
    } else {
        text_appendf(e->out, "<{ %s }>", text_cstr(&values));
    }
    if (global_once(e, g) && coff) {
        text_append(e->out, ", comdat");
    }
    text_appendf(e->out, ", align %" PRIu64 "\n", g->align > 0 ? g->align : 1);
    free(relocs);
    text_free(&types);
    text_free(&values);
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

/* DESIGN: the functions that run before main are the one of each module
   that compiles its patterns and, in a shared library, the constructor of
   the runtime. llvm.global_ctors names them, at the priority of a C
   constructor, and llc writes the section of each format: .init_array,
   __mod_init_func or .CRT$XCU, the sections of emit.c. */
static void constructors(struct emitter *e, const struct ir_module *m)
{
    struct text entries = {0};
    size_t count = 0;
    bool declared = false;
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (ir_is_patterns_start(f)) {
            text_appendf(&entries, "%s{ i32, ptr, ptr } { i32 65535, ptr ",
                         count++ > 0 ? ", " : "");
            function_name(&entries, e->o->target, f);
            text_append(&entries, ", ptr null }");
        }
        declared = declared ||
                   (e->o->constructor != NULL && f->is_extern &&
                    f->module == NULL &&
                    strcmp(f->name, e->o->constructor) == 0);
    }
    if (e->o->constructor != NULL) {
        struct text symbol = {0};
        const char *name;
        target_c_symbol(&symbol, e->o->target, e->o->constructor);
        name = text_cstr(&symbol);
        if (target_info(e->o->target)->format == FORMAT_MACHO &&
            name[0] == '_') {
            name++;
        }
        text_appendf(&entries, "%s{ i32, ptr, ptr } { i32 65535, ptr ",
                     count++ > 0 ? ", " : "");
        llvm_name(&entries, '@', name);
        text_append(&entries, ", ptr null }");
        if (!declared) {
            text_append(&e->intrinsics, "declare void ");
            llvm_name(&e->intrinsics, '@', name);
            text_append(&e->intrinsics, "() #1\n");
        }
        text_free(&symbol);
    }
    if (count > 0) {
        text_appendf(e->out, "@llvm.global_ctors = appending global [%zu x "
                             "{ i32, ptr, ptr }] [%s]\n",
                     count, text_cstr(&entries));
    }
    text_free(&entries);
}

/* The name of the source, the data layout and the triple of t. */
static void head(struct text *out, enum target t, const char *source)
{
    text_appendf(out, "source_filename = \"%s\"\n", source);
    text_appendf(out, "target datalayout = \"%s\"\n", llvm_data_layout(t));
    text_append(out, "target triple = \"");
    llvm_triple(out, t);
    text_append(out, "\"\n\n");
}

/* DESIGN: the package header and the notice keep the sections emit.c
   gives them. No symbol names the copy of the package header, so
   llvm.used keeps the private constant through every pass. */
void llvm_emit_package(struct text *out, enum target t, const char *bytes,
                       size_t length)
{
    static const char *const sections[] = {
        [FORMAT_ELF] = ".anti_package",
        [FORMAT_MACHO] = "__DATA,__anti_package",
        [FORMAT_COFF] = ".anti_package",
    };

    head(out, t, "package");
    text_appendf(out, "@anti.package = private constant [%zu x i8] ",
                 length);
    byte_string(out, (const uint8_t *)bytes, length);
    text_appendf(out, ", section \"%s\"\n"
                      "@llvm.used = appending global [1 x ptr] "
                      "[ptr @anti.package], section \"llvm.metadata\"\n",
                 sections[target_info(t)->format]);
}

void llvm_emit_licenses(struct text *out, enum target t, const char *bytes,
                        size_t length)
{
    static const char *const sections[] = {
        [FORMAT_ELF] = ".rodata",
        [FORMAT_MACHO] = "__TEXT,__const",
        [FORMAT_COFF] = ".rdata",
    };
    struct text symbol = {0};
    struct text text = {0};
    const char *name;

    target_c_symbol(&symbol, t, "anti_licenses");
    name = text_cstr(&symbol);
    if (target_info(t)->format == FORMAT_MACHO && name[0] == '_') {
        name++;
    }
    text_append_bytes(&text, bytes, length);
    llvm_name(out, '@', name);
    text_appendf(out, " = dso_local constant [%zu x i8] ", length + 1);
    /* The texts end in a NUL, which text_cstr gives after the bytes. */
    byte_string(out, (const uint8_t *)text_cstr(&text), length + 1);
    text_appendf(out, ", section \"%s\", align 1\n",
                 sections[target_info(t)->format]);
    text_free(&symbol);
    text_free(&text);
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
    e.m = m;
    /* DESIGN: a plugin of COFF reaches the names of its host through the
       __imp_ entries of the import library, which the step flags adds. */
    if (m->plugin && target_info(o->target)->format == FORMAT_COFF) {
        text_format(error, error_size,
                    "the LLVM back end does not translate a plugin for %s "
                    "before the step flags",
                    target_name(o->target));
        return false;
    }
    head(out, o->target, o->module);
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (!g->is_extern && global_once(&e, g) &&
            target_info(o->target)->format == FORMAT_COFF) {
            global_sigil(out, '$', o->target, g);
            text_append(out, " = comdat any\n");
            comdats = true;
        }
    }
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
    constructors(&e, m);
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
