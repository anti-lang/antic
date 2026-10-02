#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* Whether any block the function is inside has a statement to run. */
bool lower_has_defers(const struct lowerer *l)
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
    scope->items = alloc_grow(scope->items, &scope->capacity, scope->count,
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
    action->pair = false;
    action->second = 0;
    action->unlock_fn = RT_FUNCTION_COUNT;
    action->leave = false;
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

/* Record the delete of the error a handler binds, whose name is sym or
   which has none. */
void lower_push_error_action(struct lowerer *l, const struct symbol *sym,
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
    action->pair = false;
    action->second = 0;
    action->unlock_fn = RT_FUNCTION_COUNT;
    action->leave = false;
}

/* Record the unlock of the lock whose address is in mutex, which a
   `sync` holds until its block ends and a synchronized function until
   it returns. unlock_fn names the function of the runtime. */
void lower_push_unlock_action(struct lowerer *l, uint32_t mutex,
                              enum rt_function unlock_fn)
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
    action->pair = false;
    action->second = 0;
    action->unlock_fn = unlock_fn;
    action->leave = false;
}

void lower_jump_to_join(struct lowerer *l, struct ir_block **join)
{
    if (l->b == NULL) {
        return;
    }
    if (*join == NULL) {
        *join = lower_new_block(l);
    }
    ir_jump(l->f, l->b, *join);
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
        lower_jump_to_join(l, &join);
        l->b = next;
    }
    if (s->as.if_chain.else_body != NULL) {
        lower_block(l, s->as.if_chain.else_body);
        lower_jump_to_join(l, &join);
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
    loop.temps_at = l->temp_count;
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


/* DESIGN: the check of a walk over a collection that counts its changes.
   The test of every turn compares the two counts before it calls `next`.
   A change in the body then stops the program before the iterator reads
   the collection again. The text names the loop and the collection. The
   failure block reads the place of the last change, and the runtime
   adds it. A build without the checks cuts the branch, and the
   ordinary passes remove the block, the call and the text. */
static void walk_check(struct lowerer *l, const struct stmt *s)
{
    const struct iteration *it = &s->as.for_loop.hooks;
    struct token_text text;
    struct text message = {0};
    const struct ir_global *global;
    struct ir_operand changed = lower_expr(l, it->changed);
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_operand args[5];

    text_appendf(&message, "%s:%d: `%.*s` was changed while `for` walked it",
                 l->file, s->pos.line,
                 (int)s->as.for_loop.over_text.length,
                 s->as.for_loop.over_text.bytes);
    text.bytes = text_cstr(&message);
    text.length = message.length;
    global = lower_literal_global(l, &text);
    text_free(&message);
    fail->fail = IR_FAIL_CHECK;
    ir_branch(l->f, l->b, changed, fail, rest);
    l->b = fail;
    args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(global)));
    args[1] = ir_int_op(IR_I64, global->size - 1);
    args[2] = lower_expr(l, it->change_file);
    args[3] = lower_expr(l, it->change_file_length);
    args[4] = lower_expr(l, it->change_line);
    lower_rt_call(l, RT_FN_WALK_CHANGED, args);
    ir_jump(l->f, l->b, rest);
    l->b = rest;
}

static void bind_loop_name(struct lowerer *l, struct symbol *sym,
                           enum ir_type type, struct ir_operand value);

/* Give the names of `for (k, v) in e` the parts of the element at the
   address at. An element lent whole gives each name the address of its
   part. Any other element gives each name the value of its part: a copy,
   or the lent pointer the value of the iterator holds there. In the form
   `for (k, v) in e` a lent part gives a copy of what it points at. The
   element keeps its teardown, and the names, which are read-only, take
   none. */
static void bind_pattern(struct lowerer *l, const struct stmt *s,
                         struct ir_operand at)
{
    const struct type *tuple = s->as.for_loop.element->type;
    size_t i;

