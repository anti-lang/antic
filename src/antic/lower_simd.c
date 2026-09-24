#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: an operation on a simd struct below the vector cap is one simd
   operation of the IR. The back end maps it to the native width. The IR
   has no operation on an f16 lane, no integer division and no shift of
   lanes. Lowering writes each of those as one scalar operation per lane,
   with the checks of the scalar operator. A simd struct above the cap is
   an array, and every operation on it is a loop over its lanes. */

/* Whether the operations on the simd struct t are loops over its lanes. */
static bool simd_loops(const struct type *t)
{
    return type_simd_bytes(t) > CPU_VECTOR_BYTE_CAP;
}

/* A new slot for a value of type t, in the entry block. */
static struct ir_operand new_value(struct lowerer *l, const struct type *t)
{
    return lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, t)));
}

/* The type that an operation on a lane of type lane computes in. An f16
   lane is an f32 there. */
static enum ir_type lane_math(const struct type *lane)
{
    return lane->kind == TYPE_F16 ? IR_F32 : lower_ir_type_of(lane);
}

/* The operation of one lane that the operator op gives on lanes of type
   lane. An f16 lane takes the operation of an f32. */
static enum ir_op simd_lane_op(enum token_kind op, const struct type *lane)
{
    if (lane->kind != TYPE_F16) {
        return lower_binary_op(op, lane);
    }
    switch (op) {
    case TOKEN_PLUS: return IR_FADD;
    case TOKEN_MINUS: return IR_FSUB;
    case TOKEN_STAR: return IR_FMUL;
    case TOKEN_SLASH: return IR_FDIV;
    case TOKEN_EQ: return IR_FEQ;
    case TOKEN_NE: return IR_FNE;
    case TOKEN_LT: return IR_FLT;
    case TOKEN_LE: return IR_FLE;
    case TOKEN_GT: return IR_FGT;
    default: return IR_FGE;
    }
}

/* The lanes of one operation written out, or one loop over them for a
   simd struct above the cap. */
struct lanes {
    size_t count;               /* the lanes written out, 1 for a loop */
    bool loop;
    uint32_t index;             /* the lane of the loop, an i64 */
    struct ir_block *head;
    struct ir_block *exit;
};

static void lanes_begin(struct lowerer *l, struct lanes *c, size_t count,
                        bool loop)
{
    struct ir_block *body;
    struct ir_operand more;

    memset(c, 0, sizeof *c);
    c->loop = loop;
    c->count = loop ? 1 : count;
    if (!loop) {
        return;
    }
    c->index = ir_temp(l->f, IR_I64);
    ir_assign(l->f, l->b, c->index, lower_zero());
    l->loop_depth++;
    c->head = lower_new_block(l);
    body = lower_new_block(l);
    l->loop_depth--;
    c->exit = lower_new_block(l);
    l->loop_depth++;
    ir_jump(l->f, l->b, c->head);
    l->b = c->head;
    more = lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8,
                                   lower_temp(l, c->index),
                                   ir_int_op(IR_I64, count)));
    ir_branch(l->f, l->b, more, body, c->exit);
    l->b = body;
}

static void lanes_end(struct lowerer *l, struct lanes *c)
{
    struct ir_operand next;

    if (!c->loop) {
        return;
    }
    next = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                   lower_temp(l, c->index),
                                   ir_int_op(IR_I64, 1)));
    ir_assign(l->f, l->b, c->index, next);
    ir_jump(l->f, l->b, c->head);
    l->loop_depth--;
    l->b = c->exit;
}

/* The address of a lane of the value at base of the simd struct t. It is
   lane k written out, or the lane of the loop plus k. */
static struct ir_operand lane_at(struct lowerer *l, const struct lanes *c,
                                 const struct type *t, struct ir_operand base,
                                 size_t k)
{
    struct ir_operand index;
    struct ir_operand offset;

    if (!c->loop) {
        return lower_offset_address(l, base,
                                    lower_field_offset(l, t,
                                                       &t->fields[k].name));
    }
    index = k == 0 ? lower_temp(l, c->index)
                   : lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                             lower_temp(l, c->index),
                                             ir_int_op(IR_I64, k)));
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64, index,
                                     lower_size_operand(l, type_simd_lane(t))));
    return lower_temp(l, ir_ptradd(l->f, l->b, base, offset));
}

/* The value of a lane of type lane at the address at, an f16 read as the
   f32 it holds. */
static struct ir_operand read_lane(struct lowerer *l, const struct type *lane,
                                   struct ir_operand at)
{
    struct ir_operand v = lower_temp(l,
                                     ir_load(l->f, l->b, lower_ir_type_of(lane),
                                             at));

