/* The text of a function in LLVM IR: its operands and constants, the
   scalar, memory, wide and simd operations, the flags and the overflow of
   the arithmetic, each instruction and the definition of a function. */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "llvm_debug.h"
#include "llvm_emit_parts.h"
#include "rt_abi.h"

/* DESIGN: the IR is not in SSA form, so every temporary gets an alloca in
   the entry block. A definition stores into it and each use loads from
   it. The sroa and mem2reg passes of opt build the SSA form, as they do
   for every local clang writes. Each block of the IR is one LLVM block
   named b<index>, and the entry block of the IR is the entry of the
   function. The section "Instruction mapping" of
   docs/work-order-llvm-back-end.md gives the form of each operation, and
   every operation of the IR has one. */

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
   for a form the text has no translation of, since the lowering never
   writes it. */
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
const char *llvm_emit_value_type(struct emitter *e, enum ir_type type)
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

uint64_t llvm_emit_align_of(struct emitter *e, enum ir_type type)
{
    return layout_align(e->l, ir_scalar(type));
}

/* The constant of an integer operand of type: the signed value of its
   bits, or null for a pointer. */
void llvm_emit_int_constant(struct text *out, enum ir_type type, uint64_t value)
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

/* Append to value the LLVM operand of o as a value of type, after the
   load that reads a temporary. A global or a function is its symbol. */
void llvm_emit_operand(struct emitter *e, const struct ir_operand *o,
                       enum ir_type type, struct text *value)
{
    const char *name = llvm_emit_value_type(e, type);

    if (name == NULL) {
        return;
    }
    switch (o->kind) {
    case IR_TEMP:
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %%t%" PRIu32
                             ", align %" PRIu64 "\n",
                     e->value, name, o->as.temp, llvm_emit_align_of(e, type));
        text_appendf(value, "%%v%" PRIu32, e->value++);
        return;
    case IR_INT:
        if (type == IR_F32 || type == IR_F64) {
            float_bits(value, type, o->as.integer);
        } else {
            llvm_emit_int_constant(value, type, o->as.integer);
        }
        return;
    case IR_FLOAT:
        float_constant(value, type, o->as.floating);
        return;
    case IR_GLOBAL:
        llvm_emit_global_symbol(value, e->o->target,
                                e->m->globals[o->as.index]);
        return;
    case IR_FUNC:
        llvm_emit_function_name(value, e->o->target,
                                e->m->functions[o->as.index]);
        return;
    case IR_NONE:
    case IR_BLOCK:
    case IR_SYM:
        break;
    }
    unexpected(e, "this operand");
}

void llvm_emit_store_result(struct emitter *e, const struct ir_inst *inst,
                            const char *value)
{
    text_appendf(e->out, "  store %s %s, ptr %%t%" PRIu32 ", align %" PRIu64
                         "\n",
                 llvm_emit_value_type(e, inst->type), value, inst->result,
                 llvm_emit_align_of(e, inst->type));
}

/* The type of operand o: the type of its temporary, or the type a
   constant carries. */
static enum ir_type operand_type(const struct emitter *e,
                                 const struct ir_operand *o)
{
    return o->kind == IR_TEMP ? e->f->temps[o->as.temp] : o->type;
}

/* The next value of the function, %v<n>, which the caller defines. */
uint32_t llvm_emit_fresh(struct emitter *e)
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
    uint32_t zero = llvm_emit_fresh(e);
    uint32_t guard = zero;
    uint32_t minus = 0;
    uint32_t safe;
    uint32_t raw;

    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, 0\n", zero, type,
                 b);
    if (is_signed) {
        minus = llvm_emit_fresh(e);
        guard = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, -1\n", minus,
                     type, b);
        text_appendf(e->out, "  %%v%" PRIu32 " = or i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     guard, zero, minus);
    }
    safe = llvm_emit_fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32 ", %s 1, "
                         "%s %s\n",
                 safe, guard, type, type, b);
    raw = llvm_emit_fresh(e);
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
        uint32_t negated = llvm_emit_fresh(e);
        uint32_t chosen = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n", negated,
                     type, a);
        text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32
                             ", %s %%v%" PRIu32 ", %s %%v%" PRIu32 "\n",
                     chosen, minus, type, negated, type, raw);
        raw = chosen;
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32 ", %s 0, "
                         "%s %%v%" PRIu32 "\n",
                 llvm_emit_fresh(e), zero, type, type, raw);
}

/* Declare the intrinsic name of type result (parameters) once. */
static void intrinsic(struct emitter *e, const char *result, const char *name,
                      const char *parameters)
{
    char line[160];

    text_format_name(line, sizeof line, "declare %s @%s(%s)\n", result, name,
                     parameters);
    if (strstr(text_cstr(&e->intrinsics), line) == NULL) {
        text_append(&e->intrinsics, line);
    }
}

