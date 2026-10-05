#include "llvm_emit.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abi.h"
#include "alloc.h"
#include "llvm_debug.h"
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
   The step emit-wide adds overflow, flags, saturation, the upper half of
   a product and the simd operations, so every operation of the IR has
   its form. */

/* The classification of the parameters and the result of a function. */
struct classified {
    struct abi_param *params;       /* one per parameter */
    struct abi_param result;
};

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
    struct classified signature;    /* of the function */
    /* The four i1 values of the last flag operation, in the order of
       enum ir_flag, each a value or a constant, and its result
       temporary. */
    char flags[4][24];
    uint32_t flag_result;
    /* The i1 of the last addov, subov or mulov, and its result
       temporary, which the branchov right after it reads. */
    uint32_t overflow;
    uint32_t overflow_result;
    struct llvm_debug debug;        /* the metadata of -g */
    /* The places of a COFF plugin that hold an __imp_ entry, as the
       entries of anti_rt_imports, and the __imp_ entries they name, one
       per line. */
    struct text places;
    size_t place_count;
    struct text imports;
    bool *keeps_frame;              /* per function, see frames_kept */
};

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

/* DESIGN: a function takes the symbol of target_c_symbol or
   target_mangle, which the runtime, the C headers and the symbolizer
   read. llc applies the mangling of the triple, so the text drops the
   leading _ of Mach-O, which llc writes again. */
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

/* The constant of an integer operand of a float type. Its value is the
   bits of the float as they stand. The optimizer leaves such a zero in a
   copy. */
static void float_bits(struct text *out, enum ir_type type, uint64_t value)
{
    if (type == IR_F32) {
        uint32_t narrow = (uint32_t)value;
        float f;
        memcpy(&f, &narrow, sizeof f);
        float_constant(out, type, (double)f);
    } else {
        double d;
        memcpy(&d, &value, sizeof d);
        float_constant(out, type, d);
    }
}

static void global_symbol(struct text *out, enum target t,
                          const struct ir_global *g);
static void branch_weights(struct emitter *e, uint32_t then_block,
                           uint32_t else_block);

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
        if (type == IR_F32 || type == IR_F64) {
            float_bits(value, type, o->as.integer);
        } else {
            int_constant(value, type, o->as.integer);
        }
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

static bool imports(const struct emitter *e);

/* Append the memory effects of a C function and its guarantees to out,
   each attribute after a space. A function of the class IR_EFFECTS_ANY
   gets none, since LLVM reads no guarantee without a class. */
static void effects(struct text *out, enum ir_effects kind,
                    uint8_t guarantees)
{
    switch (kind) {
    case IR_EFFECTS_ANY:
        return;
    case IR_EFFECTS_NONE:
        text_append(out, " memory(none)");
        break;
    case IR_EFFECTS_READS_ARGS:
        text_append(out, " memory(argmem: read)");
        break;
    case IR_EFFECTS_WRITES_ARGS:
        text_append(out, " memory(argmem: readwrite, inaccessiblemem: "
                         "readwrite, errnomem: write)");
        break;
    }
    text_append(out, (guarantees & IR_WILLRETURN) != 0 ? " willreturn" : "");
    text_append(out, (guarantees & IR_NOSYNC) != 0 ? " nosync" : "");
    text_append(out, (guarantees & IR_NOFREE) != 0 ? " nofree" : "");
}

/* Declare the function f of the runtime once, with the result and the
   parameters of the text and the memory effects of its row. A COFF
   plugin takes it from its host. */
static void runtime_function(struct emitter *e, const char *result,
                             enum rt_function f, const char *parameters)
{
    const struct rt_signature *s = rt_signature(f);
    struct text line = {0};

    text_appendf(&line, "declare %s%s @%s(%s)",
                 imports(e) ? "dllimport " : "", result, s->name,
                 parameters);
    effects(&line, s->effects, s->guarantees);
    text_append(&line, "\n");
    if (strstr(text_cstr(&e->intrinsics), text_cstr(&line)) == NULL) {
        text_append(&e->intrinsics, text_cstr(&line));
    }
    text_free(&line);
}

/* An operation of two operands of one type: the plain instructions, the
   shifts with the count modulo the width, the guarded divisions and the
   comparisons, which widen their i1 to the i8 of a bool.
   DESIGN: a comparison widens its i1 to the type of the instruction. The
   trampolines of whole_tables.c write ne with the type i64, a word of 0
   or 1, and the branch after it reads the low byte of that word. */
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
                                 " to %s\n",
                         fresh(e), bit, value_type(e, inst->type));
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

/* DESIGN: an f16 is its bits in an i16, so hext and htrunc pass through
   half where the level has an instruction for them: ARM64 and x86-64-v3.
   x86-64-v1 and v2 call anti_rt_f16_to_f32 and anti_rt_f32_to_f16 of the
   runtime, as the entry on f16 in docs/decisions.md says. They are the
   functions antic folds constants with, so a folded conversion and one at
   run time agree. */
static bool f16_calls(const struct emitter *e)
{
    return cpu_arch(e->o->cpu) == ARCH_X86_64 && !cpu_has(e->o->cpu, CPU_F16C);
}

/* DESIGN: a float out of the range of the integer type saturates, and NaN
   gives 0, on every target. llvm.fptosi.sat and llvm.fptoui.sat define
   exactly that, where fptosi and fptoui give poison. */
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
        if (f16_calls(e)) {
            runtime_function(e, "float", RT_FN_F16_TO_F32, "i32");
            text_appendf(e->out, "  %%v%" PRIu32 " = zext %s %s to i32\n",
                         half, source, text_cstr(&a));
            text_appendf(e->out, "  %%v%" PRIu32 " = call float @%s(i32 "
                                 "%%v%" PRIu32 ")\n",
                         fresh(e), rt_name(RT_FN_F16_TO_F32), half);
            break;
        }
        text_appendf(e->out, "  %%v%" PRIu32 " = bitcast %s %s to half\n",
                     half, source, text_cstr(&a));
        text_appendf(e->out, "  %%v%" PRIu32 " = fpext half %%v%" PRIu32
                             " to %s\n",
                     fresh(e), half, target);
        break;
    }
    case IR_HTRUNC: {
        uint32_t half = fresh(e);
        if (f16_calls(e)) {
            runtime_function(e, "i32", RT_FN_F32_TO_F16, "float");
            text_appendf(e->out, "  %%v%" PRIu32 " = call i32 @%s(%s %s)\n",
                         half, rt_name(RT_FN_F32_TO_F16), source,
                         text_cstr(&a));
            text_appendf(e->out, "  %%v%" PRIu32 " = trunc i32 %%v%" PRIu32
                                 " to %s\n",
                         fresh(e), half, target);
            break;
        }
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
   it, so a slot in a loop is one place in the frame. Its temporary holds
   the address. */
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
   puts the new bits in and writes the unit back. A unit may start at any
   byte, so it takes the alignment its offset leaves in the aggregate. */
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

/* The vector type <N x T> of the simd struct of inst, one element per
   lane of the IR's lane type. */
static void vector_of(struct emitter *e, const struct ir_inst *inst,
                      const char *element, char *out, size_t size)
{
    text_format(out, size, "<%zu x %s>",
                e->m->aggs[inst->of.agg]->field_count, element);
}

/* DESIGN: a simd operation loads whole vectors at the alignment of the
   simd struct, works on <N x T> and stores the result, as "Instruction
   mapping" in docs/work-order-llvm-back-end.md gives it. vsplat and
   vbinary come with the step emit-memory, since the simd structs that
   tests/abi passes to C need them. A comparison stores its mask as i8
   lanes at alignment 1, since the instruction names the aggregate of
   its operands and not the one of the mask. A shift takes each count
   modulo the width of a lane, as a scalar shift does. The verifier of
   the IR gives vbinary no integer division. */
static void vsplat(struct emitter *e, const struct ir_inst *inst)
{
    const char *lane = value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    struct text dst = {0};
    struct text value = {0};
    char vector[48];
    uint32_t one;
    uint32_t all;

    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, inst->type, &value);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        one = fresh(e);
        all = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = insertelement %s poison, %s "
                             "%s, i64 0\n",
                     one, vector, lane, text_cstr(&value));
        text_appendf(e->out, "  %%v%" PRIu32 " = shufflevector %s %%v%" PRIu32
                             ", %s poison, <%zu x i32> zeroinitializer\n",
                     all, vector, one, vector,
                     e->m->aggs[inst->of.agg]->field_count);
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     vector, all, text_cstr(&dst), align);
    }
    text_free(&dst);
    text_free(&value);
}

