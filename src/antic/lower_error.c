/* The error channel in lowering. A call whose error a handler takes, the
   handler and `catch fatal`, the guard of a `?*T`, `fail` with the origin
   of its error, and a handled call as an operand. */

#include <stdlib.h>
#include <string.h>

#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: a failing call that is an operand, as in `return try f();` or
   `g(try f())`, or a call inside a `try` block that is one, has no place
   of a `let` or an assignment to write into. It writes a slot of the
   frame, whose tables are zeroed first so that the `=` the callee runs
   destroys nothing, and the value is read from there. An aggregate is
   its slot, as the value of any other call is its memory. */
struct ir_operand lower_handled_operand(struct lowerer *l,
                                        const struct expr *e)
{
    bool has_out = e->as.call.out != NULL;
    struct ir_operand out = lower_none();
    struct ir_operand err;

    if (has_out) {
        out = lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, e->type)));
        if (sema_needs_teardown(e->type)) {
            lower_clear_owned(l, e->type, out);
        }
    }
    l->out_address = out;
    err = lower_call(l, e);
    lower_handle_error(l, e, err, out, has_out, lower_none());
    if (!has_out || lower_is_aggregate(e->type)) {
        return out;
    }
    return lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(e->type), out));
}

/* DESIGN: the error a handler binds is the exit action of a scope around
   the handler, so every exit of the handler deletes it: `yield`, the
   closing brace, `break`, `continue`, `return` and `fail`. `return e` and
   `fail e` hand it to the caller, which skips it as `return` skips the
   local it hands on. A handler with a result to give takes handling,
   which `yield` reads. */
void lower_handler(struct lowerer *l, const struct handler *h,
                   uint32_t error, const struct type *error_type,
                   struct handling *handling)
{
    struct defers scope;

    memset(&scope, 0, sizeof scope);
    scope.outer = l->defers;
    l->defers = &scope;
    lower_push_error_action(l, h->symbol, error, error_type);
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
            lower_run_defers_to(l, l->try_scope->defers_at, true);
            if (l->b != NULL) {
                ir_jump(l->f, l->b, l->try_scope->handler);
            }
        } else {
            ir_jump(l->f, l->b, join);
        }
        break;
    case HANDLE_TRY:
        l->failing_error = err;
        lower_run_defers_to(l, NULL, true);
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

/* Whether the expression is a call whose error a handler takes. */
bool lower_is_handled_call(const struct expr *e)
{
    return e->kind == EXPR_CALL && e->as.call.builds == NULL &&
           e->as.call.handler.kind != HANDLE_NONE;
}

/* `let m = p catch fatal` and `let m = p catch e { }`. The handler runs
   when p is `none`, with an `anti.lang.NoneDereference` in hand, and it
   leaves the block or gives the binding a pointer with `yield`. */
void lower_pointer_guard(struct lowerer *l, const struct stmt *s)
{
    const struct symbol *sym = s->as.let.symbol;
    struct ir_block *bad = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);
    struct ir_operand place;

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
    lower_guard_missing(l, s, join);
}

/* The error forms of a `let` guard on the path where the value is `none`,
   the current block, which ends at join. */
void lower_guard_missing(struct lowerer *l, const struct stmt *s,
                         struct ir_block *join)
{
    const struct handler *h = &s->as.let.guard;
    const struct symbol *sym = s->as.let.symbol;
    const struct type *error_type = s->as.let.guard_make->type->result;
    struct ir_operand place = lower_temp(l, sym->ir);
    struct ir_operand err;
    struct handling scope;
    uint32_t error;

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
    const struct type *location = type_find_field(error, &at_name)->type;
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
void lower_fail(struct lowerer *l, const struct stmt *s)
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
    if (lower_has_defers(l)) {
        err = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, IR_PTR, err));
    }
    /* `fail e` hands the error a handler binds to the caller. */
    if (s->as.fail.value->kind == EXPR_NAME) {
        l->moved = s->as.fail.value->symbol;
    }
    l->failing_error = err;
    lower_run_defers_to(l, NULL, true);
    l->failing_error = lower_none();
    l->moved = NULL;
    if (l->b != NULL) {
        ir_ret(l->f, l->b, IR_PTR, err);
    }
    l->b = NULL;
}
