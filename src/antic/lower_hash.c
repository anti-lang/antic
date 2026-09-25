/* The default hash of a value, which `x.hash()` gives on a type without
   a hash function of its own. */

#include "../rt/hash.h"
#include "lower_lowerer.h"
#include "target.h"

/* DESIGN: a scalar hashes as the 64 bits of its value through the
   finalizer of MurmurHash3. The finalizer spreads every bit of the input
   over every bit of the output. A value made of parts starts at the
   offset basis of FNV-1a. It takes each part in order, as the mix of the
   hash so far xor the hash of the part. The order of the parts counts.
   Text and bytes go to `anti_rt_hash_bytes` of src/rt/hash.c.
   It takes eight bytes at a time through the same finalizer. The
   constants stand in src/rt/hash.h, which both sides read. Every target
   gives the same hash for the same value. A hashing collection mixes its
   seed in itself. */

static const struct name hash_name = {"hash", 4};

static struct ir_operand i64(uint64_t v)
{
    return ir_int_op(IR_I64, v);
}

static struct ir_operand binary(struct lowerer *l, enum ir_op op,
                                struct ir_operand a, struct ir_operand b)
{
    return lower_temp(l, ir_binary(l->f, l->b, op, IR_I64, a, b));
}

/* x ^ (x >> 33) */
static struct ir_operand xor_shift(struct lowerer *l, struct ir_operand x)
{
    struct ir_operand shifted = binary(l, IR_SHR_U, x, i64(33));

    return binary(l, IR_XOR, x, shifted);
}

/* The finalizer, of an i64. */
static struct ir_operand mix(struct lowerer *l, struct ir_operand x)
{
    x = xor_shift(l, x);
    x = binary(l, IR_MUL, x, i64(ANTI_HASH_MUL1));
    x = xor_shift(l, x);
    x = binary(l, IR_MUL, x, i64(ANTI_HASH_MUL2));
    return xor_shift(l, x);
}

/* The hash so far, h, with the hash of the next part. */
static struct ir_operand combine(struct lowerer *l, struct ir_operand h,
                                 struct ir_operand part)
{
    return mix(l, binary(l, IR_XOR, h, part));
}

/* The bits of the float v of IR type type, as an i64. Zero of either
   sign gives 0, since `-0.0 == 0.0`. A NaN equals nothing and keeps its
   bits. */
static struct ir_operand float_bits(struct lowerer *l, struct ir_operand v,
                                    enum ir_type type)
{
    enum ir_type bits_type = type == IR_F32 ? IR_I32 : IR_I64;
    struct ir_vtype of = {bits_type, IR_NO_AGG};
    struct ir_operand slot = lower_temp(l, ir_entry_slot(l->f, of));
    struct ir_operand nonzero;
    struct ir_operand mask;
    struct ir_operand bits;

    ir_store(l->f, l->b, type, v, slot);
    bits = lower_temp(l, ir_load(l->f, l->b, bits_type, slot));
    if (bits_type == IR_I32) {
        bits = lower_temp(l, ir_unary(l->f, l->b, IR_ZEXT, IR_I64, bits));
    }
    nonzero = lower_temp(l, ir_binary(l->f, l->b, IR_FNE, IR_I8, v,
                                      ir_float_op(type, 0.0)));
    nonzero = lower_temp(l, ir_unary(l->f, l->b, IR_ZEXT, IR_I64, nonzero));
    mask = lower_temp(l, ir_unary(l->f, l->b, IR_NEG, IR_I64, nonzero));
    return binary(l, IR_AND, bits, mask);
}