static void vbinary(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_op op = (enum ir_op)inst->field;
    const char *lane = value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    struct text dst = {0};
    struct text x = {0};
    struct text y = {0};
    char vector[48];
    uint32_t a;
    uint32_t b;
    uint32_t r;
    size_t k;

    if (plain_binary(op) == NULL && comparison(op) == NULL && op != IR_SHL &&
        op != IR_SHR_S && op != IR_SHR_U) {
        unexpected(e, "this lane operation of vbinary");
        return;
    }
    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, IR_PTR, &x);
    operand(e, &inst->c, IR_PTR, &y);
    if (e->failed || lane == NULL) {
        text_free(&dst);
        text_free(&x);
        text_free(&y);
        return;
    }
    vector_of(e, inst, lane, vector, sizeof vector);
    a = fresh(e);
    b = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 a, vector, text_cstr(&x), align);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 b, vector, text_cstr(&y), align);
    if (comparison(op) != NULL) {
        uint32_t bits = fresh(e);
        r = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     bits, comparison(op), vector, a, b);
        text_appendf(e->out, "  %%v%" PRIu32 " = zext <%zu x i1> %%v%" PRIu32
                             " to <%zu x i8>\n",
                     r, lanes, bits, lanes);
        text_appendf(e->out, "  store <%zu x i8> %%v%" PRIu32 ", ptr %s, "
                             "align 1\n",
                     lanes, r, text_cstr(&dst));
    } else {
        if (plain_binary(op) == NULL) {
            uint32_t count = fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = and %s %%v%" PRIu32 ", <",
                         count, vector, b);
            for (k = 0; k < lanes; k++) {
                text_appendf(e->out, "%s%s %u", k > 0 ? ", " : "", lane,
                             width_of(inst->type) - 1);
            }
            text_append(e->out, ">\n");
            b = count;
        }
        r = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     r,
                     plain_binary(op) != NULL ? plain_binary(op)
                     : op == IR_SHL           ? "shl"
                     : op == IR_SHR_S         ? "ashr"
                                              : "lshr",
                     vector, a, b);
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     vector, r, text_cstr(&dst), align);
    }
    text_free(&dst);
    text_free(&x);
    text_free(&y);
}

/* The name of type in the name of an intrinsic: i8 to i64, f32 or f64. */
static const char *type_suffix(struct emitter *e, enum ir_type type)
{
    if (type == IR_F32 || type == IR_F64) {
        return float_suffix(type);
    }
    return value_type(e, type);
}

/* Call the intrinsic llvm.<kind>.with.overflow.<T> on x and y, declared
   once, and return the value of the call. */
static uint32_t with_overflow(struct emitter *e, const char *kind,
                              const char *type, const char *x, const char *y)
{
    char name[64];
    char result[32];
    char parameters[32];
    uint32_t n = fresh(e);

    text_format(name, sizeof name, "llvm.%s.with.overflow.%s", kind, type);
    text_format(result, sizeof result, "{ %s, i1 }", type);
    text_format(parameters, sizeof parameters, "%s, %s", type, type);
    intrinsic(e, result, name, parameters);
    text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s, %s %s)\n", n,
                 result, name, type, x, type, y);
    return n;
}

/* The field k of the value v of type { type, i1 }. */
static uint32_t field_of(struct emitter *e, const char *type, uint32_t v,
                         unsigned k)
{
    uint32_t n = fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = extractvalue { %s, i1 } %%v%"
                         PRIu32 ", %u\n",
                 n, type, v, k);
    return n;
}

/* Keep the value %v<v> as the flag of the last flag operation. */
static void keep_flag(struct emitter *e, enum ir_flag flag, uint32_t v)
{
    text_format(e->flags[flag], sizeof e->flags[flag], "%%v%" PRIu32, v);
}

/* Keep zero and sign of the result r of type, and r as the result of the
   flag operation inst. */
static void finish_flags(struct emitter *e, const struct ir_inst *inst,
                         const char *type, uint32_t r)
{
    char value[24];
    uint32_t zero = fresh(e);
    uint32_t sign = fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %%v%" PRIu32 ", 0\n",
                 zero, type, r);
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp slt %s %%v%" PRIu32 ", 0\n",
                 sign, type, r);
    keep_flag(e, IR_FLAG_ZERO, zero);
    keep_flag(e, IR_FLAG_NEGATIVE, sign);
    e->flag_result = inst->result;
    text_format(value, sizeof value, "%%v%" PRIu32, r);
    store_result(e, inst, value);
}

/* DESIGN: addfl, subfl and mulfl give the plain result, and the four
   flags as i1 values that the flag reads after it widen. The overflow
   comes from the signed intrinsic, the carry, the borrow or the product
   too wide from the unsigned one, and zero and sign from comparisons of
   the result. A carry in runs both intrinsics again on the result and
   the carry. The two carries cannot both be set. The sum overflows when
   exactly one of the two signed steps overflowed, so the two overflows
   combine with xor, which gives the flag of adc and sbb. */
static void arithmetic_flags(struct emitter *e, const struct ir_inst *inst,
                             const char *type, const char *a, const char *b)
{
    const char *kind = inst->op == IR_ADD_FL   ? "add"
                       : inst->op == IR_SUB_FL ? "sub"
                                               : "mul";
    char signed_kind[8];
    char unsigned_kind[8];
    struct text c = {0};
    uint32_t s;
    uint32_t u;
    uint32_t r;
    uint32_t overflow;
    uint32_t carry;

    if (inst->c.kind != IR_NONE) {
        operand(e, &inst->c, IR_I8, &c);
    }
    if (e->failed) {
        text_free(&c);
        return;
    }
    text_format(signed_kind, sizeof signed_kind, "s%s", kind);
    text_format(unsigned_kind, sizeof unsigned_kind, "u%s", kind);
    s = with_overflow(e, signed_kind, type, a, b);
    u = with_overflow(e, unsigned_kind, type, a, b);
    r = field_of(e, type, s, 0);
    overflow = field_of(e, type, s, 1);
    carry = field_of(e, type, u, 1);
    if (inst->c.kind != IR_NONE) {
        uint32_t set = fresh(e);
        uint32_t wide = fresh(e);
        char partial[24];
        char in[24];
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne i8 %s, 0\n", set,
                     text_cstr(&c));
        text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %%v%" PRIu32
                             " to %s\n",
                     wide, set, type);
        text_format(partial, sizeof partial, "%%v%" PRIu32, r);
        text_format(in, sizeof in, "%%v%" PRIu32, wide);
        s = with_overflow(e, signed_kind, type, partial, in);
        u = with_overflow(e, unsigned_kind, type, partial, in);
        r = field_of(e, type, s, 0);
        s = field_of(e, type, s, 1);
        u = field_of(e, type, u, 1);
        text_appendf(e->out, "  %%v%" PRIu32 " = xor i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     e->value, overflow, s);
        overflow = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = or i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     e->value, carry, u);
        carry = fresh(e);
    }
    keep_flag(e, IR_FLAG_OVERFLOW, overflow);
    keep_flag(e, IR_FLAG_CARRY, carry);
    finish_flags(e, inst, type, r);
    text_free(&c);
}

/* DESIGN: a shift with flags takes its count modulo the width, as the
   plain shift does. The carry is the last bit moved out: none for a
   count of 0, else the top bit of the operand shifted left by the count
   less one, or the bottom bit of it shifted right. The count less one
   is taken modulo the width too, so no shift of the text is poison and
   the and with the test of the count decides. A left shift overflows
   when the arithmetic shift back does not give the operand, and a right
   shift never overflows. */
static void shift_flags(struct emitter *e, const struct ir_inst *inst,
                        const char *type, const char *a, const char *b)
{
    bool left = inst->op == IR_SHL_FL;
    unsigned mask = width_of(inst->type) - 1;
    uint32_t count = fresh(e);
    uint32_t r = fresh(e);
    uint32_t moved;
    uint32_t less;
    uint32_t wrapped;
    uint32_t shifted;
    uint32_t bit;
    uint32_t carry;

    text_appendf(e->out, "  %%v%" PRIu32 " = and %s %s, %u\n", count, type, b,
                 mask);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %%v%" PRIu32 "\n", r,
                 left                        ? "shl"
                 : inst->op == IR_SHR_S_FL   ? "ashr"
                                             : "lshr",
                 type, a, count);
    if (left) {
        uint32_t back = fresh(e);
        uint32_t overflow = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = ashr %s %%v%" PRIu32
                             ", %%v%" PRIu32 "\n",
                     back, type, r, count);
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne %s %%v%" PRIu32
                             ", %s\n",
                     overflow, type, back, a);
        keep_flag(e, IR_FLAG_OVERFLOW, overflow);
    } else {
        text_format(e->flags[IR_FLAG_OVERFLOW],
                    sizeof e->flags[IR_FLAG_OVERFLOW], "false");
    }
    moved = fresh(e);
    less = fresh(e);
    wrapped = fresh(e);
    shifted = fresh(e);
    bit = fresh(e);
    carry = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne %s %%v%" PRIu32 ", 0\n",
                 moved, type, count);
    text_appendf(e->out, "  %%v%" PRIu32 " = sub %s %%v%" PRIu32 ", 1\n",
                 less, type, count);
    text_appendf(e->out, "  %%v%" PRIu32 " = and %s %%v%" PRIu32 ", %u\n",
                 wrapped, type, less, mask);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %%v%" PRIu32 "\n",
                 shifted, left ? "shl" : "lshr", type, a, wrapped);
    if (left) {
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp slt %s %%v%" PRIu32
                             ", 0\n",
                     bit, type, shifted);
    } else {
        text_appendf(e->out, "  %%v%" PRIu32 " = trunc %s %%v%" PRIu32
                             " to i1\n",
                     bit, type, shifted);
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = and i1 %%v%" PRIu32 ", %%v%"
                         PRIu32 "\n",
                 carry, moved, bit);
    keep_flag(e, IR_FLAG_CARRY, carry);
    finish_flags(e, inst, type, r);
}

