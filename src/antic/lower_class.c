/* The functions lowering writes for a class. The init of a class with
   the defaults of its fields, `construct` and the run of its bodies, the
   `get` of a singleton, the thunks of interfaces and of promoted fields,
   and the interface tables a new object holds. */

#include <stdlib.h>

#include "alloc.h"
#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: the slot of an injectable interface is one pointer per
   interface, of the runtime module and named INJECT_SLOT_PREFIX and
   the path of the interface. It holds the provider the build chose.
   The pass over the whole program writes it. One program then holds
   one slot per interface whichever module injects it, and the run-time
   configuration has one pointer to replace. Every module that injects
   refers to it. */
static struct ir_global *inject_slot(struct lowerer *l, const struct type *t)
{
    struct ir_module *m = l->m;
    struct text name = {0};
    struct ir_global *g;

    text_appendf(&name, INJECT_SLOT_PREFIX "%.*s.%.*s",
                 (int)t->module.length, t->module.text,
                 (int)t->name.length, t->name.text);
    g = lower_find_global(m, RUNTIME_MODULE, text_cstr(&name));
    if (g == NULL) {
        g = ir_global_add(m, RUNTIME_MODULE, text_cstr(&name), NULL, 0, 1);
        g->is_extern = true;
        g->mutable = true;
    }
    text_free(&name);
    return g;
}

/* Whether field has a default, declared here or read from a library
   file, or takes `T { }` as an inline class value. An `inject` field
   has none and is written all the same, by the provider of its
   interface. */
bool lower_has_default(const struct struct_field *field)
{
    return field->injected || field->value != NULL ||
           field->constant != NULL || sema_field_takes_literal(field);
}

/* DESIGN: every complete class has a function that prepares an object
   allocated elsewhere. It stores the table pointers and writes the
   default of every field of the chain, as a literal of the class does.
   Then it runs construct when that takes no arguments. An export
   class gives it to C as `anti_<Class>_init`. The registry that
   `reflect.new` reads names it for the others as `<Class>.init`. An
   abstract class has no complete value and therefore none. */
void lower_init_name(const struct type *t, bool exported, struct text *out)
{
    if (exported) {
        text_appendf(out, "anti_%.*s_init", (int)types_c_name(t).length,
                     types_c_name(t).text);
        return;
    }
    types_symbol_name(out, t);
    text_append(out, ".init");
}

/* The init function of class t, declared in the module that declares t
   and referred to from any other. */
static struct ir_function *lower_init_function(struct lowerer *l,
                                               const struct type *t)
{
    char *module = lower_cstr(&t->module);
    struct ir_function *f;
    struct text name = {0};

    lower_init_name(t, t->item_exported, &name);
    f = lower_find_function(l->m, module, text_cstr(&name));
    if (f == NULL) {
        f = lower_defines(l, t, module)
                ? ir_function_add(l->m, module, text_cstr(&name), IR_VOID,
                                  IR_NO_AGG)
                : ir_declare_add(l->m, module, text_cstr(&name), IR_VOID,
                                 IR_NO_AGG);
        f->exported = t->item_exported;
        ir_param_add(f, IR_PTR, IR_NO_AGG);
    }
    text_free(&name);
    free(module);
    return f;
}

/* The scalar default of field. The declaring module lowers the
   expression, and any other module the value its library file carries. */
static struct ir_operand default_scalar(struct lowerer *l,
                                        const struct struct_field *field)
{
    if (field->value != NULL) {
        return lower_expr(l, field->value);
    }
    return lower_constant(l, field->constant, lower_ir_type_of(field->type));
}

