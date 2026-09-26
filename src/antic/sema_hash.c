/* The checker's part of hashing: `x.hash()` on every type. A class and a
   struct or variant with `operator fn hash` have a function of their
   own, which the call of a method reaches. Every other type has the
   default hash, which lowering writes from the type. A value of a type
   parameter has the hook when its constraints name `hash`. */
/* The default `==` of a struct and of a class value lives here as well,
   since it keeps the rule that two values equal by `eq` hash alike. */

#include <string.h>

#include "sema_checker.h"

static const struct name hash_name = {LANG_HOOK_HASH,
                                      sizeof LANG_HOOK_HASH - 1};

/* Whether `x.hash()` on a value of type t calls a function of t. A class
   has `hash` from the root. A pointer that cannot be `none` reaches the
   function of what it points at, as the call of a method does. A `?*T`
   is a pointer that may be `none`, whose hash is its address. */
static bool has_own_hash(struct checker *c, struct type *t)
{
    if (t->kind == TYPE_POINTER) {
        if (t->nullable) {
            return false;
        }
        t = t->element;
    }
    if (t->kind == TYPE_CLASS) {
        return true;
    }
    return sema_hook(c, t, LANG_HOOK_HASH) != NULL;
}

/* The checked call of the function of type t that hashes a value of it
   inside the value x.hash() hashes. The receiver stands for that value
   and is never lowered. Lowering reads the function the callee names,
   and a copy of a generic names its copy there. */
static struct expr *nested_call(struct checker *c, struct pos pos,
                                struct type *t)
{
    struct expr *hole = sema_new_node(c, EXPR_NONE, pos);
    struct expr *at = sema_new_node(c, EXPR_UNARY, pos);
    struct expr *field = sema_new_node(c, EXPR_FIELD, pos);
    struct expr *call = sema_new_node(c, EXPR_CALL, pos);

    hole->type = types_pointer(c->types, t);
    hole->prechecked = true;
    at->as.unary.op = TOKEN_STAR;
    at->as.unary.operand = hole;
    at->type = t;
    at->prechecked = true;
    field->as.field.base = at;
    field->as.field.name = hash_name;
    call->as.call.callee = field;
    call->as.call.args = NULL;
    call->as.call.arg_count = 0;
    if (sema_is_error(sema_check_expr(c, call, NULL))) {
        return NULL;
    }
    return call;
}

/* Whether the list calls of count checked calls holds one for type t. */
static bool listed(struct expr **calls, size_t count, const struct type *t)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (types_hash_call_type(calls[i]) == t) {
            return true;
        }
    }
    return false;
}

/* Append call to the list *calls of *count. */
static void append(struct checker *c, struct expr ***calls, size_t *count,
                   struct expr *call)
{
    struct expr **grown = types_alloc_array(c->arena, *count + 1,
                                            sizeof *grown);

    if (*count > 0) {
        memcpy(grown, *calls, *count * sizeof *grown);
    }
    grown[(*count)++] = call;
    *calls = grown;
}

/* Add the call of the function of t to e, once per type. */
static void add_call(struct checker *c, struct expr *e, struct type *t)
{
    struct expr *call;

    if (listed(e->as.call.hash_calls, e->as.call.hash_count, t)) {
        return;
    }
    call = nested_call(c, e->pos, t);
    if (call != NULL) {
        append(c, &e->as.call.hash_calls, &e->as.call.hash_count, call);
    }
}

/* Walk the value of type t that the default hash of the call e reaches.
   Give e the call of every function of a struct or a variant inside it.
   A class value calls the `hash` of its chain, which lowering finds in
   its members. A pointer is hashed by its address and reaches
   nothing. A value of a type parameter needs the hook. */
static void walk(struct checker *c, struct expr *e, struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_PARAM:
        sema_param_hash(c, e, t);
        return;
    case TYPE_ARRAY:
    case TYPE_SLICE:
        walk(c, e, t->element);
        return;
    case TYPE_OPTIONAL:
        walk(c, e, t->element);
        return;
    case TYPE_VARIANT:
        if (sema_hook(c, t, LANG_HOOK_HASH) != NULL) {
            add_call(c, e, t);
            return;
        }
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != NULL) {
                walk(c, e, t->params[i]);
            }
        }
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        if (t->kind == TYPE_STRUCT && sema_hook(c, t, LANG_HOOK_HASH) != NULL) {
            add_call(c, e, t);
            return;
        }
        /* A union has no field that holds its value, so its default
           hash reads its bytes. */
        if (t->is_union) {
            return;
        }
        for (i = 0; i < t->field_count; i++) {
            if (!type_field_is_unit_break(&t->fields[i])) {
                walk(c, e, t->fields[i].type);
            }
        }
        return;
    default:
        return;
    }
}

