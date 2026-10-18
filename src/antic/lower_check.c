/* The checks of a dev build in lowering. The text a failed check prints,
   the checks of the integer operators and the check of an index against
   its length. */

#include <stdio.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* The text of a failed check: the file, the line and the operation. The
   values the kind names follow it at run time. The back end formats
   nothing, and a build without the checks drops the whole string. */
const struct ir_global *lower_check_text(struct lowerer *l, int line,
                                         const char *operation)
{
    struct token_text text;
    struct text message = {0};
    const struct ir_global *g;

    text_appendf(&message, "%s:%d: %s", l->file, line, operation);
    text.bytes = text_cstr(&message);
    text.length = message.length;
    g = lower_literal_global(l, &text);
    text_free(&message);
    return g;
}

/* The value v of an integer type as the i64 the failure routine takes. */
struct ir_operand lower_widen_operand(struct lowerer *l, struct ir_operand v,
                                      const struct type *t)
{
    if (lower_ir_type_of(t) == IR_I64) {
        return v;
    }
    return lower_temp(l, ir_unary(l->f, l->b,
                                  types_is_signed(t) ? IR_SEXT : IR_ZEXT,
                                  IR_I64, v));
}

/* DESIGN: the values the failure prints are widened inside the failure
   block, which runs only when the check fails. The path a program takes
   pays the test and the branch alone. widen names the type they are
   widened from, or is NULL when they are already i64. An empty b is a
   check that prints one value. */
static void check_call(struct lowerer *l, struct ir_block *fail,
                       struct ir_block *rest, const struct ir_global *text,
                       enum check_kind kind, struct ir_operand a,
                       struct ir_operand b, const struct type *widen)
{
    struct ir_operand args[5];

    l->b = fail;
    if (widen != NULL) {
        a = lower_widen_operand(l, a, widen);
        if (b.kind != IR_NONE) {
            b = lower_widen_operand(l, b, widen);
        }
    }
    args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(text)));
    args[1] = ir_int_op(IR_I64, text->size - 1);
    args[2] = ir_int_op(IR_I32, (uint64_t)kind);
    args[3] = a;
    args[4] = b.kind == IR_NONE ? ir_int_op(IR_I64, 0) : b;
    lower_rt_call(l, RT_FN_CHECK_FAILED, args);
    l->b = rest;
}

/* DESIGN: a dev-mode check is a branch to a block that calls the runtime,
   which ends the program, as an assertion is. The failure block
   carries its own kind, so the build that compiles the program drops the
   checks and the assertions under separate options. cond decides the
   failure when bad is set, and decides the rest otherwise. */
void lower_check_branch(struct lowerer *l, struct ir_operand cond, bool bad,
                        const struct ir_global *text, enum check_kind kind,
                        struct ir_operand a, struct ir_operand b,
                        const struct type *widen)
{
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);

    fail->fail = IR_FAIL_CHECK;
    ir_branch(l->f, l->b, cond, bad ? fail : rest, bad ? rest : fail);
    check_call(l, fail, rest, text, kind, a, b, widen);
}

static enum ir_op overflow_op(enum token_kind op)
{
    return op == TOKEN_PLUS    ? IR_ADD_OV
           : op == TOKEN_MINUS ? IR_SUB_OV
                               : IR_MUL_OV;
}

/* DESIGN: the overflow test is the arithmetic itself. The operation
   gives its result and records whether it left the range, and the branch
   reads that. A dev build pays the branch and not a second add. The
   value of the expression is therefore the operation's result, which
   binary_checks returns. Every other check gives nothing back and is
   emitted before the operation, so a divisor of zero never reaches the
   instruction.

   The checks are overflow on a signed + - or *, a zero divisor of / and
   %, and a shift count outside the width of the type. Unsigned
   arithmetic wraps and is not checked. The width is the size of the type
   in bits, which a target-sized type leaves to the back end.

   By R031 of "Rules of 2026-10-13" the least value of a signed type
   divided by -1 is an overflow of / and of %. The least value is the one
   value that is below itself less one, which wraps to the largest, so
   the test names no limit and holds for a target-sized type. By R032
   `<<%` checks its count against zero alone, since it gives 0 for a
   count at or above the width. */
struct ir_operand lower_binary_checks(struct lowerer *l, enum token_kind op,
                                      const struct type *t,
                                      struct ir_operand left,
                                      struct ir_operand right, int line)
{
    char operation[64];
    struct ir_operand ok;
    struct ir_operand count;
    struct ir_operand width;