    if (tuple->kind == TYPE_POINTER) {
        tuple = tuple->element;
    }
    for (i = 0; i < s->as.for_loop.name_count; i++) {
        struct symbol *name = s->as.for_loop.names[i].symbol;
        struct ir_operand part = lower_offset_address(
            l, at, lower_field_offset(l, tuple, &tuple->fields[i].name));
        if (types_is_lent(tuple->params[i]) && !types_is_lent(name->type)) {
            part = lower_temp(l, ir_load(l->f, l->b, IR_PTR, part));
        }
        if (s->as.for_loop.element->type->kind == TYPE_POINTER) {
            bind_loop_name(l, name, IR_PTR, part);
        } else if (lower_is_aggregate(name->type)) {
            ir_memcopy(l->f, l->b, lower_temp(l, name->ir), part,
                       lower_vtype_of(l, name->type));
        } else {
            enum ir_type type = lower_ir_type_of(name->type);
            bind_loop_name(l, name, type,
                           lower_temp(l, ir_load(l->f, l->b, type, part)));
        }
    }
}

/* DESIGN: `for x in e` over a collection or an iterator is the loop of
   its `while` form. It binds the iterator once, calls `next` in the test
   and `value` at the head of the body, and `continue` goes to the test.
   The iterator lives in a scope around the loop and each value in a scope
   around one pass of the body, so each is torn down as a `let` of its
   type is, the value on every exit of the pass. */