/* Append the memory effects of a C function and its guarantees to out,
   each attribute after a space. A function of the class IR_EFFECTS_ANY
   gets none, since LLVM reads no guarantee without a class. */
void llvm_emit_effects(struct text *out, enum ir_effects kind,
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
void llvm_emit_runtime_function(struct emitter *e, const char *result,
                                enum rt_function f, const char *parameters)
{
    const struct rt_signature *s = rt_signature(f);
    struct text line = {0};

    text_appendf(&line, "declare %s%s @%s(%s)",
                 llvm_emit_imports(e) ? "dllimport " : "", result, s->name,
                 parameters);
    llvm_emit_effects(&line, s->effects, s->guarantees);
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
    const char *name = llvm_emit_value_type(e, type);
    struct text a = {0};
    struct text b = {0};
    char value[24];

    if (name == NULL) {
        return;
    }
    llvm_emit_operand(e, &inst->a, type, &a);
    llvm_emit_operand(e, &inst->b, type, &b);
    if (e->failed) {
        text_free(&a);
        text_free(&b);
        return;
    }
    switch (inst->op) {
    case IR_SHL:
    case IR_SHR_S:
    case IR_SHR_U: {
        uint32_t count = llvm_emit_fresh(e);
        uint32_t shifted = llvm_emit_fresh(e);
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
            uint32_t bit = llvm_emit_fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, %s\n", bit,
                         comparison(inst->op), name, text_cstr(&a),
                         text_cstr(&b));
            text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %%v%" PRIu32
                                 " to %s\n",
                         llvm_emit_fresh(e), bit,
                         llvm_emit_value_type(e, inst->type));
        } else {
            text_appendf(e->out, "  %%v%" PRIu32 " = %s%s%s %s %s, %s\n",
                         llvm_emit_fresh(e), plain_binary(inst->op),
                         (inst->field & IR_NUW) != 0 ? " nuw" : "",
                         (inst->field & IR_NSW) != 0 ? " nsw" : "", name,
                         text_cstr(&a), text_cstr(&b));
        }
        break;
    }
    text_format_name(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    llvm_emit_store_result(e, inst, value);
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
bool llvm_emit_f16_calls(const struct emitter *e)
{
    return cpu_arch(e->o->cpu) == ARCH_X86_64 && !cpu_has(e->o->cpu, CPU_F16C);
}

/* DESIGN: a float out of the range of the integer type saturates, and NaN
   gives 0, on every target. llvm.fptosi.sat and llvm.fptoui.sat define
   exactly that, where fptosi and fptoui give poison. */
static void conversion(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_type from = operand_type(e, &inst->a);
    const char *source = llvm_emit_value_type(e, from);
    const char *target = llvm_emit_value_type(e, inst->type);
    struct text a = {0};
    char value[24];

    if (source == NULL || target == NULL) {
        return;
    }
    llvm_emit_operand(e, &inst->a, from, &a);
    if (e->failed) {
        text_free(&a);
        return;
    }
    switch (inst->op) {
    case IR_FTOSI:
    case IR_FTOUI: {
        char name[48];
        text_format_name(name, sizeof name, "llvm.fpto%s.sat.i%u.%s",
                         inst->op == IR_FTOSI ? "si" : "ui",
                         width_of(inst->type), float_suffix(from));
        intrinsic(e, target, name, source);
        text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s)\n",
                     llvm_emit_fresh(e), target, name, source, text_cstr(&a));
        break;
    }
    case IR_HEXT: {
        uint32_t half = llvm_emit_fresh(e);
        if (llvm_emit_f16_calls(e)) {
            llvm_emit_runtime_function(e, "float", RT_FN_F16_TO_F32, "i32");
            text_appendf(e->out, "  %%v%" PRIu32 " = zext %s %s to i32\n",
                         half, source, text_cstr(&a));
            text_appendf(e->out, "  %%v%" PRIu32 " = call float @%s(i32 "
                                 "%%v%" PRIu32 ")\n",
                         llvm_emit_fresh(e), rt_name(RT_FN_F16_TO_F32), half);
            break;
        }
        text_appendf(e->out, "  %%v%" PRIu32 " = bitcast %s %s to half\n",
                     half, source, text_cstr(&a));
        text_appendf(e->out, "  %%v%" PRIu32 " = fpext half %%v%" PRIu32
                             " to %s\n",
                     llvm_emit_fresh(e), half, target);
        break;
    }
    case IR_HTRUNC: {
        uint32_t half = llvm_emit_fresh(e);
        if (llvm_emit_f16_calls(e)) {
            llvm_emit_runtime_function(e, "i32", RT_FN_F32_TO_F16, "float");
            text_appendf(e->out, "  %%v%" PRIu32 " = call i32 @%s(%s %s)\n",
                         half, rt_name(RT_FN_F32_TO_F16), source,
                         text_cstr(&a));
            text_appendf(e->out, "  %%v%" PRIu32 " = trunc i32 %%v%" PRIu32
                                 " to %s\n",
                         llvm_emit_fresh(e), half, target);
            break;
        }
        text_appendf(e->out, "  %%v%" PRIu32 " = fptrunc %s %s to half\n",
                     half, source, text_cstr(&a));
        text_appendf(e->out, "  %%v%" PRIu32 " = bitcast half %%v%" PRIu32
                             " to %s\n",
                     llvm_emit_fresh(e), half, target);
        break;
    }
    default:
        text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n",
                     llvm_emit_fresh(e),
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
    text_format_name(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    llvm_emit_store_result(e, inst, value);
    text_free(&a);
}

/* neg, not and fneg: sub from 0, xor with all ones and fneg. */
static void unary(struct emitter *e, const struct ir_inst *inst)
{
    const char *name = llvm_emit_value_type(e, inst->type);
    struct text a = {0};
    char value[24];

    if (name == NULL) {
        return;
    }
    llvm_emit_operand(e, &inst->a, inst->type, &a);
    if (e->failed) {
        text_free(&a);
        return;
    }
    if (inst->op == IR_NEG) {
        text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n",
                     llvm_emit_fresh(e), name, text_cstr(&a));
    } else if (inst->op == IR_NOT) {
        text_appendf(e->out, "  %%v%" PRIu32 " = xor %s %s, -1\n",
                     llvm_emit_fresh(e), name, text_cstr(&a));
    } else {
        text_appendf(e->out, "  %%v%" PRIu32 " = fneg %s %s\n",
                     llvm_emit_fresh(e), name, text_cstr(&a));
    }
    text_format_name(value, sizeof value, "%%v%" PRIu32, e->value - 1);
    llvm_emit_store_result(e, inst, value);
    text_free(&a);
}