    if (lane->kind == TYPE_F16) {
        v = lower_temp(l, ir_unary(l->f, l->b, IR_HEXT, IR_F32, v));
    }
    return v;
}

/* Write the value v into a lane of type lane at the address at, an f32
   rounded to an f16 lane. */
static void write_lane(struct lowerer *l, const struct type *lane,
                       struct ir_operand v, struct ir_operand at)
{
    if (lane->kind == TYPE_F16) {
        v = lower_temp(l, ir_unary(l->f, l->b, IR_HTRUNC, IR_I16, v));
    }
    ir_store(l->f, l->b, lower_ir_type_of(lane), v, at);
}

/* The address of the value e of a simd struct, which a built-in may take
   through a pointer. */
static struct ir_operand simd_value(struct lowerer *l, const struct expr *e)
{
    return e->type->kind == TYPE_POINTER ? lower_expr(l, e)
                                         : lower_address(l, e);
}

/* `a op b` on two values of a simd struct. A comparison writes the mask. */
struct ir_operand lower_simd_binary(struct lowerer *l,
                                    const struct expr *e)
{
    enum token_kind op = e->as.binary.op;
    const struct type *t = e->as.binary.left->type;
    const struct type *lane = type_simd_lane(t);
    bool checked = type_is_integer(lane) &&
                   (op == TOKEN_SLASH || op == TOKEN_SHL || op == TOKEN_SHR);
    struct ir_operand x = lower_address(l, e->as.binary.left);
    struct ir_operand y = lower_address(l, e->as.binary.right);
    struct ir_operand dst;
    struct lanes c;
    size_t k;

    dst = new_value(l, e->type);
    if (!simd_loops(t) && lane->kind != TYPE_F16 && !checked) {
        ir_vbinary(l->f, l->b, simd_lane_op(op, lane), lower_ir_type_of(lane),
                   dst,
                   x, y, lower_agg_of(l, t));
        return dst;
    }
    lanes_begin(l, &c, t->field_count, simd_loops(t));
    for (k = 0; k < c.count; k++) {
        struct ir_operand a = read_lane(l, lane, lane_at(l, &c, t, x, k));
        struct ir_operand b = read_lane(l, lane, lane_at(l, &c, t, y, k));
        struct ir_operand r;
        if (checked) {
            lower_binary_checks(l, op, lane, a, b, e->pos.line);
        }
        r = lower_temp(l, ir_binary(l->f, l->b, simd_lane_op(op, lane),
                                    lower_is_comparison(op) ? IR_I8
                                                            : lane_math(lane),
                                    a, b));
        if (lower_is_comparison(op)) {
            ir_store(l->f, l->b, IR_I8, r, lane_at(l, &c, e->type, dst, k));
        } else {
            write_lane(l, lane, r, lane_at(l, &c, t, dst, k));
        }
    }
    lanes_end(l, &c);
    return dst;
}

/* `-v` and `~v` on a value of a simd struct. */
struct ir_operand lower_simd_unary(struct lowerer *l,
                                   const struct expr *e)
{
    const struct type *t = e->type;
    const struct type *lane = type_simd_lane(t);
    enum ir_op op = e->as.unary.op == TOKEN_TILDE ? IR_NOT
                    : lane_math(lane) == IR_F32 || lane_math(lane) == IR_F64
                        ? IR_FNEG
                        : IR_NEG;
    struct ir_operand x = lower_address(l, e->as.unary.operand);
    struct ir_operand dst;
    struct lanes c;
    size_t k;

    dst = new_value(l, t);
    if (!simd_loops(t) && lane->kind != TYPE_F16) {
        ir_vunary(l->f, l->b, op, lower_ir_type_of(lane), dst, x,
                  lower_agg_of(l, t));
        return dst;
    }
    lanes_begin(l, &c, t->field_count, simd_loops(t));
    for (k = 0; k < c.count; k++) {
        struct ir_operand a = read_lane(l, lane, lane_at(l, &c, t, x, k));
        struct ir_operand r =
            lower_temp(l, ir_unary(l->f, l->b, op, lane_math(lane), a));
        write_lane(l, lane, r, lane_at(l, &c, t, dst, k));
    }
    lanes_end(l, &c);
    return dst;
}

/* `as` between a simd struct and an array or a plain struct copies the
   bytes into a value of the target type. */
