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
   part through its fields. A lock is no data and is passed over.

   DESIGN: the default `==` here and the default hash of lower_hash.c take
   each part by one rule, lower_part_of, and each switches over every kind
   it gives. Two values that `==` finds equal then hash alike, which
   "Hashing and order" of docs/anti-language-additions.md asks. */

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
    struct ir_operand args[2];

    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, a));
    args[1] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, b));
    return lower_rt_call(l, RT_FN_PATTERN_SAME, args);
}

/* Whether t is a lock or an array of locks, which no default reads. */
static bool is_lock(const struct type *t)
{
    while (t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    return types_is_mutex(t) || types_is_object_lock(t);
}

enum lower_part lower_part_of(const struct type *t, struct expr *const *calls,
                              size_t count, const struct expr **own)
{
    size_t i;

    *own = NULL;
    if (is_lock(t)) {
        return LOWER_PART_NONE;
    }
    if (!lower_is_aggregate(t)) {
        return LOWER_PART_VALUE;
    }
    if (types_is_regex(t)) {
        return LOWER_PART_PATTERN;
    }
    /* An operator goes before the rule of the kind, so a union with
       `operator fn eq` compares through it. */
    if (t->kind == TYPE_STRUCT || t->kind == TYPE_CLASS ||
        t->kind == TYPE_VARIANT) {
        for (i = 0; i < count; i++) {
            if (types_hash_call_type(calls[i]) == t) {
                *own = calls[i];
                return LOWER_PART_OWN;
            }
        }
    }
    switch (t->kind) {
    case TYPE_STR:
        return LOWER_PART_TEXT;
    case TYPE_SLICE:
        return LOWER_PART_VIEW;
    case TYPE_FN:
        return LOWER_PART_CODE;
    case TYPE_CLASS:
        return LOWER_PART_CLASS;
    case TYPE_STRUCT:
        return t->is_union ? LOWER_PART_UNION : LOWER_PART_FIELDS;
    case TYPE_TUPLE:
        return LOWER_PART_FIELDS;
    case TYPE_ARRAY:
        return LOWER_PART_ARRAY;
    case TYPE_VARIANT:
        return LOWER_PART_CASES;
    case TYPE_OPTIONAL:
        return LOWER_PART_OPTIONAL;
    default:
        /* The checker gives the default to no other part. */
        return LOWER_PART_NONE;
    }
}

struct ir_operand lower_part_scalar(struct lowerer *l, const struct type *t,
                                    struct ir_operand v, enum ir_type *type)
{
    if (t->kind == TYPE_F16) {
        *type = IR_F32;
        return lower_temp(l, ir_unary(l->f, l->b, IR_HEXT, IR_F32, v));
    }
    *type = lower_ir_type_of(t);
    return v;
}

bool lower_field_skipped(const struct struct_field *f)
{
    return types_field_is_unit_break(f) || is_lock(f->type);
}

enum lower_member lower_member_of(const struct struct_field *f)
{
    if ((f->form != FIELD_PLAIN && f->form != FIELD_USE) || f->transient ||
        lower_field_skipped(f) || types_holds_union(f->type)) {
        return LOWER_MEMBER_NONE;
    }
    if (f->bits != 0) {
        return LOWER_MEMBER_BITS;
    }
    if (f->owned && f->type->kind == TYPE_SLICE) {
        return LOWER_MEMBER_ELEMENTS;
    }
    if (f->owned && f->type->kind == TYPE_FN) {
        return LOWER_MEMBER_SNAPSHOT;
    }
    return LOWER_MEMBER_PART;
}

/* Whether the scalars x and y of type t are equal, as an i8. */
static struct ir_operand same_value(struct lowerer *l, const struct type *t,
                                    struct ir_operand x, struct ir_operand y)
{
    enum ir_type type;
    enum ir_op op;

    x = lower_part_scalar(l, t, x, &type);
    y = lower_part_scalar(l, t, y, &type);
    op = type == IR_F32 || type == IR_F64 ? IR_FEQ : IR_EQ;
    return lower_temp(l, ir_binary(l->f, l->b, op, IR_I8, x, y));
}

/* Whether the words at offset from a and from b are one address, as an
   i8. */
static struct ir_operand same_word(struct lowerer *l, struct ir_operand a,
                                   struct ir_operand b,
                                   struct ir_operand offset)
{
    struct ir_operand x = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR, lower_offset_address(l, a, offset)));
    struct ir_operand y = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR, lower_offset_address(l, b, offset)));

    return lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, x, y));
}

static void compare_at(struct lowerer *l, const struct expr *e,
                       struct comparison *cmp, struct ir_operand a,
                       struct ir_operand b, const struct type *t);

/* The fields of the struct t at a and b, in order, but those
   lower_field_skipped passes over. */
