/* The default `==` of a struct, a class value, a tuple, a variant and a
   `?T`, which `a == b` gives on a type without `operator fn eq`. */

#include "lower_lowerer.h"
#include "target.h"

/* DESIGN: the default compares the parts of the two values in order and
   leaves at the first that differs, so a part after it is never read and
   no `operator fn eq` of a later part runs. Each part compares as its
   own `==` does: a number by its value, which makes zero of either sign
   one value and NaN equal to nothing, a `str` by its bytes, a pointer by
   its address and a function by its code. A slice part compares by its
   address and its length, as the default hash takes it. A part with `operator fn eq`
   goes through that function, a class value through the `equals` of its
   chain, and any other struct part through its fields. */

static const struct name equals_name = {"equals", 6};

/* The comparison under way: the result, 1 until a part differs, and the
   block that writes 0 into it. */
struct comparison {
    uint32_t result;
    struct ir_block *differ;
};

/* Go on in a new block when same holds, and to the block that records a
   difference when it does not. */
static void require(struct lowerer *l, struct comparison *cmp,
                    struct ir_operand same)
{
    struct ir_block *next = lower_new_block(l);

    ir_branch(l->f, l->b, same, next, cmp->differ);
    l->b = next;
}

/* The call of the `operator fn eq` of the struct or class t, among the
   calls the checker gave the default of e, or NULL. */
static const struct expr *own_eq(const struct expr *e, const struct type *t)
{
    size_t i;

    for (i = 0; i < e->as.binary.eq_count; i++) {
        if (types_hash_call_type(e->as.binary.eq_calls[i]) == t) {
            return e->as.binary.eq_calls[i];
        }
    }
    return NULL;
}

/* Call fn with the addresses a and b, which pass the operands whether fn
   takes them by value or through a pointer. */
static struct ir_operand call_eq(struct lowerer *l, struct ir_function *fn,
                                 struct ir_operand a, struct ir_operand b)
{
    struct ir_operand args[2];

    args[0] = a;
    args[1] = b;
    return lower_temp(l, ir_call(l->f, l->b, IR_I8, ir_func_op(fn), args, 2));
}

/* The `equals` a class value of type t calls: the one of the lowest
   class of its chain that declares one, or that of the root. The value
   has its class, so the call is direct. */
static struct ir_operand class_equals(struct lowerer *l, const struct type *t,
                                      struct ir_operand a, struct ir_operand b)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    const struct item *m = NULL;
    struct ir_operand args[2];

    for (; t != NULL && t->kind == TYPE_CLASS && m == NULL; t = t->base) {
        m = lower_level_fn(t, &equals_name);
    }
    if (m != NULL) {
        return call_eq(l, lower_callee_function(l, m->symbol), a, b);
    }
    args[0] = a;
    args[1] = b;
    return lower_rt_call(l, RUNTIME_ROOT "equals", IR_I8, params, args, 2);
}

/* Whether the scalars x and y of type t are equal, as an i8. */
static struct ir_operand same_value(struct lowerer *l, const struct type *t,
                                    struct ir_operand x, struct ir_operand y)
{
    return lower_temp(l, ir_binary(l->f, l->b, lower_binary_op(TOKEN_EQ, t),
                                   IR_I8, x, y));
}

static void compare_at(struct lowerer *l, const struct expr *e,
                       struct comparison *cmp, struct ir_operand a,
                       struct ir_operand b, const struct type *t);

/* The fields of the struct t at a and b, in order. A unit break holds no
   value, and a Mutex is no part of a value. */
static void compare_fields(struct lowerer *l, const struct expr *e,
                           struct comparison *cmp, struct ir_operand a,
                           struct ir_operand b, const struct type *t)
{
    uint32_t agg = lower_agg_of(l, t);
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (type_field_is_unit_break(f) || types_is_mutex(f->type)) {
            continue;
        }
        if (f->bits != 0) {
            enum ir_type type = lower_ir_type_of(f->type);
            struct ir_operand x = lower_temp(
                l, ir_bitload(l->f, l->b, type, a, agg, (uint32_t)i));
            struct ir_operand y = lower_temp(
                l, ir_bitload(l->f, l->b, type, b, agg, (uint32_t)i));
            require(l, cmp, same_value(l, f->type, x, y));
        } else {
            /* The first field stands at the address of the value, which
               every other part of lowering writes it through. */
            struct ir_operand offset =
                i == 0 ? lower_zero()
                       : ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg,
                                                               (uint32_t)i));
            struct ir_operand x = lower_offset_address(l, a, offset);
            struct ir_operand y = lower_offset_address(l, b, offset);
            compare_at(l, e, cmp, x, y, f->type);
        }
    }
}

/* Two variants of type t at a and b: the same case, and then the fields
   of that case. A case without fields has nothing more to compare. */
static void compare_cases(struct lowerer *l, const struct expr *e,
                          struct comparison *cmp, struct ir_operand a,
                          struct ir_operand b, const struct type *t)
{
    enum ir_type tag_type = lower_ir_type_of(t->base);
    struct ir_operand x = lower_load_tag(l, t, a);
    struct ir_operand y = lower_load_tag(l, t, b);
    struct ir_block *join = lower_new_block(l);
    size_t i;

