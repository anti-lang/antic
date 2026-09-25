#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

static void run_defers_to(struct lowerer *l, const struct defers *stop,
                          bool failing);

/* Whether any block the function is inside has a statement to run. */
static bool has_defers(const struct lowerer *l)
{
    const struct defers *scope;

    for (scope = l->defers; scope != NULL; scope = scope->outer) {
        if (scope->count > 0) {
            return true;
        }
    }
    return false;
}

/* Room for one more exit action in scope. */
static struct exit_action *grow_defers(struct defers *scope)
{
    scope->items = ir_grow(scope->items, &scope->capacity, scope->count,
                           sizeof *scope->items);
    return scope->items;
}

/* Record one exit action of the innermost block. Every field is written
   here, because `realloc` leaves the new elements with the bytes of
   whatever stood there. */
static void push_exit_action(struct lowerer *l, const struct stmt *stmt,
                             const struct symbol *local, bool undo)
{
    struct exit_action *action;

    l->defers->items = grow_defers(l->defers);
    action = &l->defers->items[l->defers->count++];
    action->stmt = stmt;
    action->local = local;
    action->undo = undo;
    action->error = false;
    action->error_temp = 0;
    action->error_type = NULL;
    action->unlock = false;
    action->mutex = 0;
    action->unlock_fn = NULL;
    action->leave = false;
    action->snapshot = false;
}

/* Record the `leave` hook of the function around the scope. */
void lower_push_leave_action(struct lowerer *l)
{
    struct exit_action *action;

    l->defers->items = grow_defers(l->defers);
    action = &l->defers->items[l->defers->count++];
    memset(action, 0, sizeof *action);
    action->leave = true;
}

/* Record the free of the snapshot of the `keep own` parameter param. */
void lower_push_snapshot_action(struct lowerer *l, const struct symbol *param)
{
    struct exit_action *action;

    l->defers->items = grow_defers(l->defers);
    action = &l->defers->items[l->defers->count++];
    memset(action, 0, sizeof *action);
    action->local = param;
    action->snapshot = true;
}

/* Record the delete of the error a handler binds, whose name is sym or
   which has none. */
static void push_error_action(struct lowerer *l, const struct symbol *sym,
                              uint32_t error, const struct type *error_type)
{
    struct exit_action *action;

    l->defers->items = grow_defers(l->defers);
    action = &l->defers->items[l->defers->count++];
    action->stmt = NULL;
    action->local = sym;
    action->undo = false;
    action->error = true;
    action->error_temp = error;
    action->error_type = error_type;
    action->unlock = false;
    action->mutex = 0;
    action->unlock_fn = NULL;
    action->leave = false;
    action->snapshot = false;
}

/* Record the unlock of the lock whose address is in mutex, which a
   `sync` holds until its block ends and a synchronized function until
   it returns. unlock_fn names the function of the runtime. */
static void push_unlock_action(struct lowerer *l, uint32_t mutex,
                               const char *unlock_fn)
{
    struct exit_action *action;

    l->defers->items = grow_defers(l->defers);
    action = &l->defers->items[l->defers->count++];
    action->stmt = NULL;
    action->local = NULL;
    action->undo = false;
    action->error = false;
    action->error_temp = 0;
    action->error_type = NULL;
    action->unlock = true;
    action->mutex = mutex;
    action->unlock_fn = unlock_fn;
    action->leave = false;
    action->snapshot = false;
}

static void jump_to_join(struct lowerer *l, struct ir_block **join)
{
    if (l->b == NULL) {
        return;
    }
    if (*join == NULL) {
        *join = lower_new_block(l);
    }
    ir_jump(l->f, l->b, *join);
}

static void lower_stmt(struct lowerer *l, const struct stmt *s);

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
    static const enum ir_type one[] = {IR_PTR};
    static const enum ir_type two[] = {IR_PTR, IR_PTR};
    struct ir_operand args[2];
    uint32_t lock = ir_unary(l->f, l->b, IR_COPY, IR_PTR, at);

    args[0] = lower_temp(l, lock);
    if (l->dev) {
        struct text site = {0};
        struct token_text text;
        text_appendf(&site, "%s:%d", l->file, line);
        text.bytes = text_cstr(&site);
        text.length = site.length;
        args[1] = lower_literal_address(l, &text);
        text_free(&site);
        lower_sync_call(l, object ? "anti_rt_object_lock_at"
                                  : "anti_rt_mutex_lock_at",
                        IR_VOID, two, args, 2);
        push_unlock_action(l, lock, object ? "anti_rt_object_unlock_at"
                                           : "anti_rt_mutex_unlock_at");
        return;
    }
    lower_sync_call(l, object ? "anti_rt_object_lock" : "anti_rt_mutex_lock",
                    IR_VOID, one, args, 1);
    push_unlock_action(l, lock, object ? "anti_rt_object_unlock"
                                       : "anti_rt_mutex_unlock");
}

/* DESIGN: `sync m { }` takes the address of m once, locks it, and
   records its unlock as the first exit action of a scope around the
   block. Every exit of the block runs the actions of the scopes it
   leaves, `return`, `break`, `continue` and the error forms among them,
   so each unlocks after the statements of the block's own `defer` have
   run. `sync obj { }` on a synchronized object takes its hidden lock. */
static void lower_sync(struct lowerer *l, const struct stmt *s)
{
    const struct expr *m = s->as.sync.mutex;
    struct ir_operand at = m->type->kind == TYPE_POINTER ? lower_expr(l, m)
                                                         : lower_address(l, m);
    struct defers scope;

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
    char name[32];
    char length[24];
    uint32_t agg;

    snprintf(name, sizeof name, "[%zu]*byte", count);
    agg = ir_agg_find(l->m, name);
    if (agg != IR_NO_AGG) {
        return agg;
    }
    snprintf(length, sizeof length, "%zu", count);
    return ir_array_add(l->m, name, ir_scalar(IR_PTR),
                        ir_sym_int(l->m, IR_I64, count), length);
}

/* DESIGN: `select` passes the handle of each arm's channel to
   anti_rt_select, and a slot of its element type. The runtime waits until
   one channel has a value or is closed. It gives the index of that arm and writes what
   `recv` would have given into one pointer, which the arm binds. The
   arms are then compared with the index in order, as a `switch` compares
   its values. */
static void lower_select(struct lowerer *l, const struct stmt *s)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR, IR_I64, IR_PTR};
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
    index = lower_sync_call(l, "anti_rt_select", IR_I64, params, args, 4);
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
        jump_to_join(l, &join);
        l->b = next;
    }
    l->b = join;
}

/* Each condition of the chain branches to its body or to the next
   condition. The join block after the chain exists only when some path
   reaches the end of the chain. */
static void lower_if(struct lowerer *l, const struct stmt *s)
{
    size_t count = s->as.if_chain.count;
    struct ir_block *join = NULL;
    size_t i;

    for (i = 0; i < count; i++) {
        const struct if_branch *branch = &s->as.if_chain.branches[i];
        struct ir_block *then_block = lower_new_block(l);
        struct ir_block *next;

        if (i + 1 < count || s->as.if_chain.else_body != NULL) {
            next = lower_new_block(l);
        } else {
            if (join == NULL) {
                join = lower_new_block(l);
            }
            next = join;
        }
        lower_branch(l, branch->cond, then_block, next);
        l->b = then_block;
        lower_block(l, branch->body);
        jump_to_join(l, &join);
        l->b = next;
    }
    if (s->as.if_chain.else_body != NULL) {
        lower_block(l, s->as.if_chain.else_body);
        jump_to_join(l, &join);
    }
    l->b = join;
}

static void lower_loop(struct lowerer *l, const struct stmt *s)
{
    bool is_while = s->kind == STMT_WHILE;
    struct ir_block *first = lower_new_block(l);
    struct ir_block *second = lower_new_block(l);
    struct ir_block *exit = lower_new_block(l);
    struct ir_block *test = is_while ? first : second;
    struct ir_block *body = is_while ? second : first;
    struct loop loop;

    loop.continue_to = test;
    loop.break_to = exit;
    loop.outer = l->loop;
    loop.defers_at = l->defers;
    l->loop_depth++;
    ir_jump(l->f, l->b, first);
    if (is_while) {
        l->b = test;
        lower_branch(l, s->as.loop.cond, body, exit);
    }
    l->loop = &loop;
    l->b = body;
    lower_block(l, s->as.loop.body);
    if (l->b != NULL) {
        ir_jump(l->f, l->b, test);
    }
    l->loop = loop.outer;
    l->loop_depth--;
    if (!is_while) {
        l->b = test;
        lower_branch(l, s->as.loop.cond, body, exit);
    }
    l->b = exit;
}