/* The hash of the scalar v of type t. */
static struct ir_operand hash_value(struct lowerer *l, struct ir_operand v,
                                    const struct type *t)
{
    enum ir_type type = lower_ir_type_of(t);
    const struct type *form = t->kind == TYPE_ENUM ? t->base : t;

    if (t->kind == TYPE_F16) {
        v = lower_temp(l, ir_unary(l->f, l->b, IR_HEXT, IR_F32, v));
        type = IR_F32;
    }
    if (type == IR_F32 || type == IR_F64) {
        return mix(l, float_bits(l, v, type));
    }
    /* A pointer has no conversion to an integer in the IR, so its word
       goes through a slot. Every target has pointers of 64 bits. */
    if (type == IR_PTR) {
        struct ir_vtype of = {IR_PTR, IR_NO_AGG};
        struct ir_operand slot = lower_temp(l, ir_entry_slot(l->f, of));
        ir_store(l->f, l->b, IR_PTR, v, slot);
        return mix(l, lower_temp(l, ir_load(l->f, l->b, IR_I64, slot)));
    }
    if (type != IR_I64) {
        v = lower_temp(l, ir_unary(l->f, l->b,
                                   type_is_signed(form) ? IR_SEXT : IR_ZEXT,
                                   IR_I64, v));
    }
    return mix(l, v);
}

static struct ir_operand hash_at(struct lowerer *l, const struct expr *call,
                                 struct ir_operand at, const struct type *t);

/* The hash of count bytes at p. */
static struct ir_operand hash_bytes(struct lowerer *l, struct ir_operand p,
                                    struct ir_operand count)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_operand args[2];

    args[0] = p;
    args[1] = count;
    return lower_rt_call(l, "anti_rt_hash_bytes", IR_I64, params, args, 2);
}

/* Whether an element of type t is one byte, whose run hashes as bytes. */
static bool is_byte(const struct type *t)
{
    return t->kind == TYPE_U8 || t->kind == TYPE_I8;
}

/* The hash of count elements of type element from p on, in order. */
static struct ir_operand hash_run(struct lowerer *l, const struct expr *call,
                                  struct ir_operand p, struct ir_operand count,
                                  const struct type *element)
{
    struct ir_operand size;
    struct ir_block *test;
    struct ir_block *body;
    struct ir_block *done;
    uint32_t h;
    uint32_t index;
    struct ir_operand more;
    struct ir_operand offset;
    struct ir_operand at;
    struct ir_operand part;
    struct ir_operand next;

    if (is_byte(element)) {
        return hash_bytes(l, p, count);
    }
    size = lower_size_operand(l, element);
    test = lower_new_block(l);
    body = lower_new_block(l);
    done = lower_new_block(l);
    h = ir_unary(l->f, l->b, IR_COPY, IR_I64, i64(ANTI_HASH_START));
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64, i64(0));
    ir_jump(l->f, l->b, test);
    l->b = test;
    more = lower_temp(l, ir_binary(l->f, l->b, IR_SLT, IR_I8,
                                   lower_temp(l, index), count));
    ir_branch(l->f, l->b, more, body, done);
    l->b = body;
    offset = binary(l, IR_MUL, lower_temp(l, index), size);
    at = lower_temp(l, ir_ptradd(l->f, l->b, p, offset));
    part = hash_at(l, call, at, element);
    part = combine(l, lower_temp(l, h), part);
    ir_assign(l->f, l->b, h, part);
    next = binary(l, IR_ADD, lower_temp(l, index), i64(1));
    ir_assign(l->f, l->b, index, next);
    ir_jump(l->f, l->b, test);
    l->b = done;
    return lower_temp(l, h);
}

/* The count of elements of the array t through every level of it. */
static struct ir_operand element_count(struct lowerer *l, const struct type *t)
{
    struct ir_operand count = i64(1);

    for (; t->kind == TYPE_ARRAY; t = t->element) {
        struct ir_operand length =
            t->length_of != NULL
                ? ir_sym_operand(l->m, lower_sym_of(l, t->length_of))
                : i64(t->length);
        count = binary(l, IR_MUL, count, length);
    }
    return count;
}

/* The call of the `operator fn hash` of the struct or variant t, among
   the calls the checker gave the default hash, or NULL. */
static const struct expr *own_hash(const struct expr *call,
                                   const struct type *t)
{
    size_t i;

    for (i = 0; i < call->as.call.hash_count; i++) {
        if (types_hash_call_type(call->as.call.hash_calls[i]) == t) {
            return call->as.call.hash_calls[i];
        }
    }
    return NULL;
}