/* negfl is sub 0, x. It overflows at the least value and carries, as a
   borrow from 0, for every value but 0. */
static void negation_flags(struct emitter *e, const struct ir_inst *inst,
                           const char *type, const char *a)
{
    uint32_t r = fresh(e);
    uint32_t overflow = fresh(e);
    uint32_t carry = fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n", r, type, a);
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, ", overflow, type,
                 a);
    int_constant(e->out, inst->type,
                 (uint64_t)1 << (width_of(inst->type) - 1));
    text_append(e->out, "\n");
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne %s %s, 0\n", carry, type,
                 a);
    keep_flag(e, IR_FLAG_OVERFLOW, overflow);
    keep_flag(e, IR_FLAG_CARRY, carry);
    finish_flags(e, inst, type, r);
}

/* A flag operation of the rows of "Instruction mapping", which leaves
   its four flags for the reads after it. */
static void flag_operation(struct emitter *e, const struct ir_inst *inst)
{
    const char *type = value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};

    if (type == NULL) {
        return;
    }
    operand(e, &inst->a, inst->type, &a);
    if (inst->op != IR_NEG_FL) {
        operand(e, &inst->b, inst->type, &b);
    }
    if (!e->failed) {
        switch (inst->op) {
        case IR_SHL_FL:
        case IR_SHR_S_FL:
        case IR_SHR_U_FL:
            shift_flags(e, inst, type, text_cstr(&a), text_cstr(&b));
            break;
        case IR_NEG_FL:
            negation_flags(e, inst, type, text_cstr(&a));
            break;
        default:
            arithmetic_flags(e, inst, type, text_cstr(&a), text_cstr(&b));
            break;
        }
    }
    text_free(&a);
    text_free(&b);
}

/* A flag of the flag operation right before it, which the verifier of
   the IR keeps there, from the four i1 values that operation left. */
static void flag_read(struct emitter *e, const struct ir_inst *inst)
{
    char value[24];

    if (inst->a.kind != IR_TEMP || inst->a.as.temp != e->flag_result ||
        inst->field > IR_FLAG_NEGATIVE) {
        unexpected(e, "a flag apart from its flag operation");
        return;
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %s to i8\n", e->value,
                 e->flags[inst->field]);
    text_format(value, sizeof value, "%%v%" PRIu32, e->value++);
    store_result(e, inst, value);
}

/* addov, subov and mulov: the signed with.overflow intrinsic. The result
   goes to its temporary, and the i1 stays for the branchov right after
   it, in the same block. */
static void overflow_operation(struct emitter *e, const struct ir_inst *inst)
{
    const char *type = value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};
    char value[24];
    uint32_t pair;
    uint32_t r;

    if (type == NULL) {
        return;
    }
    operand(e, &inst->a, inst->type, &a);
    operand(e, &inst->b, inst->type, &b);
    if (!e->failed) {
        pair = with_overflow(e,
                             inst->op == IR_ADD_OV   ? "sadd"
                             : inst->op == IR_SUB_OV ? "ssub"
                                                     : "smul",
                             type, text_cstr(&a), text_cstr(&b));
        r = field_of(e, type, pair, 0);
        e->overflow = field_of(e, type, pair, 1);
        e->overflow_result = inst->result;
        text_format(value, sizeof value, "%%v%" PRIu32, r);
        store_result(e, inst, value);
    }
    text_free(&a);
    text_free(&b);
}

/* br on the i1 of the overflow operation right before it. */
static void branch_overflow(struct emitter *e, const struct ir_inst *inst)
{
    if (inst->a.kind != IR_TEMP || inst->a.as.temp != e->overflow_result) {
        unexpected(e, "a branchov apart from its overflow operation");
        return;
    }
    text_appendf(e->out, "  br i1 %%v%" PRIu32 ", label %%b%" PRIu32
                         ", label %%b%" PRIu32,
                 e->overflow, inst->b.as.index, inst->c.as.index);
    branch_weights(e, inst->b.as.index, inst->c.as.index);
    text_append(e->out, "\n");
}

/* Widen the operands of inst to twice the width, sext when is_signed,
   and multiply them there. Returns the product and writes its type to
   wide. */
static uint32_t wide_product(struct emitter *e, const struct ir_inst *inst,
                             bool is_signed, char *wide, size_t size)
{
    const char *type = value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};
    uint32_t x;
    uint32_t y;
    uint32_t p = 0;

    text_format(wide, size, "i%u", 2 * width_of(inst->type));
    operand(e, &inst->a, inst->type, &a);
    operand(e, &inst->b, inst->type, &b);
    if (!e->failed && type != NULL) {
        x = fresh(e);
        y = fresh(e);
        p = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n", x,
                     is_signed ? "sext" : "zext", type, text_cstr(&a), wide);
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n", y,
                     is_signed ? "sext" : "zext", type, text_cstr(&b), wide);
        text_appendf(e->out, "  %%v%" PRIu32 " = mul %s %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     p, wide, x, y);
    }
    text_free(&a);
    text_free(&b);
    return p;
}

/* Truncate the value v of type wide to the type of inst and store it as
   the result. */
static void narrow_result(struct emitter *e, const struct ir_inst *inst,
                          const char *wide, uint32_t v)
{
    char value[24];
    uint32_t r = fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = trunc %s %%v%" PRIu32 " to %s\n",
                 r, wide, v, value_type(e, inst->type));
    text_format(value, sizeof value, "%%v%" PRIu32, r);
    store_result(e, inst, value);
}

/* DESIGN: smulh and umulh multiply in twice the width and shift the
   upper half down. At 64 bits that is i128 arithmetic, which llc selects
   as the multiply that gives the upper half. */
static void high_product(struct emitter *e, const struct ir_inst *inst)
{
    char wide[8];
    uint32_t p = wide_product(e, inst, inst->op == IR_MULH_S, wide,
                              sizeof wide);
    uint32_t h;

    if (e->failed) {
        return;
    }
    h = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = lshr %s %%v%" PRIu32 ", %u\n", h,
                 wide, p, width_of(inst->type));
    narrow_result(e, inst, wide, h);
}

/* Call the intrinsic llvm.<kind>.<wide> on the value v and the constant
   bound, declared once, and return the value of the call. */
static uint32_t clamp(struct emitter *e, const char *kind, const char *wide,
                      uint32_t v, const char *bound)
{
    char name[32];
    char parameters[24];
    uint32_t n = fresh(e);

    text_format(name, sizeof name, "llvm.%s.%s", kind, wide);
    text_format(parameters, sizeof parameters, "%s, %s", wide, wide);
    intrinsic(e, wide, name, parameters);
    text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %%v%" PRIu32
                         ", %s %s)\n",
                 n, wide, name, wide, v, wide, bound);
    return n;
}

/* DESIGN: a saturating sum or difference is the intrinsic of its row. A
   saturating product multiplies in twice the width, where it cannot
   wrap, clamps to the bounds of the type with smax and smin, or umin,
   and truncates. */
static void saturating(struct emitter *e, const struct ir_inst *inst)
{
    unsigned width = width_of(inst->type);
    char wide[8];
    char bound[24];
    uint32_t p;

    if (inst->op == IR_MUL_SAT_S || inst->op == IR_MUL_SAT_U) {
        p = wide_product(e, inst, inst->op == IR_MUL_SAT_S, wide,
                         sizeof wide);
        if (e->failed) {
            return;
        }
        if (inst->op == IR_MUL_SAT_S) {
            text_format(bound, sizeof bound, "%" PRId64,
                        width == 64 ? INT64_MIN
                                    : -((int64_t)1 << (width - 1)));
            p = clamp(e, "smax", wide, p, bound);
            text_format(bound, sizeof bound, "%" PRIu64,
                        ((uint64_t)1 << (width - 1)) - 1);
            p = clamp(e, "smin", wide, p, bound);
        } else {
            text_format(bound, sizeof bound, "%" PRIu64,
                        width == 64 ? UINT64_MAX
                                    : ((uint64_t)1 << width) - 1);
            p = clamp(e, "umin", wide, p, bound);
        }
        narrow_result(e, inst, wide, p);
        return;
    }
    {
        const char *type = value_type(e, inst->type);
        const char *kind = inst->op == IR_ADD_SAT_S   ? "sadd"
                           : inst->op == IR_ADD_SAT_U ? "uadd"
                           : inst->op == IR_SUB_SAT_S ? "ssub"
                                                      : "usub";
        struct text a = {0};
        struct text b = {0};
        char name[32];
        char parameters[24];
        char value[24];

        operand(e, &inst->a, inst->type, &a);
        operand(e, &inst->b, inst->type, &b);
        if (!e->failed && type != NULL) {
            text_format(name, sizeof name, "llvm.%s.sat.%s", kind, type);
            text_format(parameters, sizeof parameters, "%s, %s", type, type);
            intrinsic(e, type, name, parameters);
            text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s, %s "
                                 "%s)\n",
                         e->value, type, name, type, text_cstr(&a), type,
                         text_cstr(&b));
            text_format(value, sizeof value, "%%v%" PRIu32, e->value++);
            store_result(e, inst, value);
        }
        text_free(&a);
        text_free(&b);
    }
}