static bool local_needs_teardown(const struct type *t);

/* DESIGN: `for x in e` over a collection or an iterator is the loop of
   its `while` form. It binds the iterator once, calls `next` in the test
   and `value` at the head of the body, and `continue` goes to the test.
   The iterator lives in a scope around the loop and each value in a scope
   around one pass of the body, so each is torn down as a `let` of its
   type is, the value on every exit of the pass. */
static void lower_for_hooks(struct lowerer *l, const struct stmt *s)
{
    const struct iteration *it = &s->as.for_loop.hooks;
    struct symbol *sym = s->as.for_loop.names[0].symbol;
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *exit = lower_new_block(l);
    struct defers around;
    struct defers pass;
    struct loop loop;

    memset(&around, 0, sizeof around);
    around.outer = l->defers;
    l->defers = &around;
    lower_bind_cursor(l, it);
    if (local_needs_teardown(it->cursor->type)) {
        push_exit_action(l, NULL, it->cursor, false);
    }
    loop.continue_to = test;
    loop.break_to = exit;
    loop.outer = l->loop;
    loop.defers_at = l->defers;
    l->loop_depth++;
    ir_jump(l->f, l->b, test);
    l->b = test;
    lower_branch(l, it->advance, body, exit);
    l->loop = &loop;
    l->b = body;
    memset(&pass, 0, sizeof pass);
    pass.outer = l->defers;
    l->defers = &pass;
    if (lower_is_aggregate(sym->type)) {
        lower_build_into(l, it->current, lower_temp(l, sym->ir));
    } else {
        /* The call may end the block it starts in, so the value is
           bound in the block that follows it. */
        struct ir_operand v = lower_expr(l, it->current);
        if (sym->address_taken) {
            ir_store(l->f, l->b, lower_ir_type_of(sym->type), v,
                     lower_temp(l, sym->ir));
        } else {
            sym->ir = ir_unary(l->f, l->b, IR_COPY,
                               lower_ir_type_of(sym->type), v);
        }
    }
    if (local_needs_teardown(sym->type)) {
        push_exit_action(l, NULL, sym, false);
    }
    lower_block(l, s->as.for_loop.body);
    lower_run_defers(l, &pass, false);
    if (l->b != NULL) {
        ir_jump(l->f, l->b, test);
    }
    l->defers = pass.outer;
    free(pass.items);
    l->loop = loop.outer;
    l->loop_depth--;
    l->b = exit;
    lower_run_defers(l, &around, false);
    l->defers = around.outer;
    free(around.items);
}

/* Give the variable of a `for` the value of this pass. A closure that
   captures the variable reads it from its own place in the slots of the
   frame. Every other variable is a temporary. */
static void bind_loop_name(struct lowerer *l, struct symbol *sym,
                           enum ir_type type, struct ir_operand value)
{
    if (sym->address_taken) {
        ir_store(l->f, l->b, type, value, lower_temp(l, sym->ir));
    } else {
        sym->ir = ir_unary(l->f, l->b, IR_COPY, type, value);
    }
}

/* DESIGN: `for` is a loop of its own rather than a rewrite into `while`,
   because the step is the target of `continue`. A textual rewrite would
   put the step after the body, where `continue` jumps over it and the
   loop would never end. */
static void lower_for(struct lowerer *l, const struct stmt *s)
{
    size_t names = s->as.for_loop.name_count;
    /* The element is the last of the names, and `for i, x in items`
       names the index before it. */
    struct symbol *sym = names > 0 ? s->as.for_loop.names[names - 1].symbol
                                   : NULL;
    struct symbol *index = names > 1 ? s->as.for_loop.names[0].symbol : NULL;
    const struct expr *over = s->as.for_loop.over;
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *step = lower_new_block(l);
    struct ir_block *exit = lower_new_block(l);
    struct loop loop;
    struct ir_operand limit;
    struct ir_operand base = lower_none();
    const struct type *seq = NULL;
    enum ir_type counter_type = IR_I64;
    uint32_t counter;
    /* DESIGN: the step keeps its sign apart from its magnitude. The
       checker stores `by k` as an int64_t, so a step of 2^63 on an
       unsigned range reads as INT64_MIN. A range of an unsigned type
       walks up, since the checker refuses a negative step on it, and its
       test compares unsigned. The magnitude is negated in uint64_t,
       where -2^63 has one. */
    int64_t stride = s->as.for_loop.step_value;
    bool unsigned_range =
        over == NULL && !type_is_signed(s->as.for_loop.low->type);
    bool down = stride < 0 && !unsigned_range;
    uint64_t k = down ? 0 - (uint64_t)stride : (uint64_t)stride;

    /* The bound is read once, before the loop. */
    if (over != NULL) {
        seq = over->type;
        base = lower_address(l, over);
        if (seq->kind == TYPE_SLICE) {
            limit = lower_temp(
                l, ir_load(l->f, l->b, IR_I64,
                           lower_offset_address(
                               l, base,
                               lower_field_offset(l, seq, &lower_len_name))));
            base = lower_temp(l, ir_load(l->f, l->b, IR_PTR, base));
        } else {
            limit = seq->length_of != NULL
                        ? ir_sym_operand(l->m, lower_sym_of(l, seq->length_of))
                        : ir_int_op(IR_I64, seq->length);
        }
        counter = ir_unary(l->f, l->b, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    } else {
        struct ir_operand low = lower_expr(l, s->as.for_loop.low);
        struct ir_operand high;
        counter_type = lower_ir_type_of(s->as.for_loop.low->type);
        high = lower_expr(l, s->as.for_loop.high);
        /* DESIGN: `by -k` walks the values of `by k` in reverse, so it
           starts at the largest of them and not at the high bound. That
           value is `low + ((high - low - 1) / k) * k`, and the division
           is by a constant and runs once.

           The counter holds the next value plus k, and the body
           subtracts before it reads. The test is then `counter >= low +
           k`, which never wraps below zero the way `counter - k` would
           on an unsigned type. An empty range leaves the counter at
           `low`, where the first test already fails. */
        if (down) {
            struct ir_block *first = lower_new_block(l);
            struct ir_operand span =
                lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                        high, low));
            limit = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, counter_type,
                                            low, ir_int_op(counter_type, k)));
            counter = ir_unary(l->f, l->b, IR_COPY, counter_type, low);
            ir_branch(l->f, l->b,
                      lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8, span,
                                              ir_int_op(counter_type, 0))),
                      first, test);
            l->b = first;
            ir_assign(
                l->f, l->b, counter,
                lower_temp(l, ir_binary(
                    l->f, l->b, IR_ADD, counter_type, limit,
                    lower_temp(l, ir_binary(
                        l->f, l->b, IR_MUL, counter_type,
                        lower_temp(l, ir_binary(
                            l->f, l->b, IR_SDIV, counter_type,
                            lower_temp(l, ir_binary(l->f, l->b, IR_SUB,
                                                    counter_type, span,
                                                    ir_int_op(counter_type,
                                                              1))),
                            ir_int_op(counter_type, k))),
                        ir_int_op(counter_type, k))))));
        } else {
            limit = high;
            counter = ir_unary(l->f, l->b, IR_COPY, counter_type, low);
        }
    }
    loop.continue_to = step;
    loop.break_to = exit;
    loop.outer = l->loop;
    loop.defers_at = l->defers;
    l->loop_depth++;
    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b,
                                      down             ? IR_SGE
                                      : unsigned_range ? IR_ULT
                                                       : IR_SLT,
                                      IR_I8, lower_temp(l, counter), limit)),
              body, exit);
    l->loop = &loop;
    l->b = body;
    /* The variable of the body: the counter of a range, or the element
       that the index reaches. A range without a name counts and reads
       nothing. */
    if (over == NULL) {
        if (down) {
            ir_assign(l->f, l->b, counter,
                      lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                              lower_temp(l, counter),
                                              ir_int_op(counter_type, k))));
        }
        if (sym != NULL) {
            bind_loop_name(l, sym, counter_type, lower_temp(l, counter));
        }
    } else {
        struct ir_operand at =
            lower_temp(l, ir_ptradd(
                              l->f, l->b, base,
                              lower_temp(l, ir_binary(
                                                l->f, l->b, IR_MUL, IR_I64,
                                                lower_temp(l, counter),
                                                lower_size_operand(
                                                    l, seq->element)))));
        /* DESIGN: `for i, x in items` destructures the `(int, T)` of
           each element. The index is the counter the loop already has,
           and the element is the value at it. The two names take their
           values from where they stand, and no pair is built. */
        if (index != NULL) {
            bind_loop_name(l, index, IR_I64, lower_temp(l, counter));
        }
        if (s->as.for_loop.by_pointer) {
            bind_loop_name(l, sym, IR_PTR, at);
        } else if (lower_is_aggregate(sym->type)) {
            ir_memcopy(l->f, l->b, lower_temp(l, sym->ir), at,
                       lower_vtype_of(l, sym->type));
        } else if (sym->address_taken) {
            bind_loop_name(l, sym, lower_ir_type_of(sym->type),
                           lower_temp(l, ir_load(l->f, l->b,
                                                 lower_ir_type_of(sym->type),
                                                 at)));
        } else {
            sym->ir = ir_load(l->f, l->b, lower_ir_type_of(sym->type), at);
        }
    }
    lower_block(l, s->as.for_loop.body);
    if (l->b != NULL) {
        ir_jump(l->f, l->b, step);
    }
    l->loop = loop.outer;
    l->loop_depth--;
    l->b = step;
    if (!down) {
        ir_assign(l->f, l->b, counter,
                  lower_temp(l, ir_binary(l->f, l->b, IR_ADD, counter_type,
                                          lower_temp(l, counter),
                                          ir_int_op(counter_type, k))));
    }
    ir_jump(l->f, l->b, test);
    l->b = exit;
}