/* Copy size bytes from src to dst, two values of ptr. */
void llvm_emit_copy_bytes(struct emitter *e, const char *dst, const char *src,
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
    text_format_name(value, sizeof value, "%%s%" PRIu32, inst->result);
    llvm_emit_store_result(e, inst, value);
}

/* DESIGN: a load and a store take the natural alignment of their type.
   The IR marks no access as unaligned, and the section "Instruction
   mapping" of docs/work-order-llvm-back-end.md gives 1 only to an access
   the IR marks. */
static void load_store(struct emitter *e, const struct ir_inst *inst)
{
    const char *name = llvm_emit_value_type(e, inst->type);
    struct text value = {0};
    struct text pointer = {0};

    if (name == NULL) {
        return;
    }
    if (inst->op == IR_STORE) {
        llvm_emit_operand(e, &inst->a, inst->type, &value);
        llvm_emit_operand(e, &inst->b, IR_PTR, &pointer);
        if (!e->failed) {
            text_appendf(e->out, "  store %s %s, ptr %s, align %" PRIu64,
                         name, text_cstr(&value), text_cstr(&pointer),
                         llvm_emit_align_of(e, inst->type));
            llvm_emit_access_fact(e, inst);
            llvm_emit_tbaa_fact(e, inst);
            text_append(e->out, "\n");
        }
    } else {
        llvm_emit_operand(e, &inst->a, IR_PTR, &pointer);
        if (!e->failed) {
            text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                                 PRIu64,
                         e->value, name, text_cstr(&pointer),
                         llvm_emit_align_of(e, inst->type));
            llvm_emit_access_fact(e, inst);
            llvm_emit_range_fact(e, inst);
            llvm_emit_tbaa_fact(e, inst);
            text_append(e->out, "\n");
            text_appendf(&value, "%%v%" PRIu32, e->value++);
            llvm_emit_store_result(e, inst, text_cstr(&value));
        }
    }
    text_free(&value);
    text_free(&pointer);
}

/* DESIGN: an offset of bytes, which a getelementptr of i8 adds without
   inbounds, so an address outside its object is no poison. An offset
   narrower than 64 bits extends with its sign. The one exception, which
   "Arithmetic, addresses and ranges" in
   docs/work-order-llvm-optimization.md gives, is the address of a field
   of an object that a non-optional reference reaches, which lowering
   marks IR_INBOUNDS. The checker guarantees that such an object exists
   and holds the field. A view through `as` and an address made from an
   integer never carry the mark, see sema_value_view. */
