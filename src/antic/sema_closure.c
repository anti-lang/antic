/* The anonymous functions and closures: the type of an anonymous
   function from its target, the check of its body as a function of its
   own, what it captures and how, the writes and calls through a capture,
   a snapshot, and a closure that would outlive what it captures. */

#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* The named function whose body is checked, around every anonymous
   function that stands in it. NULL outside a function. */
const struct item *sema_named_function(const struct checker *c)
{
    const struct item *it = c->ctx.function;

    while (it != NULL && it->enclosing != NULL) {
        it = it->enclosing;
    }
    return it;
}

/* The capture of sym in the anonymous function it, added when it is not
   there yet.
   DESIGN: the list lives in the arena, as the item does. A copy of a
   generic and an item read from a library file hold a list of the arena
   as well, so realloc can grow none of them, and a list of the heap had
   no owner to free it. A grown list leaves the old one in the arena. */
static struct capture *capture_in(struct checker *c, struct item *it,
                                  struct symbol *sym)
{
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        if (it->captures[i].symbol == sym) {
            return &it->captures[i];
        }
    }
    if (it->capture_count == it->capture_capacity) {
        size_t capacity = it->capture_capacity == 0 ? 4
                                                    : it->capture_capacity * 2;
        struct capture *grown =
            types_alloc_array(c->arena, capacity, sizeof *grown);

        if (it->capture_count > 0) {
            memcpy(grown, it->captures,
                   it->capture_count * sizeof *grown);
        }
        it->captures = grown;
        it->capture_capacity = capacity;
    }
    memset(&it->captures[it->capture_count], 0, sizeof *it->captures);
    it->captures[it->capture_count].symbol = sym;
    return &it->captures[it->capture_count++];
}

/* DESIGN: a variable an anonymous function names from another frame is
   captured by reference. The variable then lives in memory. Every
   anonymous function between the one that names it and its frame
   captures it as well. A closure made inside a closure then finds the
   address in the context of the one around it. */
void sema_capture(struct checker *c, struct symbol *sym)
{
    struct item *it;

    sym->address_taken = true;
    sym->captured = true;
    for (it = c->ctx.function; it != NULL && it != sym->frame;
         it = it->enclosing) {
        capture_in(c, it, sym);
    }
}

/* The variable at the root of the place e, when a closure captures it,
   else NULL. A place through a pointer or a slice changes what they
   point to and not the variable. */
static struct symbol *captured_root(const struct checker *c,
                                    const struct expr *e)
{
    while (e->kind == EXPR_INDEX || e->kind == EXPR_FIELD) {
        const struct expr *base = e->kind == EXPR_INDEX ? e->as.index.base
                                                        : e->as.field.base;
        if (base->type == NULL || base->type->kind == TYPE_POINTER ||
            base->type->kind == TYPE_SLICE) {
            return NULL;
        }
        e = base;
    }
    if (e->kind != EXPR_NAME || e->symbol == NULL ||
        (e->symbol->kind != SYMBOL_LOCAL &&
         e->symbol->kind != SYMBOL_PARAM) ||
        e->symbol->frame == NULL || e->symbol->frame == c->ctx.function) {
        return NULL;
    }
    return e->symbol;
}

/* Record that the closures around the place e change the variable at its
   root, with the position of the first change. */
void sema_note_write(struct checker *c, const struct expr *e)
{
    struct symbol *sym = captured_root(c, e);
    struct item *it;

    sema_note_field_write(c, e);
    for (it = c->ctx.function; sym != NULL && it != NULL && it != sym->frame;
         it = it->enclosing) {
        struct capture *cap = capture_in(c, it, sym);
        if (!cap->written) {
            cap->written = true;
            cap->write = e->pos;
        }
    }
}

/* Record a call through a captured function of the form that is not
   `concurrent`, which a closure at a `concurrent` parameter may not
   make. */
void sema_note_call(struct checker *c, const struct expr *callee)
{
    struct symbol *sym = captured_root(c, callee);
    struct item *it;

    if (sym == NULL || callee->kind != EXPR_NAME || sym->type == NULL ||
        sym->type->kind != TYPE_FN || !sym->type->context ||
        sym->type->concurrent) {
        return;
    }
    for (it = c->ctx.function; it != NULL && it != sym->frame;
         it = it->enclosing) {
        struct capture *cap = capture_in(c, it, sym);
        if (!cap->called) {
            cap->called = true;
            cap->call = callee->pos;
        }
    }
}