static enum token_kind compound_op(enum token_kind op)
{
    switch (op) {
    case TOKEN_PLUS_ASSIGN: return TOKEN_PLUS;
    case TOKEN_MINUS_ASSIGN: return TOKEN_MINUS;
    case TOKEN_STAR_ASSIGN: return TOKEN_STAR;
    case TOKEN_SLASH_ASSIGN: return TOKEN_SLASH;
    case TOKEN_PERCENT_ASSIGN: return TOKEN_PERCENT;
    case TOKEN_AMP_ASSIGN: return TOKEN_AMP;
    case TOKEN_PIPE_ASSIGN: return TOKEN_PIPE;
    case TOKEN_CARET_ASSIGN: return TOKEN_CARET;
    case TOKEN_SHL_ASSIGN: return TOKEN_SHL;
    case TOKEN_PLUS_WRAP_ASSIGN: return TOKEN_PLUS_WRAP;
    case TOKEN_MINUS_WRAP_ASSIGN: return TOKEN_MINUS_WRAP;
    case TOKEN_STAR_WRAP_ASSIGN: return TOKEN_STAR_WRAP;
    case TOKEN_SHL_WRAP_ASSIGN: return TOKEN_SHL_WRAP;
    case TOKEN_PLUS_SAT_ASSIGN: return TOKEN_PLUS_SAT;
    case TOKEN_MINUS_SAT_ASSIGN: return TOKEN_MINUS_SAT;
    case TOKEN_STAR_SAT_ASSIGN: return TOKEN_STAR_SAT;
    default: return TOKEN_SHR;
    }
}

/* Chapter 2 evaluates the place first and the value second. A compound
   assignment reads the old value before it evaluates the new operand, as
   x = x + e reads x first. */

static void destroy_value(struct lowerer *l, struct ir_operand p,
                          const struct type *t, bool replaced);

static void destroy_array(struct lowerer *l, struct ir_operand base,
                          const struct type *t, bool replaced);

/* DESIGN: `*p = try f();` gives the call the address of the place it
   assigns to. The place is read once, before the call, so a target that
   computes an address runs its parts exactly once. Nothing is cleared
   first: the place holds a value, and the `=` the callee runs destroys
   it, as in every other assignment.

   Three targets have no address to hand over. A place the back end keeps
   in a temporary has none, and a bitfield has none. A compound
   assignment wants the operand in the place, not the result. Each of
   them takes a slot of the frame instead, and the value moves from there
   into the place. All three are scalars, so the slot needs no zero
   table. A local of an aggregate type has a place of its own, and a
   bitfield is an integer. */
static struct ir_operand call_into_slot(struct lowerer *l,
                                        const struct expr *call,
                                        const struct place *p)
{
    struct ir_operand out =
        lower_temp(l, ir_slot(l->f, l->b, lower_vtype_of(l, call->type)));
    struct ir_operand err;

    l->out_address = out;
    err = lower_call(l, call);
    lower_handle_error(l, call, err, out, true, lower_none());
    return lower_temp(l, ir_load(l->f, l->b, p->type, out));
}

static void lower_flags_assign(struct lowerer *l, const struct stmt *s);

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
static void hook_changed(struct lowerer *l, const struct place *p,
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

static void lower_assign(struct lowerer *l, const struct stmt *s)
{
    const struct expr *target = s->as.assign.target;
    const struct expr *value = s->as.assign.value;
    bool handled = lower_is_handled_call(value) && value->as.call.out != NULL;
    struct place p;
    struct ir_operand old = lower_none();
    struct ir_operand v;

    if (target->kind == EXPR_TUPLE) {
        lower_flags_assign(l, s);
        return;
    }
    if (!lower_place(l, target, &p)) {
        return;
    }
    /* The place itself is the out parameter of the call that fills it. */
    if (handled && !p.in_temp && !p.bitfield &&
        s->as.assign.op == TOKEN_ASSIGN) {
        struct ir_operand err;
        l->out_address = p.address;
        err = lower_call(l, value);
        lower_handle_error(l, value, err, p.address, true, lower_none());
        hook_changed(l, &p, target);
        return;
    }
    /* DESIGN: `=` into a place that holds a value needing a teardown
       destroys the old value first, then moves the new one in. The new
       value is complete before the old one goes, so it may read it. A
       place whose table is zero, an unfilled element of `alloc(T, n)`,
       holds no value, and nothing is destroyed. The zero-table trap is for
       use, not for assignment into. */
    if (lower_is_aggregate(target->type)) {
        v = lower_expr(l, value);
        /* An `own fn` place frees the snapshot it held. */
        if (target->type->kind == TYPE_FN && target->type->owned) {
            lower_free_snapshot(l, target->type, p.address);
        }
        if (local_needs_teardown(target->type)) {
            if (target->type->kind == TYPE_ARRAY) {
                destroy_array(l, p.address, target->type, true);
            } else {
                destroy_value(l, p.address, target->type, true);
            }
        }
        ir_memcopy(l->f, l->b, p.address, v, lower_vtype_of(l, target->type));
        hook_changed(l, &p, target);
        return;
    }
    if (s->as.assign.op != TOKEN_ASSIGN) {
        old = lower_read_place(l, &p);
    }
    v = handled ? call_into_slot(l, value, &p) : lower_expr(l, value);
    if (s->as.assign.op != TOKEN_ASSIGN) {
        enum token_kind op = compound_op(s->as.assign.op);
        struct ir_operand checked =
            op == TOKEN_SHL_WRAP
                ? lower_shift_wrap(l, target->type, old, v)
                : lower_binary_checks(l, op, target->type, old, v,
                                      target->pos.line);
        v = checked.kind != IR_NONE
                ? checked
                : lower_temp(l, ir_binary(l->f, l->b,
                                          lower_binary_op(op, target->type),
                                          p.type, old, v));
    }
    if (p.in_temp) {
        ir_assign(l->f, l->b, p.temp, v);
    } else if (p.bitfield) {
        ir_bitstore(l->f, l->b, p.type, v, p.address, p.agg, p.field);
    } else {
        ir_store(l->f, l->b, p.type, v, p.address);
    }
    hook_changed(l, &p, target);
}

/* DESIGN: a local of a class whose chain declares `destruct` or owns
   memory is torn down at the end of its block, as if the program had
   written `defer destroy(&c)` after the `let`. A class value held inline
   is owned, so one that needs the teardown asks for it too. A heap object
   is never torn down by itself, and `delete` is the only way to free
   one. */
bool lower_type_needs_destruct(const struct type *t)
{
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS) {
        return false;
    }
    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            static const struct name destruct_name = {"destruct", 8};
            /* The root's `destruct` is empty and never asks for one. */
            if (m->kind == ITEM_FN &&
                lower_same_name(&m->name, &destruct_name) &&
                m->runtime == NULL && lower_has_body(m)) {
                return true;
            }
        }
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if (f->owned) {
                return true;
            }
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                lower_type_needs_destruct(f->type)) {
                return true;
            }
        }
    }
    return false;
}

/* DESIGN: a local array whose element class needs the teardown is torn
   down element by element, last to first, as locals are. An array of
   arrays is one run of elements in memory and is torn down as one. */