/* Call the function fn with the address of the value at, which is the
   receiver whether fn takes it by value or through a pointer. */
static struct ir_operand call_hash(struct lowerer *l, struct ir_function *fn,
                                   struct ir_operand at)
{
    return lower_temp(l, ir_call(l->f, l->b, IR_I64, ir_func_op(fn), &at, 1));
}

/* The `hash` a class value of type t calls: the one of the lowest class
   of its chain that declares one, or that of the root. The value has
   its class, so the call is direct. */
static struct ir_operand class_hash(struct lowerer *l, struct ir_operand at,
                                    const struct type *t)
{
    static const enum ir_type params[] = {IR_PTR};
    const struct item *m = NULL;

    for (; t != NULL && t->kind == TYPE_CLASS && m == NULL; t = t->base) {
        m = lower_level_fn(t, &hash_name);
    }
    if (m != NULL) {
        return call_hash(l, lower_callee_function(l, m->symbol), at);
    }
    return lower_rt_call(l, RUNTIME_ROOT "hash", IR_I64, params, &at, 1);
}

/* The fields of the struct or tuple t at at, taken into h in order. A
   unit break holds no value, and a Mutex is no part of a value. */
static struct ir_operand hash_fields(struct lowerer *l, const struct expr *call,
                                     struct ir_operand at, const struct type *t,
                                     struct ir_operand h)
{
    uint32_t agg = lower_agg_of(l, t);
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        struct ir_operand part;
        if (type_field_is_unit_break(f) || types_is_mutex(f->type)) {
            continue;
        }
        if (f->bits != 0) {
            struct ir_operand v = lower_temp(
                l, ir_bitload(l->f, l->b, lower_ir_type_of(f->type), at, agg,
                              (uint32_t)i));
            part = hash_value(l, v, f->type);
        } else {
            /* The first field stands at the address of the value, which
               every other part of lowering writes it through. */
            struct ir_operand offset =
                i == 0 ? lower_zero()
                       : ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg,
                                                               (uint32_t)i));
            struct ir_operand field = lower_offset_address(l, at, offset);
            part = hash_at(l, call, field, f->type);
        }
        h = combine(l, h, part);
    }
    return h;
}

/* A `?T` that holds nothing hashes as the start. One that holds a value
   takes the hash of the value into the start. */
static struct ir_operand hash_optional(struct lowerer *l,
                                       const struct expr *call,
                                       struct ir_operand at,
                                       const struct type *t)
{
    struct ir_operand has = lower_optional_flag(l, t, at);
    struct ir_block *some = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t h = ir_unary(l->f, l->b, IR_COPY, IR_I64, i64(ANTI_HASH_START));
    struct ir_operand part;

    ir_branch(l->f, l->b, has, some, done);
    l->b = some;
    part = hash_at(l, call, at, t->element);
    part = combine(l, lower_temp(l, h), part);
    ir_assign(l->f, l->b, h, part);
    ir_jump(l->f, l->b, done);
    l->b = done;
    return lower_temp(l, h);
}

/* A variant takes its tag, then the fields of the case the tag names. */
static struct ir_operand hash_variant(struct lowerer *l,
                                      const struct expr *call,
                                      struct ir_operand at,
                                      const struct type *t)
{
    enum ir_type tag_type = lower_ir_type_of(t->base);
    struct ir_operand tag = lower_load_tag(l, t, at);
    struct ir_operand first = hash_value(l, tag, t->base);
    struct ir_operand start = combine(l, i64(ANTI_HASH_START), first);
    uint32_t h = ir_unary(l->f, l->b, IR_COPY, IR_I64, start);
    struct ir_block *done = lower_new_block(l);
    size_t i;

    for (i = 0; i < t->param_count; i++) {
        struct ir_block *yes;
        struct ir_block *no;
        struct ir_operand is_case;
        struct ir_operand fields;
        struct ir_operand part;
        if (t->params[i] == NULL) {
            continue;
        }
        yes = lower_new_block(l);
        no = lower_new_block(l);
        is_case = lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, tag,
                                          ir_int_op(tag_type,
                                                    t->base->fields[i].number)));
        ir_branch(l->f, l->b, is_case, yes, no);
        l->b = yes;
        fields = lower_case_address(l, t, at);
        part = hash_fields(l, call, fields, t->params[i], lower_temp(l, h));
        ir_assign(l->f, l->b, h, part);
        ir_jump(l->f, l->b, done);
        l->b = no;
    }
    ir_jump(l->f, l->b, done);
    l->b = done;
    return lower_temp(l, h);
}