/* Put the default of field at address. */
static void store_default(struct lowerer *l, const struct struct_field *field,
                          struct ir_operand address)
{
    /* DESIGN: an `inject` field is filled by a call through the slot of
       its interface, before `construct` runs. The call is indirect, so
       a replacement of the slot reaches every site. */
    if (field->injected) {
        struct ir_operand slot =
            lower_temp(l, ir_addr(l->f, l->b, ir_global_op(inject_slot(
                                      l, field->type->element))));
        struct ir_operand provider =
            lower_temp(l, ir_load(l->f, l->b, IR_PTR, slot));
        struct ir_operand object =
            lower_temp(l, ir_call_indirect(l->f, l->b, IR_PTR, provider,
                                           lower_provider_signature(l), NULL,
                                           0));
        ir_store(l->f, l->b, IR_PTR, object, address);
        return;
    }
    /* A Mutex, and the hidden lock of a synchronized class, start free. */
    if (types_is_mutex(field->type) || types_is_object_lock(field->type)) {
        lower_zero_lock(l, field->type, address);
        return;
    }
    /* An inline class field without a default is written as `T { }`
       would write it, which the init of T does. */
    if (field->value == NULL && field->constant == NULL) {
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_init_function(l, field->type)),
                &address, 1);
        return;
    }
    if (field->value != NULL) {
        lower_store_value(l, field->type, field->value, address);
        return;
    }
    lower_store_constant(l, field->type, field->constant, address);
}

/* Put the default of field i of owner into the object at object. A
   bitfield goes into its unit and every other field to its offset. The
   object's address is the address of every class of its chain. */
void lower_store_field_default(struct lowerer *l, const struct type *owner,
                               size_t i, struct ir_operand object)
{
    const struct struct_field *field = &owner->fields[i];

    if (field->bits != 0) {
        struct ir_operand v = default_scalar(l, field);
        ir_bitstore(l->f, l->b, lower_ir_type_of(field->type), v, object,
                    lower_agg_of(l, owner), (uint32_t)i);
        return;
    }
    store_default(l, field,
                  lower_offset_address(l, object,
                                       lower_field_offset(l, owner,
                                                          &field->name)));
}

/* DESIGN: a singleton keeps its one instance in an atomic global of its
   own module. `get` reads it, and on the first call it builds an object
   and puts it there with a compare and swap. A second thread that lost
   the race frees what it made and reads the winner. */
static struct ir_global *singleton_instance(struct lowerer *l,
                                            const struct type *t)
{
    struct ir_const *value;
    struct ir_global *g;
    char *module;
    char *name;

    g = lower_class_global(l, t, "instance", &module, &name);
    if (g != NULL) {
        return g;
    }
    value = arena_alloc(l->m->arena, sizeof *value);
    value->kind = IR_CONST_INT;
    value->scalar = IR_PTR;
    value->integer = 0;
    g = ir_global_add_value(l->m, module, name, value);
    g->mutable = true;
    g->exported = t->item_exported;
    free(module);
    free(name);
    return g;
}

void lower_singleton_get(struct lowerer *l, const struct item *it)
{
    const struct type *t = it->owner->symbol->type;
    struct ir_operand address;
    struct ir_operand args[4];
    struct ir_operand made;
    struct ir_operand first;
    struct ir_operand swapped;
    struct ir_block *build;
    struct ir_block *lost;
    struct ir_block *done;
    uint32_t result;
    uint32_t width;

    /* A function a library file brought has its body there. */
    if (it->symbol->ir < l->first_function) {
        return;
    }

    l->f = l->m->functions[it->symbol->ir];
    l->b = lower_new_block(l);
    address = lower_temp(l, ir_addr(l->f, l->b,
                                    ir_global_op(singleton_instance(l, t))));
    width = ir_sym_size_of(l->m, ir_scalar(IR_PTR));
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    /* The runtime gives an i64 back, and a pointer is the same bits, so
       the call takes the pointer type with no conversion between. */
    first = lower_rt_call_as(l, RT_FN_ATOMIC_LOAD, IR_PTR, args);
    result = ir_unary(l->f, l->b, IR_COPY, IR_PTR, first);
    build = lower_new_block(l);
    lost = lower_new_block(l);
    done = lower_new_block(l);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, first,
                                      ir_int_op(IR_PTR, 0))),
              build, done);
    l->b = build;
    args[0] = lower_size_operand(l, t);
    made = lower_new_memory(l, args[0]);
    lower_prepare_object(l, t, NULL, made);
    lower_run_construct(l, t, made);
    ir_assign(l->f, l->b, result, made);
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    args[2] = ir_int_op(IR_PTR, 0);
    args[3] = made;
    swapped = lower_rt_call(l, RT_FN_ATOMIC_COMPARE_SWAP, args);
    ir_branch(l->f, l->b, swapped, done, lost);
    l->b = lost;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_c_function(l, "free", IR_VOID, IR_PTR)), &made, 1);
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    ir_assign(l->f, l->b, result,
              lower_rt_call_as(l, RT_FN_ATOMIC_LOAD, IR_PTR, args));
    ir_jump(l->f, l->b, done);
    l->b = done;
    ir_ret(l->f, l->b, IR_PTR, lower_temp(l, result));
}