static void ptradd(struct emitter *e, const struct ir_inst *inst)
{
    enum ir_type type = operand_type(e, &inst->b);
    struct text pointer = {0};
    struct text offset = {0};
    char value[24];

    llvm_emit_operand(e, &inst->a, IR_PTR, &pointer);
    llvm_emit_operand(e, &inst->b, type, &offset);
    if (!e->failed && type != IR_I64) {
        uint32_t wide = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = sext %s %s to i64\n", wide,
                     llvm_emit_value_type(e, type), text_cstr(&offset));
        text_free(&offset);
        text_appendf(&offset, "%%v%" PRIu32, wide);
    }
    if (!e->failed) {
        text_appendf(e->out, "  %%v%" PRIu32 " = getelementptr %si8, ptr %s, "
                             "i64 %s\n",
                     e->value,
                     inst->field == IR_INBOUNDS ? "inbounds " : "",
                     text_cstr(&pointer), text_cstr(&offset));
        text_format_name(value, sizeof value, "%%v%" PRIu32, e->value++);
        llvm_emit_store_result(e, inst, value);
    }
    text_free(&pointer);
    text_free(&offset);
}

static void memcopy(struct emitter *e, const struct ir_inst *inst)
{
    struct text dst = {0};
    struct text src = {0};

    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, IR_PTR, &src);
    if (!e->failed) {
        llvm_emit_copy_bytes(e, text_cstr(&dst), text_cstr(&src),
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
    n = llvm_emit_fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s to %s\n", n,
                 a > b                 ? "trunc"
                 : ext == IR_EXT_SIGN ? "sext"
                                      : "zext",
                 llvm_emit_value_type(e, from), text_cstr(v),
                 llvm_emit_value_type(e, to));
    text_free(v);
    text_appendf(v, "%%v%" PRIu32, n);
}

/* Write v = op type v, k and make v the result. */
static void unit_op(struct emitter *e, struct text *v, const char *op,
                    enum ir_type type, uint64_t k)
{
    uint32_t n = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %s, ", n, op,
                 llvm_emit_value_type(e, type), text_cstr(v));
    llvm_emit_int_constant(e->out, type, k);
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
    const char *unit_name = llvm_emit_value_type(e, unit);
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
        llvm_emit_operand(e, &inst->a, inst->type, &value);
    }
    llvm_emit_operand(e, inst->op == IR_BITLOAD ? &inst->a : &inst->b, IR_PTR,
                      &pointer);
    if (e->failed || unit_name == NULL) {
        text_free(&value);
        text_free(&pointer);
        return;
    }
    if (where->unit_offset != 0) {
        uint32_t n = llvm_emit_fresh(e);
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
        llvm_emit_store_result(e, inst, text_cstr(&word));
    } else if (inst->op == IR_BITLOAD) {
        if (where->shift != 0) {
            unit_op(e, &word, "lshr", unit, where->shift);
        }
        if (width < unit_bits) {
            unit_op(e, &word, "and", unit, field_mask(width));
        }
        convert_int(e, &word, unit, inst->type, IR_EXT_ZERO);
        llvm_emit_store_result(e, inst, text_cstr(&word));
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
        n = llvm_emit_fresh(e);
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
    text_format_name(out, size, "<%zu x %s>",
                     e->m->aggs[inst->of.agg]->field_count, element);
}

/* DESIGN: a simd operation loads whole vectors at the alignment of the
   simd struct, works on <N x T> and stores the result, as "Instruction
   mapping" in docs/work-order-llvm-back-end.md gives it. A comparison stores its mask as i8
   lanes at alignment 1, since the instruction names the aggregate of
   its operands and not the one of the mask. A shift takes each count
   modulo the width of a lane, as a scalar shift does. The verifier of
   the IR gives vbinary no integer division. */
static void vsplat(struct emitter *e, const struct ir_inst *inst)
{
    const char *lane = llvm_emit_value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    struct text dst = {0};
    struct text value = {0};
    char vector[48];
    uint32_t one;
    uint32_t all;

    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, inst->type, &value);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        one = llvm_emit_fresh(e);
        all = llvm_emit_fresh(e);
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
    const char *lane = llvm_emit_value_type(e, inst->type);
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
    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, IR_PTR, &x);
    llvm_emit_operand(e, &inst->c, IR_PTR, &y);
    if (e->failed || lane == NULL) {
        text_free(&dst);
        text_free(&x);
        text_free(&y);
        return;
    }
    vector_of(e, inst, lane, vector, sizeof vector);
    a = llvm_emit_fresh(e);
    b = llvm_emit_fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 a, vector, text_cstr(&x), align);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 b, vector, text_cstr(&y), align);
    if (comparison(op) != NULL) {
        uint32_t bits = llvm_emit_fresh(e);
        r = llvm_emit_fresh(e);
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
            uint32_t count = llvm_emit_fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = and %s %%v%" PRIu32 ", <",
                         count, vector, b);
            for (k = 0; k < lanes; k++) {
                text_appendf(e->out, "%s%s %u", k > 0 ? ", " : "", lane,
                             width_of(inst->type) - 1);
            }
            text_append(e->out, ">\n");
            b = count;
        }
        r = llvm_emit_fresh(e);
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
    return llvm_emit_value_type(e, type);
}