bool sema_hash_call(struct checker *c, struct expr *e, struct type *t,
                    struct type **result)
{
    struct type *hashed = t;

    if (t->kind == TYPE_PARAM) {
        /* Without the hook, an interface of the constraints that
           declares `hash` is reached as any function of one. */
        if (!sema_param_has(t, LANG_HOOK_HASH) &&
            sema_param_iface(t, &hash_name) != NULL) {
            return false;
        }
        if (!sema_param_hash(c, e, t)) {
            *result = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        e->type = sema_builtin(c, TYPE_U64);
        *result = e->type;
        return true;
    }
    if (has_own_hash(c, t)) {
        return false;
    }
    if (t->kind == TYPE_POINTER && !t->nullable && type_has_fields(t->element)) {
        hashed = t->element;
    }
    e->as.call.hashes = true;
    e->as.call.hash_calls = NULL;
    e->as.call.hash_count = 0;
    if (hashed->kind != TYPE_POINTER) {
        walk(c, e, hashed);
    }
    e->type = sema_builtin(c, TYPE_U64);
    *result = e->type;
    return true;
}

bool sema_is_hash_call(const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;

    return e->kind == EXPR_CALL && e->as.call.arg_count == 0 &&
           callee->kind == EXPR_FIELD &&
           sema_name_is(&callee->as.field.name, LANG_HOOK_HASH) &&
           (e->as.call.hashes ||
            (callee->as.field.base->type != NULL &&
             callee->as.field.base->type->kind == TYPE_PARAM));
}

/* The default `==` */

/* The value of type t that a hidden operand stands for, `*p`. It is
   checked already and never lowered. */
static struct expr *stand_in(struct checker *c, struct pos pos,
                             struct type *t)
{
    struct expr *hole = sema_new_node(c, EXPR_NONE, pos);
    struct expr *at = sema_new_node(c, EXPR_UNARY, pos);

    hole->type = types_pointer(c->types, t);
    hole->prechecked = true;
    at->as.unary.op = TOKEN_STAR;
    at->as.unary.operand = hole;
    at->type = t;
    at->prechecked = true;
    return at;
}

/* The checked call of the `operator fn eq` of type t that compares two
   values of it inside the operands of a default `==`. Lowering reads
   the function the callee names, and a copy of a generic names its copy
   there. */
static struct expr *nested_eq(struct checker *c, struct pos pos,
                              struct type *t)
{
    struct expr *e = sema_new_node(c, EXPR_BINARY, pos);

    e->as.binary.op = TOKEN_EQ;
    e->as.binary.left = stand_in(c, pos, t);
    e->as.binary.right = stand_in(c, pos, t);
    if (sema_is_error(sema_check_expr(c, e, NULL)) || e->kind != EXPR_CALL) {
        return NULL;
    }
    return e;
}

static void walk_eq(struct checker *c, struct expr *e, struct type *t);

/* Walk each part of the struct, tuple or case t with walk_eq. */
static void walk_parts(struct checker *c, struct expr *e, struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        if (!type_field_is_unit_break(&t->fields[i])) {
            walk_eq(c, e, t->fields[i].type);
        }
    }
}

/* Walk what the default `==` of a value of type t compares. That is the
   parts of a struct or a tuple and the fields of each case of a variant.
   It is also the value a `?T` holds and the elements of an array. A class
   value calls the `equals` of its chain, which lowering finds in its
   members. */
static void walk_inside(struct checker *c, struct expr *e, struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_STRUCT:
        if (!t->is_union) {
            walk_parts(c, e, t);
        }
        return;
    case TYPE_TUPLE:
        walk_parts(c, e, t);
        return;
    case TYPE_VARIANT:
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != NULL) {
                walk_parts(c, e, t->params[i]);
            }
        }
        return;
    case TYPE_OPTIONAL:
    case TYPE_ARRAY:
        walk_eq(c, e, t->element);
        return;
    default:
        return;
    }
}

/* Give the default `==` e the call of the `operator fn eq` of a part of
   type t, once per type. A part without one is walked inside. A part of
   a type parameter is read again in each copy. */