void lower_class_init(struct lowerer *l, const struct item *it)
{
    const struct type *t = it->symbol->type;
    struct ir_function *f;
    struct ir_block *entry;
    struct ir_operand self;

    f = lower_init_function(l, t);
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;
    self = lower_temp(l, f->params[0].temp);
    lower_prepare_object(l, t, NULL, self);
    lower_run_construct(l, t, self);
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* DESIGN: an export class whose `construct` takes arguments gives C
   `anti_<Class>_construct(self, args...)`, the counterpart of
   `Class(args)`. It prepares self as the init of the class does, then
   runs `construct` with the arguments and returns what that returns: the
   error of one that may fail, and nothing otherwise. C then needs no
   call of the init first, and cannot forget one. */
void lower_class_construct(struct lowerer *l, const struct item *it)
{
    static const struct name construct_name = {"construct", 9};
    const struct type *t = it->symbol->type;
    const struct item *m = NULL;
    const struct type *sig;
    struct ir_function *target;
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_operand self;
    uint32_t value;
    struct text name = {0};
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *c = t->members[i];
        if (c->kind == ITEM_FN && lower_same_name(&c->name, &construct_name) &&
            c->symbol != NULL && lower_has_body(c) && c->param_count > 0) {
            m = c;
        }
    }
    if (m == NULL) {
        return;
    }
    sig = m->symbol->type;
    target = lower_callee_function(l, m->symbol);
    text_appendf(&name, "anti_%.*s_construct", (int)types_c_name(t).length,
                 types_c_name(t).text);
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result), IR_NO_AGG);
    text_free(&name);
    f->exported = true;
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    ir_call(l->f, l->b, IR_VOID, ir_func_op(lower_init_function(l, t)), &self,
            1);
    args = alloc_zeroed(f->param_count, sizeof *args);
    for (i = 0; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    value = ir_call(l->f, l->b, lower_ir_type_of(sig->result),
                    ir_func_op(target),
                    args, f->param_count);
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(l->f, l->b, IR_VOID, lower_none());
    } else {
        ir_ret(l->f, l->b, IR_PTR, lower_temp(l, value));
    }
}

/* Branch to a new block when the class value at p has a table, and give
   the block after it, where both paths meet. A place that `=` fills has a
   zero table when it is an element of `alloc(T, n)` never filled. It
   holds nothing. */
struct ir_block *lower_when_made(struct lowerer *l, struct ir_operand p)
{
    struct ir_block *made = lower_new_block(l);
    struct ir_block *after = lower_new_block(l);
    struct ir_operand table = lower_temp(l, ir_load(l->f, l->b, IR_PTR, p));

    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, table,
                                      ir_int_op(IR_PTR, 0))),
              made, after);
    l->b = made;
    return after;
}

/* The public function `name` of t or of a class above it. */
const struct item *lower_find_member_fn(const struct type *t,
                                        const struct name *name)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind == ITEM_FN && m->pub && m->type_param_count == 0 &&
                lower_same_name(&m->name, name)) {
                return m;
            }
        }
    }
    return NULL;
}

/* DESIGN: an interface table holds thunks. A thunk takes `self` as a
   pointer to the sub-object. It subtracts the offset of that sub-object
   to reach the object, then calls the class's own function. Every
   function is written once. It serves the direct call and the call
   through the interface alike. */