/* vunary: fneg of float lanes, sub from zeroinitializer for neg and xor
   with all ones for not. */
static void vunary(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_op op = (enum ir_op)inst->field;
    const char *lane = value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    struct text dst = {0};
    struct text x = {0};
    char vector[48];
    uint32_t v;
    uint32_t r;
    size_t k;

    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, IR_PTR, &x);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        v = fresh(e);
        r = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     v, vector, text_cstr(&x), align);
        if (op == IR_FNEG) {
            text_appendf(e->out, "  %%v%" PRIu32 " = fneg %s %%v%" PRIu32 "\n",
                         r, vector, v);
        } else if (op == IR_NEG) {
            text_appendf(e->out, "  %%v%" PRIu32 " = sub %s zeroinitializer, "
                                 "%%v%" PRIu32 "\n",
                         r, vector, v);
        } else {
            text_appendf(e->out, "  %%v%" PRIu32 " = xor %s %%v%" PRIu32 ", <",
                         r, vector, v);
            for (k = 0; k < lanes; k++) {
                text_appendf(e->out, "%s%s -1", k > 0 ? ", " : "", lane);
            }
            text_append(e->out, ">\n");
        }
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     vector, r, text_cstr(&dst), align);
    }
    text_free(&dst);
    text_free(&x);
}

/* vshuffle: shufflevector with the constant lane indices of the IR. */
static void vshuffle(struct emitter *e, const struct ir_inst *inst)
{
    const char *lane = value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    struct text dst = {0};
    struct text x = {0};
    char vector[48];
    uint32_t v;
    uint32_t r;
    size_t k;

    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, IR_PTR, &x);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        v = fresh(e);
        r = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     v, vector, text_cstr(&x), align);
        text_appendf(e->out, "  %%v%" PRIu32 " = shufflevector %s %%v%" PRIu32
                             ", %s poison, <%zu x i32> <",
                     r, vector, v, vector, inst->arg_count);
        for (k = 0; k < inst->arg_count; k++) {
            text_appendf(e->out, "%si32 %" PRIu64, k > 0 ? ", " : "",
                         inst->args[k].as.integer);
        }
        text_append(e->out, ">\n");
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     vector, r, text_cstr(&dst), align);
    }
    text_free(&dst);
    text_free(&x);
}

/* The lanes of c where the mask b holds, and of args[0] where it does
   not, into a. The mask is a simd struct of i8 lanes, whose lowest bit
   the select reads. */
static void vselect(struct emitter *e, const struct ir_inst *inst)
{
    const char *lane = value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    struct text dst = {0};
    struct text mask = {0};
    struct text x = {0};
    struct text y = {0};
    char vector[48];

    operand(e, &inst->a, IR_PTR, &dst);
    operand(e, &inst->b, IR_PTR, &mask);
    operand(e, &inst->c, IR_PTR, &x);
    if (inst->arg_count == 1) {
        operand(e, &inst->args[0], IR_PTR, &y);
    } else {
        unexpected(e, "a vselect without its second operand");
    }
    if (!e->failed && lane != NULL) {
        uint32_t bytes = fresh(e);
        uint32_t bits = fresh(e);
        uint32_t a = fresh(e);
        uint32_t b = fresh(e);
        uint32_t r = fresh(e);
        vector_of(e, inst, lane, vector, sizeof vector);
        text_appendf(e->out, "  %%v%" PRIu32 " = load <%zu x i8>, ptr %s, "
                             "align 1\n",
                     bytes, lanes, text_cstr(&mask));
        text_appendf(e->out, "  %%v%" PRIu32 " = trunc <%zu x i8> %%v%" PRIu32
                             " to <%zu x i1>\n",
                     bits, lanes, bytes, lanes);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     a, vector, text_cstr(&x), align);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     b, vector, text_cstr(&y), align);
        text_appendf(e->out, "  %%v%" PRIu32 " = select <%zu x i1> %%v%" PRIu32
                             ", %s %%v%" PRIu32 ", %s %%v%" PRIu32 "\n",
                     r, lanes, bits, vector, a, vector, b);
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, align %"
                             PRIu64 "\n",
                     vector, r, text_cstr(&dst), align);
    }
    text_free(&dst);
    text_free(&mask);
    text_free(&x);
    text_free(&y);
}

/* The lanes [from, from + count) of the vector v of lanes of type lane
   as a vector of count lanes. */
static uint32_t lanes_of(struct emitter *e, const char *lane, size_t lanes,
                         uint32_t v, size_t from, size_t count)
{
    uint32_t n = fresh(e);
    size_t k;

    text_appendf(e->out, "  %%v%" PRIu32 " = shufflevector <%zu x %s> %%v%"
                         PRIu32 ", <%zu x %s> poison, <%zu x i32> <",
                 n, lanes, lane, v, lanes, lane, count);
    for (k = 0; k < count; k++) {
        text_appendf(e->out, "%si32 %zu", k > 0 ? ", " : "", from + k);
    }
    text_append(e->out, ">\n");
    return n;
}

/* DESIGN: a fold of float lanes takes the order of the entry on v.sum()
   in docs/decisions.md: the upper half of the lanes onto the lower half
   until one is left. A float sum depends on the order, and the least and
   the greatest keep the upper lane where it is less or greater by the
   comparison of the IR, which decides a NaN and the two zeros. So the
   text halves the vector with shufflevector, where
   "Instruction mapping" names llvm.vector.reduce.fadd, fmin and fmax,
   whose order or whose NaN and zeros differ. The decision is above the
   work order. A fold of integer lanes takes its intrinsic, since the
   wrapping sum, the least, the greatest and the bits of a mask come out
   the same in every order. Returns the value of the lane left. */
static uint32_t halve(struct emitter *e, enum ir_op op, const char *lane,
                      size_t lanes, uint32_t v)
{
    size_t width = lanes;
    size_t half;
    uint32_t r;

    for (half = lanes / 2; half >= 1; half /= 2) {
        uint32_t lo = lanes_of(e, lane, width, v, 0, half);
        uint32_t hi = lanes_of(e, lane, width, v, half, half);
        if (op == IR_FADD) {
            v = fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = fadd <%zu x %s> %%v%"
                                 PRIu32 ", %%v%" PRIu32 "\n",
                         v, half, lane, lo, hi);
        } else {
            uint32_t c = fresh(e);
            v = fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = %s <%zu x %s> %%v%" PRIu32
                                 ", %%v%" PRIu32 "\n",
                         c, comparison(op), half, lane, hi, lo);
            text_appendf(e->out, "  %%v%" PRIu32 " = select <%zu x i1> %%v%"
                                 PRIu32 ", <%zu x %s> %%v%" PRIu32
                                 ", <%zu x %s> %%v%" PRIu32 "\n",
                         v, half, c, half, lane, hi, half, lane, lo);
        }
        width = half;
    }
    r = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = extractelement <%zu x %s> %%v%"
                         PRIu32 ", i64 0\n",
                 r, width, lane, v);
    return r;
}

/* vreduce: the intrinsic of its operation on integer lanes, and the
   halving fold on float lanes. */
static void vreduce(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_op op = (enum ir_op)inst->field;
    const char *lane = value_type(e, inst->type);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    const char *fold;
    struct text x = {0};
    char vector[48];
    char name[64];
    uint32_t v;
    char value[24];

    switch (op) {
    case IR_ADD: fold = "add"; break;
    case IR_SLT: fold = "smin"; break;
    case IR_ULT: fold = "umin"; break;
    case IR_SGT: fold = "smax"; break;
    case IR_UGT: fold = "umax"; break;
    case IR_OR: fold = "or"; break;
    case IR_AND: fold = "and"; break;
    case IR_FADD:
    case IR_FLT:
    case IR_FGT:
        fold = NULL;
        break;
    default:
        unexpected(e, "this fold of vreduce");
        return;
    }
    operand(e, &inst->a, IR_PTR, &x);
    if (e->failed || lane == NULL) {
        text_free(&x);
        return;
    }
    vector_of(e, inst, lane, vector, sizeof vector);
    v = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 v, vector, text_cstr(&x), layout_align(e->l, inst->of));
    if (fold == NULL) {
        v = halve(e, op, lane, lanes, v);
    } else {
        text_format(name, sizeof name, "llvm.vector.reduce.%s.v%zu%s", fold,
                    lanes, type_suffix(e, inst->type));
        intrinsic(e, lane, name, vector);
        text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %%v%" PRIu32
                             ")\n",
                     e->value, lane, name, vector, v);
        v = fresh(e);
    }
    text_format(value, sizeof value, "%%v%" PRIu32, v);
    store_result(e, inst, value);
    text_free(&x);
}