/* Call the intrinsic llvm.<kind>.with.overflow.<T> on x and y, declared
   once, and return the value of the call. */
static uint32_t with_overflow(struct emitter *e, const char *kind,
                              const char *type, const char *x, const char *y)
{
    char name[64];
    char result[32];
    char parameters[32];
    uint32_t n = llvm_emit_fresh(e);

    text_format_name(name, sizeof name, "llvm.%s.with.overflow.%s", kind, type);
    text_format_name(result, sizeof result, "{ %s, i1 }", type);
    text_format_name(parameters, sizeof parameters, "%s, %s", type, type);
    intrinsic(e, result, name, parameters);
    text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s, %s %s)\n", n,
                 result, name, type, x, type, y);
    return n;
}

/* The field k of the value v of type { type, i1 }. */
static uint32_t field_of(struct emitter *e, const char *type, uint32_t v,
                         unsigned k)
{
    uint32_t n = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = extractvalue { %s, i1 } %%v%"
                         PRIu32 ", %u\n",
                 n, type, v, k);
    return n;
}

/* Keep the value %v<v> as the flag of the last flag operation. */
static void keep_flag(struct emitter *e, enum ir_flag flag, uint32_t v)
{
    text_format_name(e->flags[flag], sizeof e->flags[flag], "%%v%" PRIu32, v);
}

/* Keep zero and sign of the result r of type, and r as the result of the
   flag operation inst. */
static void finish_flags(struct emitter *e, const struct ir_inst *inst,
                         const char *type, uint32_t r)
{
    char value[24];
    uint32_t zero = llvm_emit_fresh(e);
    uint32_t sign = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %%v%" PRIu32 ", 0\n",
                 zero, type, r);
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp slt %s %%v%" PRIu32 ", 0\n",
                 sign, type, r);
    keep_flag(e, IR_FLAG_ZERO, zero);
    keep_flag(e, IR_FLAG_NEGATIVE, sign);
    e->flag_result = inst->result;
    text_format_name(value, sizeof value, "%%v%" PRIu32, r);
    llvm_emit_store_result(e, inst, value);
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
        llvm_emit_operand(e, &inst->c, IR_I8, &c);
    }
    if (e->failed) {
        text_free(&c);
        return;
    }
    text_format_name(signed_kind, sizeof signed_kind, "s%s", kind);
    text_format_name(unsigned_kind, sizeof unsigned_kind, "u%s", kind);
    s = with_overflow(e, signed_kind, type, a, b);
    u = with_overflow(e, unsigned_kind, type, a, b);
    r = field_of(e, type, s, 0);
    overflow = field_of(e, type, s, 1);
    carry = field_of(e, type, u, 1);
    if (inst->c.kind != IR_NONE) {
        uint32_t set = llvm_emit_fresh(e);
        uint32_t wide = llvm_emit_fresh(e);
        char partial[24];
        char in[24];
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne i8 %s, 0\n", set,
                     text_cstr(&c));
        text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %%v%" PRIu32
                             " to %s\n",
                     wide, set, type);
        text_format_name(partial, sizeof partial, "%%v%" PRIu32, r);
        text_format_name(in, sizeof in, "%%v%" PRIu32, wide);
        s = with_overflow(e, signed_kind, type, partial, in);
        u = with_overflow(e, unsigned_kind, type, partial, in);
        r = field_of(e, type, s, 0);
        s = field_of(e, type, s, 1);
        u = field_of(e, type, u, 1);
        text_appendf(e->out, "  %%v%" PRIu32 " = xor i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     e->value, overflow, s);
        overflow = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = or i1 %%v%" PRIu32 ", %%v%"
                             PRIu32 "\n",
                     e->value, carry, u);
        carry = llvm_emit_fresh(e);
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
    uint32_t count = llvm_emit_fresh(e);
    uint32_t r = llvm_emit_fresh(e);
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
        uint32_t back = llvm_emit_fresh(e);
        uint32_t overflow = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = ashr %s %%v%" PRIu32
                             ", %%v%" PRIu32 "\n",
                     back, type, r, count);
        text_appendf(e->out, "  %%v%" PRIu32 " = icmp ne %s %%v%" PRIu32
                             ", %s\n",
                     overflow, type, back, a);
        keep_flag(e, IR_FLAG_OVERFLOW, overflow);
    } else {
        text_format_name(e->flags[IR_FLAG_OVERFLOW],
                         sizeof e->flags[IR_FLAG_OVERFLOW], "false");
    }
    moved = llvm_emit_fresh(e);
    less = llvm_emit_fresh(e);
    wrapped = llvm_emit_fresh(e);
    shifted = llvm_emit_fresh(e);
    bit = llvm_emit_fresh(e);
    carry = llvm_emit_fresh(e);
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
    uint32_t r = llvm_emit_fresh(e);
    uint32_t overflow = llvm_emit_fresh(e);
    uint32_t carry = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = sub %s 0, %s\n", r, type, a);
    text_appendf(e->out, "  %%v%" PRIu32 " = icmp eq %s %s, ", overflow, type,
                 a);
    llvm_emit_int_constant(e->out, inst->type,
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
    const char *type = llvm_emit_value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};

    if (type == NULL) {
        return;
    }
    llvm_emit_operand(e, &inst->a, inst->type, &a);
    if (inst->op != IR_NEG_FL) {
        llvm_emit_operand(e, &inst->b, inst->type, &b);
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
    text_format_name(value, sizeof value, "%%v%" PRIu32, e->value++);
    llvm_emit_store_result(e, inst, value);
}

