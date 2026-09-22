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

static bool is_saturating(enum ir_op op)
{
    return op == IR_ADD_SAT_S || op == IR_ADD_SAT_U || op == IR_SUB_SAT_S ||
           op == IR_SUB_SAT_U || op == IR_MUL_SAT_S || op == IR_MUL_SAT_U;
}

static bool expand_block(struct ir_function *f, struct ir_block *b)
{
    struct ir_block out;
    struct expander x;
    bool changed = false;
    size_t i;

    for (i = 0; i < b->count && !changed; i++) {
        changed = is_saturating(b->insts[i].op);
    }
    if (!changed) {
        return false;
    }
    memset(&out, 0, sizeof out);
    x.f = f;
    x.out = &out;
    for (i = 0; i < b->count; i++) {
        const struct ir_inst *inst = &b->insts[i];
        if (is_saturating(inst->op)) {
            f->at_line = inst->line;
            x.type = inst->type;
            saturate(&x, inst);
        } else {
            ir_inst_add(&out, inst);
        }
        free(b->insts[i].args);
    }
    free(b->insts);
    b->insts = out.insts;
    b->count = out.count;
    b->capacity = out.capacity;
    return true;
}

bool expand_function(struct ir_function *f)
{
    uint32_t line = f->at_line;
    bool changed = false;
    size_t b;

    for (b = 0; b < f->block_count; b++) {
        changed = expand_block(f, f->blocks[b]) || changed;
    }
    f->at_line = line;
    return changed;
}