static void lower_for_hooks(struct lowerer *l, const struct stmt *s)
{
    const struct iteration *it = &s->as.for_loop.hooks;
    /* A pattern, and the copy of a value with lent parts, bind the whole
       element first and read it. */
    struct symbol *sym = s->as.for_loop.element != NULL
                             ? s->as.for_loop.element
                             : s->as.for_loop.names[0].symbol;
    /* `for x in &e` binds the lent pointer, and `for x in e` the copy. A
       value with lent parts is bound as it is, and the copy read from it. */
    const struct expr *current =
        s->as.for_loop.by_pointer || types_holds_lent(sym->type) ? it->place
                                                                : it->current;
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
    if (sema_needs_teardown(it->cursor->type)) {
        push_exit_action(l, NULL, it->cursor, false);
    }
    loop.continue_to = test;
    loop.break_to = exit;
    loop.outer = l->loop;
    loop.defers_at = l->defers;
    loop.temps_at = l->temp_count;
    l->loop_depth++;
    ir_jump(l->f, l->b, test);
    l->b = test;
    if (it->changed != NULL) {
        walk_check(l, s);
    }
    lower_branch(l, it->advance, body, exit);
    l->loop = &loop;
    l->b = body;
    memset(&pass, 0, sizeof pass);
    pass.outer = l->defers;
    l->defers = &pass;
    if (lower_is_aggregate(sym->type)) {
        lower_build_into(l, current, lower_temp(l, sym->ir));
    } else {
        /* The call may end the block it starts in, so the value is
           bound in the block that follows it. */
        struct ir_operand v = lower_expr(l, current);
        if (sym->address_taken) {
            ir_store(l->f, l->b, lower_ir_type_of(sym->type), v,
                     lower_temp(l, sym->ir));
        } else {
            sym->ir = ir_unary(l->f, l->b, IR_COPY,
                               lower_ir_type_of(sym->type), v);
        }
    }
    /* A copy of an element that the iterator lends belongs to the
       collection, as the copy of a slice element does. A value with lent
       parts gives the loop the parts it receives by value. The teardown
       of the value leaves its pointers alone. */
    if ((it->place == NULL || types_holds_lent(sym->type)) &&
        sema_needs_teardown(sym->type)) {
        push_exit_action(l, NULL, sym, false);
    }
    if (s->as.for_loop.pattern) {
        bind_pattern(l, s, lower_temp(l, sym->ir));
    } else if (s->as.for_loop.element != NULL) {
        struct symbol *copy = s->as.for_loop.names[0].symbol;
        lower_copy_parts(l, sym->type, copy->type, lower_temp(l, sym->ir),
                         lower_temp(l, copy->ir));
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
    struct symbol *index = names > 1 && !s->as.for_loop.pattern
                               ? s->as.for_loop.names[0].symbol
                               : NULL;
    const struct expr *over = s->as.for_loop.over;
    struct ir_block *test = over != NULL ? lower_new_block(l) : NULL;
    struct ir_block *body = lower_new_block(l);
    struct ir_block *step = lower_new_block(l);
    struct ir_block *exit = lower_new_block(l);
    struct loop loop;
    struct ir_operand low = lower_none();
    struct ir_operand high = lower_none();
    struct ir_operand base = lower_none();
    const struct type *seq = NULL;
    enum ir_type counter_type = IR_I64;
    uint32_t counter;
    /* DESIGN: the step keeps its sign apart from its magnitude. The
       checker stores `by k` as an int64_t, so a step of 2^63 on an
       unsigned range reads as INT64_MIN. A range of an unsigned type
       walks up, since the checker refuses a negative step on it, and its
       bounds compare unsigned. The magnitude is negated in uint64_t,
       where -2^63 has one. The checker refuses a step that does not fit
       the type of the range, so k fits the unsigned type of its width. */
    int64_t stride = s->as.for_loop.step_value;
    bool unsigned_range =
        over == NULL && !types_is_signed(s->as.for_loop.low->type);
    bool down = stride < 0 && !unsigned_range;
    uint64_t k = down ? 0 - (uint64_t)stride : (uint64_t)stride;

    /* The bound is read once, before the loop. */
    if (over != NULL) {
        struct ir_operand limit;

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
        ir_jump(l->f, l->b, test);
        l->b = test;
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_SLT, IR_I8,
                                          lower_temp(l, counter), limit)),
                  body, exit);
    } else {
        struct ir_block *first = down ? lower_new_block(l) : body;
        struct ir_operand read;
        /* DESIGN: a range never computes a value past its ends. The
           counter holds the value of this turn, and the step measures
           the distance to the bound that ends the walk before it moves:
           `high - counter` upward, `counter - low` downward. Both are
           differences of two values inside the range, so they fit the
           unsigned type of the width and never wrap. The walk leaves when
           the distance is at most k upward and below k downward, where
           the next value would be past the end. A test of the moved
           counter against the bound would wrap at the end of the type:
           `0 as u8..255 by 10` would never stop. Every comparison and the
           division are unsigned for that reason, and only the test of an
           empty range compares by the sign of the type.

           The bounds are copied, since the body may assign the variable
           a bound names. */
        counter_type = lower_ir_type_of(s->as.for_loop.low->type);
        read = lower_expr(l, s->as.for_loop.low);
        low = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, counter_type,
                                     read));
        read = lower_expr(l, s->as.for_loop.high);
        high = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, counter_type,
                                      read));
        counter = ir_unary(l->f, l->b, IR_COPY, counter_type, low);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b,
                                          unsigned_range ? IR_ULT : IR_SLT,
                                          IR_I8, low, high)),
                  first, exit);
        /* DESIGN: `by -k` walks the values of `by k` in reverse, so it
           starts at the largest of them and not at the high bound. That
           value is `low + ((high - low - 1) / k) * k`, and the division
           is by a constant and runs once. */
        if (down) {
            struct ir_operand span;
            struct ir_operand steps;
            struct ir_operand last;

            l->b = first;
            span = lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                           high, low));
            span = lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                           span, ir_int_op(counter_type, 1)));
            steps = lower_temp(l, ir_binary(l->f, l->b, IR_UDIV, counter_type,
                                            span,
                                            ir_int_op(counter_type, k)));
            last = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, counter_type,
                                           steps,
                                           ir_int_op(counter_type, k)));
            ir_assign(l->f, l->b, counter,
                      lower_temp(l, ir_binary(l->f, l->b, IR_ADD,
                                              counter_type, low, last)));
            ir_jump(l->f, l->b, body);
        }
    }
    loop.continue_to = step;
    loop.break_to = exit;
    loop.outer = l->loop;
    loop.defers_at = l->defers;
    loop.temps_at = l->temp_count;
    l->loop_depth++;
    l->loop = &loop;
    l->b = body;
    /* The variable of the body: the counter of a range, or the element
       that the index reaches. A range without a name counts and reads
       nothing. */
    if (over == NULL) {
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
        /* A pattern reads its parts where the element stands. */
        if (s->as.for_loop.pattern) {
            bind_pattern(l, s, at);
        } else if (s->as.for_loop.by_pointer) {
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
    if (over == NULL) {
        struct ir_block *advance = lower_new_block(l);
        struct ir_operand left =
            down ? lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                           lower_temp(l, counter), low))
                 : lower_temp(l, ir_binary(l->f, l->b, IR_SUB, counter_type,
                                           high, lower_temp(l, counter)));

        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, down ? IR_ULT : IR_ULE,
                                          IR_I8, left,
                                          ir_int_op(counter_type, k))),
                  exit, advance);
        l->b = advance;
        ir_assign(l->f, l->b, counter,
                  lower_temp(l, ir_binary(l->f, l->b, down ? IR_SUB : IR_ADD,
                                          counter_type,
                                          lower_temp(l, counter),
                                          ir_int_op(counter_type, k))));
        ir_jump(l->f, l->b, body);
    } else {
        ir_assign(l->f, l->b, counter,
                  lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                          lower_temp(l, counter),
                                          ir_int_op(IR_I64, k))));
        ir_jump(l->f, l->b, test);
    }
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

