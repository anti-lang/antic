/* Function bodies in lowering. A function of the module, its parameters,
   its trace and its lock, an anonymous function, and the context of a
   closure with the snapshot of a `snapshot fn`. */

#include <stdlib.h>
#include <string.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: a hook is never instrumented. `enter` and `leave` around a
   body of `enter` would call themselves without end, and a handler's
   nine are hooks as much as a class's own. The name decides, so a
   function of any signature under one of the nine names is left alone. */
/* Whether the function it takes the `enter`, `leave` and `failed` hooks.
   It is a `pub` function of an instrumented class, or a `trace fn` of
   one, and it has an object to hook. */
static bool traced_function(const struct lowerer *l, const struct item *it)
{
    const struct type *owner = it->owner != NULL && it->owner->symbol != NULL
                                   ? it->owner->symbol->type
                                   : NULL;

    if (!it->has_self || owner == NULL || owner->kind != TYPE_CLASS ||
        sema_root_hook(&it->name)) {
        return false;
    }
    if (l->hooks && l->trace_marked && it->trace) {
        return true;
    }
    return it->vis == VIS_PUB && lower_traced_class(l, owner);
}

/* The literal of the full name of the function, `module.Class.f`, which
   the `enter`, `leave` and `failed` hooks take. */
static void take_trace_name(struct lowerer *l)
{
    struct token_text text;
    struct text name = {0};

    ir_name_append(&name, l->f->module, l->f->name);
    text.bytes = text_cstr(&name);
    text.length = name.length;
    l->trace_name = lower_literal_global(l, &text);
    l->trace_name_length = (int64_t)name.length;
    text_free(&name);
}

static void snapshot_entry(struct lowerer *l, const struct item *it,
                           struct ir_block *entry);

/* Whether it is a function of a synchronized class that takes the hidden
   lock of its object. */
static bool synchronized_function(const struct item *it)
{
    return it->owner != NULL && it->owner->symbol != NULL &&
           it->owner->symbol->type->kind == TYPE_CLASS &&
           it->owner->symbol->type->safety == SAFETY_SYNCHRONIZED &&
           it->has_self && it->vis != VIS_PRIVATE &&
           !lower_name_is(&it->name, "construct") &&
           !lower_name_is(&it->name, "destruct");
}

/* The copy of a generic of another module, or an anonymous function of
   one, and so the source its lines name. NULL for any other function. */
static const char *home_file_of(const struct item *it)
{
    while (it->enclosing != NULL) {
        it = it->enclosing;
    }
    return it->home_file;
}

static void lower_function_body(struct lowerer *l, const struct item *it);

void lower_function(struct lowerer *l, const struct item *it)
{
    const char *home = home_file_of(it);
    const char *file = l->file;
    uint32_t file_index = l->file_index;

    /* A function a library file brought has its body there. */
    if (it->symbol->ir < l->first_function) {
        return;
    }
    if (home != NULL) {
        l->file = home;
        l->file_index = ir_file_add(l->m, home);
        l->m->functions[it->symbol->ir]->file = l->file_index;
    }
    lower_function_body(l, it);
    l->file = file;
    l->file_index = file_index;
}

static uint32_t lower_captures_agg(struct lowerer *l, const struct item *it);