static const struct type *innermost(const struct type *t)
{
    while (t != NULL && t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    return t;
}

static bool local_needs_teardown(const struct type *t)
{
    return lower_type_needs_destruct(innermost(t));
}

/* The count of elements of the class in the array t, through every
   level of it. */
static struct ir_operand element_count(struct lowerer *l, const struct type *t)
{
    struct ir_operand count = ir_int_op(IR_I64, 1);

    for (; t->kind == TYPE_ARRAY; t = t->element) {
        struct ir_operand length =
            t->length_of != NULL ? ir_sym_operand(l->m,
                                                  lower_sym_of(l, t->length_of))
                                 : ir_int_op(IR_I64, t->length);
        count = lower_temp(l,
                           ir_binary(l->f, l->b, IR_MUL, IR_I64, count,
                                     length));
    }
    return count;
}

/* The teardown of the class value at p. The end of a block checks its
   table in the runtime. An assignment passes over a value whose table is
   zero, which was never made. */
static void destroy_value(struct lowerer *l, struct ir_operand p,
                          const struct type *t, bool replaced)
{
    struct ir_operand args[2];
    struct ir_block *after;

    if (!replaced) {
        lower_object_call(l, "anti_rt_destroy", p, t);
        return;
    }
    after = lower_when_made(l, p);
    args[0] = p;
    args[1] = ir_int_op(IR_PTR, 0);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_class_function(l, t, "destroy")),
            args, 2);
    ir_jump(l->f, l->b, after);
    l->b = after;
}

static void destroy_array(struct lowerer *l, struct ir_operand base,
                          const struct type *t, bool replaced)
{
    const struct type *element = innermost(t);
    struct ir_operand size = lower_size_operand(l, element);
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    uint32_t index = ir_unary(l->f, l->b, IR_COPY, IR_I64,
                              element_count(l, t));
    struct ir_operand at;

    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 0))),
              body, done);
    l->b = body;
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    at = lower_temp(l, ir_ptradd(l->f, l->b, base,
                                 lower_temp(l, ir_binary(l->f, l->b, IR_MUL,
                                                         IR_I64,
                                                         lower_temp(l, index),
                                                         size))));
    destroy_value(l, at, element, replaced);
    ir_jump(l->f, l->b, test);
    l->b = done;
}

/* DESIGN: the out pointer the compiler supplies for `let n = f(args) catch
   e { }` points at storage that holds no value yet, and the `=` the callee
   writes destroys the old value first. That `=` reads the table to learn
   whether there is one, so the table is zero before the call. The bytes an
   earlier call left in the frame are otherwise a table the callee follows,
   which is a free of whatever the frame held. It is the zero table of
   `alloc(T, n)`, in a frame instead of on the heap. */
void lower_clear_tables(struct lowerer *l, struct ir_operand base,
                        const struct type *t)
{
    const struct type *element = innermost(t);
    struct ir_block *test;
    struct ir_block *body;
    struct ir_block *done;
    struct ir_operand size;
    struct ir_operand at;
    uint32_t index;

    if (!lower_type_needs_destruct(element)) {
        return;
    }
    if (t->kind != TYPE_ARRAY) {
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), base);
        return;
    }
    size = lower_size_operand(l, element);
    test = lower_new_block(l);
    body = lower_new_block(l);
    done = lower_new_block(l);
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64, element_count(l, t));
    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 0))),
              body, done);
    l->b = body;
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    at = lower_temp(l, ir_ptradd(l->f, l->b, base,
                                 lower_temp(l, ir_binary(l->f, l->b, IR_MUL,
                                                         IR_I64,
                                                         lower_temp(l, index),
                                                         size))));
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
    ir_jump(l->f, l->b, test);
    l->b = done;
}

static void destroy_local(struct lowerer *l, const struct symbol *sym)
{
    if (sym->type->kind == TYPE_ARRAY) {
        destroy_array(l, lower_temp(l, sym->ir), sym->type, false);
        return;
    }
    destroy_value(l, lower_temp(l, sym->ir), sym->type, false);
}

/* DESIGN: the error a handler binds is the exit action of a scope around
   the handler, so every exit of the handler deletes it: `yield`, the
   closing brace, `break`, `continue`, `return` and `fail`. `return e` and
   `fail e` hand it to the caller, which skips it as `return` skips the
   local it hands on. A handler with a result to give takes handling,
   which `yield` reads. */
static void lower_handler(struct lowerer *l, const struct handler *h,
                          uint32_t error, const struct type *error_type,
                          struct handling *handling)
{
    struct defers scope;

    memset(&scope, 0, sizeof scope);
    scope.outer = l->defers;
    l->defers = &scope;
    push_error_action(l, h->symbol, error, error_type);
    if (handling != NULL) {
        handling->defers_at = scope.outer;
        handling->outer = l->handling;
        l->handling = handling;
    }
    if (h->kind == HANDLE_BLOCK) {
        lower_block(l, h->body);
    }
    if (handling != NULL) {
        l->handling = handling->outer;
    }
    lower_run_defers(l, &scope, false);
    l->defers = scope.outer;
    free(scope.items);
}

/* `catch fatal`: call `fatal` of the class of the error err through its
   table, which the class of type error_type holds, then go on at join. */
static void call_fatal(struct lowerer *l, struct ir_operand err,
                       const struct type *error_type, struct ir_block *join)
{
    static const struct name fatal_name = {"fatal", 5};
    size_t index = lower_table_index(error_type->element, &fatal_name, 1);
    struct ir_operand table = lower_load_table(l, err, error_type);
    struct ir_operand entry =
        lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                              lower_offset_address(
                                  l, table, lower_entry_offset(l, index))));
    struct ir_operand self = err;

    ir_call_indirect(l->f, l->b, IR_VOID, entry, lower_fatal_signature(l),
                     &self,
                     1);
    ir_jump(l->f, l->b, join);
}

/* DESIGN: a call that can fail gives a pointer. A pointer of `none` is
   success, so the branch after the call is the whole of the error
   machinery. It is one compare and one branch, and nothing unwinds. */
void lower_handle_error(struct lowerer *l, const struct expr *call,
                        struct ir_operand err, struct ir_operand out,
                        bool has_out, struct ir_operand release)
{
    const struct handler *h = &call->as.call.handler;
    struct ir_block *bad = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);
    struct handling scope;
    uint32_t error;

    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, err,
                                      ir_int_op(IR_PTR, 0))),
              join, bad);
    l->b = bad;
    /* A `construct` that fails leaves no object behind, so the memory
       it was given goes back before the handler runs. */
    if (release.kind != IR_NONE) {
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_c_function(l, "free", IR_VOID, IR_PTR)),
                &release, 1);
    }
    switch (h->kind) {
    case HANDLE_NONE:
    case HANDLE_ENCLOSING:
        /* The `try` block around the call holds the one handler. The
           error leaves every block between the call and that handler,
           and each runs its `undo` and `defer` statements on the way. */
        if (l->try_scope != NULL) {
            ir_assign(l->f, l->b, l->try_scope->error, err);
            run_defers_to(l, l->try_scope->defers_at, true);
            if (l->b != NULL) {
                ir_jump(l->f, l->b, l->try_scope->handler);
            }
        } else {
            ir_jump(l->f, l->b, join);
        }
        break;
    case HANDLE_TRY:
        l->failing_error = err;
        run_defers_to(l, NULL, true);
        l->failing_error = lower_none();
        if (l->b != NULL) {
            ir_ret(l->f, l->b, IR_PTR, err);
        }
        break;
    case HANDLE_FATAL:
        call_fatal(l, err, call->as.call.callee->type->result, join);
        break;
    case HANDLE_BLOCK:
        error = ir_unary(l->f, l->b, IR_COPY, IR_PTR, err);
        if (h->symbol != NULL) {
            h->symbol->ir = error;
        }
        scope.join = join;
        scope.out = out;
        scope.has_out = has_out;
        scope.error = error;
        scope.error_type = call->as.call.callee->type->result;
        lower_handler(l, h, error, scope.error_type, &scope);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, join);
        }
        break;
    }
    l->b = join;
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
    const struct type *up;
    struct ir_operand *args;
    const struct item *m = NULL;
    uint32_t result;
    bool fails;
    size_t count;
    size_t i;

    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l,
                        ir_addr(l->f, l->b,
                                ir_global_op(lower_class_table(l, t)))),
             dest);
    lower_store_interface_tables(l, t, dest);
    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *field = &up->fields[i];
            /* An `own fn` field holds no snapshot before `construct`
               runs, since `=` into it frees the one it held. The memory
               of a local or of `malloc` holds whatever it held. */
            if (!lower_has_default(field) && field->type->kind == TYPE_FN &&
                field->type->owned) {
                struct ir_operand at = lower_offset_address(
                    l, dest, lower_field_offset(l, up, &field->name));
                ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
                ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                         lower_context_word(l, field->type, at));
                continue;
            }
            if (!lower_has_default(field)) {
                continue;
            }
            lower_store_field_default(l, up, i, dest);
        }
    }
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
    args = ir_alloc(2 * e->as.call.arg_count + 1, sizeof *args);
    args[0] = dest;
    count = 1;
    /* An argument at a parameter of the form of two words, a `keep own`
       one among them, passes as two words, as at any call. */
    for (i = 0; i < e->as.call.arg_count; i++) {
        const struct type *sig = m->symbol->type;
        struct ir_operand value = lower_argument(l, e->as.call.args[i]);
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

/* Whether the expression is a call whose error a handler takes. */
bool lower_is_handled_call(const struct expr *e)
{
    return e->kind == EXPR_CALL && e->as.call.builds == NULL &&
           e->as.call.handler.kind != HANDLE_NONE;
}

/* `let m = p catch fatal` and `let m = p catch e { }`. The handler runs
   when p is `none`, with an `anti.lang.NoneDereference` in hand, and it
   leaves the block or gives the binding a pointer with `yield`. */
static void lower_pointer_guard(struct lowerer *l, const struct stmt *s)
{
    const struct handler *h = &s->as.let.guard;
    const struct symbol *sym = s->as.let.symbol;
    const struct type *error_type = s->as.let.guard_make->type->result;
    struct ir_block *bad = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);
    struct ir_operand place;
    struct ir_operand err;
    struct handling scope;
    uint32_t error;

    if (sym == NULL || l->b == NULL) {
        return;
    }
    place = lower_temp(l, sym->ir);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                      lower_temp(l,
                                                 ir_load(l->f, l->b, IR_PTR,
                                                         place)),
                                      ir_int_op(IR_PTR, 0))),
              bad, join);
    l->b = bad;
    err = lower_temp(
        l, ir_call(l->f, l->b, IR_PTR,
                   ir_func_op(lower_callee_function(l, s->as.let.guard_make)),
                   NULL, 0));
    if (h->kind == HANDLE_FATAL) {
        call_fatal(l, err, error_type, join);
        l->b = join;
        return;
    }
    error = ir_unary(l->f, l->b, IR_COPY, IR_PTR, err);
    if (h->symbol != NULL) {
        h->symbol->ir = error;
    }
    scope.join = join;
    scope.out = place;
    scope.has_out = true;
    scope.error = error;
    scope.error_type = error_type;
    lower_handler(l, h, error, error_type, &scope);
    if (l->b != NULL) {
        ir_jump(l->f, l->b, join);
    }
    l->b = join;
}

