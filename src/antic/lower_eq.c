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
   chain, a Regex through its text and its mode, and any other struct
   part through its fields. A lock is no data and is passed over. */

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
   class of its chain that declares one, or the default the compiler
   writes for t. The value has its class, so the call is direct. */
static struct ir_operand class_equals(struct lowerer *l, const struct type *t,
                                      struct ir_operand a, struct ir_operand b)
{
    const struct type *up;
    const struct item *m = NULL;

    for (up = t; up != NULL && up->kind == TYPE_CLASS && m == NULL;
         up = up->base) {
        m = lower_level_fn(up, &equals_name);
    }
    if (m != NULL) {
        return call_eq(l, lower_callee_function(l, m->symbol), a, b);
    }
    return call_eq(l, lower_class_function(l, t, "equals"), a, b);
}

/* Whether the Regex values at a and b have one text and one mode, which
   the runtime answers from their handles. */
static struct ir_operand same_pattern(struct lowerer *l, struct ir_operand a,
                                      struct ir_operand b)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    struct ir_operand args[2];

    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
    args[1] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
    return lower_rt_call(l, "anti_rt_pattern_same", IR_I8, params, args, 2);
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
        if (type_field_is_unit_break(f) || types_is_mutex(f->type) ||
            types_is_object_lock(f->type)) {
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

/* Two arrays of type t at a and b, element by element through every
   level of the array. The walk leaves at the first element that differs,
   as require does for every part. */
static void compare_array(struct lowerer *l, const struct expr *e,
                          struct comparison *cmp, struct ir_operand a,
                          struct ir_operand b, const struct type *t)
{
    struct ir_operand count = ir_int_op(IR_I64, 1);
    const struct type *element = t;
    struct ir_operand size;
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t index;
    struct ir_operand offset;

    for (; element->kind == TYPE_ARRAY; element = element->element) {
        struct ir_operand length =
            element->length_of != NULL
                ? ir_sym_operand(l->m, lower_sym_of(l, element->length_of))
                : ir_int_op(IR_I64, element->length);
        count = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64, count,
                                        length));
    }
    size = lower_size_operand(l, element);
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SLT, IR_I8,
                                      lower_temp(l, index), count)),
              body, done);
    l->b = body;
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                     lower_temp(l, index), size));
    {
        struct ir_operand x = lower_temp(l, ir_ptradd(l->f, l->b, a, offset));
        struct ir_operand y = lower_temp(l, ir_ptradd(l->f, l->b, b, offset));
        compare_at(l, e, cmp, x, y, element);
    }
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    ir_jump(l->f, l->b, test);
    l->b = done;
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
        /* A union or a Match is no part a default reads. The checker
           refuses `==` on either, and the default of a class passes over
           a field that holds one. */
        if (t->is_union || types_is_match(t)) {
            return;
        }
        if (types_is_regex(t)) {
            require(l, cmp, same_pattern(l, a, b));
            return;
        }
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
    case TYPE_ARRAY:
        compare_array(l, e, cmp, a, b, t);
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

/* Whether a value of type t holds a union or a Match in place, which no
   default reads. It may stand there directly or in a struct, an array, a
   `?T` or a case of a variant. */
bool lower_unreadable(const struct type *t)
{
    size_t i;

    while (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        t = t->element;
    }
    if (t->kind == TYPE_VARIANT) {
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != NULL && lower_unreadable(t->params[i])) {
                return true;
            }
        }
        return false;
    }
    if (t->kind != TYPE_STRUCT) {
        return false;
    }
    if (t->is_union || types_is_match(t)) {
        return true;
    }
    for (i = 0; i < t->field_count; i++) {
        if (!type_field_is_unit_break(&t->fields[i]) &&
            lower_unreadable(t->fields[i].type)) {
            return true;
        }
    }
    return false;
}

/* Two `own` slices of type t at a and b: the same length, and then equal
   elements, one by one. */
static void compare_owned(struct lowerer *l, const struct expr *e,
                          struct comparison *cmp, struct ir_operand a,
                          struct ir_operand b, const struct type *t)
{
    const struct type *element = t->element;
    struct ir_operand count = lower_slice_length(l, a, t);
    struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
    struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
    struct ir_operand size = lower_size_operand(l, element);
    struct ir_block *test;
    struct ir_block *body;
    struct ir_block *done;
    struct ir_operand offset;
    uint32_t index;