/* An assignment evaluates the place first and the value second. A
   compound assignment reads the old value before it evaluates the new
   operand, as x = x + e reads x first. */
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
        lower_hook_changed(l, &p, target);
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
        if (sema_needs_teardown(target->type)) {
            lower_destroy_owned(l, target->type, p.address,
                                ir_int_op(IR_PTR, 0), true);
        }
        ir_memcopy(l->f, l->b, p.address, v, lower_vtype_of(l, target->type));
        lower_clear_moved(l, value);
        lower_hook_changed(l, &p, target);
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
    lower_hook_changed(l, &p, target);
}

/* DESIGN: a call whose result the statement drops gives a value that no
   local holds, so the statement tears it down where it ends, as a `let`
   of it would be torn down at the end of its block. value is what the
   call lowered to. */
static void drop_result(struct lowerer *l, const struct expr *e,
                        struct ir_operand value)
{
    if (e->kind != EXPR_CALL || e->as.call.hashes || e->type == NULL ||
        l->b == NULL || !sema_needs_teardown(e->type)) {
        return;
    }
    lower_destroy_owned(l, e->type, value, ir_int_op(IR_PTR, 0), false);
}

/* DESIGN: a local whose type owns something is torn down at the end of
   its block, as if the program had written `defer destroy(&c)` after the
   `let`: a class value, a struct, a tuple or a variant part by part, an
   array element by element and a `?T` when its flag is set, at any depth.
   sema_needs_teardown says which, and lower_destroy_owned is the one
   teardown. A local that may have moved, and an `own` parameter, are torn
   down only when their table is not zero. A `keep own` parameter frees
   its snapshot, which holds `none` once it moved. A heap object is never
   torn down by itself, and `delete` is the only way to free one. */
static void destroy_local(struct lowerer *l, const struct symbol *sym)
{
    lower_destroy_owned(l, sym->type, lower_temp(l, sym->ir),
                        ir_int_op(IR_PTR, 0), sym->moved || sym->own_param);
}

/* DESIGN: an `own` parameter belongs to its function, which tears it
   down at every exit unless it moved on. A move clears its table, so the
   teardown passes over a value whose table is zero. */
void lower_push_own_action(struct lowerer *l, const struct symbol *param)
{
    if (sema_needs_teardown(param->type)) {
        push_exit_action(l, NULL, param, false);
    }
}

/* DESIGN: a local that moves into an `own` parameter hands its value to
   the call. A value that needs a teardown is copied into a slot of the
   frame, which the call takes. The local's table is then cleared, so its
   own teardown passes over it. value is what the argument lowered to. */
struct ir_operand lower_move_argument(struct lowerer *l, const struct expr *arg,
                                      struct ir_operand value)
{
    uint32_t slot;

    if (!sema_needs_teardown(arg->type) || l->b == NULL) {
        return value;
    }
    slot = ir_entry_slot(l->f, lower_vtype_of(l, arg->type));
    ir_memcopy(l->f, l->b, lower_temp(l, slot), value,
               lower_vtype_of(l, arg->type));
    lower_clear_owned(l, arg->type, value);
    return lower_temp(l, slot);
}

/* An `own` parameter that `=` or `let` moved into a place, and a local
   that moved into a literal, hold their value no more, so the tables are
   cleared once the bytes are copied. */