/* `let (a, b) = e;`. The value stands in the place of the statement,
   and every name takes the element that stands for it. */
static void destructure(struct lowerer *l, const struct stmt *s)
{
    const struct symbol *value = s->as.let.symbol;
    const struct type *t = value->type;
    size_t i;

    for (i = 0; i < s->as.let.name_count; i++) {
        struct symbol *bound = s->as.let.names[i].symbol;
        struct ir_operand at =
            lower_offset_address(l, lower_temp(l, value->ir),
                                 lower_field_offset(l, t, &t->fields[i].name));
        if (bound == NULL) {
            return;
        }
        if (lower_is_aggregate(bound->type)) {
            ir_memcopy(l->f, l->b, lower_temp(l, bound->ir), at,
                       lower_vtype_of(l, bound->type));
        } else if (bound->address_taken) {
            ir_store(l->f, l->b, lower_ir_type_of(bound->type),
                     lower_temp(l,
                                ir_load(l->f, l->b,
                                        lower_ir_type_of(bound->type), at)),
                     lower_temp(l, bound->ir));
        } else {
            bound->ir = ir_load(l->f, l->b, lower_ir_type_of(bound->type), at);
        }
        if (local_needs_teardown(bound->type)) {
            push_exit_action(l, NULL, bound, false);
        }
    }
}

static void lower_let_value(struct lowerer *l, const struct stmt *s)
{
    struct symbol *sym = s->as.let.symbol;
    struct ir_operand v;

    /* `let c = alloc T(args) catch e { }`: the object goes on the heap,
       its `construct` runs, and the handler may put another pointer in
       c's place with `yield`. */
    if (s->as.let.value->kind == EXPR_ALLOC &&
        s->as.let.value->as.alloc.value != NULL &&
        s->as.let.value->as.alloc.value->kind == EXPR_CALL &&
        s->as.let.value->as.alloc.value->as.call.builds != NULL) {
        const struct expr *call = s->as.let.value->as.alloc.value;
        struct ir_operand place = lower_temp(l, sym->ir);
        struct ir_operand size = lower_size_operand(l, sym->type->element);
        struct ir_operand object =
            lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                  ir_func_op(lower_c_function(l, "malloc",
                                                              IR_PTR,
                                                              IR_I64)),
                                  &size, 1));
        struct ir_operand err;
        ir_store(l->f, l->b, IR_PTR, object, place);
        err = lower_construct(l, call, object);
        if (err.kind != IR_NONE) {
            lower_handle_error(l, call, err, place, true, object);
        }
        return;
    }
    /* `let n = f(args) catch e { }`: the call writes n through the out
       parameter the compiler supplies, and the handler runs on an
       error. */
    if (lower_is_handled_call(s->as.let.value)) {
        struct ir_operand out = lower_temp(l, sym->ir);
        struct ir_operand err;
        bool has_out = s->as.let.value->as.call.out != NULL;
        if (has_out) {
            lower_clear_tables(l, out, sym->type);
        }
        l->out_address = out;
        err = lower_call(l, s->as.let.value);
        lower_handle_error(l, s->as.let.value, err, out, has_out, lower_none());
        /* DESIGN: the binding is a local of its type and is torn down at
           the end of its block like any other. It is registered after the
           handler. Every path that reaches this point has a value in the
           slot: the call wrote it, or the handler gave one with `yield`.
           A handler that leaves the block never passes here. The defers it
           runs on the way out leave the slot alone. The zero table of a
           call that wrote nothing so reaches no teardown. */
        if (has_out && local_needs_teardown(sym->type)) {
            push_exit_action(l, NULL, sym, false);
        }
        return;
    }
    /* A function with its context and a match are aggregates that may be
       `none`, so the guard and the `else` below read them as well. */
    if (lower_is_aggregate(sym->type)) {
        lower_build_into(l, s->as.let.value, lower_temp(l, sym->ir));
        if (local_needs_teardown(sym->type)) {
            push_exit_action(l, NULL, sym, false);
        }
        if (!lower_none_in_first_word(sym->type)) {
            return;
        }
    } else {
        v = lower_expr(l, s->as.let.value);
        /* A local that is not address-taken gets a temporary of its own.
           An assignment to the variable it was copied from leaves it
           unchanged. */
        if (sym->address_taken) {
            ir_store(l->f, l->b, lower_ir_type_of(sym->type), v,
                     lower_temp(l, sym->ir));
        } else {
            sym->ir = ir_unary(l->f, l->b, IR_COPY,
                               lower_ir_type_of(sym->type), v);
        }
    }
    /* `let m = p catch fatal` and `let m = p catch e { }` guard the
       pointer with the error forms. The error is built on the `none`
       path alone, so the pointer that is there costs one comparison. */
    if (s->as.let.guard.kind != HANDLE_NONE) {
        lower_pointer_guard(l, s);
    }
    /* `let m = p else { }` is the one check of the nullable rules that
       emits anything. It emits what the program wrote: a comparison
       against `none` and the block that leaves. The binding below it is
       the same value, narrowed by the branch. */
    if (s->as.let.otherwise != NULL) {
        struct ir_block *otherwise = lower_new_block(l);
        struct ir_block *rest = lower_new_block(l);
        struct ir_operand held =
            sym->address_taken || lower_none_in_first_word(sym->type)
                ? lower_temp(l,
                             ir_load(l->f, l->b, IR_PTR,
                                     lower_temp(l, sym->ir)))
                : lower_temp(l, sym->ir);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, held,
                                          ir_int_op(IR_PTR, 0))),
                  otherwise, rest);
        l->b = otherwise;
        lower_block(l, s->as.let.otherwise);
        /* The block leaves, which the checker refused to compile
           otherwise, so nothing joins it back to the rest. */
        if (l->b != NULL) {
            ir_jump(l->f, l->b, rest);
        }
        l->b = rest;
    }
}

/* Whether s is `let (result, flags) = e;` of the flags form, whose value
   is the operation rather than a tuple. */
static bool is_flags_let(const struct stmt *s)
{
    return s->kind == STMT_LET && s->as.let.name_count == 2 &&
           s->as.let.value->type != NULL &&
           type_is_integer(s->as.let.value->type);
}