static void lower_function_body(struct lowerer *l, const struct item *it)
{
    struct defers around;
    struct ir_block *entry;
    size_t first;
    size_t at;
    size_t i;

    l->f = l->m->functions[it->symbol->ir];
    l->f->decl_line = (uint32_t)it->pos.line;
    l->loop = NULL;
    l->defers = NULL;
    l->trace_name = NULL;
    l->failing_error = lower_none();
    l->may_fail = it->may_fail;
    l->result_out = lower_none();
    l->temp_count = 0;
    entry = lower_new_block(l);
    l->b = entry;
    /* DESIGN: `self` is the first IR parameter of a member function
       and has no entry in the declared list. Every declared parameter
       therefore sits one place further along. One that does not keep its
       argument takes two places, which a slot joins into its pair. */
    first = it->has_self ? 1 : 0;
    if (it->self != NULL) {
        it->self->ir = l->f->params[0].temp;
        /* A closure captures `self` by its address. */
        if (it->self->address_taken) {
            it->self->ir = ir_slot(l->f, entry, ir_scalar(IR_PTR));
        }
    }
    at = first;
    for (i = 0; i < it->param_count; i++) {
        struct symbol *sym = it->params[i].symbol;
        sym->ir = l->f->params[at].temp;
        if (lower_is_context(sym->type) ||
            (sym->address_taken && !lower_is_aggregate(sym->type))) {
            sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        }
        at += lower_is_context(sym->type) ? 2 : 1;
    }
    lower_reserve_slots(l, entry, it->body);
    if (it->self != NULL && it->self->address_taken) {
        ir_store(l->f, entry, IR_PTR, lower_temp(l, l->f->params[0].temp),
                 lower_temp(l, it->self->ir));
    }
    at = first;
    for (i = 0; i < it->param_count; i++) {
        const struct symbol *sym = it->params[i].symbol;
        if (lower_is_context(sym->type)) {
            ir_store(l->f, entry, IR_PTR, lower_temp(l, l->f->params[at].temp),
                     lower_temp(l, sym->ir));
            ir_store(l->f, entry, IR_PTR,
                     lower_temp(l, l->f->params[at + 1].temp),
                     lower_offset_address(
                         l, lower_temp(l, sym->ir),
                         ir_sym_operand(l->m,
                                        ir_sym_offset_of(
                                            l->m, lower_agg_of(l, sym->type),
                                            1))));
            at += 2;
            continue;
        }
        if (sym->address_taken && !lower_is_aggregate(sym->type)) {
            ir_store(l->f, entry, l->f->params[at].type,
                     lower_temp(l, l->f->params[at].temp),
                     lower_temp(l, sym->ir));
        }
        at++;
    }
    /* The out pointer of a `may fail` function with a result follows the
       parameters the declaration wrote, which is the ABI its callers
       already pass. */
    if (it->may_fail && at < l->f->param_count &&
        it->symbol->type->has_out) {
        l->result_out = lower_temp(l, l->f->params[at].temp);
    }
    /* DESIGN: the context of a closure is its last parameter. It holds
       the address of every variable the closure captures, in the order
       of the captures. Each name reaches its variable through the
       address loaded here. */
    if (it->capture_count > 0 && it->snapshot) {
        snapshot_entry(l, it, entry);
    } else if (it->capture_count > 0) {
        struct ir_operand context =
            lower_temp(l, l->f->params[l->f->param_count - 1].temp);
        uint32_t agg = lower_captures_agg(l, it);
        for (i = 0; i < it->capture_count; i++) {
            it->captures[i].symbol->ir = ir_load(
                l->f, entry, IR_PTR,
                lower_offset_address(
                    l, context,
                    i == 0 ? lower_zero()
                           : ir_sym_operand(l->m,
                                            ir_sym_offset_of(l->m, agg,
                                                             (uint32_t)i))));
        }
    }
    l->b = entry;
    /* DESIGN: the `enter` hook stands before the first statement and the
       `leave` hook is an exit action of a scope around the whole body,
       so every `return`, every `fail` and the closing brace run it, and
       it runs after the locals of the body are gone. */
    memset(&around, 0, sizeof around);
    l->defers = &around;
    for (i = 0; i < it->param_count; i++) {
        const struct symbol *sym = it->params[i].symbol;
        if (sym->own_param ||
            (sym->type->kind == TYPE_FN && sym->type->owned)) {
            lower_push_own_action(l, sym);
        }
    }
    if (traced_function(l, it)) {
        take_trace_name(l);
        l->trace_self = lower_temp(l, l->f->params[0].temp);
        lower_hook_call(l, HOOK_ENTER);
        lower_push_leave_action(l);
    }
    /* DESIGN: code outside a synchronized class calls a function of it,
       which takes the hidden lock of its object when it starts. Every
       exit gives it back, as the unlock of `sync` does. A
       private function runs inside the lock of the one that called it,
       and `construct` and `destruct` run where no other thread sees the
       object. */
    if (synchronized_function(it)) {
        lower_hold_lock(l,
                        lower_object_lock_address(
                            l, it->owner->symbol->type,
                            lower_temp(l, l->f->params[0].temp)),
                        true, it->pos.line);
    }
    lower_block(l, it->body);
    if (l->b != NULL) {
        lower_run_defers(l, &around, false);
    }
    l->defers = NULL;
    free(around.items);
    /* Semantic analysis rejects a function with a result that can reach
       its end, so only a function without one gets here. A `may fail`
       function reports success there. */
    if (l->b != NULL) {
        if (it->may_fail) {
            ir_ret(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0));
        } else {
            ir_ret(l->f, l->b, IR_VOID, lower_none());
        }
    }
}