/* addov, subov and mulov: the signed with.overflow intrinsic. The result
   goes to its temporary, and the i1 stays for the branchov right after
   it, in the same block. */
static void overflow_operation(struct emitter *e, const struct ir_inst *inst)
{
    const char *type = llvm_emit_value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};
    char value[24];
    uint32_t pair;
    uint32_t r;

    if (type == NULL) {
        return;
    }
    llvm_emit_operand(e, &inst->a, inst->type, &a);
    llvm_emit_operand(e, &inst->b, inst->type, &b);
    if (!e->failed) {
        pair = with_overflow(e,
                             inst->op == IR_ADD_OV   ? "sadd"
                             : inst->op == IR_SUB_OV ? "ssub"
                                                     : "smul",
                             type, text_cstr(&a), text_cstr(&b));
        /* DESIGN: the path where the check found no overflow reads the
           result, and the overflow block reads the operands alone, so
           the result of an operation that lowering marks IR_NSW is the
           plain operation with nsw. A wrap makes it poison on the
           overflow path alone, which never reads it. */
        if ((inst->field & IR_NSW) != 0) {
            r = llvm_emit_fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = %s nsw %s %s, %s\n", r,
                         inst->op == IR_ADD_OV   ? "add"
                         : inst->op == IR_SUB_OV ? "sub"
                                                 : "mul",
                         type, text_cstr(&a), text_cstr(&b));
        } else {
            r = field_of(e, type, pair, 0);
        }
        e->overflow = field_of(e, type, pair, 1);
        e->overflow_result = inst->result;
        text_format_name(value, sizeof value, "%%v%" PRIu32, r);
        llvm_emit_store_result(e, inst, value);
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
    llvm_emit_branch_weights(e, inst->b.as.index, inst->c.as.index);
    text_append(e->out, "\n");
}

/* Widen the operands of inst to twice the width, sext when is_signed,
   and multiply them there. Returns the product and writes its type to
   wide. */