/* Write the flags of want into the Flags value at address. */
static void store_flags(struct lowerer *l, struct ir_operand address,
                        const struct type *t, uint8_t want,
                        const struct ir_operand flags[4])
{
    int k;

    for (k = 0; k < 4; k++) {
        if ((want >> k) & 1) {
            struct ir_operand at = lower_offset_address(
                l, address, lower_field_offset(l, t, &t->fields[k].name));
            ir_store(l->f, l->b, IR_I8, flags[k], at);
        }
    }
}

/* DESIGN: `let (result, flags) = e;` binds the result as any `let` binds
   a value, and writes the fields of the flags that the function reads.
   The flags are a local of their own, or the Flags variable in scope
   that the statement assigns. Either is a place of the frame. */
static void lower_flags_let(struct lowerer *l, const struct stmt *s)
{
    struct symbol *result = s->as.let.names[0].symbol;
    struct symbol *flags = s->as.let.names[1].symbol;
    struct ir_operand values[4];
    struct ir_operand v =
        lower_flag_operation(l, s->as.let.value, flags->flags_read, values);

    if (result->address_taken) {
        ir_store(l->f, l->b, lower_ir_type_of(result->type), v,
                 lower_temp(l, result->ir));
    } else {
        result->ir = ir_unary(l->f, l->b, IR_COPY,
                              lower_ir_type_of(result->type), v);
    }
    store_flags(l, lower_temp(l, flags->ir), flags->type, flags->flags_read,
                values);
}

/* `(result, flags) = e;` assigns the result and the flags to the two
   names, which exist. */
static void lower_flags_assign(struct lowerer *l, const struct stmt *s)
{
    const struct expr *target = s->as.assign.target;
    const struct expr *flags = target->as.tuple.elements[1];
    struct ir_operand values[4];
    struct place result;
    struct place into;
    struct ir_operand v = lower_flag_operation(
        l, s->as.assign.value, flags->symbol->flags_read, values);

    if (!lower_place(l, target->as.tuple.elements[0], &result) ||
        !lower_place(l, flags, &into)) {
        return;
    }
    if (result.in_temp) {
        ir_assign(l->f, l->b, result.temp, v);
    } else {
        ir_store(l->f, l->b, result.type, v, result.address);
    }
    store_flags(l, into.address, flags->type, flags->symbol->flags_read,
                values);
}

static void lower_let(struct lowerer *l, const struct stmt *s)
{
    if (is_flags_let(s)) {
        lower_flags_let(l, s);
        return;
    }
    lower_let_value(l, s);
    if (s->as.let.name_count > 0 && l->b != NULL) {
        destructure(l, s);
    }
}

/* DESIGN: an assertion is a branch to a block that calls the runtime and
   falls through to the rest. The failure block carries a flag. The build
   that compiles the program cuts the branch, and the ordinary passes
   remove the block, the call and the text. */
static void assert_branch(struct lowerer *l, struct ir_operand cond,
                          const struct ir_global *text)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_operand args[2];

    fail->fail = IR_FAIL_ASSERT;
    ir_branch(l->f, l->b, cond, rest, fail);
    l->b = fail;
    args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(text)));
    args[1] = ir_int_op(IR_I64, text->size - 1);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_assert_failed", params,
                                         2)),
            args, 2);
    ir_jump(l->f, l->b, rest);
    l->b = rest;
}

/* DESIGN: the first `fail` of an error writes its position and, when
   backtraces are on, its frames. An error whose `at` holds a line
   already keeps both, so the one that `try` forwards or a handler fails
   again names where it began. The test is a load and a branch, and the
   rest runs once per error. Whether backtraces are on is a call of the
   runtime, which the program's build and the command line decide. */
static void write_origin(struct lowerer *l, const struct stmt *s,
                         struct ir_operand err)
{
    static const struct name at_name = {LANG_ERROR_AT,
                                        sizeof LANG_ERROR_AT - 1};
    static const struct name frames_name = {LANG_ERROR_FRAMES,
                                            sizeof LANG_ERROR_FRAMES - 1};
    static const struct name line_name = {LANG_LOCATION_LINE,
                                          sizeof LANG_LOCATION_LINE - 1};
    const struct type *error = s->as.fail.error;
    const struct type *location = lower_field_of(error, &at_name)->type;
    struct ir_block *empty = lower_new_block(l);
    struct ir_block *capture = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_operand at;
    struct ir_operand line;
    struct ir_operand place;
    struct ir_operand on;
    struct ir_operand skip;
    struct ir_operand trace;

    at = lower_offset_address(l, err, lower_field_offset(l, error, &at_name));
    line = lower_temp(
        l, ir_load(l->f, l->b, IR_I64,
                   lower_offset_address(
                       l, at, lower_field_offset(l, location, &line_name))));
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, line,
                                      ir_int_op(IR_I64, 0))),
              empty, rest);
    l->b = empty;
    place = lower_const_address(l, lower_location_value(l, s->pos, location),
                                location);
    ir_memcopy(l->f, l->b, at, place, lower_vtype_of(l, location));
    on = lower_temp(l, ir_call(l->f, l->b, IR_I8,
                               ir_func_op(lower_rt_function_giving(
                                   l, "anti_rt_backtrace_on", IR_I8, NULL, 0)),
                               NULL, 0));
    ir_branch(l->f, l->b, on, capture, rest);
    l->b = capture;
    skip = ir_int_op(IR_I64, 0);
    trace = lower_temp(
        l, ir_call(l->f, l->b, IR_PTR,
                   ir_func_op(lower_callee_function(l, s->as.fail.capture)),
                   &skip, 1));
    ir_store(l->f, l->b, IR_PTR, trace,
             lower_offset_address(l, err,
                                  lower_field_offset(l, error, &frames_name)));
    ir_jump(l->f, l->b, rest);
    l->b = rest;
}

/* `fail e;` and `fail "text";` leave on the error channel. The error is
   built before the deferred statements of the block run, so an `undo` or
   a `defer` cannot change what the function reports. */
static void lower_fail(struct lowerer *l, const struct stmt *s)
{
    struct ir_operand err;

    if (s->as.fail.make != NULL) {
        struct ir_operand args[2];
        struct ir_function *maker = lower_callee_function(l, s->as.fail.make);
        args[0] = ir_int_op(IR_I64, 0);
        args[1] = lower_expr(l, s->as.fail.value);
        err = lower_temp(
            l, ir_call(l->f, l->b, IR_PTR, ir_func_op(maker), args, 2));
    } else {
        err = lower_expr(l, s->as.fail.value);
    }
    if (s->as.fail.error != NULL && s->as.fail.capture != NULL) {
        err = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, IR_PTR, err));
        write_origin(l, s, err);
    }
    if (has_defers(l)) {
        err = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, IR_PTR, err));
    }
    /* `fail e` hands the error a handler binds to the caller. */
    if (s->as.fail.value->kind == EXPR_NAME) {
        l->moved = s->as.fail.value->symbol;
    }
    l->failing_error = err;
    run_defers_to(l, NULL, true);
    l->failing_error = lower_none();
    l->moved = NULL;
    if (l->b != NULL) {
        ir_ret(l->f, l->b, IR_PTR, err);
    }
    l->b = NULL;
}

/* DESIGN: the cursor moves to the line of the statement before anything
   of it is emitted, so `-g` writes one `.loc` per statement. A statement
   that holds a block leaves the cursor on the last line of the block.
   That is where the code after the block comes from. */