struct ir_operand lower_simd_cast(struct lowerer *l,
                                  const struct expr *e)
{
    struct ir_operand src = lower_address(l, e->as.cast.operand);
    struct ir_operand dst;

    dst = new_value(l, e->type);
    ir_memcopy(l->f, l->b, dst, src, lower_vtype_of(l, e->type));
    return dst;
}

/* The address of the lanes of `load` and `store`: element index of the
   slice at slice onward. The dev-mode check covers the first and the
   last of the count elements, and the message names the element that
   lies outside. */
static struct ir_operand simd_elements(struct lowerer *l, const struct expr *e,
                                       const struct expr *slice_expr,
                                       struct ir_operand slice,
                                       struct ir_operand index, size_t count)
{
    const struct type *lane = type_simd_lane(e->as.simd.simd);
    const struct ir_global *text =
        lower_check_text(l, e->pos.line, "index out of bounds");
    struct ir_operand length = lower_slice_length(l, slice, slice_expr->type);
    struct ir_operand base = lower_temp(l, ir_load(l->f, l->b, IR_PTR, slice));
    struct ir_operand ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, index, length));
    struct ir_operand offset;

    lower_check_branch(l, ok, false, text, CHECK_BOUNDS, index, length, NULL);
    if (count > 1) {
        struct ir_operand room =
            lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64, length, index));
        struct ir_operand last =
            lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64, index,
                                    ir_int_op(IR_I64, count - 1)));
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8,
                                     ir_int_op(IR_I64, count - 1), room));
        lower_check_branch(l, ok, false, text, CHECK_BOUNDS, last, length,
                           NULL);
    }
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64, index,
                                     lower_size_operand(l, lane)));
    return lower_temp(l, ir_ptradd(l->f, l->b, base, offset));
}

/* The operation of one lane that folds lanes of type lane for op. It is
   the sum, a less-than for the least or a greater-than for the
   greatest. */
static enum ir_op fold_op(enum simd_op op, const struct type *lane)
{
    bool is_float = lane_math(lane) == IR_F32 || lane_math(lane) == IR_F64;

    switch (op) {
    case SIMD_OP_MIN:
        return is_float ? IR_FLT : type_is_signed(lane) ? IR_SLT : IR_ULT;
    case SIMD_OP_MAX:
        return is_float ? IR_FGT : type_is_signed(lane) ? IR_SGT : IR_UGT;
    default:
        return is_float ? IR_FADD : IR_ADD;
    }
}

/* DESIGN: a sum, a least and a greatest fold the upper half of the
   lanes onto the lower half until one lane is left. Every target folds
   in that order, which splits a wide vector at its native width. A float
   sum therefore gives one result everywhere. The least keeps the upper
   lane where it is less than the lower one, and the lower lane
   otherwise. The greatest keeps it where it is greater. Here the order
   is written out. The lanes go to a scratch array of the type the
   operation computes in, as products for a dot. Each half folds in a
   loop. */
static struct ir_operand fold_lanes(struct lowerer *l, enum simd_op op,
                                    const struct type *t, struct ir_operand x,
                                    struct ir_operand y)
{
    const struct type *lane = type_simd_lane(t);
    enum ir_type math = lane_math(lane);
    enum ir_op fold = fold_op(op, lane);
    struct text name = {0};
    char length[24];
    struct ir_operand scratch;
    struct ir_operand result;
    struct lanes c;
    size_t half;
    uint32_t agg;