/* DESIGN: a worker may take a function value as a parameter, and a
   closure only through a `concurrent` one. A parameter without the mark
   therefore takes a function that captures nothing. */
void sema_refuse_worker_closure(struct checker *c, const struct expr *arg,
                                const struct type *param)
{
    if (param->kind == TYPE_FN && param->context && !param->concurrent &&
        arg->type != NULL && arg->type->kind == TYPE_FN &&
        arg->type->context) {
        sema_error_at(c, arg->pos, "a worker takes a closure through a "
                      "`concurrent` parameter alone");
    }
}

/* The captured variable declared deepest of the closure that the value e
   gives, or NULL when e gives none or one that captures nothing. */
const struct symbol *sema_held_deepest(const struct expr *e)
{
    const struct symbol *deepest = NULL;
    size_t i;

    if (e->kind == EXPR_NAME && e->symbol != NULL) {
        return e->symbol->holds;
    }
    /* A snapshot holds copies and depends on no variable. */
    if (e->kind != EXPR_FN || e->as.fn->snapshot) {
        return NULL;
    }
    for (i = 0; i < e->as.fn->capture_count; i++) {
        const struct symbol *sym = e->as.fn->captures[i].symbol;
        if (deepest == NULL || sym->depth > deepest->depth) {
            deepest = sym;
        }
    }
    return deepest;
}

/* DESIGN: a closure never outlives the variables it captures. A local of
   its frame may hold one. A local declared in a block outside that of a
   captured variable would hold it after the variable is gone. The
   assignment is refused with the name of the variable. */
void sema_check_closure_lifetime(struct checker *c,
                                 const struct expr *target,
                                 const struct expr *value)
{
    const struct symbol *deepest = sema_held_deepest(value);
    struct symbol *sym = target->kind == EXPR_NAME ? target->symbol : NULL;

    if (sym == NULL || deepest == NULL || sym->type == NULL ||
        sym->type->kind != TYPE_FN || !sym->type->context) {
        return;
    }
    if (deepest->depth > sym->depth) {
        sema_error_at(c, value->pos, "`%.*s` outlives `%.*s`, which the "
                      "closure captures, so a `snapshot fn` takes its value "
                      "instead", (int)sym->name.length,
                      sym->name.text, (int)deepest->name.length,
                      deepest->name.text);
        return;
    }
    if (sym->holds == NULL || deepest->depth > sym->holds->depth) {
        sym->holds = deepest;
    }
    if (value->kind == EXPR_FN) {
        sym->closure = value->as.fn;
    }
}

/* The checker's state of one function body, which an anonymous function
   sets aside while its own body is checked. */
struct body_state {
    int loop_depth;
    struct stmt *fallthrough;
    struct handler_state handler;
    bool saw_fail;
    const struct expr *top_call;
    int deferring;
    const struct stmt *deferred;
    int deferred_loops;
    const struct expr *field_base;
    const struct held_mutex *held_outside;
    const struct stmt *loops_open[LOOPS_OPEN_MAX];
};

static void set_aside(struct checker *c, struct body_state *s)
{
    s->loop_depth = c->loop_depth;
    s->fallthrough = c->fallthrough;
    s->handler = c->handler;
    s->saw_fail = c->saw_fail;
    s->top_call = c->top_call;
    s->deferring = c->deferring;
    s->deferred = c->deferred;
    s->deferred_loops = c->deferred_loops;
    s->field_base = c->field_base;
    s->held_outside = c->held_outside;
    memcpy(s->loops_open, c->loops_open, sizeof s->loops_open);
    c->loop_depth = 0;
    c->fallthrough = NULL;
    memset(&c->handler, 0, sizeof c->handler);
    c->deferring = 0;
    c->deferred = NULL;
    c->deferred_loops = 0;
    c->field_base = NULL;
    /* DESIGN: the `sync` blocks around an anonymous function stay on the
       chain, and held_outside marks where they begin. A guarded field
       reads the blocks of its own function alone, since the anonymous
       function may run after the block around it. A `sync` reads the
       whole chain, by R093 of "Rules of 2026-10-13": a thread that runs
       the function inside the block takes the Mutex it holds. */
    c->held_outside = c->held;
}

static void take_back(struct checker *c, const struct body_state *s)
{
    c->loop_depth = s->loop_depth;
    c->fallthrough = s->fallthrough;
    c->handler = s->handler;
    c->saw_fail = s->saw_fail;
    c->top_call = s->top_call;
    c->deferring = s->deferring;
    c->deferred = s->deferred;
    c->deferred_loops = s->deferred_loops;
    c->field_base = s->field_base;
    c->held_outside = s->held_outside;
    memcpy(c->loops_open, s->loops_open, sizeof c->loops_open);
}