static void compare_fields(struct lowerer *l, const struct expr *e,
                           struct comparison *cmp, struct ir_operand a,
                           struct ir_operand b, const struct type *t)
{
    uint32_t agg = lower_agg_of(l, t);
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (lower_field_skipped(f)) {
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
    struct ir_operand count = lower_array_count(l, t);
    const struct type *element = t;
    struct ir_operand size;
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t index;
    struct ir_operand offset;

    while (element->kind == TYPE_ARRAY) {
        element = element->element;
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

    switch (lower_part_of(t, e->as.binary.eq_calls, e->as.binary.eq_count,
                          &own)) {
    case LOWER_PART_NONE:
        return;
    case LOWER_PART_VALUE: {
        enum ir_type type = lower_ir_type_of(t);
        struct ir_operand x = lower_temp(l, ir_load(l->f, l->b, type, a));
        struct ir_operand y = lower_temp(l, ir_load(l->f, l->b, type, b));
        require(l, cmp, same_value(l, t, x, y));
        return;
    }
    case LOWER_PART_TEXT:
        require(l, cmp, lower_compare_text(l, TOKEN_EQ, t, a, b));
        return;
    case LOWER_PART_VIEW: {
        struct ir_operand n;
        struct ir_operand m;
        require(l, cmp, same_word(l, a, b, lower_zero()));
        /* Each length is bound first, since C leaves the order of two
           calls in one argument list open. */
        n = lower_slice_length(l, a, t);
        m = lower_slice_length(l, b, t);
        require(l, cmp,
                lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, n, m)));
        return;
    }
    case LOWER_PART_CODE:
        require(l, cmp, same_word(l, a, b, lower_zero()));
        return;
    case LOWER_PART_OWN:
        require(l, cmp, call_eq(l, lower_callee_function(
                                       l, own->as.call.callee->symbol),
                                a, b));
        return;
    case LOWER_PART_CLASS:
        require(l, cmp, class_equals(l, t, a, b));
        return;
    case LOWER_PART_PATTERN:
        require(l, cmp, same_pattern(l, a, b));
        return;
    case LOWER_PART_UNION:
        /* The checker refuses `==` on a union without `operator fn eq`,
           and the default of a class passes over a field that holds
           one. */
        return;
    case LOWER_PART_FIELDS:
        compare_fields(l, e, cmp, a, b, t);
        return;
    case LOWER_PART_ARRAY:
        compare_array(l, e, cmp, a, b, t);
        return;
    case LOWER_PART_CASES:
        compare_cases(l, e, cmp, a, b, t);
        return;
    case LOWER_PART_OPTIONAL:
        compare_optional(l, e, cmp, a, b, t);
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

/* The addresses of the field f of level up of a class, at self in *a
   and at other in *b. */
static void member_at(struct lowerer *l, const struct type *up,
                      const struct struct_field *f, struct ir_operand self,
                      struct ir_operand other, struct ir_operand *a,
                      struct ir_operand *b)
{
    struct ir_operand offset = lower_field_offset(l, up, &f->name);

    *a = lower_offset_address(l, self, offset);
    *b = lower_offset_address(l, other, offset);
}

/* Field index of level up of a class, at self and at other. */
static void compare_member(struct lowerer *l, const struct expr *e,
                           struct comparison *cmp, const struct type *up,
                           size_t index, struct ir_operand self,
                           struct ir_operand other)
{
    const struct struct_field *f = &up->fields[index];
    struct ir_operand a;
    struct ir_operand b;

    switch (lower_member_of(f)) {
    case LOWER_MEMBER_NONE:
        return;
    case LOWER_MEMBER_BITS: {
        enum ir_type type = lower_ir_type_of(f->type);
        uint32_t agg = lower_agg_of(l, up);
        struct ir_operand x = lower_temp(
            l, ir_bitload(l->f, l->b, type, self, agg, (uint32_t)index));
        struct ir_operand y = lower_temp(
            l, ir_bitload(l->f, l->b, type, other, agg, (uint32_t)index));
        require(l, cmp, same_value(l, f->type, x, y));
        return;
    }
    case LOWER_MEMBER_ELEMENTS:
        member_at(l, up, f, self, other, &a, &b);
        compare_owned(l, e, cmp, a, b, f->type);
        return;
    case LOWER_MEMBER_SNAPSHOT: {
        /* The code and the snapshot, each compared as a pointer
           compares. */
        uint32_t agg = lower_agg_of(l, f->type);
        struct ir_operand second =
            ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg, 1));
        member_at(l, up, f, self, other, &a, &b);
        require(l, cmp, same_word(l, a, b, lower_zero()));
        require(l, cmp, same_word(l, a, b, second));
        return;
    }
    case LOWER_MEMBER_PART:
        member_at(l, up, f, self, other, &a, &b);
        compare_at(l, e, cmp, a, b, f->type);
        return;
    }
}