void lower_clear_moved(struct lowerer *l, const struct expr *value)
{
    if (value->kind != EXPR_NAME || !value->moves || value->symbol == NULL ||
        value->symbol->caught || !sema_needs_teardown(value->type) ||
        l->b == NULL) {
        return;
    }
    lower_clear_owned(l, value->type, lower_temp(l, value->symbol->ir));
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
        if (sema_needs_teardown(bound->type)) {
            push_exit_action(l, NULL, bound, false);
        }
    }
}

/* DESIGN: `let v = o else { }` and `let v = o catch ...` on a `?T` of a
   value build o in a slot of its own and test its flag. The path where
   it is there copies the value into v. The other runs the `else` or the
   handler, which leaves or gives v a value with `yield`. v is torn down
   from there on, as any local of its type is. */
static void lower_let_unwrap(struct lowerer *l, const struct stmt *s)
{
    struct symbol *sym = s->as.let.symbol;
    const struct type *t = s->as.let.value->type;
    struct ir_operand held =
        lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, t)));
    struct ir_block *have = lower_new_block(l);
    struct ir_block *missing = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_operand place;

    lower_build_into(l, s->as.let.value, held);
    if (l->b == NULL || sym == NULL) {
        return;
    }
    lower_clear_moved(l, s->as.let.value);
    place = lower_temp(l, sym->ir);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                      lower_optional_flag(l, t, held),
                                      ir_int_op(IR_I8, 0))),
              have, missing);
    l->b = have;
    if (lower_is_aggregate(sym->type)) {
        ir_memcopy(l->f, l->b, place, held, lower_vtype_of(l, sym->type));
    } else {
        ir_store(l->f, l->b, lower_ir_type_of(sym->type),
                 lower_temp(l, ir_load(l->f, l->b,
                                       lower_ir_type_of(sym->type), held)),
                 place);
    }
    ir_jump(l->f, l->b, rest);
    l->b = missing;
    if (s->as.let.otherwise != NULL) {
        lower_block(l, s->as.let.otherwise);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, rest);
        }
    } else {
        lower_guard_missing(l, s, rest);
    }
    l->b = rest;
    if (sema_needs_teardown(sym->type)) {
        push_exit_action(l, NULL, sym, false);
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
        struct ir_operand object = lower_new_memory(l, size);
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
        if (has_out && sema_needs_teardown(sym->type)) {
            lower_clear_owned(l, sym->type, out);
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
        if (has_out && sema_needs_teardown(sym->type) &&
            s->as.let.name_count == 0) {
            push_exit_action(l, NULL, sym, false);
        }
        return;
    }
    if ((s->as.let.otherwise != NULL ||
         s->as.let.guard.kind != HANDLE_NONE) &&
        s->as.let.value->type->kind == TYPE_OPTIONAL) {
        lower_let_unwrap(l, s);
        return;
    }
    /* A function with its context is an aggregate that may be `none`, so
       the guard and the `else` below read it as well. */
    if (lower_is_aggregate(sym->type)) {
        lower_build_into(l, s->as.let.value, lower_temp(l, sym->ir));
        lower_clear_moved(l, s->as.let.value);
        /* A destructuring hands each part to its name, which tears it
           down, so the value it takes apart takes no teardown. */
        if (sema_needs_teardown(sym->type) && s->as.let.name_count == 0) {
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
           types_is_integer(s->as.let.value->type);
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
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_operand args[2];

    fail->fail = IR_FAIL_ASSERT;
    ir_branch(l->f, l->b, cond, rest, fail);
    l->b = fail;
    args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(text)));
    args[1] = ir_int_op(IR_I64, text->size - 1);
    lower_rt_call(l, RT_FN_ASSERT_FAILED, args);
    ir_jump(l->f, l->b, rest);
    l->b = rest;
}

/* DESIGN: the cursor moves to the line of the statement before anything
   of it is emitted, so `-g` writes one `.loc` per statement. A statement
   that holds a block leaves the cursor on the last line of the block.
   That is where the code after the block comes from. */
static void lower_stmt_kind(struct lowerer *l, const struct stmt *s);

