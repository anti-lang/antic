/* Locks and channels in lowering. The lock word of a Mutex and the
   hidden lock of a synchronized class, the calls that take and give them,
   `sync` and `select`, and the operations that make, use and end a
   channel or a Mutex. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* Write a free lock at at. It is the zero word of a Mutex, or the hidden
   lock of a synchronized object that no thread holds. */
void lower_zero_lock(struct lowerer *l, const struct type *t,
                     struct ir_operand at)
{
    size_t i;

    if (types_is_mutex(t)) {
        ir_store(l->f, l->b, IR_LOCK, ir_int_op(IR_LOCK, 0), at);
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        struct ir_operand into = lower_offset_address(
            l, at, lower_field_offset(l, t, &t->fields[i].name));
        if (types_is_mutex(t->fields[i].type)) {
            lower_zero_lock(l, t->fields[i].type, into);
        } else {
            ir_store(l->f, l->b, IR_I64, ir_int_op(IR_I64, 0), into);
        }
    }
}

/* The address of the hidden lock of the synchronized object at object,
   whose class is t or inherits the class that declares the lock. */
struct ir_operand lower_object_lock_address(struct lowerer *l,
                                            const struct type *t,
                                            struct ir_operand object)
{
    static const struct name lock = {HIDDEN_LOCK, sizeof HIDDEN_LOCK - 1};
    const struct type *owner = lower_field_owner(t, &lock);

    return lower_offset_address(l, object,
                                lower_field_offset(l, owner, &lock));
}

struct ir_operand lower_sync_op(struct lowerer *l,
                                const struct expr *e)
{
    const struct expr *target = e->as.sync_op.target;
    const struct type *element = NULL;
    struct ir_operand args[2];
    struct ir_operand handle;
    uint32_t slot;

    switch (e->as.sync_op.op) {
    /* A new Mutex is the zero word, which is unlocked on every
       system. */
    case SYNC_MUTEX_NEW:
        slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
        ir_store(l->f, l->b, IR_LOCK, ir_int_op(IR_LOCK, 0),
                 lower_temp(l, slot));
        return lower_temp(l, slot);
    case SYNC_CHAN_NEW:
        args[0] = lower_size_operand(l, e->type->element);
        args[1] = lower_expr(l, e->as.sync_op.value);
        handle = lower_rt_call(l, RT_FN_CHAN_NEW, args);
        break;
    /* The word holds nothing of the system, and a dev build forgets the
       orders it recorded for the lock. */
    case SYNC_MUTEX_DESTROY:
        args[0] = target->type->kind == TYPE_POINTER
                      ? lower_expr(l, target)
                      : lower_address(l, target);
        lower_rt_call(l, RT_FN_MUTEX_DESTROY, args);
        return lower_none();
    case SYNC_SEND:
        element = target->type->element;
        args[0] = lower_load_handle(l, target);
        slot = ir_entry_slot(l->f, lower_vtype_of(l, element));
        lower_store_value(l, element, e->as.sync_op.value, lower_temp(l, slot));
        args[1] = lower_temp(l, slot);
        lower_rt_call(l, RT_FN_CHAN_SEND, args);
        return lower_none();
    /* `recv` gives the address of the slot it filled, or `none` when
       the channel is closed and empty. */
    case SYNC_RECV:
        element = target->type->element;
        args[0] = lower_load_handle(l, target);
        slot = ir_entry_slot(l->f, lower_vtype_of(l, element));
        args[1] = lower_temp(l, slot);
        return lower_rt_call(l, RT_FN_CHAN_RECV, args);
    case SYNC_CLOSE:
    case SYNC_CHAN_DELETE:
        args[0] = lower_load_handle(l, target);
        lower_rt_call(l,
                      e->as.sync_op.op == SYNC_CLOSE ? RT_FN_CHAN_CLOSE
                                                     : RT_FN_CHAN_DELETE,
                      args);
        return lower_none();
    }
    /* A new channel is a slot that holds the handle. */
    slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, handle, lower_temp(l, slot));
    return lower_temp(l, slot);
}

/* The site of a lock in a dev build, `file:line`, as a literal. */
static struct ir_operand lock_site(struct lowerer *l, int line)
{
    struct text site = {0};
    struct token_text text;
    struct ir_operand at;

    text_appendf(&site, "%s:%d", l->file, line);
    text.bytes = text_cstr(&site);
    text.length = site.length;
    at = lower_literal_address(l, &text);
    text_free(&site);
    return at;
}