    require(l, cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, x, y)));
    for (i = 0; i < t->param_count; i++) {
        struct ir_block *yes;
        struct ir_block *no;
        struct ir_operand is_case;
        if (t->params[i] == NULL) {
            continue;
        }
        yes = lower_new_block(l);
        no = lower_new_block(l);
        is_case = lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, x,
                                          ir_int_op(tag_type,
                                                    t->base->fields[i].number)));
        ir_branch(l->f, l->b, is_case, yes, no);
        l->b = yes;
        {
            struct ir_operand at_a = lower_case_address(l, t, a);
            struct ir_operand at_b = lower_case_address(l, t, b);
            compare_fields(l, e, cmp, at_a, at_b, t->params[i]);
        }
        ir_jump(l->f, l->b, join);
        l->b = no;
    }
    ir_jump(l->f, l->b, join);
    l->b = join;
}

/* Two values of the `?T` t at a and b: both empty, or both holding equal
   values. */
static void compare_optional(struct lowerer *l, const struct expr *e,
                             struct comparison *cmp, struct ir_operand a,
                             struct ir_operand b, const struct type *t)
{
    struct ir_operand x = lower_optional_flag(l, t, a);
    struct ir_operand y = lower_optional_flag(l, t, b);
    struct ir_block *held = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);

    require(l, cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, x, y)));
    ir_branch(l->f, l->b, x, held, join);
    l->b = held;
    compare_at(l, e, cmp, a, b, t->element);
    ir_jump(l->f, l->b, join);
    l->b = join;
}

/* Compare the values of type t in memory at a and b. */
static void compare_at(struct lowerer *l, const struct expr *e,
                       struct comparison *cmp, struct ir_operand a,
                       struct ir_operand b, const struct type *t)
{
    const struct expr *own;

    if (!lower_is_aggregate(t)) {
        enum ir_type type = lower_ir_type_of(t);
        struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, type, a));
        struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, type, b));
        require(l, cmp, same_value(l, t, x, y));
        return;
    }
    switch (t->kind) {
    case TYPE_STR:
        require(l, cmp, lower_compare_text(l, TOKEN_EQ, t, a, b));
        return;
    case TYPE_SLICE: {
        /* A slice part compares as the view it is: its address and its
           length. */
        struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
        struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
        require(l, cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, x,
                                                y)));
        require(l, cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                                lower_slice_length(l, a, t),
                                                lower_slice_length(l, b, t))));
        return;
    }
    case TYPE_FN: {
        /* A function with its context compares by its code. */
        struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
        struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
        require(l, cmp, same_value(l, t, x, y));
        return;
    }
    case TYPE_CLASS:
        own = own_eq(e, t);
        if (own != NULL) {
            require(l, cmp, call_eq(l, lower_callee_function(
                                           l, own->as.call.callee->symbol),
                                    a, b));
            return;
        }
        require(l, cmp, class_equals(l, t, a, b));
        return;
    case TYPE_STRUCT:
        own = own_eq(e, t);
        if (own != NULL) {
            require(l, cmp, call_eq(l, lower_callee_function(
                                           l, own->as.call.callee->symbol),
                                    a, b));
            return;
        }
        compare_fields(l, e, cmp, a, b, t);
        return;
    case TYPE_TUPLE:
        compare_fields(l, e, cmp, a, b, t);
        return;
    case TYPE_VARIANT:
        own = own_eq(e, t);
        if (own != NULL) {
            require(l, cmp, call_eq(l, lower_callee_function(
                                           l, own->as.call.callee->symbol),
                                    a, b));
            return;
        }
        compare_cases(l, e, cmp, a, b, t);
        return;
    case TYPE_OPTIONAL:
        compare_optional(l, e, cmp, a, b, t);
        return;
    default:
        /* The checker gives the default to no other part. */
        return;
    }
}

struct ir_operand lower_equals(struct lowerer *l, const struct expr *e)
{
    const struct type *t = e->as.binary.left->type;
    struct ir_operand a = lower_expr(l, e->as.binary.left);
    struct ir_operand b = lower_expr(l, e->as.binary.right);
    struct ir_block *done = lower_new_block(l);
    struct comparison cmp;
    struct ir_operand result;

    cmp.result = ir_unary(l->f, l->b, IR_COPY, IR_I8, ir_int_op(IR_I8, 1));
    cmp.differ = lower_new_block(l);
    compare_at(l, e, &cmp, a, b, t);
    ir_jump(l->f, l->b, done);
    l->b = cmp.differ;
    ir_assign(l->f, l->b, cmp.result, ir_int_op(IR_I8, 0));
    ir_jump(l->f, l->b, done);
    l->b = done;
    result = lower_temp(l, cmp.result);
    if (e->as.binary.op == TOKEN_NE) {
        result = lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, result,
                                         ir_int_op(IR_I8, 0)));
    }
    return result;
}