/* DESIGN: a snapshot is one record: its size in bytes, then a copy of
   each value it captures in the order of the captures. A snapshot on
   the heap holds the bytes of each `str` after the record, and the `ptr`
   of the `str` holds their offset from the start. It then moves and
   copies as a block of bytes, so freeing it and `dup` need nothing of
   the closure. A snapshot in the frame holds each `str` as it was, since
   the caller waits and the bytes stay. */
static uint32_t lower_snapshot_agg(struct lowerer *l, const struct item *it)
{
    struct ir_field *fields = ir_alloc(it->capture_count + 1,
                                       sizeof *fields);
    struct text name = {0};
    uint32_t agg;
    size_t i;

    fields[0].name = "size";
    fields[0].type = ir_scalar(IR_I64);
    for (i = 0; i < it->capture_count; i++) {
        fields[i + 1].name = lower_cstr(&it->captures[i].symbol->name);
        fields[i + 1].type = lower_vtype_of(l, it->captures[i].symbol->type);
    }
    text_appendf(&name, "%s.snapshot", l->m->functions[it->symbol->ir]->name);
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, text_cstr(&name), fields,
                        it->capture_count + 1, false, 0);
    text_free(&name);
    for (i = 0; i < it->capture_count; i++) {
        free((char *)fields[i + 1].name);
    }
    free(fields);
    return agg;
}

/* The entry of a snapshot: each captured name reaches its copy in the
   record that the context points at. A `str` of a snapshot on the heap
   is rebuilt in a slot from its offset. */
static void snapshot_entry(struct lowerer *l, const struct item *it,
                           struct ir_block *entry)
{
    struct ir_operand base =
        lower_temp(l, l->f->params[l->f->param_count - 1].temp);
    uint32_t agg = lower_snapshot_agg(l, it);
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        struct symbol *sym = it->captures[i].symbol;
        uint32_t at = ir_ptradd(
            l->f, entry, base,
            ir_sym_operand(l->m,
                           ir_sym_offset_of(l->m, agg, (uint32_t)(i + 1))));
        struct ir_operand len_offset;
        struct ir_operand offset;
        struct ir_operand length;
        struct ir_operand length_at;
        uint32_t slot;
        uint32_t ptr;

        if (!it->snapshot_heap || sym->type->kind != TYPE_STR) {
            sym->ir = at;
            continue;
        }
        len_offset = lower_field_offset(l, sym->type, &lower_len_name);
        slot = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        offset = lower_temp(l, ir_load(l->f, entry, IR_I64, lower_temp(l, at)));
        ptr = ir_ptradd(l->f, entry, base, offset);
        ir_store(l->f, entry, IR_PTR, lower_temp(l, ptr), lower_temp(l, slot));
        /* The length is read before the address it goes to is made,
           each bound first, since C leaves the order of two calls in
           one argument list open. */
        length = lower_temp(l, ir_load(l->f, entry, IR_I64,
                                       lower_temp(l, ir_ptradd(
                                                         l->f, entry,
                                                         lower_temp(l, at),
                                                         len_offset))));
        length_at = lower_temp(l, ir_ptradd(l->f, entry, lower_temp(l, slot),
                                            len_offset));
        ir_store(l->f, entry, IR_I64, length, length_at);
        sym->ir = slot;
    }
}