/* The type of the anonymous function it, from the types it writes and
   the target where it leaves one out. NULL after an error. */
static struct type *anonymous_type(struct checker *c, struct item *it,
                                   const struct type *target)
{
    size_t shown = target != NULL
                       ? target->param_count - (target->has_out ? 1 : 0)
                       : 0;
    struct type **params = types_alloc_array(
        c->arena, it->param_count + 2, sizeof *params);
    struct type *result = sema_builtin(c, TYPE_VOID);
    size_t i;

    if (target != NULL && shown != it->param_count) {
        sema_error_at(c, it->pos, "the anonymous function takes %zu "
                      "parameter%s, and `%s` gives %zu", it->param_count,
                      it->param_count == 1 ? "" : "s", sema_tn(target), shown);
        return NULL;
    }
    for (i = 0; i < it->param_count; i++) {
        struct param *p = &it->params[i];
        if (p->type != NULL) {
            params[i] = sema_param_form(c, sema_resolve_type(c, p->type),
                                        p->keep, p->concurrent,
                                        p->owned && p->type->kind == TYPEX_FN,
                                        p->pos);
        } else if (target != NULL && !p->keep && !p->concurrent &&
                   !p->owned) {
            params[i] = target->params[i];
        } else {
            sema_error_at(c, p->pos, "`%.*s` needs a type, which an "
                          "anonymous function takes from a parameter of "
                          "function type", (int)p->name.length, p->name.text);
            return NULL;
        }
        params[i] = sema_lent_form(c, params[i], p->lent, p->owned, p->pos);
        if (sema_is_error(params[i])) {
            return NULL;
        }
    }
    if (it->result != NULL) {
        result = sema_resolve_type(c, it->result);
    } else if (target != NULL) {
        result = target->has_out ? target->params[shown]->element
                 : target->may_fail ? sema_builtin(c, TYPE_VOID)
                                    : target->result;
    }
    if (sema_is_error(result)) {
        return NULL;
    }
    if (target != NULL && target->may_fail && !it->may_fail) {
        it->may_fail = true;
    }
    if (target != NULL && target->never && it->result == NULL) {
        it->never = true;
    }
    if (it->never && it->may_fail) {
        sema_error_at(c, it->pos,
                      "a function that returns `never` cannot fail");
        return NULL;
    }
    if (it->never) {
        return types_fn_never(c->types, types_fn(c->types, params,
                                                 it->param_count, result));
    }
    if (it->may_fail) {
        struct type *error = sema_error_class(c, it->pos);
        bool out = result->kind != TYPE_VOID;
        if (error == NULL) {
            return NULL;
        }
        if (out) {
            params[it->param_count] = types_pointer(c->types, result);
        }
        return types_fn_failing(c->types, params,
                                it->param_count + (out ? 1 : 0),
                                types_pointer_nullable(c->types, error), out);
    }
    return types_fn(c->types, params, it->param_count, result);
}

/* Whether a value of t copies fully into a snapshot. A number, `bool`,
   `char`, an enum and a struct of those do. So does a `str` at the top,
   whose bytes the snapshot copies. */
static bool snapshot_copies(const struct type *t, bool top)
{
    size_t i;

    /* The kinds from TYPE_BOOL to TYPE_F64 are the scalars, bool, char,
       the integers and the floats, in one range of enum type_kind. */
    if (t->kind >= TYPE_BOOL && t->kind <= TYPE_F64) {
        return true;
    }
    if (t->kind == TYPE_ENUM) {
        return true;
    }
    if (t->kind == TYPE_STR) {
        return top;
    }
    if (t->kind != TYPE_STRUCT) {
        return false;
    }
    for (i = 0; i < t->field_count; i++) {
        if (!snapshot_copies(t->fields[i].type, false)) {
            return false;
        }
    }
    return true;
}

/* DESIGN: a snapshot is read-only, so two threads that call it never
   write it at once. It holds values that copy fully: an address would
   copy the address and not what it points at. The first write of a
   captured value and each capture of another type are refused. An
   atomic local is shared state, and a copy of it is not, so a snapshot
   names none, by R090 of "Rules of 2026-10-13". The name of an `if let`
   that its block never assigns is read as the value it binds, in the
   snapshot as everywhere in the block, so that value is what it holds. */