static void lower_stmt(struct lowerer *l, const struct stmt *s)
{
    struct ir_operand v;

    if (s->pos.line > 0) {
        l->f->at_line = (uint32_t)s->pos.line;
    }
    switch (s->kind) {
    case STMT_LET:
        lower_let(l, s);
        return;
    /* `yield v` writes the value the failing call would have written and
       leaves the handler. */
    case STMT_YIELD: {
        const struct handling *h = l->handling;
        if (h == NULL) {
            return;
        }
        if (s->as.yielded != NULL && h->has_out) {
            lower_store_value(l, s->as.yielded->type, s->as.yielded, h->out);
        } else if (s->as.yielded != NULL) {
            lower_expr(l, s->as.yielded);
        }
        /* `yield` is an exit of the handler and of every block inside
           it, so the error ends here with their locals. */
        if (l->b != NULL) {
            run_defers_to(l, h->defers_at, false);
        }
        if (l->b != NULL) {
            ir_jump(l->f, l->b, h->join);
            l->b = NULL;
        }
        return;
    }
    /* DESIGN: `try { } catch e { }` gives every failing call of the body
       one handler. The first error abandons the rest of the block and
       runs its deferred statements on the way out. */
    case STMT_TRY: {
        struct try_scope scope;
        struct ir_block *handler = lower_new_block(l);
        struct ir_block *join = lower_new_block(l);
        const struct handler *h = &s->as.try_block.handler;
        scope.handler = handler;
        scope.error = ir_unary(l->f, l->b, IR_COPY, IR_PTR,
                               ir_int_op(IR_PTR, 0));
        scope.defers_at = l->defers;
        scope.outer = l->try_scope;
        l->try_scope = &scope;
        lower_block(l, s->as.try_block.body);
        l->try_scope = scope.outer;
        if (l->b != NULL) {
            ir_jump(l->f, l->b, join);
        }
        l->b = handler;
        if (h->symbol != NULL) {
            h->symbol->ir = scope.error;
        }
        lower_handler(l, h, scope.error,
                      h->symbol != NULL ? h->symbol->type : NULL, NULL);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, join);
        }
        l->b = join;
        return;
    }
    case STMT_CONST:
        /* Semantic analysis computed the value, and every use is a
           constant operand. */
        return;
    case STMT_EXPR:
        if (lower_is_handled_call(s->as.expr)) {
            struct ir_operand err = lower_call(l, s->as.expr);
            lower_handle_error(l, s->as.expr, err, lower_none(), false,
                               lower_none());
            return;
        }
        lower_expr(l, s->as.expr);
        return;
    case STMT_ASSIGN:
        lower_assign(l, s);
        return;
    case STMT_IF:
        lower_if(l, s);
        return;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        lower_loop(l, s);
        return;
    case STMT_FOR:
        if (s->as.for_loop.hooks.cursor != NULL) {
            lower_for_hooks(l, s);
            return;
        }
        lower_for(l, s);
        return;
    /* DESIGN: a switch lowers to a chain of comparisons, one block per
       arm and one join. The back end turns a dense chain into a jump
       table where it pays. An arm that ends in `fallthrough;` keeps its
       last block open. Once every arm stands, that block jumps to the
       first block of the arm the text writes next, which enters its
       body past its test. The arm's block has closed by then, so its
       `defer` statements have run. */
    case STMT_SWITCH: {
        struct ir_block *join = NULL;
        struct ir_operand over;
        enum ir_type type;
        const struct stmt *otherwise = s->as.switch_stmt.otherwise;
        size_t arms = s->as.switch_stmt.count + (otherwise != NULL ? 1 : 0);
        struct ir_block **entry =
            arena_alloc(l->m->arena, ir_product(arms + 1, sizeof *entry));
        struct ir_block **tail =
            arena_alloc(l->m->arena, ir_product(arms + 1, sizeof *tail));
        const struct stmt **falls =
            arena_alloc(l->m->arena, ir_product(arms + 1, sizeof *falls));
        uint32_t line;
        size_t i;
        size_t k;
        const struct type *variant = s->as.switch_stmt.value->type;
        struct ir_operand address = lower_none();
        over = lower_expr(l, s->as.switch_stmt.value);
        /* DESIGN: a switch on a variant reads the tag once and compares it
           with the number of each arm's case. The value stays where it
           is. An arm that binds the fields copies them before its body
           runs, so the body may replace the value. */
        if (variant->kind == TYPE_VARIANT) {
            address = over;
            over = lower_load_tag(l, variant, address);
        } else if (s->as.switch_stmt.bound != NULL) {
            /* A `str` is bound by its address, which each arm's call of
               `text.equal` reads. */
            lower_bind_value(l, s->as.switch_stmt.bound, over);
        } else {
            type = lower_ir_type_of(s->as.switch_stmt.value->type);
            over = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, type, over));
        }
        for (i = 0; i < s->as.switch_stmt.count && l->b != NULL; i++) {
            struct ir_block *arm = lower_new_block(l);
            struct ir_block *next_test = lower_new_block(l);
            const struct switch_arm *at = &s->as.switch_stmt.arms[i];
            const struct stmt *body = at->body;
            struct ir_operand test;
            if (at->test != NULL) {
                test = lower_expr(l, at->test);
            } else if (at->variant_case != 0) {
                const struct struct_field *number =
                    &variant->base->fields[at->variant_case - 1];
                test = lower_temp(
                    l, ir_binary(l->f, l->b, IR_EQ, IR_I8, over,
                                 ir_int_op(lower_ir_type_of(variant->base),
                                           number->number)));
            } else {
                struct ir_operand value = lower_expr(l, at->value);
                test = lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, over,
                                               value));
            }
            k = otherwise != NULL && i >= s->as.switch_stmt.otherwise_at
                    ? i + 1
                    : i;
            ir_branch(l->f, l->b, test, arm, next_test);
            l->b = arm;
            entry[k] = arm;
            if (at->bound != NULL) {
                ir_memcopy(l->f, l->b, lower_temp(l, at->bound->ir),
                           lower_case_address(l, variant, address),
                           lower_vtype_of(l, at->bound->type));
            }
            lower_stmt(l, body);
            if ((falls[k] = sema_arm_fallthrough(body)) != NULL) {
                tail[k] = l->b;
            } else {
                jump_to_join(l, &join);
            }
            l->b = next_test;
        }
        if (l->b != NULL && otherwise != NULL) {
            k = s->as.switch_stmt.otherwise_at;
            entry[k] = l->b;
            lower_stmt(l, otherwise);
            if ((falls[k] = sema_arm_fallthrough(otherwise)) != NULL) {
                tail[k] = l->b;
                l->b = NULL;
            }
        }
        jump_to_join(l, &join);
        line = l->f->at_line;
        for (k = 0; k + 1 < arms; k++) {
            if (tail[k] != NULL && entry[k + 1] != NULL) {
                l->f->at_line = (uint32_t)falls[k]->pos.line;
                ir_jump(l->f, tail[k], entry[k + 1]);
            }
        }
        l->f->at_line = line;
        l->b = join;
        return;
    }
    /* The switch that holds the arm makes the jump. */
    case STMT_FALLTHROUGH:
        return;
    /* DESIGN: the text of a failure is built here and lives in the
       read-only data of the module. The back end needs no formatting,
       and a build without assertions drops the whole string. */
    case STMT_ASSERT: {
        struct token_text text;
        struct text message = {0};
        struct ir_operand cond = lower_expr(l, s->as.assertion.cond);
        if (s->as.assertion.message.length > 0) {
            text_appendf(&message, "%s:%d: assertion failed: %.*s", l->file,
                         s->as.assertion.cond->pos.line,
                         (int)s->as.assertion.message.length,
                         s->as.assertion.message.bytes);
        } else {
            text_appendf(&message, "%s:%d: assertion failed: %.*s", l->file,
                         s->as.assertion.cond->pos.line,
                         (int)s->as.assertion.text.length,
                         s->as.assertion.text.bytes);
        }
        text.bytes = text_cstr(&message);
        text.length = message.length;
        assert_branch(l, cond, lower_literal_global(l, &text));
        text_free(&message);
        return;
    }
    case STMT_DEFER:
    case STMT_UNDO:
        push_exit_action(l, s->as.deferred, NULL, s->kind == STMT_UNDO);
        return;
    case STMT_FAIL:
        lower_fail(l, s);
        return;
    case STMT_BREAK:
        run_defers_to(l, l->loop->defers_at, false);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, l->loop->break_to);
        }
        l->b = NULL;
        return;
    case STMT_CONTINUE:
        run_defers_to(l, l->loop->defers_at, false);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, l->loop->continue_to);
        }
        l->b = NULL;
        return;
    case STMT_RETURN:
        /* DESIGN: a `may fail` function puts what it computed through
           its out pointer and returns `none` on the error channel, which
           is the convention its callers already read. The value is
           written before the deferred statements run, as it is for an
           ordinary `return`. */
        if (l->result_out.kind != IR_NONE && s->as.return_value != NULL) {
            lower_store_value(l, s->as.return_value->type, s->as.return_value,
                              l->result_out);
            if (s->as.return_value->kind == EXPR_NAME) {
                l->moved = s->as.return_value->symbol;
            }
            run_defers_to(l, NULL, false);
            l->moved = NULL;
            if (l->b != NULL) {
                ir_ret(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0));
            }
            l->b = NULL;
            return;
        }
        if (s->as.return_value == NULL) {
            run_defers_to(l, NULL, false);
            if (l->b == NULL) {
                return;
            }
            /* A `may fail` function without a result reports success at
               its closing brace and at every `return`. */
            if (l->may_fail) {
                ir_ret(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0));
            } else {
                ir_ret(l->f, l->b, IR_VOID, lower_none());
            }
        } else {
            v = lower_expr(l, s->as.return_value);
            /* The value is computed before the deferred statements run,
               so a `defer` cannot change what the function returns. A
               function without one keeps the value where it is. */
            if (has_defers(l) &&
                !lower_is_aggregate(s->as.return_value->type)) {
                v = lower_temp(
                    l, ir_unary(l->f, l->b, IR_COPY,
                                lower_ir_type_of(s->as.return_value->type), v));
            }
            /* DESIGN: `return local` hands the value to the caller, so
               the local is not destroyed on the way out. Its `own`
               fields belong to the returned value now. Every other local
               of the scope is destroyed as usual. */
            if (s->as.return_value->kind == EXPR_NAME) {
                l->moved = s->as.return_value->symbol;
            }
            run_defers_to(l, NULL, s->error_exit);
            l->moved = NULL;
            if (l->b == NULL) {
                return;
            }
            ir_ret(l->f, l->b, l->f->result == IR_AGG ? IR_PTR : l->f->result,
                   v);
        }
        l->b = NULL;
        return;
    case STMT_BLOCK:
        lower_block(l, s->as.block);
        return;
    case STMT_SYNC:
        lower_sync(l, s);
        return;
    case STMT_SELECT:
        lower_select(l, s);
        return;
    }
}