static uint32_t wide_product(struct emitter *e, const struct ir_inst *inst,
                             bool is_signed, char *wide, size_t size)
{
    const char *type = llvm_emit_value_type(e, inst->type);
    struct text a = {0};
    struct text b = {0};
    uint32_t x;
    uint32_t y;
    uint32_t p = 0;

    text_format_name(wide, size, "i%u", 2 * width_of(inst->type));
    llvm_emit_operand(e, &inst->a, inst->type, &a);
    llvm_emit_operand(e, &inst->b, inst->type, &b);
    if (!e->failed && type != NULL) {
        x = llvm_emit_fresh(e);
        y = llvm_emit_fresh(e);
        p = llvm_emit_fresh(e);
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
    uint32_t r = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = trunc %s %%v%" PRIu32 " to %s\n",
                 r, wide, v, llvm_emit_value_type(e, inst->type));
    text_format_name(value, sizeof value, "%%v%" PRIu32, r);
    llvm_emit_store_result(e, inst, value);
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
    h = llvm_emit_fresh(e);
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
    uint32_t n = llvm_emit_fresh(e);

    text_format_name(name, sizeof name, "llvm.%s.%s", kind, wide);
    text_format_name(parameters, sizeof parameters, "%s, %s", wide, wide);
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
            text_format_name(bound, sizeof bound, "%" PRId64,
                             width == 64 ? INT64_MIN
                                    : -((int64_t)1 << (width - 1)));
            p = clamp(e, "smax", wide, p, bound);
            text_format_name(bound, sizeof bound, "%" PRIu64,
                             ((uint64_t)1 << (width - 1)) - 1);
            p = clamp(e, "smin", wide, p, bound);
        } else {
            text_format_name(bound, sizeof bound, "%" PRIu64,
                             width == 64 ? UINT64_MAX
                                    : ((uint64_t)1 << width) - 1);
            p = clamp(e, "umin", wide, p, bound);
        }
        narrow_result(e, inst, wide, p);
        return;
    }
    {
        const char *type = llvm_emit_value_type(e, inst->type);
        const char *kind = inst->op == IR_ADD_SAT_S   ? "sadd"
                           : inst->op == IR_ADD_SAT_U ? "uadd"
                           : inst->op == IR_SUB_SAT_S ? "ssub"
                                                      : "usub";
        struct text a = {0};
        struct text b = {0};
        char name[32];
        char parameters[24];
        char value[24];

        llvm_emit_operand(e, &inst->a, inst->type, &a);
        llvm_emit_operand(e, &inst->b, inst->type, &b);
        if (!e->failed && type != NULL) {
            text_format_name(name, sizeof name, "llvm.%s.sat.%s", kind, type);
            text_format_name(parameters, sizeof parameters, "%s, %s", type,
                             type);
            intrinsic(e, type, name, parameters);
            text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %s, %s "
                                 "%s)\n",
                         e->value, type, name, type, text_cstr(&a), type,
                         text_cstr(&b));
            text_format_name(value, sizeof value, "%%v%" PRIu32, e->value++);
            llvm_emit_store_result(e, inst, value);
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
    const char *lane = llvm_emit_value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    struct text dst = {0};
    struct text x = {0};
    char vector[48];
    uint32_t v;
    uint32_t r;
    size_t k;

    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, IR_PTR, &x);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        v = llvm_emit_fresh(e);
        r = llvm_emit_fresh(e);
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
    const char *lane = llvm_emit_value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    struct text dst = {0};
    struct text x = {0};
    char vector[48];
    uint32_t v;
    uint32_t r;
    size_t k;

    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, IR_PTR, &x);
    if (!e->failed && lane != NULL) {
        vector_of(e, inst, lane, vector, sizeof vector);
        v = llvm_emit_fresh(e);
        r = llvm_emit_fresh(e);
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
    const char *lane = llvm_emit_value_type(e, inst->type);
    uint64_t align = layout_align(e->l, inst->of);
    size_t lanes = e->m->aggs[inst->of.agg]->field_count;
    struct text dst = {0};
    struct text mask = {0};
    struct text x = {0};
    struct text y = {0};
    char vector[48];

    llvm_emit_operand(e, &inst->a, IR_PTR, &dst);
    llvm_emit_operand(e, &inst->b, IR_PTR, &mask);
    llvm_emit_operand(e, &inst->c, IR_PTR, &x);
    if (inst->arg_count == 1) {
        llvm_emit_operand(e, &inst->args[0], IR_PTR, &y);
    } else {
        unexpected(e, "a vselect without its second operand");
    }
    if (!e->failed && lane != NULL) {
        uint32_t bytes = llvm_emit_fresh(e);
        uint32_t bits = llvm_emit_fresh(e);
        uint32_t a = llvm_emit_fresh(e);
        uint32_t b = llvm_emit_fresh(e);
        uint32_t r = llvm_emit_fresh(e);
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
    uint32_t n = llvm_emit_fresh(e);
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
            v = llvm_emit_fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = fadd <%zu x %s> %%v%"
                                 PRIu32 ", %%v%" PRIu32 "\n",
                         v, half, lane, lo, hi);
        } else {
            uint32_t c = llvm_emit_fresh(e);
            v = llvm_emit_fresh(e);
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
    r = llvm_emit_fresh(e);
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
    const char *lane = llvm_emit_value_type(e, inst->type);
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
    llvm_emit_operand(e, &inst->a, IR_PTR, &x);
    if (e->failed || lane == NULL) {
        text_free(&x);
        return;
    }
    vector_of(e, inst, lane, vector, sizeof vector);
    v = llvm_emit_fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %" PRIu64
                         "\n",
                 v, vector, text_cstr(&x), layout_align(e->l, inst->of));
    if (fold == NULL) {
        v = halve(e, op, lane, lanes, v);
    } else {
        text_format_name(name, sizeof name, "llvm.vector.reduce.%s.v%zu%s",
                         fold, lanes, type_suffix(e, inst->type));
        intrinsic(e, lane, name, vector);
        text_appendf(e->out, "  %%v%" PRIu32 " = call %s @%s(%s %%v%" PRIu32
                             ")\n",
                     e->value, lane, name, vector, v);
        v = llvm_emit_fresh(e);
    }
    text_format_name(value, sizeof value, "%%v%" PRIu32, v);
    llvm_emit_store_result(e, inst, value);
    text_free(&x);
}