struct ir_function *lower_interface_thunk(struct lowerer *l,
                                          const struct type *t,
                                          const struct struct_field *sub,
                                          const struct item *fn)
{
    const struct type *sig = fn->symbol->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *target = lower_callee_function(l, fn->symbol);
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_block *entry;
    struct ir_operand back;
    uint32_t value;
    struct text name = {0};
    size_t i;

    types_symbol_name(&name, t);
    text_appendf(&name, ".%.*s.%.*s.thunk", (int)sub->name.length,
                 sub->name.text, (int)fn->name.length, fn->name.text);
    f = lower_find_function(l->m, l->module_name, text_cstr(&name));
    if (f != NULL) {
        text_free(&name);
        return f;
    }
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result),
                        lower_result_agg(l, sig->result));
    text_free(&name);
    f->result_agg = lower_result_agg(l, sig->result);
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;
    /* The thunk passes on each IR parameter as it came, so a parameter
       of two words passes as two. */
    args = alloc_zeroed(f->param_count + 1, sizeof *args);
    back = lower_temp(l, ir_binary(f, entry, IR_SUB, IR_I64,
                                   ir_int_op(IR_I64, 0),
                                   lower_field_offset(l, sub->home,
                                                      &sub->name)));
    args[0] = lower_temp(l,
                         ir_ptradd(f, entry, lower_temp(l, f->params[0].temp),
                                   back));
    for (i = 1; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    value = ir_call(f, entry, lower_ir_type_of(sig->result),
                    ir_func_op(target), args, f->param_count);
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(f, entry, IR_VOID, lower_none());
    } else {
        /* An aggregate result travels as the address of its storage,
           which is what the called function already returned. */
        ir_ret(f, entry, f->result == IR_AGG ? IR_PTR : f->result,
               lower_temp(l, value));
    }
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `T.f` of a body qualified by an interface is a function of
   its own, which the module that names it writes. It moves the object to
   the sub-object, reads the entry of its table and calls that. A class
   below that replaces the body is therefore honoured. The call names the
   interface and the slot, as every call through a table does. */
struct ir_function *lower_reach_thunk(struct lowerer *l,
                                      const struct struct_field *sub,
                                      const struct symbol *sym)
{
    const struct type *sig = sym->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    size_t index = lower_table_index(sub->type, &sym->item->name,
                                     sig->param_count);
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_operand table;
    struct ir_operand target;
    struct ir_inst *call;
    uint32_t value;
    struct text name = {0};
    size_t i;

    types_symbol_name(&name, sub->home);
    text_appendf(&name, ".%.*s.%.*s.reach", (int)sub->name.length,
                 sub->name.text, (int)sym->item->name.length,
                 sym->item->name.text);
    f = lower_find_function(l->m, l->module_name, text_cstr(&name));
    if (f != NULL) {
        text_free(&name);
        return f;
    }
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result),
                        lower_result_agg(l, sig->result));
    text_free(&name);
    f->result_agg = lower_result_agg(l, sig->result);
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    l->f = f;
    l->b = ir_block_add(f);
    args = alloc_zeroed(f->param_count + 1, sizeof *args);
    args[0] = lower_offset_address(l, lower_temp(l, f->params[0].temp),
                                   lower_field_offset(l, sub->home,
                                                      &sub->name));
    for (i = 1; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    table = lower_load_table(l, args[0], sub->type);
    target = lower_temp(
        l, ir_load_access(l->f, l->b,
                          lower_offset_address(l, table,
                                               lower_entry_offset(l, index)),
                          IR_ACCESS_ENTRY));
    value = ir_call_indirect(l->f, l->b, lower_ir_type_of(sig->result), target,
                             lower_signature(l, sig), args, f->param_count);
    call = &l->b->insts[l->b->count - 1];
    call->c = ir_global_op(lower_class_descriptor(l, sub->type));
    call->field = (uint32_t)index;
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(l->f, l->b, IR_VOID, lower_none());
    } else {
        /* An aggregate result travels as the address of its storage,
           which is what the called function already returned. */
        ir_ret(l->f, l->b, f->result == IR_AGG ? IR_PTR : f->result,
               lower_temp(l, value));
    }
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `construct` runs after a literal has written every field,
   base first down the chain. A base therefore sees its own fields
   before the class below it adds to them. A class without one adds
   nothing. */
static void lower_run_construct_bodies(struct lowerer *l, const struct type *t,
                                       struct ir_operand dest)
{
    static const struct name construct_name = {"construct", 9};
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS) {
        return;
    }
    lower_run_construct_bodies(l, t->base, dest);
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind != ITEM_FN || !lower_same_name(&m->name, &construct_name) ||
            m->symbol == NULL || !lower_has_body(m) ||
            m->symbol->type->param_count != 1) {
            continue;
        }
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_callee_function(l, m->symbol)), &dest, 1);
    }
}

