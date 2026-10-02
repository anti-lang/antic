/* The checks of the calls that threads share data through: the atomic
   operations, the Mutex, channels and `sync`, and the workers that
   `parallel` and `dispatch` run and `join` waits for. */

#include <string.h>

#include "sema_checker.h"

/* Atomic operations */

/* DESIGN: an atomic field is read and written by calls alone, so that
   every access is one sequentially consistent operation. The checker
   rewrites `p.add(1)` on an atomic place into one node, which lowering
   turns into a call of the runtime. */
static const struct {
    const char *name;
    enum atomic_op op;
    size_t args;
    bool gives_value;
    bool gives_bool;
} atomic_ops[] = {
    {"load", ATOMIC_LOAD, 0, true, false},
    {"store", ATOMIC_STORE, 1, false, false},
    {"swap", ATOMIC_SWAP, 1, true, false},
    {"add", ATOMIC_ADD, 1, true, false},
    {"sub", ATOMIC_SUB, 1, true, false},
    {"and", ATOMIC_AND, 1, true, false},
    {"or", ATOMIC_OR, 1, true, false},
    {"compare_swap", ATOMIC_CAS, 2, false, true}
};

/* The type of the atomic field or local that e denotes, or NULL. */
static struct type *atomic_place(const struct expr *e)
{
    const struct struct_field *f;
    struct type *s;

    if (e->kind == EXPR_NAME) {
        return e->symbol != NULL && e->symbol->atomic ? e->symbol->type : NULL;
    }
    if (e->kind != EXPR_FIELD) {
        return NULL;
    }
    if (e->symbol != NULL && e->symbol->item != NULL &&
        e->symbol->item->is_static && e->symbol->item->atomic) {
        return e->symbol->type;
    }
    s = e->as.field.base->type != NULL ? sema_struct_of(e->as.field.base->type)
                                       : NULL;
    if (s == NULL) {
        return NULL;
    }
    f = types_find_field(s, &e->as.field.name);
    return f != NULL && f->atomic ? f->type : NULL;
}

/* Whether t is one word an atomic operation of the runtime moves: an
   integer, a `bool` or a pointer. */
static bool swaps_as_word(const struct type *t)
{
    return types_is_integer(t) || t->kind == TYPE_BOOL ||
           t->kind == TYPE_POINTER;
}

/* DESIGN: `compare_swap` also takes a plain field of one word that
   `unchecked(unguarded-field)` marks in a concurrent class, after the
   field's type or in the class header. That is the lock-free structure
   of "Concurrent classes", whose fields the checker cannot follow. The
   call is the node of an atomic field, and it counts as a write, so the
   clause covers the field it marks. A field of a class that declares
   its own `compare_swap`, or of a pointer to one, keeps the call of that
   function. Any other field is refused, and the message names both
   fixes. */
static bool unchecked_swap(struct checker *c, struct expr *e,
                           struct type **out)
{
    struct expr *callee = e->as.call.callee;
    struct expr *place = callee->as.field.base;
    const struct type *s;
    const struct struct_field *f = NULL;
    const struct type *home;
    size_t i;