static void instruction(struct emitter *e, const struct ir_inst *inst)
{
    struct text value = {0};

    switch (inst->op) {
    case IR_COPY:
        llvm_emit_operand(e, &inst->a, inst->type, &value);
        if (!e->failed) {
            llvm_emit_store_result(e, inst, text_cstr(&value));
        }
        break;
    case IR_JUMP:
        text_appendf(e->out, "  br label %%b%" PRIu32 "\n", inst->a.as.index);
        break;
    case IR_UNREACHABLE:
        text_append(e->out, "  unreachable\n");
        break;
    /* DESIGN: the slot of the lifetime is the alloca the IR_SLOT of its
       temporary wrote, which the verifier checks, since the intrinsics
       take an alloca and no other pointer. */
    case IR_LIFE_START:
    case IR_LIFE_END: {
        const char *name = inst->op == IR_LIFE_START ? "llvm.lifetime.start.p0"
                                                     : "llvm.lifetime.end.p0";
        intrinsic(e, "void", name, "ptr");
        text_appendf(e->out, "  call void @%s(ptr %%s%" PRIu32 ")\n", name,
                     inst->a.as.temp);
        break;
    }
    case IR_BRANCH:
        llvm_emit_operand(e, &inst->a, IR_I8, &value);
        if (e->failed) {
            break;
        }
        text_appendf(e->out, "  %%v%" PRIu32 " = trunc i8 %s to i1\n",
                     e->value, text_cstr(&value));
        text_appendf(e->out, "  br i1 %%v%" PRIu32 ", label %%b%" PRIu32
                             ", label %%b%" PRIu32,
                     e->value++, inst->b.as.index, inst->c.as.index);
        llvm_emit_branch_weights(e, inst->b.as.index, inst->c.as.index);
        text_append(e->out, "\n");
        break;
    case IR_CALL:
        llvm_emit_call(e, inst);
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
            llvm_emit_return_aggregate(e, &inst->a);
            break;
        }
        if (inst->type == IR_VOID) {
            text_append(e->out, "  ret void\n");
            break;
        }
        llvm_emit_operand(e, &inst->a, inst->type, &value);
        if (!e->failed) {
            text_appendf(e->out, "  ret %s %s\n",
                         llvm_emit_value_type(e, inst->type),
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
        llvm_emit_operand(e, &inst->a, IR_PTR, &value);
        if (!e->failed) {
            llvm_emit_store_result(e, inst, text_cstr(&value));
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

/* Whether f is the main of the module that is compiled, which the
   runtime calls through its entry. */
bool llvm_emit_is_main(const struct emitter *e, const struct ir_function *f)
{
    return ir_is_main_of(f, e->o->module);
}

void llvm_emit_definition(struct emitter *e, const struct ir_function *f)
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
    llvm_emit_linkage(e, f);
    if (!llvm_emit_signature(e, f, true,
                             llvm_emit_is_main(e, f) ? &e->entry : NULL)) {
        return;
    }
    text_append(out, f->never_returns ? " noreturn cold" : "");
    /* main is noinline for the reason main_kept gives. */
    text_append(out, e->keeps_frame[f->index]
                         ? " noinline \"disable-tail-calls\"=\"true\" #0"
                     : llvm_emit_is_main(e, f) ? " noinline #0"
                                     : " #0");
    if (llvm_emit_link_once(e, f) &&
        target_info(e->o->target)->format == FORMAT_COFF) {
        text_append(out, " comdat");
    }
    llvm_debug_open(&e->debug, out, f,
                    !f->exported && !e->o->one_module && !e->o->exports);
    text_append(out, " {\nb0:\n");
    for (t = 0; t < f->temp_count && !e->failed; t++) {
        const char *name = llvm_emit_value_type(e, f->temps[t]);
        if (name != NULL) {
            text_appendf(out, "  %%t%" PRIu32 " = alloca %s, align %" PRIu64
                              "\n",
                         t, name, llvm_emit_align_of(e, f->temps[t]));
        }
    }
    /* The allocas of the slots and the copies come after the ones of the
       temporaries, and the body after all of them, in the entry block. */
    e->out = &body;
    e->allocas = &allocas;
    llvm_emit_classify(e, f, &e->signature);
    llvm_emit_prologue(e, f, &e->signature);
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
    llvm_debug_spans_free(&spans);
    free(e->signature.params);
    e->signature.params = NULL;
}