/* DESIGN: a lock is taken by its address, the word of a Mutex or the
   hidden lock of a synchronized object. A thread that holds the hidden
   lock takes it again without waiting. A dev build passes the site of
   each lock as well, `file:line`. The runtime then records the order in
   which each thread takes its locks and reports two orders that
   conflict. The
   unlock is an exit action of the scope that l->defers holds. */
void lower_hold_lock(struct lowerer *l, struct ir_operand at, bool object,
                     int line)
{
    struct ir_operand args[2];
    uint32_t lock = ir_unary(l->f, l->b, IR_COPY, IR_PTR, at);

    args[0] = lower_temp(l, lock);
    if (l->dev) {
        args[1] = lock_site(l, line);
        lower_rt_call(l, object ? RT_FN_OBJECT_LOCK_AT : RT_FN_MUTEX_LOCK_AT,
                      args);
        lower_push_unlock_action(l, lock,
                                 object ? RT_FN_OBJECT_UNLOCK_AT
                                        : RT_FN_MUTEX_UNLOCK_AT);
        return;
    }
    lower_rt_call(l, object ? RT_FN_OBJECT_LOCK : RT_FN_MUTEX_LOCK, args);
    lower_push_unlock_action(l, lock,
                             object ? RT_FN_OBJECT_UNLOCK
                                    : RT_FN_MUTEX_UNLOCK);
}

/* Take the hidden lock at `at`, with its site in a dev build. The caller
   gives it back on every path with lower_object_unlock_call. */
void lower_object_lock_call(struct lowerer *l, struct ir_operand at, int line)
{
    struct ir_operand args[2];

    args[0] = at;
    if (l->dev) {
        args[1] = lock_site(l, line);
        lower_rt_call(l, RT_FN_OBJECT_LOCK_AT, args);
        return;
    }
    lower_rt_call(l, RT_FN_OBJECT_LOCK, args);
}

void lower_object_unlock_call(struct lowerer *l, struct ir_operand at)
{
    lower_rt_call(l, l->dev ? RT_FN_OBJECT_UNLOCK_AT : RT_FN_OBJECT_UNLOCK,
                  &at);
}

/* The runtime function that gives back the two locks of `sync a, b`. */
static enum rt_function unlock_pair(const struct lowerer *l)
{
    return l->dev ? RT_FN_OBJECT_UNLOCK_PAIR_AT : RT_FN_OBJECT_UNLOCK_PAIR;
}

/* DESIGN: the runtime takes the two hidden locks of `sync a, b` in the
   order of their addresses, and one alone when a and b are the same
   lock, so the order the program names them in never matters. */
void lower_lock_pair_call(struct lowerer *l, struct ir_operand a,
                          struct ir_operand b, int line)
{
    struct ir_operand args[3];

    args[0] = a;
    args[1] = b;
    if (l->dev) {
        args[2] = lock_site(l, line);
        lower_rt_call(l, RT_FN_OBJECT_LOCK_PAIR_AT, args);
        return;
    }
    lower_rt_call(l, RT_FN_OBJECT_LOCK_PAIR, args);
}

void lower_unlock_pair_call(struct lowerer *l, struct ir_operand a,
                            struct ir_operand b)
{
    struct ir_operand args[2];

    args[0] = a;
    args[1] = b;
    lower_rt_call(l, unlock_pair(l), args);
}

/* The address of the hidden lock of the synchronized object that e
   names, in place or through a pointer, in a temporary of its own. */
static uint32_t object_lock_of(struct lowerer *l, const struct expr *e)
{
    const struct type *t = e->type->kind == TYPE_POINTER ? e->type->element
                                                          : e->type;
    struct ir_operand at = e->type->kind == TYPE_POINTER ? lower_expr(l, e)
                                                          : lower_address(l, e);

    return ir_unary(l->f, l->b, IR_COPY, IR_PTR,
                    lower_object_lock_address(l, t, at));
}

/* `sync a, b { }` takes both locks, and an exit action of its scope
   gives both back on every exit of the block. */
static void lower_sync_pair(struct lowerer *l, const struct stmt *s)
{
    uint32_t first = object_lock_of(l, s->as.sync.mutex);
    uint32_t second = object_lock_of(l, s->as.sync.second);
    struct exit_action *action;
    struct defers scope;

    memset(&scope, 0, sizeof scope);
    scope.outer = l->defers;
    l->defers = &scope;
    lower_lock_pair_call(l, lower_temp(l, first), lower_temp(l, second),
                         s->pos.line);
    lower_push_unlock_action(l, first, unlock_pair(l));
    action = &l->defers->items[l->defers->count - 1];
    action->pair = true;
    action->second = second;
    lower_block(l, s->as.sync.body);
    lower_run_defers(l, &scope, false);
    l->defers = scope.outer;
    free(scope.items);
}