    if (!types_is_integer(t)) {
        return lower_none();
    }
    switch (op) {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR: {
        const struct ir_global *text;
        struct ir_block *fail;
        struct ir_block *rest;
        struct ir_operand result;
        if (!types_is_signed(t)) {
            return lower_none();
        }
        text_format(operation, sizeof operation, "overflow in %s",
                    op == TOKEN_PLUS ? "+" : op == TOKEN_MINUS ? "-" : "*");
        text = lower_check_text(l, line, operation);
        /* The failure block prints left and right alone, so the path
           that reads the result is the one without overflow. */
        result = lower_temp(l, ir_binary_wrap(l->f, l->b, overflow_op(op),
                                              lower_ir_type_of(t), left,
                                              right, IR_NSW));
        fail = lower_new_block(l);
        rest = lower_new_block(l);
        fail->fail = IR_FAIL_CHECK;
        ir_branch_ov(l->f, l->b, result, fail, rest);
        check_call(l, fail, rest, text, CHECK_OVERFLOW, left, right, t);
        return result;
    }
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, right,
                                     ir_int_op(lower_ir_type_of(t), 0)));
        text_format(operation, sizeof operation, "division by zero in %s",
                    op == TOKEN_SLASH ? "/" : "%");
        lower_check_branch(l, ok, false, lower_check_text(l, line, operation),
                           types_is_signed(t) ? CHECK_LEFT : CHECK_LEFT_U, left,
                           lower_none(), t);
        if (types_is_signed(t)) {
            enum ir_type type = lower_ir_type_of(t);
            struct ir_operand by_minus_one =
                lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, right,
                                        ir_int_op(type, UINT64_MAX)));
            struct ir_operand below =
                lower_temp(l, ir_binary(l->f, l->b, IR_SUB, type, left,
                                        ir_int_op(type, 1)));
            struct ir_operand least =
                lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8, below,
                                        left));
            struct ir_operand bad =
                lower_temp(l, ir_binary(l->f, l->b, IR_AND, IR_I8,
                                        by_minus_one, least));
            text_format(operation, sizeof operation, "overflow in %s",
                        op == TOKEN_SLASH ? "/" : "%");
            lower_check_branch(l, bad, true,
                               lower_check_text(l, line, operation),
                               CHECK_OVERFLOW, left, right, t);
        }
        return lower_none();
    case TOKEN_SHL_WRAP:
        if (!types_is_signed(t)) {
            return lower_none();
        }
        count = lower_widen_operand(l, right, t);
        width = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                        lower_size_operand(l, t),
                                        ir_int_op(IR_I64, 8)));
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_SGE, IR_I8, count,
                                     ir_int_op(IR_I64, 0)));
        lower_check_branch(l, ok, false,
                           lower_check_text(l, line,
                                            "shift count out of range for "
                                            "<<%"),
                           CHECK_SHIFT, count, width, NULL);
        return lower_none();
    case TOKEN_SHL:
    case TOKEN_SHR:
        count = lower_widen_operand(l, right, t);
        width = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                        lower_size_operand(l, t),
                                        ir_int_op(IR_I64, 8)));
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, count, width));
        text_format(operation, sizeof operation,
                    "shift count out of range for %s",
                    op == TOKEN_SHL ? "<<" : ">>");
        lower_check_branch(l, ok, false, lower_check_text(l, line, operation),
                           CHECK_SHIFT, count, width, NULL);
        return lower_none();
    default:
        return lower_none();
    }
}

/* DESIGN: a value stored into a bitfield fits its width, by R048 of
   "Rules of 2026-10-13", as the value of a narrowing `as` fits its type.
   The type of a bitfield is an integer of a fixed width, so both ends are
   numbers of the IR. One unsigned comparison holds both ends of a signed
   field of n bits: the value plus 2^(n-1) is below 2^n. The sum wraps for
   no value that fits, and a value it wraps for lands at or above 2^n. A
   field as wide as its type holds every value of the type. A number of
   the IR that fits is one the checker held already and gets no test. */
void lower_bitfield_check(struct lowerer *l, const struct struct_field *field,
                          struct ir_operand v, int line)
{
    enum ir_type type = lower_ir_type_of(field->type);
    bool is_signed = types_is_signed(field->type);
    int bits = field->bits;
    struct ir_operand moved = v;
    struct ir_operand ok;
    struct text operation = {0};