    text_appendf(&name, "[%zu]%s", t->field_count, ir_type_name(math));
    snprintf(length, sizeof length, "%zu", t->field_count);
    agg = ir_array_add(l->m, text_cstr(&name), ir_scalar(math),
                       ir_sym_int(l->m, IR_I64, t->field_count), length);
    text_free(&name);
    scratch = lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));
    lanes_begin(l, &c, t->field_count, true);
    {
        struct ir_operand at = lane_at(l, &c, t, x, 0);
        struct ir_operand v = read_lane(l, lane, at);
        struct ir_operand to;
        if (y.kind != IR_NONE) {
            struct ir_operand w = read_lane(l, lane, lane_at(l, &c, t, y, 0));
            v = lower_temp(l, ir_binary(l->f, l->b,
                                        fold_op(SIMD_OP_SUM, lane) == IR_FADD
                                            ? IR_FMUL
                                            : IR_MUL,
                                        math, v, w));
        }
        to = lower_temp(l, ir_ptradd(
                               l->f, l->b, scratch,
                               lower_temp(l, ir_binary(
                                                 l->f, l->b, IR_MUL, IR_I64,
                                                 lower_temp(l, c.index),
                                                 ir_sym_operand(
                                                     l->m,
                                                     ir_sym_size_of(
                                                         l->m,
                                                         ir_scalar(math)))))));
        ir_store(l->f, l->b, math, v, to);
    }
    lanes_end(l, &c);
    for (half = t->field_count / 2; half >= 1; half /= 2) {
        struct ir_operand size =
            ir_sym_operand(l->m, ir_sym_size_of(l->m, ir_scalar(math)));
        struct ir_operand low_at;
        struct ir_operand high_at;
        struct ir_operand low;
        struct ir_operand high;
        lanes_begin(l, &c, half, true);
        low_at = lower_temp(l, ir_ptradd(
                                   l->f, l->b, scratch,
                                   lower_temp(l, ir_binary(
                                                     l->f, l->b, IR_MUL, IR_I64,
                                                     lower_temp(l, c.index),
                                                     size))));
        high_at = lower_temp(l, ir_ptradd(
                                    l->f, l->b, low_at,
                                    lower_temp(l, ir_binary(
                                                      l->f, l->b, IR_MUL,
                                                      IR_I64,
                                                      ir_int_op(IR_I64, half),
                                                      size))));
        low = lower_temp(l, ir_load(l->f, l->b, math, low_at));
        high = lower_temp(l, ir_load(l->f, l->b, math, high_at));
        if (op == SIMD_OP_SUM || op == SIMD_OP_DOT) {
            ir_store(l->f, l->b, math,
                     lower_temp(l,
                                ir_binary(l->f, l->b, fold, math, low, high)),
                     low_at);
        } else {
            struct ir_block *take = lower_new_block(l);
            struct ir_block *keep = lower_new_block(l);
            ir_branch(l->f, l->b,
                      lower_temp(l,
                                 ir_binary(l->f, l->b, fold, IR_I8, high, low)),
                      take, keep);
            l->b = take;
            ir_store(l->f, l->b, math, high, low_at);
            ir_jump(l->f, l->b, keep);
            l->b = keep;
        }
        lanes_end(l, &c);
    }
    result = lower_temp(l, ir_load(l->f, l->b, math, scratch));
    /* The value of f16 lanes goes back to an f16, which reads as an f32. */
    if (lane->kind == TYPE_F16) {
        result = lower_temp(l, ir_unary(l->f, l->b, IR_HTRUNC, IR_I16, result));
        result = lower_temp(l, ir_unary(l->f, l->b, IR_HEXT, IR_F32, result));
    }
    return result;
}

/* A built-in of a simd struct or a function of `anti.simd`. */
struct ir_operand lower_simd(struct lowerer *l, const struct expr *e)
{
    const struct type *t = e->as.simd.simd;
    const struct type *lane = type_simd_lane(t);
    struct expr *const *args = e->as.simd.args;
    enum ir_type bits = lower_ir_type_of(lane);
    bool loops = simd_loops(t);
    struct ir_operand x;
    struct ir_operand y;
    struct ir_operand z;
    struct ir_operand dst;
    struct lanes c;
    size_t k;