/* Run the statements of one block scope, last declared first.
   DESIGN: the statements of `undo` run before the `defer` statements of
   the same block, so the block undoes what it did while its locals are
   still there. One reverse pass takes the `undo` actions and a second
   takes the rest, which is the order the specification gives. */
void lower_run_defers(struct lowerer *l, const struct defers *scope,
                      bool failing)
{
    size_t i;

    if (failing) {
        for (i = scope->count; i > 0 && l->b != NULL; i--) {
            const struct exit_action *action = &scope->items[i - 1];
            if (action->undo) {
                lower_stmt(l, action->stmt);
            }
        }
    }
    for (i = scope->count; i > 0 && l->b != NULL; i--) {
        const struct exit_action *action = &scope->items[i - 1];
        if (action->undo) {
            continue;
        }
        if (action->snapshot) {
            lower_free_snapshot(l, action->local->type,
                                lower_temp(l, action->local->ir));
        } else if (action->leave) {
            if (failing) {
                lower_hook_failed(l, l->failing_error);
            }
            lower_hook_call(l, HOOK_LEAVE);
        } else if (action->unlock) {
            static const enum ir_type handle[] = {IR_PTR};
            struct ir_operand mutex = lower_temp(l, action->mutex);
            ir_call(l->f, l->b, IR_VOID,
                    ir_func_op(lower_rt_function(l, action->unlock_fn,
                                                 handle,
                                                 1)),
                    &mutex, 1);
        } else if (action->error) {
            if (action->local == NULL || action->local != l->moved) {
                lower_object_call(l, "anti_rt_delete",
                                  lower_temp(l, action->error_temp),
                                  action->error_type);
            }
        } else if (action->stmt != NULL) {
            lower_stmt(l, action->stmt);
        } else if (action->local != l->moved) {
            destroy_local(l, action->local);
        }
    }
}

/* Run every scope from the innermost out to stop, which is not run. */
static void run_defers_to(struct lowerer *l, const struct defers *stop,
                          bool failing)
{
    const struct defers *scope;

    for (scope = l->defers; scope != stop; scope = scope->outer) {
        lower_run_defers(l, scope, failing);
    }
}

/* Anti has no labels, so a statement after return, break or continue is
   unreachable. Lowering skips it. */
void lower_block(struct lowerer *l, const struct block *b)
{
    struct defers scope;
    size_t i;

    memset(&scope, 0, sizeof scope);
    scope.outer = l->defers;
    l->defers = &scope;
    for (i = 0; i < b->count && l->b != NULL; i++) {
        lower_stmt(l, b->stmts[i]);
    }
    /* The closing brace is an exit of the block, and never an error. */
    lower_run_defers(l, &scope, false);
    l->defers = scope.outer;
    free(scope.items);
}

static void reserve_handler(struct lowerer *l, struct ir_block *entry,
                            const struct handler *h)
{
    if (h->kind == HANDLE_BLOCK && h->body != NULL) {
        lower_reserve_slots(l, entry, h->body);
    }
}

/* The handler of the failing call that e is, or that `alloc T(args)`
   runs. */
static void reserve_call_handler(struct lowerer *l, struct ir_block *entry,
                                 const struct expr *e)
{
    if (e == NULL) {
        return;
    }
    if (e->kind == EXPR_ALLOC && e->as.alloc.value != NULL) {
        e = e->as.alloc.value;
    }
    if (e->kind == EXPR_OPTIONAL) {
        e = e->as.optional.access;
    }
    if (e->kind == EXPR_CALL) {
        reserve_handler(l, entry, &e->as.call.handler);
    }
}

static void reserve_stmt(struct lowerer *l, struct ir_block *entry,
                         const struct stmt *s)
{
    struct symbol *sym;
    size_t j;

    switch (s->kind) {
    case STMT_LET:
        sym = s->as.let.symbol;
        /* The pair of the flags form has no place of its own. */
        if (!is_flags_let(s) &&
            (sym->address_taken || lower_is_aggregate(sym->type))) {
            sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        }
        /* The names of `let (a, b) = e;` are locals like any other,
           and the value they come from is the symbol above. A flags
           name that assigns a Flags variable has its place already. */
        for (j = 0; j < s->as.let.name_count; j++) {
            struct symbol *bound = s->as.let.names[j].symbol;
            if (bound != NULL && !s->as.let.names[j].assigns &&
                (bound->address_taken || lower_is_aggregate(bound->type))) {
                bound->ir = ir_slot(l->f, entry,
                                    lower_vtype_of(l, bound->type));
            }
        }
        reserve_call_handler(l, entry, s->as.let.value);
        reserve_handler(l, entry, &s->as.let.guard);
        if (s->as.let.otherwise != NULL) {
            lower_reserve_slots(l, entry, s->as.let.otherwise);
        }
        break;
    case STMT_EXPR:
        reserve_call_handler(l, entry, s->as.expr);
        break;
    case STMT_ASSIGN:
        reserve_call_handler(l, entry, s->as.assign.value);
        break;
    case STMT_IF:
        for (j = 0; j < s->as.if_chain.count; j++) {
            lower_reserve_slots(l, entry, s->as.if_chain.branches[j].body);
        }
        if (s->as.if_chain.else_body != NULL) {
            lower_reserve_slots(l, entry, s->as.if_chain.else_body);
        }
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        lower_reserve_slots(l, entry, s->as.loop.body);
        break;
    case STMT_FOR:
        /* The element of a `for` over an array or a slice is copied
           into a place of its own when it is an aggregate. */
        sym = s->as.for_loop.name_count > 0
                  ? s->as.for_loop
                        .names[s->as.for_loop.name_count - 1].symbol
                  : NULL;
        if (sym != NULL &&
            (sym->address_taken || lower_is_aggregate(sym->type))) {
            sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        }
        /* The index of `for i, x in items` has a place of its own when a
           closure captures it. */
        for (j = 0; j + 1 < s->as.for_loop.name_count; j++) {
            struct symbol *name = s->as.for_loop.names[j].symbol;
            if (name != NULL && name->address_taken) {
                name->ir = ir_slot(l->f, entry,
                                   lower_vtype_of(l, name->type));
            }
        }
        lower_reserve_slots(l, entry, s->as.for_loop.body);
        break;
    case STMT_DEFER:
    case STMT_UNDO:
        reserve_stmt(l, entry, s->as.deferred);
        break;
    case STMT_SWITCH:
        for (j = 0; j < s->as.switch_stmt.count; j++) {
            /* The name an arm binds holds a copy of the fields of its
               case, a struct in a place of its own. */
            sym = s->as.switch_stmt.arms[j].bound;
            if (sym != NULL) {
                sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
            }
            reserve_stmt(l, entry, s->as.switch_stmt.arms[j].body);
        }
        if (s->as.switch_stmt.otherwise != NULL) {
            reserve_stmt(l, entry, s->as.switch_stmt.otherwise);
        }
        break;
    case STMT_TRY:
        lower_reserve_slots(l, entry, s->as.try_block.body);
        reserve_handler(l, entry, &s->as.try_block.handler);
        break;
    case STMT_BLOCK:
        lower_reserve_slots(l, entry, s->as.block);
        break;
    case STMT_SYNC:
        lower_reserve_slots(l, entry, s->as.sync.body);
        break;
    case STMT_SELECT:
        for (j = 0; j < s->as.select.count; j++) {
            reserve_stmt(l, entry, s->as.select.arms[j].body);
        }
        break;
    default:
        break;
    }
}

void lower_reserve_slots(struct lowerer *l, struct ir_block *entry,
                         const struct block *b)
{
    size_t i;

    for (i = 0; i < b->count; i++) {
        reserve_stmt(l, entry, b->stmts[i]);
    }
}