/* The aggregate of the context of the closure it: one pointer per
   variable it captures, in the order of the captures. */
static uint32_t lower_captures_agg(struct lowerer *l, const struct item *it)
{
    struct ir_field *fields = ir_alloc(it->capture_count, sizeof *fields);
    struct text name = {0};
    uint32_t agg;
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        fields[i].name = lower_cstr(&it->captures[i].symbol->name);
        fields[i].type = ir_scalar(IR_PTR);
    }
    text_appendf(&name, "%s.context", l->m->functions[it->symbol->ir]->name);
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, text_cstr(&name), fields,
                        it->capture_count, false, 0);
    text_free(&name);
    for (i = 0; i < it->capture_count; i++) {
        free((char *)fields[i].name);
    }
    free(fields);
    return agg;
}

/* The IR function of the anonymous function it. The first expression
   that names it declares it and queues it, and it is lowered after the
   function being lowered now. A `defer` lowers its statement at every
   exit, so one expression may be met more than once. A closure takes its
   context after every other parameter. */
static struct ir_function *lower_anonymous_function(struct lowerer *l,
                                                    struct item *it)
{
    const struct type *t = it->symbol->type;
    struct text name = {0};
    struct ir_function *f;
    size_t i;

    for (i = 0; i < l->anonymous_count; i++) {
        if (l->anonymous[i] == it) {
            return l->m->functions[it->symbol->ir];
        }
    }
    text_appendf(&name, "%s.%zu", l->f->name, l->anonymous_named++);
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(t->result),
                        lower_result_agg(l, t->result));
    text_free(&name);
    for (i = 0; i < t->param_count; i++) {
        lower_add_param(l, f, t->params[i]);
    }
    if (it->capture_count > 0) {
        ir_param_add(f, IR_PTR, IR_NO_AGG);
    }
    it->symbol->ir = f->index;
    l->anonymous = ir_grow(l->anonymous, &l->anonymous_capacity,
                           l->anonymous_count, sizeof *l->anonymous);
    l->anonymous[l->anonymous_count++] = it;
    return f;
}

/* A function with its context in a slot of the frame: code and then
   context, in the aggregate of the form t. */
struct ir_operand lower_pair(struct lowerer *l, const struct type *t,
                             struct ir_operand code,
                             struct ir_operand context)
{
    uint32_t agg = lower_agg_of(l, t);
    struct ir_operand slot =
        lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));

    ir_store(l->f, l->b, IR_PTR, code, slot);
    ir_store(l->f, l->b, IR_PTR, context,
             lower_offset_address(l, slot,
                                  ir_sym_operand(l->m,
                                                 ir_sym_offset_of(l->m, agg,
                                                                  1))));
    return slot;
}

/* The copy of the captured value at src into the record at dst. */
static void copy_captured(struct lowerer *l, const struct type *t,
                          struct ir_operand src, struct ir_operand dst)
{
    if (lower_is_aggregate(t)) {
        ir_memcopy(l->f, l->b, dst, src, lower_vtype_of(l, t));
        return;
    }
    ir_store(l->f, l->b, lower_ir_type_of(t),
             lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(t), src)),
             dst);
}

/* DESIGN: a snapshot copies each captured value into its record when it
   is made. At an `own` field or a `keep own` parameter the record goes
   on the heap with the bytes of each `str` after it. The runtime counts
   it until it is freed. Anywhere else it sits in a slot of the
   frame, and nothing is allocated. */