    if (bits == 0 || bits >= types_bits(field->type)) {
        return;
    }
    if (v.kind == IR_INT) {
        int width = types_bits(field->type);
        uint64_t half = is_signed ? (uint64_t)1 << (bits - 1) : 0;
        uint64_t mask =
            width == 64 ? UINT64_MAX : ((uint64_t)1 << width) - 1;
        if (((v.as.integer + half) & mask) < (uint64_t)1 << bits) {
            return;
        }
    }
    if (is_signed) {
        moved = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, type, v,
                                        ir_int_op(type,
                                                  (uint64_t)1 << (bits - 1))));
    }
    ok = lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, moved,
                                 ir_int_op(type, (uint64_t)1 << bits)));
    text_appendf(&operation, "value out of range for the %d bit%s of %.*s",
                 bits, bits == 1 ? "" : "s", (int)field->name.length,
                 field->name.text);
    lower_check_branch(l, ok, false,
                       lower_check_text(l, line, text_cstr(&operation)),
                       is_signed ? CHECK_VALUE : CHECK_VALUE_U, v,
                       lower_none(), field->type);
    text_free(&operation);
}

/* DESIGN: `+|=`, `-|=` and `*|=` on a bitfield saturate at its width, by
   R049 of "Rules of 2026-10-13". The operator has saturated v at the type
   of the field, and holding that to the ends of the narrower width gives
   what the operator gives at the width. The IR has no smaller-of-two, so
   each end is a saturating step of the type and its inverse: v plus the
   distance from the largest value of the field to the largest of the
   type saturates exactly where v is above the field, and less that
   distance it is the smaller of v and the end. The least end of a signed
   field is the same with `-`. */
struct ir_operand lower_bitfield_saturate(struct lowerer *l,
                                          const struct struct_field *field,
                                          struct ir_operand v)
{
    enum ir_type type = lower_ir_type_of(field->type);
    bool is_signed = types_is_signed(field->type);
    int width = types_bits(field->type);
    int bits = field->bits;
    uint64_t all = width == 64 ? UINT64_MAX : ((uint64_t)1 << width) - 1;
    uint64_t above;
    struct ir_operand distance;

    if (bits == 0 || bits >= width) {
        return v;
    }
    /* Both ends of a signed field lie the same distance inside the ends
       of its type. */
    above = is_signed ? (all >> 1) - (((uint64_t)1 << (bits - 1)) - 1)
                      : all - (((uint64_t)1 << bits) - 1);
    distance = ir_int_op(type, above);
    v = lower_temp(l, ir_binary(l->f, l->b,
                                is_signed ? IR_ADD_SAT_S : IR_ADD_SAT_U, type,
                                v, distance));
    v = lower_temp(l, ir_binary(l->f, l->b, IR_SUB, type, v, distance));
    if (is_signed) {
        v = lower_temp(l, ir_binary(l->f, l->b, IR_SUB_SAT_S, type, v,
                                    distance));
        v = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, type, v, distance));
    }
    return v;
}

/* DESIGN: unary `-` of a signed integer is 0 less the value, so its
   overflow test is that of a subtraction, by R031 of "Rules of
   2026-10-13". The least value is the one that fails it. A build without
   the checks turns the operation back into the plain subtraction, which
   is what a negation emits. */
struct ir_operand lower_negate_checked(struct lowerer *l,
                                       const struct type *t,
                                       struct ir_operand v, int line)
{
    enum ir_type type = lower_ir_type_of(t);
    const struct ir_global *text = lower_check_text(l, line, "overflow in -");
    struct ir_operand result =
        lower_temp(l, ir_binary_wrap(l->f, l->b, IR_SUB_OV, type,
                                     ir_int_op(type, 0), v, IR_NSW));
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);

    fail->fail = IR_FAIL_CHECK;
    ir_branch_ov(l->f, l->b, result, fail, rest);
    check_call(l, fail, rest, text, CHECK_VALUE, v, lower_none(), t);
    return result;
}

/* DESIGN: one unsigned comparison covers both ends. A negative index is
   a large unsigned value, so it fails the same test as an index past the
   length. The check stays a compare and a branch. */
void lower_bounds_check(struct lowerer *l, const struct expr *e,
                        struct ir_operand index, struct ir_operand length)
{
    struct ir_operand ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, index, length));

    lower_check_branch(l, ok, false,
                       lower_check_text(l, e->as.index.index->pos.line,
                                        "index out of bounds"),
                       CHECK_BOUNDS, index, length, NULL);
}
