/* The checker's part of hashing: `x.hash()` on every type. A class and a
   struct or variant with `operator fn hash` have a function of their
   own, which the call of a method reaches. Every other type has the
   default hash, which lowering writes from the type. A value of a type
   parameter has the hook when its constraints name `hash`. */

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
   and is never lowered: lowering reads the function the callee names,
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

/* Add the call of the function of t to e, once per type. */
static void add_call(struct checker *c, struct expr *e, struct type *t)
{
    struct expr **calls;
    struct expr *call;
    size_t i;

    for (i = 0; i < e->as.call.hash_count; i++) {
        if (types_hash_call_type(e->as.call.hash_calls[i]) == t) {
            return;
        }
    }
    call = nested_call(c, e->pos, t);
    if (call == NULL) {
        return;
    }
    calls = types_alloc_array(c->arena, e->as.call.hash_count + 1,
                              sizeof *calls);
    if (e->as.call.hash_count > 0) {
        memcpy(calls, e->as.call.hash_calls,
               e->as.call.hash_count * sizeof *calls);
    }
    calls[e->as.call.hash_count++] = call;
    e->as.call.hash_calls = calls;
}

/* Walk the value of type t that the default hash of the call e reaches,
   and give e the call of every function of a struct or a variant inside
   it. A class value calls the `hash` of its chain, which lowering finds
   in its members. A pointer is hashed by its address and reaches
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