/* Every field of the chain of t, at self and at other. */
static void compare_members(struct lowerer *l, const struct expr *e,
                            struct comparison *cmp, const struct type *t,
                            struct ir_operand self, struct ir_operand other)
{
    const struct type *up;
    size_t i;

    for (up = t; up != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            compare_member(l, e, cmp, up, i, self, other);
        }
    }
}

/* The fields of two objects of the synchronized class of it, with both
   hidden locks held. Both ends of the comparison give the locks back
   before they reach done, and a difference found before the locks were
   taken goes on to cmp->differ as before. */
static void compare_locked(struct lowerer *l, const struct item *it,
                           struct comparison *cmp, struct ir_operand self,
                           struct ir_operand other, struct ir_block *done)
{
    const struct type *t = it->symbol->type;
    struct ir_operand ours = lower_object_lock_address(l, t, self);
    struct ir_operand theirs = lower_object_lock_address(l, t, other);
    struct ir_block *before = cmp->differ;
    struct ir_block *release = lower_new_block(l);
    struct ir_block *differ = lower_new_block(l);

    lower_lock_pair_call(l, ours, theirs, it->pos.line);
    cmp->differ = differ;
    compare_members(l, it->default_eq, cmp, t, self, other);
    ir_jump(l->f, l->b, release);
    l->b = differ;
    ir_assign(l->f, l->b, cmp->result, ir_int_op(IR_I8, 0));
    ir_jump(l->f, l->b, release);
    l->b = release;
    lower_unlock_pair_call(l, ours, theirs);
    ir_jump(l->f, l->b, done);
    cmp->differ = before;
}

/* Two objects of the class of it through the `operator fn eq` of its
   module, under both hidden locks for a synchronized class. */
static void compare_by_operator(struct lowerer *l, const struct item *it,
                                struct comparison *cmp,
                                struct ir_operand self,
                                struct ir_operand other,
                                struct ir_block *done)
{
    const struct type *t = it->symbol->type;
    const struct symbol *op = it->operator_eq->as.call.callee->symbol;
    bool locked = t->safety == SAFETY_SYNCHRONIZED;
    struct ir_operand ours = lower_none();
    struct ir_operand theirs = lower_none();
    struct ir_operand same;

    if (locked) {
        ours = lower_object_lock_address(l, t, self);
        theirs = lower_object_lock_address(l, t, other);
        lower_lock_pair_call(l, ours, theirs, it->pos.line);
    }
    same = call_eq(l, lower_callee_function(l, op), self, other);
    ir_assign(l->f, l->b, cmp->result, same);
    if (locked) {
        lower_unlock_pair_call(l, ours, theirs);
    }
    ir_jump(l->f, l->b, done);
}

/* DESIGN: the default `equals` of a class is code the compiler writes,
   `C.equals`, so it reads no field list and works under `--no-reflect`.
   The same object is equal to itself. Two objects of different classes
   are never equal, which their descriptors tell. Every field of the chain
   is then compared as the default `==` of a struct compares a part. An
   `own` slice compares element by element and an `own fn` by its code and
   its snapshot. A lock, a `transient` field, the sub-object of an
   interface and a field that holds a union or a Match are passed over.
   A class whose module gives it `operator fn eq` compares through it.
   A synchronized class compares under `sync self, other`, which takes
   both locks in the runtime's order. A concurrent class has no default
   `==`, and the entry of its table compares identity unless its module
   gives it an operator. */
void lower_class_equals(struct lowerer *l, const struct item *it)
{
    const struct type *t = it->symbol->type;
    struct ir_function *f = lower_class_function(l, t, "equals");
    const struct expr *e = it->default_eq;
    struct comparison cmp;
    struct ir_block *done;
    struct ir_block *next;
    struct ir_operand self;
    struct ir_operand other;
    struct ir_operand ours;
    struct ir_operand theirs;

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
    if (t->safety == SAFETY_CONCURRENT && it->operator_eq == NULL) {
        ir_jump(l->f, l->b, cmp.differ);
    } else {
        require(l, &cmp, lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                                 other,
                                                 ir_int_op(IR_PTR, 0))));
        ours = lower_rt_call(l, RT_FN_DESCRIPTOR, &self);
        theirs = lower_rt_call(l, RT_FN_DESCRIPTOR, &other);
        require(l, &cmp, lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                                 ours, theirs)));
        if (it->operator_eq != NULL) {
            compare_by_operator(l, it, &cmp, self, other, done);
        } else if (t->safety == SAFETY_SYNCHRONIZED) {
            compare_locked(l, it, &cmp, self, other, done);
        } else {
            compare_members(l, e, &cmp, t, self, other);
            ir_jump(l->f, l->b, done);
        }
    }
    l->b = cmp.differ;
    ir_assign(l->f, l->b, cmp.result, ir_int_op(IR_I8, 0));
    ir_jump(l->f, l->b, done);
    l->b = done;
    ir_ret(l->f, l->b, IR_I8, lower_temp(l, cmp.result));
}
