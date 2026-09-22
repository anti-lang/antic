#include "expand.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* DESIGN: a saturating operation becomes the plain operation and a test
   whether it left the range. A choice between its result and the bound
   it passed follows. The choice is a mask of all ones or of zero, so no branch is
   taken. The test reads the operands and the wrapped result, as the
   carry and the overflow flag of the processor do. Lowering writes one
   operation, and its expansion waits for the back end. c_long has its
   width and c_wchar its signedness there alone. */

struct expander {
    struct ir_function *f;
    struct ir_block *out;
    enum ir_type type;
};

static int bits(enum ir_type type)
{
    return type == IR_I8 ? 8 : type == IR_I16 ? 16 : type == IR_I32 ? 32 : 64;
}

static struct ir_operand temp(const struct expander *x, uint32_t t)
{
    return ir_temp_op(x->f, t);
}

static struct ir_operand constant(const struct expander *x, uint64_t v)
{
    int n = bits(x->type);

    return ir_int_op(x->type, n == 64 ? v : v & (((uint64_t)1 << n) - 1));
}

static struct ir_operand binary(struct expander *x, enum ir_op op,
                                struct ir_operand a, struct ir_operand b)
{
    return temp(x, ir_binary(x->f, x->out, op, x->type, a, b));
}

static struct ir_operand compare(struct expander *x, enum ir_op op,
                                 struct ir_operand a, struct ir_operand b)
{
    return temp(x, ir_binary(x->f, x->out, op, IR_I8, a, b));
}

/* All ones when the bool b is 1, and zero when it is 0. */
static struct ir_operand mask_of(struct expander *x, struct ir_operand b)
{
    struct ir_operand wide =
        x->type == IR_I8
            ? b
            : temp(x, ir_unary(x->f, x->out, IR_ZEXT, x->type, b));

    return temp(x, ir_unary(x->f, x->out, IR_NEG, x->type, wide));
}

/* All ones when v is negative, and zero otherwise. */
static struct ir_operand sign_of(struct expander *x, struct ir_operand v)
{
    return binary(x, IR_SHR_S, v, constant(x, (uint64_t)bits(x->type) - 1));
}

/* Write op a, b into the temporary result of the expanded instruction. */
static void finish(struct expander *x, uint32_t result, enum ir_op op,
                   struct ir_operand a, struct ir_operand b)
{
    struct ir_inst inst;

    memset(&inst, 0, sizeof inst);
    inst.op = op;
    inst.type = x->type;
    inst.line = x->f->at_line;
    inst.result = result;
    inst.a = a;
    inst.b = b;
    inst.of = ir_scalar(IR_VOID);
    ir_inst_add(x->out, &inst);
}

/* Whether a op b left the range of the signed type. The operands and the
   wrapped result r tell: the sign of r disagrees with the signs that the
   operands give it. */
static struct ir_operand signed_overflow(struct expander *x, enum ir_op op,
                                         struct ir_operand a,
                                         struct ir_operand b,
                                         struct ir_operand r)
{
    struct ir_operand p;
    struct ir_operand q;
    struct ir_operand both;

    if (op == IR_MUL) {
        struct ir_operand high = binary(x, IR_MULH_S, a, b);
        struct ir_operand sign = sign_of(x, r);
        return compare(x, IR_NE, high, sign);
    }
    p = binary(x, IR_XOR, a, op == IR_ADD ? r : b);
    q = binary(x, IR_XOR, op == IR_ADD ? b : a, r);
    both = binary(x, IR_AND, p, q);
    return compare(x, IR_SLT, both, constant(x, 0));
}

/* Whether a op b left the range of the unsigned type. That is a carry
   out of the sum, a borrow of the difference or a product above the
   type. */
static struct ir_operand unsigned_overflow(struct expander *x, enum ir_op op,
                                           struct ir_operand a,
                                           struct ir_operand b,
                                           struct ir_operand r)
{
    if (op == IR_MUL) {
        struct ir_operand high = binary(x, IR_MULH_U, a, b);
        return compare(x, IR_NE, high, constant(x, 0));
    }
    return op == IR_ADD ? compare(x, IR_ULT, r, a) : compare(x, IR_ULT, a, b);
}

/* The saturating operation inst. An unsigned one sets every bit above
   the maximum or clears every bit below zero. A signed one takes the
   maximum or the minimum by the sign that the exact result has. */