static void check_snapshot(struct checker *c, const struct expr *e)
{
    const struct item *it = e->as.fn;
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        const struct capture *cap = &it->captures[i];
        const struct symbol *sym = cap->symbol;
        const struct type *held = sym->type;
        if (held == NULL || sema_is_error(held)) {
            continue;
        }
        if (sym->steady && types_is_nullable(held)) {
            const struct type *proved = sema_narrowed_type(c, sym);
            if (proved != NULL) {
                held = proved;
            }
        }
        if (sym->atomic) {
            sema_error_at(c, e->pos, "a snapshot holds copies, and the "
                          "atomic `%.*s` is shared state",
                          (int)sym->name.length, sym->name.text);
        } else if (!snapshot_copies(held, true)) {
            sema_error_at(c, e->pos, "a snapshot holds values that copy "
                          "fully, and `%.*s` is `%s`",
                          (int)sym->name.length, sym->name.text,
                          sema_tn(held));
        } else if (cap->written) {
            sema_error_at(c, cap->write, "`%.*s` is changed in a snapshot, "
                          "which is read-only", (int)sym->name.length,
                          sym->name.text);
        }
    }
}

/* DESIGN: an anonymous function is checked as a function of its own,
   whose scope sits inside the scope where it stands. A name of the
   function around it is then found. Naming a local or a parameter of
   another frame captures it. The types come from the target, a parameter
   of function type, and never from the body. The value is an ordinary
   function when nothing is captured. A closure takes the form of two
   words. It is `concurrent` when it changes no captured variable whose
   type is not thread-safe and calls no captured function that is not. */
struct type *sema_check_anonymous(struct checker *c, struct expr *e,
                                  struct type *expected)
{
    struct item *it = e->as.fn;
    const struct type *target = NULL;
    struct body_state state;
    struct context at;
    struct context saved;
    struct scope params;
    struct symbol *sym;
    struct type *fn;
    size_t i;
    bool safe = true;

    if (c->ctx.function == NULL) {
        sema_error_at(c, e->pos, "an anonymous function stands in the body "
                      "of a function");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (expected != NULL && expected->kind == TYPE_FN && !expected->bound) {
        target = expected;
    }
    it->enclosing = c->ctx.function;
    it->capture_count = 0;
    fn = anonymous_type(c, it, target);
    if (fn == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    sym = arena_alloc(c->arena, sizeof *sym);
    sym->kind = SYMBOL_FN;
    sym->name = it->name;
    sym->pos = it->pos;
    sym->type = fn;
    sym->item = it;
    sym->may_fail = it->may_fail;
    it->symbol = sym;
    set_aside(c, &state);
    at = c->ctx;
    at.function = it;
    at.in_try = NULL;
    sema_enter(c, &at, &saved);
    c->saw_fail = false;
    c->top_call = NULL;
    sema_enter_scope(c, &params);
    for (i = 0; i < it->param_count; i++) {
        struct symbol *p =
            sema_declare(c, SYMBOL_PARAM, &it->params[i].name,
                         it->params[i].pos,
                         "`%.*s` is already declared in this block");
        if (p != NULL) {
            p->type = fn->params[i];
            it->params[i].symbol = p;
        }
    }
    sema_check_block(c, it->body);
    sema_leave_scope(c, &params);
    if (fn->never && !sema_block_returns(it->body)) {
        sema_error_at(c, it->pos, "the anonymous function returns `never` "
                      "and can reach its end");
    }
    if (sema_declared_result(c)->kind != TYPE_VOID &&
        !sema_block_returns(it->body)) {
        sema_error_at(c, it->pos, "the anonymous function can reach its end "
                      "without `return`");
    }
    if (it->may_fail && !c->saw_fail && it->may_fail_pos.line != 0) {
        diagnostics_warn(c->diags, NAME_NEVER_FAILS, it->may_fail_pos.line,
                         it->may_fail_pos.column,
                         "the anonymous function may fail and never does");
    }
    sema_leave(c, &saved);
    take_back(c, &state);
    if (it->snapshot) {
        check_snapshot(c, e);
    }
    if (it->capture_count == 0) {
        return fn;
    }
    /* A snapshot writes nothing it holds, so every thread may call it. */
    if (it->snapshot) {
        return types_fn_form(c->types, fn, true, true);
    }
    for (i = 0; i < it->capture_count; i++) {
        const struct capture *cap = &it->captures[i];
        if ((cap->written && !sema_thread_safe_symbol(cap->symbol)) ||
            cap->called) {
            safe = false;
        }
    }
    return types_fn_form(c->types, fn, true, safe);
}