static void classify(struct emitter *e, const struct ir_function *f,
                     struct classified *c)
{
    c->params = alloc_zeroed(f->param_count + 1, sizeof *c->params);
    abi_classify(e->o->target, e->l, f, c->params, &c->result);
}

/* The LLVM type of the result r: void for one through the sret pointer,
   and a literal struct for two words. */
static void result_type(struct text *out, const struct abi_param *r)
{
    if (r->kind == ABI_SRET) {
        text_append(out, "void");
    } else if (r->kind == ABI_COERCE && r->word_count == 2) {
        text_appendf(out, "{ %s, %s }", r->types[0], r->types[1]);
    } else {
        text_append(out, r->types[0]);
    }
}

static uint64_t round_to_word(uint64_t size)
{
    return (size + 7) / 8 * 8;
}

static uint64_t word_align(uint64_t align)
{
    return align < 8 ? 8 : align;
}

/* The name %a<n> of a new alloca of size bytes aligned to align in the
   entry block, for a copy that a call or a signature needs. */
static uint32_t entry_slot(struct emitter *e, uint64_t size, uint64_t align)
{
    text_appendf(e->allocas, "  %%a%" PRIu32 " = alloca [%" PRIu64
                             " x i8], align %" PRIu64 "\n",
                 e->slots, size, align);
    return e->slots++;
}

/* DESIGN: a coerced aggregate lives in a slot of its words' size, a
   multiple of 8 bytes aligned to 8 or more, so a word that reaches past
   the end of the aggregate stays inside the slot. Word k of System V
   stands at 8 k bytes, and every other convention has one word. Returns
   the slot. */
static uint32_t word_slot(struct emitter *e, const struct abi_param *p)
{
    return entry_slot(e, round_to_word(p->size), word_align(p->align));
}

/* Append to address the address of word k of slot a. */
static void word_address(struct emitter *e, uint32_t a, size_t k,
                         struct text *address)
{
    if (k == 0) {
        text_appendf(address, "%%a%" PRIu32, a);
        return;
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = getelementptr i8, ptr %%a%"
                         PRIu32 ", i64 %zu\n",
                 e->value, a, 8 * k);
    text_appendf(address, "%%v%" PRIu32, e->value++);
}

/* Load the words of p from slot a, appending each value to values with
   its type, and its type alone to types. */
static void load_words(struct emitter *e, const struct abi_param *p,
                       uint32_t a, uint32_t *words)
{
    size_t k;

    for (k = 0; k < p->word_count; k++) {
        struct text address = {0};
        word_address(e, a, k, &address);
        words[k] = fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align 8\n",
                     words[k], p->types[k], text_cstr(&address));
        text_free(&address);
    }
}

/* Append a separator to list unless it is empty. */
static void separate(struct text *list)
{
    if (list->length > 0) {
        text_append(list, ", ");
    }
}

/* The attribute of an i8 or i16 parameter, which extends as the
   signature records. */
static const char *extension(enum ir_type type, enum ir_ext ext)
{
    if (type != IR_I8 && type != IR_I16) {
        return "";
    }
    return ext == IR_EXT_SIGN   ? " signext"
           : ext == IR_EXT_ZERO ? " zeroext"
                                : "";
}

/* Append to head the parameters of f as c classifies them, with their
   attributes and, when names is set, the names %sret, %p<i> and
   %p<i>.<k>. Append their types alone to types. */
static void parameters(const struct ir_function *f, const struct classified *c,
                       bool names, struct text *head, struct text *types)
{
    size_t i;
    size_t k;

    if (c->result.kind == ABI_SRET) {
        text_appendf(head, "ptr sret([%" PRIu64 " x i8]) align %" PRIu64 "%s",
                     c->result.size, c->result.align, names ? " %sret" : "");
        text_append(types, "ptr");
    }
    for (i = 0; i < f->param_count; i++) {
        const struct abi_param *p = &c->params[i];
        for (k = 0; k < p->word_count; k++) {
            separate(head);
            separate(types);
            text_append(types, p->types[k]);
            if (p->kind == ABI_BYVAL) {
                text_appendf(head, "ptr byval([%" PRIu64 " x i8]) align %"
                                   PRIu64,
                             p->size, p->align);
            } else {
                text_appendf(head, "%s%s", p->types[k],
                             p->kind == ABI_DIRECT
                                 ? extension(f->params[i].type,
                                             f->params[i].ext)
                                 : "");
            }
            if (names && p->kind == ABI_COERCE) {
                text_appendf(head, " %%p%zu.%zu", i, k);
            } else if (names) {
                text_appendf(head, " %%p%zu", i);
            }
        }
    }
    if (f->variadic) {
        separate(head);
        separate(types);
        text_append(head, "...");
        text_append(types, "...");
    }
}

/* DESIGN: a parameter arrives as abi_classify passes it, and its
   temporary holds what the IR expects: the scalar, or the address of the
   aggregate. A coerced aggregate is rebuilt from its words in a slot, and
   a vector is stored in a slot. A byval or indirect one is already in
   memory that the callee may write, a copy the caller or LLVM made. */
static void prologue(struct emitter *e, const struct ir_function *f,
                     const struct classified *c)
{
    size_t i;
    size_t k;

    for (i = 0; i < f->param_count && !e->failed; i++) {
        const struct abi_param *p = &c->params[i];
        uint32_t temp = f->params[i].temp;
        uint32_t a = 0;
        switch (p->kind) {
        case ABI_DIRECT:
            text_appendf(e->out, "  store %s %%p%zu, ptr %%t%" PRIu32
                                 ", align %" PRIu64 "\n",
                         value_type(e, f->params[i].type), i, temp,
                         align_of(e, f->params[i].type));
            continue;
        case ABI_BYVAL:
        case ABI_INDIRECT:
        case ABI_SRET:
            text_appendf(e->out, "  store ptr %%p%zu, ptr %%t%" PRIu32
                                 ", align 8\n",
                         i, temp);
            continue;
        case ABI_VECTOR:
            a = entry_slot(e, p->size, p->align);
            text_appendf(e->out, "  store %s %%p%zu, ptr %%a%" PRIu32
                                 ", align %" PRIu64 "\n",
                         p->types[0], i, a, p->align);
            break;
        case ABI_COERCE:
            a = word_slot(e, p);
            for (k = 0; k < p->word_count; k++) {
                struct text address = {0};
                word_address(e, a, k, &address);
                text_appendf(e->out, "  store %s %%p%zu.%zu, ptr %s, "
                                     "align 8\n",
                             p->types[k], i, k, text_cstr(&address));
                text_free(&address);
            }
            break;
        }
        text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                             ", align 8\n",
                     a, temp);
    }
}

/* Return the aggregate at the address in operand a as c classifies the
   result: a copy through the sret pointer, the vector, or the words
   loaded from a slot of their size. */
static void return_aggregate(struct emitter *e, const struct ir_operand *a)
{
    const struct abi_param *r = &e->signature.result;
    struct text pointer = {0};
    uint32_t words[2];
    uint32_t slot;

    operand(e, a, IR_PTR, &pointer);
    if (e->failed) {
        text_free(&pointer);
        return;
    }
    if (r->kind == ABI_SRET) {
        copy_bytes(e, "%sret", text_cstr(&pointer), r->size);
        text_append(e->out, "  ret void\n");
    } else if (r->kind == ABI_VECTOR) {
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     e->value, r->types[0], text_cstr(&pointer), r->align);
        text_appendf(e->out, "  ret %s %%v%" PRIu32 "\n", r->types[0],
                     e->value++);
    } else {
        char name[24];
        slot = word_slot(e, r);
        text_format(name, sizeof name, "%%a%" PRIu32, slot);
        copy_bytes(e, name, text_cstr(&pointer), r->size);
        load_words(e, r, slot, words);
        if (r->word_count == 1) {
            text_appendf(e->out, "  ret %s %%v%" PRIu32 "\n", r->types[0],
                         words[0]);
        } else {
            uint32_t first = fresh(e);
            uint32_t both = fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = insertvalue { %s, %s } "
                                 "poison, %s %%v%" PRIu32 ", 0\n",
                         first, r->types[0], r->types[1], r->types[0],
                         words[0]);
            text_appendf(e->out, "  %%v%" PRIu32 " = insertvalue { %s, %s } "
                                 "%%v%" PRIu32 ", %s %%v%" PRIu32 ", 1\n",
                         both, r->types[0], r->types[1], first, r->types[1],
                         words[1]);
            text_appendf(e->out, "  ret { %s, %s } %%v%" PRIu32 "\n",
                         r->types[0], r->types[1], both);
        }
    }
    text_free(&pointer);
}