static void saturate(struct expander *x, const struct ir_inst *inst)
{
    bool is_signed = inst->op == IR_ADD_SAT_S || inst->op == IR_SUB_SAT_S ||
                     inst->op == IR_MUL_SAT_S;
    enum ir_op op = inst->op == IR_ADD_SAT_S || inst->op == IR_ADD_SAT_U
                        ? IR_ADD
                    : inst->op == IR_SUB_SAT_S || inst->op == IR_SUB_SAT_U
                        ? IR_SUB
                        : IR_MUL;
    struct ir_operand r = binary(x, op, inst->a, inst->b);
    struct ir_operand out;
    struct ir_operand mask;
    struct ir_operand sign;
    struct ir_operand bound;
    struct ir_operand differ;
    struct ir_operand choice;

    if (!is_signed) {
        out = unsigned_overflow(x, op, inst->a, inst->b, r);
        mask = mask_of(x, out);
        if (op == IR_SUB) {
            struct ir_operand keep =
                binary(x, IR_XOR, mask, constant(x, UINT64_MAX));
            finish(x, inst->result, IR_AND, r, keep);
        } else {
            finish(x, inst->result, IR_OR, r, mask);
        }
        return;
    }
    /* The exact result has the sign of a after + and -, since the two
       operands of an overflow agree in sign for + and disagree for -.
       A product has the sign that the two operands give it. */
    out = signed_overflow(x, op, inst->a, inst->b, r);
    mask = mask_of(x, out);
    sign = sign_of(x, op == IR_MUL ? binary(x, IR_XOR, inst->a, inst->b)
                                   : inst->a);
    bound = binary(x, IR_XOR, sign,
                   constant(x, ((uint64_t)1 << (bits(x->type) - 1)) - 1));
    differ = binary(x, IR_XOR, r, bound);
    choice = binary(x, IR_AND, differ, mask);
    finish(x, inst->result, IR_XOR, r, choice);
}

/* A bool as a value of the type, 0 or 1. */
static struct ir_operand widen_bool(struct expander *x, struct ir_operand b)
{
    return x->type == IR_I8
               ? b
               : temp(x, ir_unary(x->f, x->out, IR_ZEXT, x->type, b));
}

static struct ir_operand both(struct expander *x, struct ir_operand p,
                              struct ir_operand q)
{
    return temp(x, ir_binary(x->f, x->out, IR_AND, IR_I8, p, q));
}

/* The bit that a shift by n moved out last: bit n - 1 of a moves out
   right, and the top bit of a << (n - 1) left. A count of 0 moves none.
   n - 1 of a count of 0 is a count out of the width. Its shift gives what
   the processor gives, and the test of n drops that bit. */
static struct ir_operand last_bit(struct expander *x, bool left,
                                  struct ir_operand a, struct ir_operand n)
{
    struct ir_operand moved = compare(x, IR_NE, n, constant(x, 0));
    struct ir_operand less = binary(x, IR_SUB, n, constant(x, 1));
    struct ir_operand shifted = binary(x, left ? IR_SHL : IR_SHR_U, a, less);
    struct ir_operand bit;

    if (left) {
        bit = compare(x, IR_SLT, shifted, constant(x, 0));
    } else {
        struct ir_operand low = binary(x, IR_AND, shifted, constant(x, 1));
        bit = compare(x, IR_NE, low, constant(x, 0));
    }
    return both(x, moved, bit);
}

/* The flag that a portable sequence gives for flag of the operation inst,
   whose result is r. c is the carry or the borrow in, or none. */
static struct ir_operand flag_of(struct expander *x, const struct ir_inst *inst,
                                 enum ir_flag flag, struct ir_operand r)
{
    struct ir_operand a = inst->a;
    struct ir_operand b = inst->b;
    struct ir_operand c = inst->c;

    if (flag == IR_FLAG_ZERO) {
        return compare(x, IR_EQ, r, constant(x, 0));
    }
    if (flag == IR_FLAG_NEGATIVE) {
        return compare(x, IR_SLT, r, constant(x, 0));
    }
    switch (inst->op) {
    case IR_ADD_FL:
    case IR_SUB_FL: {
        bool add = inst->op == IR_ADD_FL;
        struct ir_operand below;
        struct ir_operand equal;
        if (flag == IR_FLAG_OVERFLOW) {
            return signed_overflow(x, add ? IR_ADD : IR_SUB, a, b, r);
        }
        /* A carry leaves a sum below a, or at a with a carry in. A
           borrow takes b above a, or equal to a with a borrow in. */
        below = add ? compare(x, IR_ULT, r, a) : compare(x, IR_ULT, a, b);
        if (c.kind == IR_NONE) {
            return below;
        }
        equal = add ? compare(x, IR_EQ, r, a) : compare(x, IR_EQ, a, b);
        return temp(x, ir_binary(x->f, x->out, IR_OR, IR_I8, below,
                                 both(x, c, equal)));
    }
    case IR_NEG_FL:
        if (flag == IR_FLAG_OVERFLOW) {
            struct ir_operand sign = binary(x, IR_AND, a, r);
            return compare(x, IR_SLT, sign, constant(x, 0));
        }
        return compare(x, IR_NE, a, constant(x, 0));
    case IR_MUL_FL:
        return flag == IR_FLAG_OVERFLOW
                   ? signed_overflow(x, IR_MUL, a, b, r)
                   : unsigned_overflow(x, IR_MUL, a, b, r);
    case IR_SHL_FL:
        if (flag == IR_FLAG_OVERFLOW) {
            struct ir_operand back = binary(x, IR_SHR_S, r, b);
            return compare(x, IR_NE, back, a);
        }
        return last_bit(x, true, a, b);
    default:
        if (flag == IR_FLAG_OVERFLOW) {
            return ir_int_op(IR_I8, 0);
        }
        return last_bit(x, false, a, b);
    }
}