/* DESIGN: `sync m { }` takes the address of m once, locks it, and
   records its unlock as the first exit action of a scope around the
   block. Every exit of the block runs the actions of the scopes it
   leaves, `return`, `break`, `continue` and the error forms among them,
   so each unlocks after the statements of the block's own `defer` have
   run. `sync obj { }` on a synchronized object takes its hidden lock. */
void lower_sync(struct lowerer *l, const struct stmt *s)
{
    const struct expr *m = s->as.sync.mutex;
    struct ir_operand at;
    struct defers scope;

    if (s->as.sync.second != NULL) {
        lower_sync_pair(l, s);
        return;
    }
    at = m->type->kind == TYPE_POINTER ? lower_expr(l, m)
                                       : lower_address(l, m);
    memset(&scope, 0, sizeof scope);
    scope.outer = l->defers;
    l->defers = &scope;
    if (s->as.sync.object) {
        const struct type *t = m->type->kind == TYPE_POINTER
                                   ? m->type->element
                                   : m->type;
        lower_hold_lock(l, lower_object_lock_address(l, t, at), true,
                        s->pos.line);
    } else {
        lower_hold_lock(l, at, false, s->pos.line);
    }
    lower_block(l, s->as.sync.body);
    lower_run_defers(l, &scope, false);
    l->defers = scope.outer;
    free(scope.items);
}

/* An array of count pointers, the form in which `select` hands its
   channels and its slots to the runtime. */
static uint32_t pointer_array(struct lowerer *l, size_t count)
{
    return ir_array_of(l->m, "*byte", ir_scalar(IR_PTR), count);
}

/* DESIGN: `select` passes the handle of each arm's channel to
   anti_rt_select, and a slot of its element type. The runtime waits until
   one channel has a value or is closed. It gives the index of that arm and writes what
   `recv` would have given into one pointer, which the arm binds. The
   arms are then compared with the index in order, as a `switch` compares
   its values. */
void lower_select(struct lowerer *l, const struct stmt *s)
{
    size_t count = s->as.select.count;
    uint32_t agg = pointer_array(l, count);
    uint32_t chans = ir_entry_slot(l->f, ir_aggregate(agg));
    uint32_t slots = ir_entry_slot(l->f, ir_aggregate(agg));
    uint32_t got = ir_entry_slot(l->f, ir_scalar(IR_PTR));
    struct ir_operand pointer_size =
        ir_sym_operand(l->m, ir_sym_size_of(l->m, ir_scalar(IR_PTR)));
    struct ir_block *join = NULL;
    struct ir_operand args[4];
    struct ir_operand index;
    size_t i;

    for (i = 0; i < count; i++) {
        const struct switch_arm *arm = &s->as.select.arms[i];
        struct ir_operand handle = lower_load_handle(l, arm->value);
        struct ir_operand offset;
        uint32_t slot;
        offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                         ir_int_op(IR_I64, i), pointer_size));
        ir_store(l->f, l->b, IR_PTR, handle,
                 lower_offset_address(l, lower_temp(l, chans), offset));
        slot = ir_entry_slot(l->f,
                             lower_vtype_of(l, arm->value->type->element));
        ir_store(l->f, l->b, IR_PTR, lower_temp(l, slot),
                 lower_offset_address(l, lower_temp(l, slots), offset));
    }
    args[0] = lower_temp(l, chans);
    args[1] = lower_temp(l, slots);
    args[2] = ir_int_op(IR_I64, count);
    args[3] = lower_temp(l, got);
    index = lower_rt_call(l, RT_FN_SELECT, args);
    for (i = 0; i < count && l->b != NULL; i++) {
        const struct switch_arm *arm = &s->as.select.arms[i];
        struct ir_block *body = lower_new_block(l);
        struct ir_block *next = i + 1 < count ? lower_new_block(l) : NULL;
        if (next != NULL) {
            struct ir_operand test =
                lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, index,
                                        ir_int_op(IR_I64, i)));
            ir_branch(l->f, l->b, test, body, next);
        } else {
            ir_jump(l->f, l->b, body);
        }
        l->b = body;
        if (arm->bound != NULL) {
            lower_bind_value(l, arm->bound,
                             lower_temp(l,
                                        ir_load(l->f, l->b, IR_PTR,
                                                lower_temp(l, got))));
        }
        lower_stmt(l, arm->body);
        lower_jump_to_join(l, &join);
        l->b = next;
    }
    l->b = join;
}