/* The bodies of the chain, and then the `created` hook of the object
   the literal built. One object gives one hook, whatever its chain
   declares. */
void lower_run_construct(struct lowerer *l, const struct type *t,
                         struct ir_operand dest)
{
    lower_run_construct_bodies(l, t, dest);
    if (t != NULL && t->kind == TYPE_CLASS) {
        lower_hook_object(l, HOOK_CREATED, dest);
    }
}

/* Store the table pointer of every interface sub-object of t into the
   object at dest. */
void lower_store_interface_tables(struct lowerer *l, const struct type *t,
                                  struct ir_operand dest)
{
    const struct type *up;
    size_t i;

    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *field = &up->fields[i];
            struct ir_operand table;
            struct ir_operand at;

            if (field->form != FIELD_IMPL) {
                continue;
            }
            /* The table is made before the block is read: the order of
               the arguments of a C call is unspecified. */
            struct ir_global *made = lower_interface_table(l, t, field);
            table = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(made)));

            at = lower_offset_address(l, dest,
                                      lower_field_offset(l, up, &field->name));
            ir_store_access(l->f, l->b, table, at, IR_ACCESS_TABLE);
        }
    }
}

/* DESIGN: `T(args)` writes the table pointers and the defaults of the
   whole chain. Then it runs every `construct` without arguments, base
   first, and last the one the class declares with the arguments. The
   object is complete before its own body sees it. */
struct ir_operand lower_construct(struct lowerer *l,
                                  const struct expr *e,
                                  struct ir_operand dest)
{
    static const struct name construct_name = {"construct", 9};
    const struct type *t = e->as.call.builds;
    struct ir_operand *args;
    struct ir_operand *values;
    const struct item *m = NULL;
    uint32_t result;
    bool fails;
    size_t count;
    size_t i;

    lower_prepare_object(l, t, NULL, dest);
    if (t->base != NULL) {
        lower_run_construct_bodies(l, t->base, dest);
    }
    for (i = 0; i < t->member_count; i++) {
        if (t->members[i]->kind == ITEM_FN &&
            lower_same_name(&t->members[i]->name, &construct_name)) {
            m = t->members[i];
        }
    }
    if (m == NULL || m->symbol == NULL) {
        lower_hook_object(l, HOOK_CREATED, dest);
        return lower_none();
    }
    args = alloc_zeroed(alloc_sum(alloc_product(e->as.call.arg_count, 2), 1),
                        sizeof *args);
    values = alloc_zeroed(alloc_sum(e->as.call.arg_count, 1), sizeof *values);
    args[0] = dest;
    count = 1;
    /* An argument at a parameter of the form of two words, a `keep own`
       one among them, passes as two words, as at any call. */
    for (i = 0; i < e->as.call.arg_count; i++) {
        const struct type *sig = m->symbol->type;
        struct ir_operand value = lower_argument(l, e->as.call.args[i]);
        values[i] = value;
        lower_push_argument(l, args, &count, value,
                            i + 1 < sig->param_count ? sig->params[i + 1]
                                                     : NULL);
    }
    /* A `construct` that may fail returns `?*Error`, which the checker
       gave its type. One that cannot fail returns nothing. */
    fails = m->symbol->type->result->kind != TYPE_VOID;
    result = ir_call(l->f, l->b, fails ? IR_PTR : IR_VOID,
                     ir_func_op(lower_callee_function(l, m->symbol)), args,
                     count);
    free(args);
    /* The arguments follow `self`, the first parameter. */
    lower_drop_arguments(l, m->symbol, m->symbol->type,
                         (const struct expr *const *)e->as.call.args, values,
                         e->as.call.arg_count, 1);
    free(values);
    if (!fails) {
        lower_hook_object(l, HOOK_CREATED, dest);
        return lower_none();
    }
    /* A `construct` that failed leaves no object, and its memory goes
       back before the handler runs, so the hook is the success path's. */
    if (l->hooks && l->b != NULL) {
        struct ir_block *made = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                          lower_temp(l, result),
                                          ir_int_op(IR_PTR, 0))),
                  made, after);
        l->b = made;
        lower_hook_object(l, HOOK_CREATED, dest);
        ir_jump(l->f, l->b, after);
        l->b = after;
    }
    return lower_temp(l, result);
}