    switch (e->as.simd.op) {
    case SIMD_OP_SPLAT:
        x = lower_expr(l, args[0]);
        dst = new_value(l, t);
        if (!loops) {
            ir_vsplat(l->f, l->b, bits, dst, x, lower_agg_of(l, t));
            return dst;
        }
        lanes_begin(l, &c, t->field_count, true);
        ir_store(l->f, l->b, bits, x, lane_at(l, &c, t, dst, 0));
        lanes_end(l, &c);
        return dst;
    case SIMD_OP_LOAD:
        x = lower_address(l, args[0]);
        y = lower_expr(l, args[1]);
        z = simd_elements(l, e, args[0], x, y, t->field_count);
        dst = new_value(l, t);
        ir_memcopy(l->f, l->b, dst, z, lower_vtype_of(l, t));
        return dst;
    case SIMD_OP_STORE:
        dst = simd_value(l, args[0]);
        x = lower_address(l, args[1]);
        y = lower_expr(l, args[2]);
        z = simd_elements(l, e, args[1], x, y, t->field_count);
        ir_memcopy(l->f, l->b, z, dst, lower_vtype_of(l, t));
        return lower_none();
    case SIMD_OP_SHUFFLE:
        x = simd_value(l, args[0]);
        dst = new_value(l, t);
        if (!loops) {
            ir_vshuffle(l->f, l->b, bits, dst, x, e->as.simd.lanes,
                        t->field_count, lower_agg_of(l, t));
            return dst;
        }
        lanes_begin(l, &c, t->field_count, false);
        for (k = 0; k < c.count; k++) {
            struct ir_operand v = lower_temp(
                l, ir_load(l->f, l->b, bits,
                           lane_at(l, &c, t, x, e->as.simd.lanes[k])));
            ir_store(l->f, l->b, bits, v, lane_at(l, &c, t, dst, k));
        }
        lanes_end(l, &c);
        return dst;
    case SIMD_OP_SUM:
    case SIMD_OP_MIN:
    case SIMD_OP_MAX:
        x = simd_value(l, args[0]);
        if (loops || lane->kind == TYPE_F16) {
            return fold_lanes(l, e->as.simd.op, t, x, lower_none());
        }
        return lower_temp(l, ir_vreduce(l->f, l->b,
                                        fold_op(e->as.simd.op, lane), bits, x,
                                        lower_agg_of(l, t)));
    case SIMD_OP_DOT:
        x = simd_value(l, args[0]);
        y = lower_address(l, args[1]);
        if (loops || lane->kind == TYPE_F16) {
            return fold_lanes(l, SIMD_OP_DOT, t, x, y);
        }
        dst = new_value(l, t);
        ir_vbinary(l->f, l->b, lane_math(lane) == IR_F32 ||
                                       lane_math(lane) == IR_F64
                                   ? IR_FMUL
                                   : IR_MUL,
                   bits, dst, x, y, lower_agg_of(l, t));
        return lower_temp(l, ir_vreduce(l->f, l->b, fold_op(SIMD_OP_SUM, lane),
                                        bits, dst, lower_agg_of(l, t)));
    case SIMD_OP_SELECT: {
        const struct type *mask = args[0]->type;
        z = lower_address(l, args[0]);
        x = lower_address(l, args[1]);
        y = lower_address(l, args[2]);
        dst = new_value(l, t);
        if (!loops) {
            ir_vselect(l->f, l->b, bits, dst, z, x, y, lower_agg_of(l, t));
            return dst;
        }
        /* A lane of the mask is 0 or 1, and its negation is a mask of
           the bits of a lane, which chooses without a branch. */
        {
            enum ir_type word = lane->kind == TYPE_F32   ? IR_I32
                                : lane->kind == TYPE_F64 ? IR_I64
                                                         : bits;
            struct ir_operand m;
            struct ir_operand a;
            struct ir_operand b;
            struct ir_operand r;
            lanes_begin(l, &c, t->field_count, true);
            m = lower_temp(l, ir_load(l->f, l->b, IR_I8,
                                      lane_at(l, &c, mask, z, 0)));
            if (word != IR_I8) {
                m = lower_temp(l, ir_unary(l->f, l->b, IR_ZEXT, word, m));
            }
            m = lower_temp(l, ir_unary(l->f, l->b, IR_NEG, word, m));
            a = lower_temp(l,
                           ir_load(l->f, l->b, word, lane_at(l, &c, t, x, 0)));
            b = lower_temp(l,
                           ir_load(l->f, l->b, word, lane_at(l, &c, t, y, 0)));
            a = lower_temp(l, ir_binary(l->f, l->b, IR_AND, word, a, m));
            m = lower_temp(l, ir_unary(l->f, l->b, IR_NOT, word, m));
            b = lower_temp(l, ir_binary(l->f, l->b, IR_AND, word, b, m));
            r = lower_temp(l, ir_binary(l->f, l->b, IR_OR, word, a, b));
            ir_store(l->f, l->b, word, r, lane_at(l, &c, t, dst, 0));
            lanes_end(l, &c);
        }
        return dst;
    }
    default: {
        /* `simd.any` and `simd.all` fold the bytes of a mask. */
        enum ir_op fold = e->as.simd.op == SIMD_OP_ANY ? IR_OR : IR_AND;
        uint32_t acc;
        x = lower_address(l, args[0]);
        if (!loops) {
            return lower_temp(l, ir_vreduce(l->f, l->b, fold, IR_I8, x,
                                            lower_agg_of(l, t)));
        }
        acc = ir_temp(l->f, IR_I8);
        ir_assign(l->f, l->b, acc,
                  ir_int_op(IR_I8, e->as.simd.op == SIMD_OP_ALL ? 1 : 0));
        lanes_begin(l, &c, t->field_count, true);
        y = lower_temp(l, ir_load(l->f, l->b, IR_I8, lane_at(l, &c, t, x, 0)));
        ir_assign(l->f, l->b, acc,
                  lower_temp(l,
                             ir_binary(l->f, l->b, fold, IR_I8,
                                       lower_temp(l, acc), y)));
        lanes_end(l, &c);
        return lower_temp(l, acc);
    }
    }
}