/* The flag operation at b->insts[at] and its reads, which follow it, as
   plain operations. The result of the operation may be one of its
   operands, so it is computed into a new temporary and goes to its own
   last. Returns the count of the reads. */
static size_t flags(struct expander *x, const struct ir_block *b, size_t at)
{
    static const enum ir_op plain[] = {
        [IR_ADD_FL] = IR_ADD, [IR_SUB_FL] = IR_SUB, [IR_MUL_FL] = IR_MUL,
        [IR_SHL_FL] = IR_SHL, [IR_SHR_S_FL] = IR_SHR_S,
        [IR_SHR_U_FL] = IR_SHR_U,
    };
    const struct ir_inst *inst = &b->insts[at];
    struct ir_operand r;
    size_t count = 0;

    if (inst->op == IR_NEG_FL) {
        r = temp(x, ir_unary(x->f, x->out, IR_NEG, x->type, inst->a));
    } else {
        r = binary(x, plain[inst->op], inst->a, inst->b);
    }
    if (inst->c.kind != IR_NONE) {
        struct ir_operand one = widen_bool(x, inst->c);
        r = binary(x, inst->op == IR_ADD_FL ? IR_ADD : IR_SUB, r, one);
    }
    while (at + 1 + count < b->count &&
           b->insts[at + 1 + count].op == IR_FLAG) {
        const struct ir_inst *read = &b->insts[at + 1 + count];
        struct ir_operand value =
            flag_of(x, inst, (enum ir_flag)read->field, r);
        ir_assign(x->f, x->out, read->result, value);
        count++;
    }
    ir_assign(x->f, x->out, inst->result, r);
    return count;
}

static bool is_saturating(enum ir_op op)
{
    return op == IR_ADD_SAT_S || op == IR_ADD_SAT_U || op == IR_SUB_SAT_S ||
           op == IR_SUB_SAT_U || op == IR_MUL_SAT_S || op == IR_MUL_SAT_U;
}

static bool is_flag_operation(enum ir_op op)
{
    return op == IR_ADD_FL || op == IR_SUB_FL || op == IR_MUL_FL ||
           op == IR_SHL_FL || op == IR_SHR_S_FL || op == IR_SHR_U_FL ||
           op == IR_NEG_FL;
}

/* Whether inst is an operation that this pass expands. */
static bool expands(const struct ir_inst *inst,
                    bool (*native)(const struct ir_inst *inst))
{
    return is_saturating(inst->op) ||
           (is_flag_operation(inst->op) && !native(inst));
}

static bool expand_block(struct ir_function *f, struct ir_block *b,
                         bool (*native)(const struct ir_inst *inst))
{
    struct ir_block out;
    struct expander x;
    bool changed = false;
    size_t i;
    size_t k;

    for (i = 0; i < b->count && !changed; i++) {
        changed = expands(&b->insts[i], native);
    }
    if (!changed) {
        return false;
    }
    memset(&out, 0, sizeof out);
    x.f = f;
    x.out = &out;
    for (i = 0; i < b->count; i++) {
        const struct ir_inst *inst = &b->insts[i];
        size_t reads = 0;
        f->at_line = inst->line;
        x.type = inst->type;
        if (is_saturating(inst->op)) {
            saturate(&x, inst);
        } else if (expands(inst, native)) {
            reads = flags(&x, b, i);
        } else {
            ir_inst_add(&out, inst);
        }
        for (k = 0; k <= reads; k++) {
            free(b->insts[i + k].args);
        }
        i += reads;
    }
    free(b->insts);
    b->insts = out.insts;
    b->count = out.count;
    b->capacity = out.capacity;
    return true;
}

bool expand_function(struct ir_function *f,
                     bool (*native)(const struct ir_inst *inst))
{
    uint32_t line = f->at_line;
    bool changed = false;
    size_t b;

    for (b = 0; b < f->block_count; b++) {
        changed = expand_block(f, f->blocks[b], native) || changed;
    }
    f->at_line = line;
    return changed;
}