bool lower_is_fresh(const struct expr *e)
{
    return e->kind == EXPR_CALL || e->kind == EXPR_STRUCT_LIT ||
           e->kind == EXPR_TUPLE ||
           (e->kind == EXPR_NAME && e->symbol != NULL &&
            e->symbol->kind == SYMBOL_CONST && sema_holds_class(e->type));
}

/* DESIGN: a fresh value that a statement reads through a field or passes
   as a receiver lives to the end of the statement, as a temporary of C++
   does, and is then torn down. `Box.make(1).n` reads the field and then
   tears the box down. The value may be made on one path of the statement
   alone, so a slot of the frame holds its address once it is made and
   zero before, and the end of the statement tears down what the slot
   holds. A condition of `if` or of a loop ends its values before its
   branch, and an exit of the function ends every value still kept. */
void lower_keep_temp(struct lowerer *l, const struct expr *e,
                     struct ir_operand address)
{
    struct statement_temp *t;

    if (l->b == NULL || e == NULL || e->type == NULL ||
        !lower_is_fresh(e) ||
        (e->kind == EXPR_CALL && e->as.call.hashes) ||
        e->type->kind == TYPE_POINTER || !sema_needs_teardown(e->type)) {
        return;
    }
    l->temps = alloc_grow(l->temps, &l->temp_capacity, l->temp_count,
                          sizeof *l->temps);
    t = &l->temps[l->temp_count++];
    t->holder = ir_entry_zero_slot(l->f);
    t->type = e->type;
    ir_store(l->f, l->b, IR_PTR, address, lower_temp(l, t->holder));
}

void lower_end_temps(struct lowerer *l, size_t mark, bool pop)
{
    size_t i;

    for (i = l->temp_count; i > mark && l->b != NULL; i--) {
        const struct statement_temp *t = &l->temps[i - 1];
        struct ir_operand holder = lower_temp(l, t->holder);
        struct ir_operand p =
            lower_temp(l, ir_load(l->f, l->b, IR_PTR, holder));
        struct ir_block *made = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, p,
                                          ir_int_op(IR_PTR, 0))),
                  made, after);
        l->b = made;
        lower_destroy_owned(l, t->type, p, ir_int_op(IR_PTR, 0), false);
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), holder);
        ir_jump(l->f, l->b, after);
        l->b = after;
    }
    if (pop && l->temp_count > mark) {
        l->temp_count = mark;
    }
}

void lower_stmt(struct lowerer *l, const struct stmt *s)
{
    size_t mark = l->temp_count;

    lower_stmt_kind(l, s);
    lower_end_temps(l, mark, true);
}