    require(l, cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, count,
                                            lower_slice_length(l, b, t))));
    test = lower_new_block(l);
    body = lower_new_block(l);
    done = lower_new_block(l);
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SLT, IR_I8,
                                      lower_temp(l, index), count)),
              body, done);
    l->b = body;
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                     lower_temp(l, index), size));
    {
        struct ir_operand p = lower_temp(l, ir_ptradd(l->f, l->b, x, offset));
        struct ir_operand q = lower_temp(l, ir_ptradd(l->f, l->b, y, offset));
        compare_at(l, e, cmp, p, q, element);
    }
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    ir_jump(l->f, l->b, test);
    l->b = done;
}

/* Field index of level up of a class, at self and at other. */
static void compare_member(struct lowerer *l, const struct expr *e,
                           struct comparison *cmp, const struct type *up,
                           size_t index, struct ir_operand self,
                           struct ir_operand other)
{
    const struct struct_field *f = &up->fields[index];
    struct ir_operand offset;
    struct ir_operand a;
    struct ir_operand b;

    if ((f->form != FIELD_PLAIN && f->form != FIELD_USE) || f->transient ||
        types_is_mutex(f->type) || types_is_object_lock(f->type) ||
        lower_unreadable(f->type)) {
        return;
    }
    if (f->bits != 0) {
        enum ir_type type = lower_ir_type_of(f->type);
        uint32_t agg = lower_agg_of(l, up);
        struct ir_operand x = lower_temp(
            l, ir_bitload(l->f, l->b, type, self, agg, (uint32_t)index));
        struct ir_operand y = lower_temp(
            l, ir_bitload(l->f, l->b, type, other, agg, (uint32_t)index));
        require(l, cmp, same_value(l, f->type, x, y));
        return;
    }
    offset = lower_field_offset(l, up, &f->name);
    a = lower_offset_address(l, self, offset);
    b = lower_offset_address(l, other, offset);
    if (f->owned && f->type->kind == TYPE_SLICE) {
        compare_owned(l, e, cmp, a, b, f->type);
        return;
    }
    /* An `own fn` is the code and the snapshot, each compared as a
       pointer compares. */
    if (f->owned && f->type->kind == TYPE_FN) {
        uint32_t agg = lower_agg_of(l, f->type);
        struct ir_operand second =
            ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg, 1));
        struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
        struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
        require(l, cmp, same_value(l, f->type, x, y));
        x = lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                                  lower_offset_address(l, a, second)));
        y = lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                                  lower_offset_address(l, b, second)));
        require(l, cmp, same_value(l, f->type, x, y));
        return;
    }
    compare_at(l, e, cmp, a, b, f->type);
}

/* DESIGN: the default `equals` of a class is code the compiler writes,
   `C.equals`, so it reads no field list and works under `--no-reflect`.
   The same object is equal to itself. Two objects of different classes
   are never equal, which their descriptors tell. Every field of the chain
   is then compared as the default `==` of a struct compares a part. An
   `own` slice compares element by element and an `own fn` by its code and
   its snapshot. A lock, a `transient` field, the sub-object of an
   interface and a field that holds a union or a Match are passed over. */
void lower_class_equals(struct lowerer *l, const struct item *it)
{
    static const enum ir_type one[] = {IR_PTR};
    const struct type *t = it->symbol->type;
    struct ir_function *f = lower_class_function(l, t, "equals");
    const struct expr *e = it->default_eq;
    const struct type *up;
    struct comparison cmp;
    struct ir_block *done;
    struct ir_block *next;
    struct ir_operand self;
    struct ir_operand other;
    struct ir_operand ours;
    struct ir_operand theirs;
    size_t i;

    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    other = lower_temp(l, f->params[1].temp);
    cmp.result = ir_unary(l->f, l->b, IR_COPY, IR_I8, ir_int_op(IR_I8, 1));
    cmp.differ = lower_new_block(l);
    done = lower_new_block(l);
    next = lower_new_block(l);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, self, other)),
              done, next);
    l->b = next;
    require(l, &cmp, lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, other,
                                             ir_int_op(IR_PTR, 0))));
    ours = lower_rt_call(l, "anti_rt_descriptor", IR_PTR, one, &self, 1);
    theirs = lower_rt_call(l, "anti_rt_descriptor", IR_PTR, one, &other, 1);
    require(l, &cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, ours,
                                             theirs)));
    for (up = t; up != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            compare_member(l, e, &cmp, up, i, self, other);
        }
    }
    ir_jump(l->f, l->b, done);
    l->b = cmp.differ;
    ir_assign(l->f, l->b, cmp.result, ir_int_op(IR_I8, 0));
    ir_jump(l->f, l->b, done);
    l->b = done;
    ir_ret(l->f, l->b, IR_I8, lower_temp(l, cmp.result));
}