/* The hash of the value of type t in memory at at. */
static struct ir_operand hash_at(struct lowerer *l, const struct expr *call,
                                 struct ir_operand at, const struct type *t)
{
    const struct expr *own;
    struct ir_operand p;
    struct ir_operand count;

    if (types_is_mutex(t)) {
        return i64(ANTI_HASH_START);
    }
    if (!lower_is_aggregate(t)) {
        struct ir_operand v =
            lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(t), at));
        return hash_value(l, v, t);
    }
    switch (t->kind) {
    case TYPE_STR:
    case TYPE_SLICE:
        p = lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
        count = lower_slice_length(l, at, t);
        if (t->kind == TYPE_STR) {
            return hash_bytes(l, p, count);
        }
        return hash_run(l, call, p, count, t->element);
    case TYPE_ARRAY: {
        const struct type *element = t;
        while (element->kind == TYPE_ARRAY) {
            element = element->element;
        }
        count = element_count(l, t);
        return hash_run(l, call, at, count, element);
    }
    case TYPE_FN: {
        /* A function with its context, or a bound one: two words. */
        uint32_t agg = lower_agg_of(l, t);
        struct ir_operand second = lower_offset_address(
            l, at, ir_sym_operand(l->m, ir_sym_offset_of(l->m, agg, 1)));
        struct ir_operand code = lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
        struct ir_operand word = lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                                                       second));
        struct ir_operand h =
            combine(l, i64(ANTI_HASH_START), hash_value(l, code, t));
        return combine(l, h, hash_value(l, word, t));
    }
    case TYPE_CLASS:
        return class_hash(l, at, t);
    case TYPE_OPTIONAL:
        return hash_optional(l, call, at, t);
    case TYPE_VARIANT:
        own = own_hash(call, t);
        if (own != NULL) {
            return call_hash(l, lower_callee_function(
                                    l, own->as.call.callee->symbol),
                             at);
        }
        return hash_variant(l, call, at, t);
    case TYPE_STRUCT:
        own = own_hash(call, t);
        if (own != NULL) {
            return call_hash(l, lower_callee_function(
                                    l, own->as.call.callee->symbol),
                             at);
        }
        /* A union holds one of its fields and says not which, so it
           hashes its bytes. */
        if (t->is_union) {
            return hash_bytes(l, at, lower_size_operand(l, t));
        }
        return hash_fields(l, call, at, t, i64(ANTI_HASH_START));
    case TYPE_TUPLE:
        return hash_fields(l, call, at, t, i64(ANTI_HASH_START));
    default:
        return i64(ANTI_HASH_START);
    }
}

struct ir_operand lower_hash(struct lowerer *l, const struct expr *e)
{
    const struct expr *receiver = e->as.call.callee->as.field.base;
    const struct type *t = receiver->type;

    /* A pointer that cannot be `none` reaches the struct it points at,
       as the call of a method does. */
    if (t->kind == TYPE_POINTER && !t->nullable &&
        type_has_fields(t->element)) {
        struct ir_operand at = lower_expr(l, receiver);
        return hash_at(l, e, at, t->element);
    }
    if (lower_is_aggregate(t)) {
        struct ir_operand at = lower_address(l, receiver);
        return hash_at(l, e, at, t);
    }
    return hash_value(l, lower_expr(l, receiver), t);
}