/* Append to args the argument o for parameter p, after the instructions
   that make it, and its type to types. An aggregate argument is the
   address of the value. */
static void argument(struct emitter *e, const struct abi_param *p,
                     const struct ir_param *param, const struct ir_operand *o,
                     struct text *args, struct text *types)
{
    struct text value = {0};
    uint32_t words[2];
    uint32_t slot;
    size_t k;

    if (p->kind == ABI_DIRECT) {
        operand(e, o, param->type, &value);
        separate(args);
        separate(types);
        text_appendf(args, "%s%s %s", p->types[0],
                     extension(param->type, param->ext), text_cstr(&value));
        text_append(types, p->types[0]);
        text_free(&value);
        return;
    }
    operand(e, o, IR_PTR, &value);
    if (e->failed) {
        text_free(&value);
        return;
    }
    switch (p->kind) {
    case ABI_BYVAL:
        separate(args);
        text_appendf(args, "ptr byval([%" PRIu64 " x i8]) align %" PRIu64
                           " %s",
                     p->size, p->align, text_cstr(&value));
        break;
    case ABI_INDIRECT: {
        char name[24];
        slot = entry_slot(e, p->size, p->align);
        text_format(name, sizeof name, "%%a%" PRIu32, slot);
        copy_bytes(e, name, text_cstr(&value), p->size);
        separate(args);
        text_appendf(args, "ptr %s", name);
        break;
    }
    case ABI_VECTOR:
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     e->value, p->types[0], text_cstr(&value), p->align);
        separate(args);
        text_appendf(args, "%s %%v%" PRIu32, p->types[0], e->value++);
        break;
    default: {
        char name[24];
        slot = word_slot(e, p);
        text_format(name, sizeof name, "%%a%" PRIu32, slot);
        copy_bytes(e, name, text_cstr(&value), p->size);
        load_words(e, p, slot, words);
        for (k = 0; k < p->word_count; k++) {
            separate(args);
            text_appendf(args, "%s %%v%" PRIu32, p->types[k], words[k]);
        }
        break;
    }
    }
    for (k = 0; k < p->word_count; k++) {
        separate(types);
        text_append(types, p->types[k]);
    }
    text_free(&value);
}

/* Keep the aggregate result of a call, the value call of c, in a slot
   whose address the result temporary of inst holds: the words of a
   coerced result or the vector. */
static void keep_result(struct emitter *e, const struct ir_inst *inst,
                        const struct abi_param *r, uint32_t call)
{
    uint32_t slot;
    size_t k;

    if (r->kind == ABI_VECTOR) {
        slot = entry_slot(e, r->size, r->align);
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %%a%" PRIu32
                             ", align %" PRIu64 "\n",
                     r->types[0], call, slot, r->align);
    } else {
        slot = word_slot(e, r);
        for (k = 0; k < r->word_count; k++) {
            struct text address = {0};
            uint32_t word = call;
            if (r->word_count == 2) {
                word = fresh(e);
                text_appendf(e->out, "  %%v%" PRIu32 " = extractvalue "
                                     "{ %s, %s } %%v%" PRIu32 ", %zu\n",
                             word, r->types[0], r->types[1], call, k);
            }
            word_address(e, slot, k, &address);
            text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, "
                                 "align 8\n",
                         r->types[k], word, text_cstr(&address));
            text_free(&address);
        }
    }
    text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                         ", align 8\n",
                 slot, inst->result);
}

/* DESIGN: the result of a call takes the type of the instruction, which
   may differ from the one the callee declares, as a pointer that a
   function of the runtime returns as an i64. Returns the value of type to, which is v when
   the two LLVM types agree. */
static uint32_t reinterpret(struct emitter *e, uint32_t v, enum ir_type from,
                            enum ir_type to)
{
    const char *a = value_type(e, from);
    const char *b = value_type(e, to);
    bool float_a = from == IR_F32 || from == IR_F64;
    bool float_b = to == IR_F32 || to == IR_F64;
    const char *op;
    uint32_t n;

    if (a == NULL || b == NULL || strcmp(a, b) == 0) {
        return v;
    }
    if (from == IR_PTR) {
        op = "ptrtoint";
    } else if (to == IR_PTR) {
        op = "inttoptr";
    } else if (float_a || float_b ||
               layout_size(e->l, ir_scalar(from)) ==
                   layout_size(e->l, ir_scalar(to))) {
        op = "bitcast";
    } else if (layout_size(e->l, ir_scalar(from)) >
               layout_size(e->l, ir_scalar(to))) {
        op = "trunc";
    } else {
        op = "zext";
    }
    n = fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %%v%" PRIu32 " to %s\n", n,
                 op, a, v, b);
    return n;
}

/* DESIGN: a call names its function type, which abi_classify gives for
   the callee, or for the signature of an indirect call. An argument past
   the parameters of the callee, the variadic part of a C function or the
   context a named function never reads, passes in its own type. A call
   through a table is a call of the pointer the IR loaded from it, whose
   descriptor and slot the instruction carries for the passes before. An
   aggregate result lives in a slot of the call, whose address the result
   temporary holds. */
static void call(struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_function *callee =
        e->m->functions[inst->b.kind == IR_FUNC ? inst->b.as.index
                                                : inst->a.as.index];
    struct classified c;
    struct text target = {0};
    struct text args = {0};
    struct text types = {0};
    struct text result = {0};
    uint32_t sret = 0;
    uint32_t value;
    size_t i;

    classify(e, callee, &c);
    result_type(&result, &c.result);
    if (inst->a.kind == IR_FUNC) {
        function_name(&target, e->o->target,
                      e->m->functions[inst->a.as.index]);
    } else {
        operand(e, &inst->a, IR_PTR, &target);
    }
    if (c.result.kind == ABI_SRET) {
        sret = word_slot(e, &c.result);
        text_appendf(&args, "ptr sret([%" PRIu64 " x i8]) align %" PRIu64
                            " %%a%" PRIu32,
                     c.result.size, c.result.align, sret);
        text_append(&types, "ptr");
    }
    for (i = 0; i < inst->arg_count && !e->failed; i++) {
        struct ir_param extra;
        struct text unnamed = {0};
        if (i < callee->param_count) {
            argument(e, &c.params[i], &callee->params[i], &inst->args[i],
                     &args, &types);
            continue;
        }
        memset(&extra, 0, sizeof extra);
        extra.type = inst->args[i].type;
        if (value_type(e, extra.type) != NULL) {
            struct abi_param p;
            memset(&p, 0, sizeof p);
            p.kind = ABI_DIRECT;
            p.word_count = 1;
            text_format(p.types[0], sizeof p.types[0], "%s",
                        value_type(e, extra.type));
            /* The variadic part of a C function stays out of its type. */
            argument(e, &p, &extra, &inst->args[i], &args,
                     callee->variadic ? &unnamed : &types);
        }
        text_free(&unnamed);
    }
    if (callee->variadic) {
        separate(&types);
        text_append(&types, "...");
    }
    if (!e->failed) {
        bool named = strcmp(text_cstr(&result), "void") != 0;
        value = e->value;
        text_append(e->out, "  ");
        if (named) {
            text_appendf(e->out, "%%v%" PRIu32 " = ", e->value++);
        }
        text_appendf(e->out, "call %s (%s) %s(%s)\n", text_cstr(&result),
                     text_cstr(&types), text_cstr(&target),
                     text_cstr(&args));
        if (inst->result == IR_NO_RESULT) {
            /* Nothing reads the result. */
        } else if (c.result.kind == ABI_SRET) {
            text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                                 ", align 8\n",
                         sret, inst->result);
        } else if (callee->result == IR_AGG) {
            keep_result(e, inst, &c.result, value);
        } else {
            char name[24];
            value = reinterpret(e, value, callee->result, inst->type);
            text_format(name, sizeof name, "%%v%" PRIu32, value);
            store_result(e, inst, name);
        }
    }
    free(c.params);
    text_free(&target);
    text_free(&args);
    text_free(&types);
    text_free(&result);
}