static struct ir_operand lower_snapshot(struct lowerer *l,
                                        const struct expr *e,
                                        struct ir_operand code)
{
    const struct item *it = e->as.fn;
    uint32_t agg = lower_snapshot_agg(l, it);
    struct ir_operand fixed =
        ir_sym_operand(l->m, ir_sym_size_of(l->m, ir_aggregate(agg)));
    struct ir_operand size = fixed;
    struct ir_operand block;
    struct ir_operand cursor;
    size_t i;

    if (it->snapshot_heap) {
        for (i = 0; i < it->capture_count; i++) {
            const struct symbol *sym = it->captures[i].symbol;
            struct ir_operand len;
            if (sym->type->kind != TYPE_STR) {
                continue;
            }
            len = lower_temp(
                l, ir_load(l->f, l->b, IR_I64,
                           lower_offset_address(
                               l, lower_temp(l, sym->ir),
                               lower_field_offset(l, sym->type,
                                                  &lower_len_name))));
            size = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64, size,
                                           len));
        }
        block = lower_rt_call(l, RT_FN_SNAPSHOT_NEW, &size);
    } else {
        block = lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));
        ir_store(l->f, l->b, IR_I64, fixed, block);
    }
    cursor = fixed;
    for (i = 0; i < it->capture_count; i++) {
        const struct symbol *sym = it->captures[i].symbol;
        struct ir_operand src = lower_temp(l, sym->ir);
        struct ir_operand dst = lower_offset_address(
            l, block,
            ir_sym_operand(l->m,
                           ir_sym_offset_of(l->m, agg, (uint32_t)(i + 1))));
        struct ir_operand len_offset;
        struct ir_operand len;
        struct ir_operand args[4];

        if (!it->snapshot_heap || sym->type->kind != TYPE_STR) {
            copy_captured(l, sym->type, src, dst);
            continue;
        }
        len_offset = lower_field_offset(l, sym->type, &lower_len_name);
        len = lower_temp(l, ir_load(l->f, l->b, IR_I64,
                                    lower_offset_address(l, src, len_offset)));
        args[0] = block;
        args[1] = cursor;
        args[2] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, src));
        args[3] = len;
        lower_rt_call(l, RT_FN_SNAPSHOT_TEXT, args);
        ir_store(l->f, l->b, IR_I64, cursor, dst);
        ir_store(l->f, l->b, IR_I64, len,
                 lower_offset_address(l, dst, len_offset));
        cursor = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64, cursor,
                                         len));
    }
    return lower_pair(l, e->type, code, block);
}

/* DESIGN: an anonymous function that captures nothing is the address of
   its code, as a named function is. A closure is the code and a context
   in the frame that makes it: a record of the address of every variable
   it captures. Creating one allocates nothing. The record and the pair
   sit in slots of the frame, which outlive every local the closure may
   be held in. */
struct ir_operand lower_closure(struct lowerer *l,
                                const struct expr *e)
{
    struct item *it = e->as.fn;
    struct ir_function *f = lower_anonymous_function(l, it);
    struct ir_operand code = lower_temp(l, ir_addr(l->f, l->b,
                                                   ir_func_op(f)));
    uint32_t agg;
    struct ir_operand record;
    size_t i;

    if (it->capture_count == 0) {
        return code;
    }
    if (it->snapshot) {
        return lower_snapshot(l, e, code);
    }
    agg = lower_captures_agg(l, it);
    record = lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));
    for (i = 0; i < it->capture_count; i++) {
        ir_store(l->f, l->b, IR_PTR,
                 lower_temp(l, it->captures[i].symbol->ir),
                 lower_offset_address(
                     l, record,
                     i == 0 ? lower_zero()
                            : ir_sym_operand(l->m,
                                             ir_sym_offset_of(l->m, agg,
                                                              (uint32_t)i))));
    }
    return lower_pair(l, e->type, code, record);
}
