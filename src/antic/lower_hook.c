/* The sites of the hooks in lowering. Which classes are traced, the calls
   of a hook with its handler, and the `changed` hook of a field that an
   assignment writes. */

#include <string.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: a hook site is one call of the runtime with the object and the
   hook. The runtime holds the handler, the order of the two calls and
   the compare against the root's empty body. The order then stands in
   one place and the compiler writes no branch. `--no-hooks` drops every
   site, the five always-on ones as well. */
void lower_hook_object(struct lowerer *l, enum hook_kind hook,
                       struct ir_operand object)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_operand args[2];

    if (!l->hooks || l->b == NULL || object.kind == IR_NONE) {
        return;
    }
    args[0] = object;
    args[1] = ir_int_op(IR_I64, (uint64_t)hook);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook", params, 2)), args,
            2);
}

/* Whether the `--trace` pattern names the module path of the class, a
   package above it, or the class itself. */
static bool pattern_names(const char *pattern, const struct type *t)
{
    size_t length = strlen(pattern);

    if (t->name.length == length &&
        memcmp(t->name.text, pattern, length) == 0) {
        return true;
    }
    if (t->module.length == length &&
        memcmp(t->module.text, pattern, length) == 0) {
        return true;
    }
    return t->module.length > length && t->module.text[length] == '.' &&
           memcmp(t->module.text, pattern, length) == 0;
}

/* DESIGN: a class is instrumented when it asked for the call hooks with
   the contextual `trace` and the build compiles marked code. A `--trace`
   pattern that names its package or itself instruments it as well, which
   reaches code that did not ask. The library file carries the marking,
   so a class of another module answers the same question. */
bool lower_traced_class(const struct lowerer *l, const struct type *t)
{
    size_t i;

    if (!l->hooks || t == NULL || t->kind != TYPE_CLASS) {
        return false;
    }
    if (l->trace_marked && t->traced) {
        return true;
    }
    for (i = 0; i < l->pattern_count; i++) {
        if (pattern_names(l->patterns[i], t)) {
            return true;
        }
    }
    return false;
}

/* The `enter` or the `leave` hook of the function being lowered, which
   names itself. A function that is not instrumented writes none. */
void lower_hook_call(struct lowerer *l, enum hook_kind hook)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64, IR_PTR, IR_I64};
    struct ir_operand args[4];

    if (l->trace_name == NULL || l->b == NULL) {
        return;
    }
    args[0] = l->trace_self;
    args[1] = ir_int_op(IR_I64, (uint64_t)hook);
    args[2] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(l->trace_name)));
    args[3] = ir_int_op(IR_I64, (uint64_t)l->trace_name_length);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_call", params, 4)),
            args,
            4);
}

/* The `failed` hook, before the `leave` of an exit that gives an error. */
void lower_hook_failed(struct lowerer *l, struct ir_operand err)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR, IR_I64, IR_PTR};
    struct ir_operand args[4];

    if (l->trace_name == NULL || l->b == NULL ||
        err.kind == IR_NONE) {
        return;
    }
    args[0] = l->trace_self;
    args[1] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(l->trace_name)));
    args[2] = ir_int_op(IR_I64, (uint64_t)l->trace_name_length);
    args[3] = err;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_failed", params, 4)),
            args, 4);
}

/* The `copied` hook, after `dup` made the object at `made` out of the
   one at `from`. */
void lower_hook_copied(struct lowerer *l, struct ir_operand made,
                       struct ir_operand from)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    struct ir_operand args[2];

    if (!l->hooks || l->b == NULL || made.kind == IR_NONE) {
        return;
    }
    args[0] = made;
    args[1] = from;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_copied", params, 2)),
            args, 2);
}

/* The level of the chain of t that declares the field name, with the
   index of its record in the field list of that level. NULL when no
   level lists it, which a bitfield and a table field are. */
static const struct type *record_of_field(const struct type *t,
                                          const struct name *name,
                                          size_t *index)
{
    const struct type *up;
    size_t i;
    size_t n;

    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        n = 0;
        for (i = 0; i < up->field_count; i++) {
            if (!lower_listed_field(&up->fields[i])) {
                continue;
            }
            if (lower_same_name(&up->fields[i].name, name)) {
                *index = n;
                return up;
            }
            n++;
        }
    }
    return NULL;
}

/* DESIGN: the `changed` hook takes the record of the written field from
   the field list its descriptor carries. It reads what reflection
   already holds, and the compiler writes no record of its own.
   `--no-reflect` leaves the descriptor without the list, and the list
   itself is still written for the hook. */
void lower_hook_changed(struct lowerer *l, const struct place *p,
                        const struct expr *target)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    struct ir_operand args[2];
    struct ir_operand list;
    struct ir_operand at;
    const struct type *up;
    size_t index = 0;

    if (!l->trace_writes || l->b == NULL ||
        target->kind != EXPR_FIELD || p->object.kind == IR_NONE ||
        !lower_traced_class(l, p->owner)) {
        return;
    }
    up = record_of_field(p->owner, &target->as.field.name, &index);
    if (up == NULL) {
        return;
    }
    list = lower_temp(l, ir_addr(l->f, l->b,
                                 ir_global_op(lower_class_fields(l, up))));
    at = ir_sym_operand(l->m,
                        ir_sym_offset_of(l->m,
                                         lower_fields_agg(l,
                                                          lower_own_fields(up)),
                                         (uint32_t)index));
    args[0] = p->object;
    args[1] = lower_offset_address(l, list, at);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_changed", params, 2)),
            args, 2);
}