static void lower_stmt_kind(struct lowerer *l, const struct stmt *s)
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
            lower_run_defers_to(l, h->defers_at, false);
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
        /* DESIGN: a call whose error a handler takes and whose result the
           statement drops writes the result into a slot of its own, as
           an operand does, since no place of a `let` is there to take
           it. What the call or a `yield` of the handler left there is
           torn down where the statement ends. A handler that leaves no
           value leaves the zero tables, which the teardown passes
           over. */
        if (lower_is_handled_call(s->as.expr)) {
            const struct expr *call = s->as.expr;
            struct ir_operand value = lower_handled_operand(l, call);
            if (call->as.call.out != NULL && l->b != NULL &&
                sema_needs_teardown(call->type)) {
                lower_destroy_owned(l, call->type, value,
                                    ir_int_op(IR_PTR, 0), true);
            }
            return;
        }
        drop_result(l, s->as.expr, lower_expr(l, s->as.expr));
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
            arena_alloc(l->m->arena, alloc_product(arms + 1, sizeof *entry));
        struct ir_block **tail =
            arena_alloc(l->m->arena, alloc_product(arms + 1, sizeof *tail));
        const struct stmt **falls =
            arena_alloc(l->m->arena, alloc_product(arms + 1, sizeof *falls));
        uint32_t line;
        size_t i;
        size_t k;
        const struct type *variant = s->as.switch_stmt.value->type;
        struct ir_operand address = lower_none();
        struct defers arm_scope;
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
            memset(&arm_scope, 0, sizeof arm_scope);
            arm_scope.outer = l->defers;
            l->defers = &arm_scope;
            /* DESIGN: an arm that binds gets a copy of the fields of its
               case. What they own is copied as `dup` copies it, and the
               arm tears the copy down on every exit, so the variant keeps
               its own. */
            if (at->bound != NULL) {
                /* The address and the type are bound first, since C
                   leaves the order of two calls with effects in one
                   argument list open. */
                struct ir_operand fields =
                    lower_case_address(l, variant, address);
                struct ir_vtype bound = lower_vtype_of(l, at->bound->type);
                ir_memcopy(l->f, l->b, lower_temp(l, at->bound->ir), fields,
                           bound);
                if (sema_needs_teardown(at->bound->type)) {
                    lower_copy_owned(l, at->bound->type, fields,
                                     lower_temp(l, at->bound->ir));
                    push_exit_action(l, NULL, at->bound, false);
                }
            }
            lower_stmt(l, body);
            if (l->b != NULL) {
                lower_run_defers(l, &arm_scope, false);
            }
            l->defers = arm_scope.outer;
            free(arm_scope.items);
            if ((falls[k] = sema_arm_fallthrough(body)) != NULL) {
                tail[k] = l->b;
            } else {
                lower_jump_to_join(l, &join);
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
        lower_jump_to_join(l, &join);
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
    /* DESIGN: `break` and `continue` in a handler leave their statement
       before its end, so each ends the temporaries that statements inside
       the loop still keep, as an exit of the function ends them all. The
       temporaries of the loop itself stay. */
    case STMT_BREAK:
        lower_end_temps(l, l->loop->temps_at, false);
        lower_run_defers_to(l, l->loop->defers_at, false);
        if (l->b != NULL) {
            ir_jump(l->f, l->b, l->loop->break_to);
        }
        l->b = NULL;
        return;
    case STMT_CONTINUE:
        lower_end_temps(l, l->loop->temps_at, false);
        lower_run_defers_to(l, l->loop->defers_at, false);
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
            lower_run_defers_to(l, NULL, false);
            l->moved = NULL;
            if (l->b != NULL) {
                ir_ret(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0));
            }
            l->b = NULL;
            return;
        }
        if (s->as.return_value == NULL) {
            lower_run_defers_to(l, NULL, false);
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
            if (lower_has_defers(l) &&
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
            lower_run_defers_to(l, NULL, s->error_exit);
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
        if (action->leave) {
            if (failing) {
                lower_hook_failed(l, l->failing_error);
            }
            lower_hook_call(l, HOOK_LEAVE);
        } else if (action->unlock) {
            struct ir_operand locks[2];
            locks[0] = lower_temp(l, action->mutex);
            locks[1] = action->pair ? lower_temp(l, action->second)
                                    : lower_none();
            lower_rt_call(l, action->unlock_fn, locks);
        } else if (action->error) {
            if (action->local == NULL || action->local != l->moved) {
                lower_object_call(l, RT_FN_DELETE,
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
void lower_run_defers_to(struct lowerer *l, const struct defers *stop,
                         bool failing)
{
    const struct defers *scope;

    /* An exit of the function ends every value a statement still keeps,
       before the locals it was made after. */
    if (stop == NULL) {
        lower_end_temps(l, 0, false);
    }
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
        sym = s->as.for_loop.element != NULL ? s->as.for_loop.element
              : s->as.for_loop.name_count > 0
                  ? s->as.for_loop
                        .names[s->as.for_loop.name_count - 1].symbol
                  : NULL;
        if (sym != NULL &&
            (sym->address_taken || lower_is_aggregate(sym->type))) {
            sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        }
        /* The index of `for i, x in items` has a place of its own when a
           closure captures it. So has each name of a pattern, and one
           that is an aggregate always. */
        for (j = 0; j < s->as.for_loop.name_count; j++) {
            struct symbol *name = s->as.for_loop.names[j].symbol;
            bool read = s->as.for_loop.element != NULL;
            if (name == NULL || name == sym ||
                (j + 1 == s->as.for_loop.name_count && !read)) {
                continue;
            }
            if (name->address_taken ||
                (read && lower_is_aggregate(name->type))) {
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