/* The metadata of a branch whose one side is the failure arm of an
   assertion or a check, or the arm of a none guard, which marks that side
   cold. */
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
    case IR_UNREACHABLE:
        text_append(e->out, "  unreachable\n");
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
    case IR_CALL:
        call(e, inst);
        break;
    case IR_VSPLAT:
        vsplat(e, inst);
        break;
    case IR_VBINARY:
        vbinary(e, inst);
        break;
    case IR_VSELECT:
        vselect(e, inst);
        break;
    case IR_VREDUCE:
        vreduce(e, inst);
        break;
    case IR_VUNARY:
        vunary(e, inst);
        break;
    case IR_VSHUFFLE:
        vshuffle(e, inst);
        break;
    case IR_ADD_FL: case IR_SUB_FL: case IR_MUL_FL: case IR_SHL_FL:
    case IR_SHR_S_FL: case IR_SHR_U_FL: case IR_NEG_FL:
        flag_operation(e, inst);
        break;
    case IR_ADD_OV: case IR_SUB_OV: case IR_MUL_OV:
        overflow_operation(e, inst);
        break;
    case IR_BRANCH_OV:
        branch_overflow(e, inst);
        break;
    case IR_MULH_S: case IR_MULH_U:
        high_product(e, inst);
        break;
    case IR_ADD_SAT_S: case IR_ADD_SAT_U: case IR_SUB_SAT_S:
    case IR_SUB_SAT_U: case IR_MUL_SAT_S: case IR_MUL_SAT_U:
        saturating(e, inst);
        break;
    case IR_FLAG:
        flag_read(e, inst);
        break;
    case IR_RET:
        if (e->f->result == IR_AGG) {
            return_aggregate(e, &inst->a);
            break;
        }
        if (inst->type == IR_VOID) {
            text_append(e->out, "  ret void\n");
            break;
        }
        operand(e, &inst->a, inst->type, &value);
        if (!e->failed) {
            text_appendf(e->out, "  ret %s %s\n", value_type(e, inst->type),
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

/* The result type, the name and the parameters of f in parentheses, as
   abi_classify gives them, with the names of the parameters when names
   is set. The function type goes to type unless it is NULL. Returns
   false after a failure. */
static bool signature(struct emitter *e, const struct ir_function *f,
                      bool names, struct text *type)
{
    struct classified c;
    struct text head = {0};
    struct text types = {0};
    struct text result = {0};

    classify(e, f, &c);
    result_type(&result, &c.result);
    parameters(f, &c, names, &head, &types);
    text_appendf(e->out, "%s ", text_cstr(&result));
    function_name(e->out, e->o->target, f);
    text_appendf(e->out, "(%s)", text_cstr(&head));
    if (type != NULL) {
        text_appendf(type, "%s (%s)", text_cstr(&result), text_cstr(&types));
    }
    text_free(&head);
    text_free(&types);
    text_free(&result);
    free(c.params);
    return !e->failed;
}

/* Whether f is a copy of a generic in an object of one module, which
   every module that uses it defines. */
static bool link_once(const struct emitter *e, const struct ir_function *f)
{
    return e->o->one_module && ir_is_copy_name(f->name);
}

/* DESIGN: the linkage of a function. A whole program keeps every
   function internal, unless it is an export fn or the program hosts
   plugins. An object of one module makes every function global and
   hidden for the other objects, and a copy of a generic weak, in a COMDAT
   on COFF, which has no hidden symbols. An export fn is dso_local with
   default visibility. */
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

/* The function of the runtime that walks the stack from the frame of its
   caller, which anti.lang.StackTrace.capture declares and calls. */
#define TRACE_WALK "anti_rt_trace_walk"

/* The function that the call inst calls, or IR_NO_INDEX. The IR calls
   a static function of a class through a temporary that `addr` of the
   function fills, which callees maps to the function. */
static uint32_t callee_of(const struct ir_inst *inst, const uint32_t *callees)
{
    if (inst->a.kind == IR_FUNC) {
        return inst->a.as.index;
    }
    return inst->a.kind == IR_TEMP ? callees[inst->a.as.temp] : IR_NO_INDEX;
}

/* Whether the call inst passes another skip than the constant 0 to a
   function that keeps its frame. The skip is the first argument of
   anti_rt_trace_walk, of StackTrace.capture and of debug.backtrace. */
static bool skips_own_frame(const struct ir_module *m, const bool *kept,
                            const struct ir_inst *inst, const uint32_t *callees)
{
    uint32_t callee;

    if (inst->op != IR_CALL) {
        return false;
    }
    callee = callee_of(inst, callees);
    if (callee == IR_NO_INDEX || !kept[callee]) {
        return false;
    }
    if (m->functions[callee]->is_extern) {
        return true;
    }
    return inst->arg_count > 0 && (inst->args[0].kind != IR_INT ||
                                   inst->args[0].as.integer != 0);
}

/* Fill callees with the function each temporary of f holds the address
   of, from an `addr` or a copy of the function, or IR_NO_INDEX. */
static void callees_of(const struct ir_function *f, uint32_t *callees)
{
    size_t b;
    size_t k;

    for (k = 0; k < f->temp_count; k++) {
        callees[k] = IR_NO_INDEX;
    }
    for (b = 0; b < f->block_count; b++) {
        const struct ir_block *block = f->blocks[b];
        for (k = 0; k < block->count; k++) {
            const struct ir_inst *inst = &block->insts[k];
            if ((inst->op == IR_ADDR || inst->op == IR_COPY) &&
                inst->a.kind == IR_FUNC &&
                inst->result != IR_NO_RESULT) {
                callees[inst->result] = inst->a.as.index;
            }
        }
    }
}

/* DESIGN: StackTrace.capture skips its own frame by count, `skip + 1`,
   and debug.backtrace and a caller of capture(1) count their own the same
   way. Inlined into its caller, or left by a tail call, such a function
   would skip the frame of its caller as well, and the trace would not
   begin where the specification says. So a function keeps its frame,
   `noinline` and without tail calls, when it calls anti_rt_trace_walk,
   or when it passes another skip than 0 to a function that keeps its
   frame. A call without arguments passes no skip. A function that captures with skip 0
   may be inlined, and its frames are then the ones of its caller, with
   the lines of the callee under -g, as "Debug information" of
   docs/work-order-llvm-back-end.md says. The rule runs to a fixed point
   over the functions of the text. */
static void frames_kept(const struct ir_module *m, bool *kept)
{
    bool changed = true;
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        kept[i] = f->is_extern && f->module == NULL &&
                  strcmp(f->name, TRACE_WALK) == 0;
    }
    while (changed) {
        changed = false;
        for (i = 0; i < m->function_count; i++) {
            const struct ir_function *f = m->functions[i];
            uint32_t *callees;
            size_t b;
            if (kept[i] || f->is_extern) {
                continue;
            }
            callees = alloc_zeroed(f->temp_count + 1, sizeof *callees);
            callees_of(f, callees);
            for (b = 0; !kept[i] && b < f->block_count; b++) {
                const struct ir_block *block = f->blocks[b];
                size_t k;
                for (k = 0; !kept[i] && k < block->count; k++) {
                    if (skips_own_frame(m, kept, &block->insts[k], callees)) {
                        kept[i] = true;
                        changed = true;
                    }
                }
            }
            free(callees);
        }
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
    struct debug_spans spans = {0};
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
    text_append(out, f->never_returns ? " noreturn cold" : "");
    text_append(out, e->keeps_frame[f->index]
                         ? " noinline \"disable-tail-calls\"=\"true\" #0"
                         : " #0");
    if (link_once(e, f) &&
        target_info(e->o->target)->format == FORMAT_COFF) {
        text_append(out, " comdat");
    }
    llvm_debug_open(&e->debug, out, f,
                    !f->exported && !e->o->one_module && !e->o->exports);
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
    classify(e, f, &e->signature);
    prologue(e, f, &e->signature);
    for (b = 0; b < f->block_count && !e->failed; b++) {
        const struct ir_block *block = f->blocks[b];
        if (b > 0) {
            text_appendf(&body, "\nb%zu:\n", b);
        }
        for (i = 0; i < block->count && !e->failed; i++) {
            size_t from = body.length;
            instruction(e, &block->insts[i]);
            llvm_debug_at(&e->debug, &body, from, block->insts[i].line,
                          &spans);
        }
    }
    e->out = out;
    e->allocas = NULL;
    text_append_bytes(out, allocas.data, allocas.length);
    llvm_debug_place(&e->debug, &spans, out->length);
    text_append_bytes(out, body.data, body.length);
    text_append(out, "}\n\n");
    text_free(&body);
    text_free(&allocas);
    debug_spans_free(&spans);
    free(e->signature.params);
    e->signature.params = NULL;
}

/* The symbol of a global after sigil, without the _ of Mach-O, which llc
   writes. */
static void global_name(struct text *out, enum target t,
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
    text_append(out, name);
    text_free(&symbol);
}

static void global_sigil(struct text *out, char sigil, enum target t,
                         const struct ir_global *g)
{
    struct text name = {0};

    global_name(&name, t, g);
    llvm_name(out, sigil, text_cstr(&name));
    text_free(&name);
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
/* Whether the text is a COFF plugin, which reaches the names of its
   host through the __imp_ entries of the import library of the host. */
static bool imports(const struct emitter *e)
{
    return e->m->plugin &&
           target_info(e->o->target)->format == FORMAT_COFF;
}

/* Whether relocation r of a global names a function or a datum of the
   host of a COFF plugin. */
static bool imported(const struct emitter *e, const struct ir_reloc *r)
{
    return imports(e) && (r->fn ? e->m->functions[r->global]->is_extern
                                : e->m->globals[r->global]->is_extern);
}

/* DESIGN: a COFF plugin holds the address of the __imp_ entry of a name
   of its host where its data holds an address of the host. The loader
   replaces it with the address the entry holds, at each place
   anti_rt_imports lists. Append the entry of r, which global g holds at
   its offset, to values, and record the place. */
static void import_place(struct emitter *e, struct text *values,
                         const struct ir_global *g, const struct ir_reloc *r)
{
    struct text entry = {0};
    struct text line = {0};

    text_append(&entry, "__imp_");
    if (r->fn) {
        function_symbol(&entry, e->o->target, e->m->functions[r->global]);
    } else {
        global_name(&entry, e->o->target, e->m->globals[r->global]);
    }
    llvm_name(values, '@', text_cstr(&entry));
    text_appendf(&line, "\n%s\n", text_cstr(&entry));
    if (strstr(text_cstr(&e->imports), text_cstr(&line)) == NULL) {
        text_appendf(&e->imports, "%s%s\n", e->imports.length == 0 ? "\n" : "",
                     text_cstr(&entry));
    }
    text_append(&e->places, e->place_count++ > 0 ? ", ptr " : "ptr ");
    if (r->offset == 0) {
        global_symbol(&e->places, e->o->target, g);
    } else {
        text_append(&e->places, "getelementptr (i8, ptr ");
        global_symbol(&e->places, e->o->target, g);
        text_appendf(&e->places, ", i64 %" PRIu64 ")", r->offset);
    }
    text_free(&entry);
    text_free(&line);
}

/* The declaration of every __imp_ entry the data of a COFF plugin names,
   and the table anti_rt_imports of the places that hold one. The table
   stands in every COFF plugin, since its .def file exports it, and in
   writable data, since the loader marks it done. */
static void import_table(struct emitter *e)
{
    const char *p = text_cstr(&e->imports);

    if (!imports(e)) {
        return;
    }
    while (*p != '\0') {
        const char *end;
        struct text entry = {0};
        if (*p == '\n') {
            p++;
            continue;
        }
        end = strchr(p, '\n');
        text_append_bytes(&entry, p, (size_t)(end - p));
        llvm_name(e->out, '@', text_cstr(&entry));
        text_append(e->out, " = external global ptr\n");
        text_free(&entry);
        p = end;
    }
    if (e->place_count == 0) {
        text_append(e->out, "@anti_rt_imports = dso_local global <{ i64, "
                            "i64 }> <{ i64 0, i64 0 }>, align 8\n");
        return;
    }
    text_appendf(e->out, "@anti_rt_imports = dso_local global <{ i64, i64, "
                         "[%zu x ptr] }> <{ i64 %zu, i64 0, [%zu x ptr] "
                         "[%s] }>, align 8\n",
                 e->place_count, e->place_count, e->place_count,
                 text_cstr(&e->places));
}

static void global(struct emitter *e, const struct ir_global *g, bool twin)
{
    enum target t = e->o->target;
    bool coff = target_info(t)->format == FORMAT_COFF;
    uint64_t word = layout_size(e->l, ir_scalar(IR_PTR));
    struct text types = {0};
    struct text values = {0};
    struct ir_reloc *relocs;
    uint64_t from = 0;
    bool zero = true;
    bool writable = false;
    size_t i;

    if (g->is_extern && twin) {
        return;
    }
    global_symbol(e->out, t, g);
    if (g->is_extern) {
        text_appendf(e->out, " = external %sglobal [%" PRIu64 " x i8], "
                             "align %" PRIu64 "\n",
                     imports(e) ? "dllimport " : "", g->size,
                     g->align > 0 ? g->align : 1);
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
        if (imported(e, &relocs[i])) {
            import_place(e, &values, g, &relocs[i]);
            writable = true;
        } else if (relocs[i].fn) {
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
    text_appendf(e->out, "%s <{ %s }> ",
                 g->mutable || writable ? "global" : "constant",
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
   of RUNTIME_MODULE. An alias gives main the second name, which stays global while main
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
    text_append(e->out, imports(e) ? "declare dllimport " : "declare ");
    if (signature(e, f, false, NULL)) {
        effects(e->out, f->effects, f->guarantees);
        text_append(e->out, f->never_returns ? " noreturn cold #1\n"
                                             : " #1\n");
    }
}

/* DESIGN: every Anti function is nounwind, since Anti has no exceptions
   and fails through a return value. A declaration is a function of
   another module or a function of C, which unwinds through no Anti frame
   either. Windows walks a stack through the unwind tables of each frame,
   and its frames of a page or more call the stack probe. Every other
   target keeps the frame record of a function that calls another, since
   anti_rt_trace_walk and the report of --memory-checks follow the chain of
   records. Eddie decided it on 2026-10-04, see "Attributes and metadata"
   in docs/work-order-llvm-back-end.md. */
static void attributes(struct emitter *e)
{
    const struct target_info *info = target_info(e->o->target);
    bool windows = info->os == OS_WINDOWS;

    text_appendf(e->out, "attributes #0 = { nounwind%s \"frame-pointer\"="
                         "\"%s\"%s \"target-cpu\"=\"%s\" "
                         "\"target-features\"=\"%s\" }\n",
                 windows ? " uwtable(sync)" : "",
                 windows ? "none" : "non-leaf",
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
    llvm_debug_flags(&e->debug, e->out);
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
    llvm_debug_finish(&e->debug, e->out);
}

/* The number of module flags of the target: the size of wchar_t, the
   level of position-independent code where the relocation model is pic,
   and the unwind tables of Windows. */
static uint32_t flag_count(enum target t)
{
    return 1 + (strcmp(llvm_relocation_model(t), "pic") == 0) +
           (target_info(t)->os == OS_WINDOWS);
}

/* DESIGN: two globals of the IR may name one symbol, as every module
   names the descriptor of the root that the runtime defines. A
   declaration is written once, and not at all for a symbol the text
   defines. Whether the declaration of global i is such a twin. */
static bool declared_elsewhere(const struct ir_module *m,
                               const struct text *symbols, size_t i)
{
    size_t j;

    for (j = 0; j < m->global_count; j++) {
        if (j != i && (j < i || !m->globals[j]->is_extern) &&
            strcmp(text_cstr(&symbols[i]), text_cstr(&symbols[j])) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: the functions that run before main are the one of each module
   that compiles its patterns and, in a shared library, the constructor of
   the runtime. llvm.global_ctors names them, at the priority of a C
   constructor, and llc writes the section of each format: .init_array,
   __mod_init_func or .CRT$XCU. */
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

/* DESIGN: the package header and the notice keep a section of their own
   per object format. No symbol names the copy of the package header, so
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

void llvm_emit_names(struct text *out, enum target t,
                     const struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        if (!m->functions[i]->is_extern) {
            function_symbol(out, t, m->functions[i]);
            text_append(out, "\n");
        }
    }
    for (i = 0; i < m->global_count; i++) {
        if (!m->globals[i]->is_extern) {
            global_name(out, t, m->globals[i]);
            text_append(out, " DATA\n");
        }
    }
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
    struct text declarations = {0};
    struct text data = {0};
    uint32_t flags = flag_count(o->target);
    struct text *symbols;
    bool comdats = false;
    size_t i;

    memset(&e, 0, sizeof e);
    e.o = o;
    e.l = l;
    e.error = error;
    e.error_size = error_size;
    e.cold_id = flags + 1;
    e.m = m;
    /* The ids of the debug metadata follow every other id, so the text
       without them names the ids of a build without -g. */
    llvm_debug_init(&e.debug, o->target, m, o->module, o->debug,
                    o->optimized, e.cold_id + COLD_COUNT, o->spans);
    e.keeps_frame = alloc_zeroed(m->function_count + 1, sizeof *e.keeps_frame);
    frames_kept(m, e.keeps_frame);
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
    /* The definitions go straight into out, where the spans of the
       debug information they carry stand. */
    for (i = 0; i < m->function_count && !e.failed; i++) {
        const struct ir_function *f = m->functions[i];
        e.out = f->is_extern ? &declarations : out;
        if (f->is_extern) {
            declaration(&e, f);
        } else {
            definition(&e, f);
        }
    }
    e.out = &data;
    symbols = alloc_zeroed(m->global_count + 1, sizeof *symbols);
    for (i = 0; i < m->global_count; i++) {
        global_symbol(&symbols[i], o->target, m->globals[i]);
    }
    for (i = 0; i < m->global_count && !e.failed; i++) {
        global(&e, m->globals[i],
               m->globals[i]->is_extern &&
                   declared_elsewhere(m, symbols, i));
    }
    for (i = 0; i < m->global_count; i++) {
        text_free(&symbols[i]);
    }
    free(symbols);
    import_table(&e);
    constructors(&e, m);
    if (!e.failed) {
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
    text_free(&data);
    text_free(&declarations);
    text_free(&e.intrinsics);
    text_free(&e.entry);
    text_free(&e.places);
    text_free(&e.imports);
    free(e.keeps_frame);
    llvm_debug_free(&e.debug);
    return !e.failed;
}