static void walk_eq(struct checker *c, struct expr *e, struct type *t)
{
    struct expr *call;

    if ((t->kind == TYPE_STRUCT || t->kind == TYPE_CLASS ||
         t->kind == TYPE_VARIANT) &&
        sema_operator_symbol(c, t, LANG_HOOK_EQ) != NULL) {
        if (listed(e->as.binary.eq_calls, e->as.binary.eq_count, t)) {
            return;
        }
        call = nested_eq(c, e->pos, t);
        if (call != NULL) {
            append(c, &e->as.binary.eq_calls, &e->as.binary.eq_count, call);
        }
        return;
    }
    walk_inside(c, e, t);
}

/* Whether a part of type t of a struct, a tuple or a variant has `==`. A
   slice part has it and compares as the view it is, by its address and
   its length. An array part has it when its elements have it, and
   compares element by element. */
static bool part_has_eq(struct checker *c, struct type *t)
{
    if (t->kind == TYPE_ARRAY) {
        return part_has_eq(c, t->element);
    }
    return t->kind == TYPE_SLICE || sema_meets_hook(c, t, LANG_HOOK_EQ);
}

const struct struct_field *sema_eq_gap(struct checker *c, struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (type_field_is_unit_break(f) || types_is_mutex(f->type)) {
            continue;
        }
        if (!part_has_eq(c, f->type)) {
            return f;
        }
    }
    return NULL;
}

/* Whether a value of type t holds a union in place, directly, in a
   struct or in an array. A class value compares through its own
   `equals`. */
static bool holds_union(const struct type *t)
{
    size_t i;

    while (t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    if (t->kind != TYPE_STRUCT) {
        return false;
    }
    if (t->is_union) {
        return true;
    }
    for (i = 0; i < t->field_count; i++) {
        if (!type_field_is_unit_break(&t->fields[i]) &&
            holds_union(t->fields[i].type)) {
            return true;
        }
    }
    return false;
}

/* Whether a class of the chain of t below the root replaces `equals`. */
static bool own_equals(const struct type *t)
{
    static const struct name equals = {"equals", 6};
    size_t i;

    for (; t != NULL && t->base != NULL; t = t->base) {
        for (i = 0; i < t->member_count; i++) {
            if (t->members[i]->kind == ITEM_FN &&
                sema_same_name(&t->members[i]->name, &equals)) {
                return true;
            }
        }
    }
    return false;
}

const struct struct_field *sema_class_gap(const struct type *t)
{
    size_t i;

    if (own_equals(t)) {
        return NULL;
    }
    for (; t != NULL; t = t->base) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                holds_union(f->type)) {
                return f;
            }
        }
    }
    return NULL;
}

/* DESIGN: a struct and a class get a default `==` and no default `<`,
   since what order means is the type's own choice. The default of a
   class value is the `equals` of its class, which the class replaces
   with `concrete fn equals`. That of a struct compares its fields in
   order, each with its own `==`, so it keeps the rule that values equal
   by `eq` hash alike wherever each field keeps it. A union holds one
   field and says not which, a simd struct compares lane by lane, and a
   Mutex is no value, so none of the three has the default. A class
   that holds a union in place has none either, since the `equals` of
   `Object` walks every other field as this default does. A tuple, a
   variant and a `?T` have it when every part has `==`: a tuple part by
   part, a variant by its case and then the fields of that case, a `?T`
   by its flag and then the value it holds. An array part compares
   element by element. */
bool sema_default_eq(struct checker *c, struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        return sema_class_gap(t) == NULL;
    case TYPE_STRUCT:
        return !t->is_union && !type_is_simd(t) && !types_is_mutex(t) &&
               sema_eq_gap(c, t) == NULL;
    case TYPE_TUPLE:
        return sema_eq_gap(c, t) == NULL;
    case TYPE_VARIANT:
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != NULL && sema_eq_gap(c, t->params[i]) != NULL) {
                return false;
            }
        }
        return true;
    case TYPE_OPTIONAL:
        return sema_meets_hook(c, t->element, LANG_HOOK_EQ);
    default:
        return false;
    }
}

bool sema_equals(struct checker *c, struct expr *e, struct type *t)
{
    if (!sema_default_eq(c, t)) {
        return false;
    }
    e->as.binary.equals = true;
    e->as.binary.eq_calls = NULL;
    e->as.binary.eq_count = 0;
    walk_inside(c, e, t);
    return true;
}

const char *sema_no_order(const struct type *t, const char *hook)
{
    if (strcmp(hook, LANG_HOOK_LT) != 0 ||
        (t->kind != TYPE_STRUCT && t->kind != TYPE_CLASS)) {
        return "";
    }
    return ". A struct or a class has no default order and declares "
           "`operator fn lt`";
}