    if (place->kind != EXPR_FIELD || place->type == NULL) {
        return false;
    }
    s = sema_struct_of(place->type);
    if (s != NULL && sema_method_symbol(c, s, &callee->as.field.name) != NULL) {
        return false;
    }
    s = sema_struct_of(place->as.field.base->type);
    for (; s != NULL && f == NULL; s = s->kind == TYPE_CLASS ? s->base : NULL) {
        f = types_find_field(s, &place->as.field.name);
    }
    if (f == NULL || f->form != FIELD_PLAIN) {
        return false;
    }
    home = f->home;
    if (home == NULL || home->safety != SAFETY_CONCURRENT ||
        !(f->unchecked || home->unchecked_fields)) {
        sema_error_at(c, callee->pos, "`compare_swap` takes an atomic field, "
                      "and `%.*s` is a plain one: mark it `atomic`, or "
                      "`unchecked(unguarded-field, \"reason\")` in a "
                      "concurrent class",
                      (int)f->name.length, f->name.text);
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    if (f->bits != 0 || !swaps_as_word(f->type)) {
        sema_error_at(c, callee->pos, "`compare_swap` on a field that "
                      "`unchecked` marks takes one word, an integer, a "
                      "`bool` or a pointer, found `%s`", sema_tn(f->type));
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    if (e->as.call.arg_count != 2) {
        sema_error_at(c, e->pos, "`compare_swap` takes 2 arguments");
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    sema_note_field_write(c, place);
    e->as.atomic.a = e->as.call.args[0];
    e->as.atomic.b = e->as.call.args[1];
    e->as.atomic.op = ATOMIC_CAS;
    e->as.atomic.place = place;
    e->kind = EXPR_ATOMIC;
    for (i = 0; i < 2; i++) {
        struct expr *arg = i == 0 ? e->as.atomic.a : e->as.atomic.b;
        if (!sema_require(c, arg, sema_check_expr(c, arg, f->type),
                          f->type)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
    }
    *out = sema_builtin(c, TYPE_BOOL);
    return true;
}

/* Rewrite a call on an atomic place. Returns false when the callee is no
   such call, and reports nothing then. */
bool sema_atomic_call(struct checker *c, struct expr *e, struct type **out)
{
    struct expr *callee = e->as.call.callee;
    struct expr *place;
    struct type *t;
    struct context quiet;
    size_t i;

    bool was;

    if (callee->kind != EXPR_FIELD) {
        return false;
    }
    place = callee->as.field.base;
    if (place->kind == EXPR_NAME) {
        const struct symbol *sym = sema_lookup(c, &place->as.name);
        if (sym == NULL || !sym->atomic) {
            return false;
        }
    } else if (place->kind != EXPR_FIELD) {
        return false;
    }
    /* DESIGN: the base is checked quietly, because this is a probe. A
       call on anything else reaches the branches below, which report
       what is wrong with it. A probe inside another keeps its flag. */
    was = c->atomic_place;
    c->atomic_place = true;
    sema_enter_quiet(c, &quiet);
    /* The place is storage, so an f16 there stays an f16. */
    t = sema_check_storage(c, place);
    sema_leave(c, &quiet);
    c->atomic_place = was;
    if (t == NULL || sema_is_error(t)) {
        return false;
    }
    t = atomic_place(place);
    if (t == NULL) {
        return sema_name_is(&callee->as.field.name, "compare_swap") &&
               unchecked_swap(c, e, out);
    }
    for (i = 0; i < sizeof atomic_ops / sizeof atomic_ops[0]; i++) {
        if (!sema_name_is(&callee->as.field.name, atomic_ops[i].name)) {
            continue;
        }
        if (e->as.call.arg_count != atomic_ops[i].args) {
            sema_error_at(c, e->pos, "`%s` takes %zu argument%s",
                          atomic_ops[i].name,
                          atomic_ops[i].args,
                          atomic_ops[i].args == 1 ? "" : "s");
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        /* An f16 has its bits exchanged and compared, and no arithmetic. */
        if ((atomic_ops[i].op == ATOMIC_ADD || atomic_ops[i].op == ATOMIC_SUB ||
             atomic_ops[i].op == ATOMIC_AND || atomic_ops[i].op == ATOMIC_OR) &&
            sema_refuses_half(c, e->pos, t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        e->as.atomic.a = atomic_ops[i].args > 0 ? e->as.call.args[0] : NULL;
        e->as.atomic.b = atomic_ops[i].args > 1 ? e->as.call.args[1] : NULL;
        e->as.atomic.op = atomic_ops[i].op;
        e->as.atomic.place = place;
        e->kind = EXPR_ATOMIC;
        if (e->as.atomic.a != NULL &&
            !sema_require(c, e->as.atomic.a,
                          sema_check_expr(c, e->as.atomic.a, t), t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        if (e->as.atomic.b != NULL &&
            !sema_require(c, e->as.atomic.b,
                          sema_check_expr(c, e->as.atomic.b, t), t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        *out = atomic_ops[i].gives_bool  ? sema_builtin(c, TYPE_BOOL)
               : atomic_ops[i].gives_value ? t
                                           : sema_builtin(c, TYPE_VOID);
        return true;
    }
    sema_error_at(c, callee->pos, "an atomic %s has no `%.*s`",
                  place->kind == EXPR_NAME ? "local" : "field",
                  (int)callee->as.field.name.length,
                  callee->as.field.name.text);
    *out = sema_builtin(c, TYPE_ERROR);
    return true;
}

/* Locking and channels */

/* The channel that the operation what reads. A pointer to one is
   written `*p`, as it is for the value a `switch` takes. */
static struct type *channel_of(struct checker *c, struct expr *e,
                               const char *what)
{
    struct type *t = sema_check_expr(c, e, NULL);

    if (!sema_is_error(t) && !types_is_chan(t)) {
        sema_error_at(c, e->pos, "`%s` takes a channel, found `%s`", what,
                      sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    return t;
}

/* Rewrite the call e into the operation op on target. None of the calls
   can fail, so a handler on one is refused. */
static bool sync_call(struct checker *c, struct expr *e, enum sync_op op,
                      struct expr *target)
{
    if (e->as.call.handler.kind != HANDLE_NONE) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, so it has no error to handle");
        return false;
    }
    e->kind = EXPR_SYNC_OP;
    memset(&e->as, 0, sizeof e->as);
    e->as.sync_op.op = op;
    e->as.sync_op.target = target;
    return true;
}

/* `close(c)` ends what a channel takes. What it holds is still
   received. */
struct type *sema_check_close(struct checker *c, struct expr *e)
{
    struct type *t;

    if (e->as.call.arg_count != 1) {
        sema_error_at(c, e->pos, "`" CHAN_CLOSE "` takes 1 argument, found %zu",
                      e->as.call.arg_count);
        return sema_builtin(c, TYPE_ERROR);
    }
    t = channel_of(c, e->as.call.args[0], CHAN_CLOSE);
    if (sema_is_error(t) || !sync_call(c, e, SYNC_CLOSE, e->as.call.args[0])) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return sema_builtin(c, TYPE_VOID);
}

/* `Mutex.new()` makes a mutex, which the runtime holds. */
struct type *sema_check_mutex_new(struct checker *c, struct expr *e)
{
    const struct name *name = &e->as.call.callee->as.field.name;

    if (!sema_name_is(name, MUTEX_NEW)) {
        sema_error_at(c, e->as.call.callee->pos, "`" LANG_MUTEX "` has no "
                      "function `%.*s`", (int)name->length, name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.call.arg_count != 0) {
        sema_error_at(c, e->pos, "`" LANG_MUTEX "." MUTEX_NEW "` takes no "
                      "arguments");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!sync_call(c, e, SYNC_MUTEX_NEW, NULL)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return types_mutex(c->types);
}

/* `m.destroy()` releases the mutex that m names, a Mutex in a place or
   a pointer to one. */
struct type *sema_check_mutex_destroy(struct checker *c, struct expr *e,
                                      struct type *base)
{
    struct expr *m = e->as.call.callee->as.field.base;

    if (e->as.call.arg_count != 0) {
        sema_error_at(c, e->pos, "`" MUTEX_DESTROY "` takes no arguments");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (base->kind == TYPE_POINTER) {
        sema_usable_pointer(c, m, base);
    } else if (!sema_is_place(m)) {
        sema_error_at(c, m->pos, "calling `" MUTEX_DESTROY "` needs a place");
        return sema_builtin(c, TYPE_ERROR);
    } else {
        sema_mark_address_taken(c, m);
    }
    if (!sync_call(c, e, SYNC_MUTEX_DESTROY, m)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return sema_builtin(c, TYPE_VOID);
}

/* `chan T(n)`, `send(c, v)` and `recv(c)`, which the parser writes. The
   nodes the checker writes from calls carry their type already. */
struct type *sema_check_sync_op(struct checker *c, struct expr *e)
{
    struct type *i64 = sema_builtin(c, TYPE_I64);
    struct type *t;

    switch (e->as.sync_op.op) {
    case SYNC_CHAN_NEW:
        t = sema_chan_element(c, e->as.sync_op.element);
        if (!sema_require(c, e->as.sync_op.value,
                          sema_check_expr(c, e->as.sync_op.value, i64), i64) ||
            sema_is_error(t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_chan(c->types, t);
    case SYNC_SEND:
        t = channel_of(c, e->as.sync_op.target, "send");
        if (sema_is_error(t)) {
            sema_check_expr(c, e->as.sync_op.value, NULL);
            return t;
        }
        if (!sema_require(c, e->as.sync_op.value,
                          sema_check_expr(c, e->as.sync_op.value, t->element),
                          t->element)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        /* The channel holds a copy of the value, which an `own` field
           would give two owners. */
        sema_refuse_owned_copy(c, e->as.sync_op.value, t->element);
        return sema_builtin(c, TYPE_VOID);
    case SYNC_RECV:
        t = channel_of(c, e->as.sync_op.target, "recv");
        return sema_is_error(t) ? t
                                : types_pointer_nullable(c->types, t->element);
    default:
        return e->type;
    }
}

/* Workers */

/* DESIGN: the safety of `parallel` rests on the types. The runtime does
   the splitting, so a worker reaches one contiguous slice of the array
   and nothing else. Its parameters and its result are pointer-free, so
   no worker can reach memory that another one writes. The rule is no
   data race, not memory safety. */
static bool worker_type(struct checker *c, struct pos pos, const char *what,
                        struct type *t)
{
    if (sema_is_error(t)) {
        return false;
    }
    if (!types_pointer_free(t)) {
        sema_error_at(c, pos,
                      "%s has type `%s`, which holds a pointer. A worker "
                      "takes and returns values alone", what, sema_tn(t));
        return false;
    }
    return true;
}

/* The function type of the worker that the call at slot of `parallel`
   or `dispatch` names. It has one parameter before the arguments of the
   call, for the chunk or the object that first names, of type given. A
   generic worker gives the signature of its copy. NULL after an error. */
static struct type *worker_callee(struct checker *c, struct expr **slot,
                                  const char *form, const char *first,
                                  struct type *given)
{
    struct expr *call = *slot;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    size_t arg_count = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    const struct expr *outer_callee = c->callee;
    struct symbol *sym;
    struct type *fn;

    if (callee->kind != EXPR_NAME) {
        sema_error_at(c, callee->pos, "`%s` runs a worker named here", form);
        return NULL;
    }
    sym = sema_lookup(c, &callee->as.name);
    callee->symbol = sym;
    c->callee = callee;
    fn = sema_check_expr(c, callee, NULL);
    c->callee = outer_callee;
    if (sema_is_error(fn)) {
        return NULL;
    }
    if (fn->kind != TYPE_FN || sym == NULL || !sym->worker) {
        sema_error_at(c, callee->pos, "`%.*s` is not a `worker fn`",
                      (int)callee->as.name.length, callee->as.name.text);
        return NULL;
    }
    if (sym->item == NULL || sym->item->type_param_count == 0) {
        if (callee->type_arg_count > 0) {
            sema_refuse_type_args(c, callee, &callee->as.name);
            return NULL;
        }
    } else if (fn->param_count == arg_count + 1) {
        /* The copy is recorded on a call, so a worker named without
           arguments becomes a call of none. */
        if (call->kind != EXPR_CALL) {
            call = sema_new_node(c, EXPR_CALL, callee->pos);
            call->as.call.callee = callee;
            *slot = call;
        }
        fn = sema_worker_copy(c, call, fn, sym, given);
        if (fn == NULL) {
            return NULL;
        }
        callee->type = fn;
    }
    if (fn->param_count == 0) {
        sema_error_at(c, call->pos, "`%.*s` has no parameter for the %s",
                      (int)callee->as.name.length, callee->as.name.text,
                      first);
        return NULL;
    }
    if (fn->param_count != arg_count + 1) {
        sema_error_at(c, call->pos,
                      "`%.*s` takes %zu argument%s beside the %s, "
                      "found %zu", (int)callee->as.name.length,
                      callee->as.name.text, fn->param_count - 1,
                      fn->param_count == 2 ? "" : "s", first, arg_count);
        return NULL;
    }
    return fn;
}

/* Check the result and the arguments of the call of the worker fn, each
   a value without a pointer, and give the call its type. */
static bool worker_args(struct checker *c, struct expr *call,
                        const struct type *fn)
{
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct expr **args = call->kind == EXPR_CALL ? call->as.call.args : NULL;
    size_t arg_count = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    bool ok = worker_type(c, callee->pos, "the result of a worker",
                          fn->result);
    size_t i;

    /* DESIGN: a worker may take a function value as a parameter. Every
       thread shares the code of a function. A closure reaches a worker
       only through a `concurrent` parameter. The checker has proved such
       a closure safe to call from more than one thread at once. */
    for (i = 0; i < arg_count; i++) {
        const struct type *param = fn->params[i + 1];
        ok = sema_require(c, args[i],
                          sema_check_expr(c, args[i], fn->params[i + 1]),
                          fn->params[i + 1]) && ok;
        sema_refuse_lock_copy(c, args[i], param);
        sema_refuse_worker_closure(c, args[i], param);
        /* DESIGN: a worker may take a pointer to a thread-safe object,
           the one exception to the rule that its parameters hold no
           pointer. A Mutex counts, since a worker cannot take a copy of
           one. */
        if (param->kind == TYPE_POINTER && sema_thread_safe(param->element) &&
            !types_is_chan(param->element)) {
            continue;
        }
        if (param->kind != TYPE_FN || param->bound) {
            ok = worker_type(c, args[i]->pos, "an argument of a worker",
                             fn->params[i + 1]) && ok;
        }
    }
    if (call->kind == EXPR_CALL) {
        call->type = fn->result;
    }
    return ok;
}

/* Check `parallel a by n -> f(x)`. It splits a into n chunks and runs f
   on each through the worker pool. The results come back in chunk
   order. */
struct type *sema_check_parallel(struct checker *c, struct expr *e)
{
    struct expr *call = e->as.parallel.call;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct type *array = sema_check_expr(c, e->as.parallel.array, NULL);
    struct type *fn;
    bool ok = true;

    if (e->as.parallel.chunks != NULL) {
        struct expr *n = e->as.parallel.chunks;
        ok = sema_require(c, n,
                          sema_check_expr(c, n, sema_builtin(c, TYPE_I64)),
                          sema_builtin(c, TYPE_I64));
    }
    if (sema_is_error(array)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (array->kind != TYPE_SLICE) {
        sema_error_at(c, e->as.parallel.array->pos,
                      "`parallel` splits a slice, and this is `%s`",
                      sema_tn(array));
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = worker_callee(c, &e->as.parallel.call, "parallel", "chunk", array);
    if (fn == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    call = e->as.parallel.call;
    if (fn->params[0]->kind != TYPE_SLICE ||
        fn->params[0]->element != array->element) {
        sema_error_at(c, callee->pos, "`%.*s` takes `%s` as its chunk, and the "
                      "slice is `%s`", (int)callee->as.name.length,
                      callee->as.name.text, sema_tn(fn->params[0]),
                      sema_tn(array));
        return sema_builtin(c, TYPE_ERROR);
    }
    ok = worker_type(c, callee->pos, "the chunk", array->element) && ok;
    ok = worker_args(c, call, fn) && ok;
    return ok ? types_slice(c->types, fn->result) : sema_builtin(c, TYPE_ERROR);
}

/* DESIGN: `dispatch obj -> f(args)` gives one object to the pool for a
   `worker fn` whose first parameter is a pointer to the object's class.
   The other arguments follow the pointer-free rule of `parallel`, and the
   expression gives a Job of the worker's result. */
struct type *sema_check_dispatch(struct checker *c, struct expr *e)
{
    struct expr *call = e->as.dispatch.call;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct type *object = sema_check_expr(c, e->as.dispatch.object, NULL);
    struct type *fn;

    if (sema_is_error(object)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (object->kind != TYPE_POINTER || object->element->kind != TYPE_CLASS) {
        sema_error_at(c, e->as.dispatch.object->pos,
                      "`dispatch` submits a class pointer, and this is `%s`",
                      sema_tn(object));
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = worker_callee(c, &e->as.dispatch.call, "dispatch", "object", object);
    if (fn == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    call = e->as.dispatch.call;
    if (fn->params[0] != object) {
        sema_error_at(c, callee->pos,
                      "`%.*s` takes `%s` as its object, and this is "
                      "`%s`", (int)callee->as.name.length, callee->as.name.text,
                      sema_tn(fn->params[0]), sema_tn(object));
        return sema_builtin(c, TYPE_ERROR);
    }
    return worker_args(c, call, fn) ? types_job(c->types, fn->result)
                                    : sema_builtin(c, TYPE_ERROR);
}

/* `join(job)` waits for one job and gives its result. `join_all(jobs)`
   waits for a slice of jobs and gives nothing. */
struct type *sema_check_join(struct checker *c, struct expr *e)
{
    struct type *t = sema_check_expr(c, e->as.join.job, NULL);
    const char *what = e->as.join.all ? "join_all" : "join";
    struct type *job = e->as.join.all && t->kind == TYPE_SLICE ? t->element : t;

    if (sema_is_error(t)) {
        return t;
    }
    if (e->as.join.all && t->kind != TYPE_SLICE) {
        sema_error_at(c, e->as.join.job->pos, "`join_all` waits for a slice of "
                      "jobs, and this is `%s`", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!types_is_job(job)) {
        sema_error_at(c, e->as.join.job->pos,
                      "`%s` waits for a job of `dispatch`, "
                      "and this is `%s`", what, sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    return e->as.join.all ? sema_builtin(c, TYPE_VOID) : job->result;
}
