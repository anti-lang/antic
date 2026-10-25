/* The safety check `use-after-free`, E2 of "Compile-time checks of
   2026-10-20" in docs/decisions.md: a value used on a path after
   `delete`, `destroy` or a move of it. The pass walks a function body
   once the checker has checked it, with the types, the symbols and the
   marks of the moves in place. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* DESIGN: the pass follows each path of the body in the order it runs.
   A local or a parameter of the function, and an `own` field of `self`,
   is alive until a path deletes, destroys or moves it, and alive again
   once a path gives it a new value. Where paths join, a value dead on
   one of them is dead after the join. A loop body is walked twice: the
   first walk, which reports nothing, finds what the end of a turn
   leaves dead, and the second starts from the join of the state before
   the loop and that one, so a use in the next turn is refused. A closure
   is a frame of its own, which may run at any time, and a `defer` or an
   `undo` runs at an exit. A value one of them names that is dead where
   it stands is refused, as the order of the text refused it before, and
   neither changes the values of the path it stands on. The check
   refuses only what it proves, and the run-time checks keep the rest. */

/* How a value died. */
enum death_kind {
    DIED_DELETED,
    DIED_DESTROYED,
    DIED_MOVED,
    DIED_STORED
};

struct death {
    bool dead;
    bool mixed;                     /* alive on another path into a join */
    bool turn_before;               /* dead since the end of the last turn */
    bool reported;                  /* a use of it was refused */
    enum death_kind kind;
    int line;
    struct name into;               /* the place a move went to */
    struct name by;                 /* the function that took it */
};

/* The values of one path. A value past count is alive. */
struct state {
    bool live;                      /* the path reaches the walk */
    struct death *deaths;
    size_t count;
};

/* A value the pass follows: a local or a parameter, or the `own` field
   field of `self`, whose symbol is then `self`. */
struct value {
    const struct symbol *symbol;
    const struct struct_field *field;
};

struct loop_flow {
    struct state breaks;
    struct state turns;             /* the ends of the turns, and `continue` */
    size_t base;                    /* the pending statements of the loop */
};

/* DESIGN: a `defer` or an `undo` runs where its block is left, so the
   walk reads it at each exit with the state of that exit, and not where
   it stands, as sema_uninit.c does. An exit runs the pending statements
   of every block it leaves, innermost first and the last declared first
   in each block, so a `defer` reads what a later one deleted. An error
   exit, `fail`, `try` and a call that hands its error to a `try` block,
   runs the `undo` statements of a block before its `defer` statements,
   as lowering does. The walk of an exit goes on from a copy of the
   state, which the path goes on without. */
struct pending {
    const struct stmt *s;           /* the statement the exit runs */
    bool undo;
    size_t block;                   /* the depth of its block */
};

struct exits {
    struct pending *items;
    size_t count;
    size_t capacity;
    size_t depth;                   /* the depth of the block walked now */
};

/* The values the walk follows, which the walk of a closure shares. The
   index of a value is its index in every state. */
struct values {
    struct value *items;
    size_t count;
    size_t capacity;
};

struct free_walk {
    struct checker *c;
    const struct item *frame;       /* the function whose values are followed */
    const struct free_walk *outer;  /* the walk a closure stands in */
    struct values *values;
    struct state now;
    bool quiet;                     /* the first walk of a loop body */
    bool deferred;                  /* a `defer` or an `undo` */
    struct loop_flow *loop;
    struct state *yields;           /* the handler a `yield` leaves */
    struct state *tried;            /* the handler of the `try` block */
    struct state *fall;             /* the arm a `fallthrough` enters */
    struct exits *exits;            /* NULL inside a deferred statement */
    size_t yield_base;              /* the pending statements of the handler */
    size_t try_base;                /* those of the body of the `try` block */
    size_t fall_base;               /* those of the arm */
};

static void walk_block(struct free_walk *w, const struct block *b);
static void walk_stmt(struct free_walk *w, const struct stmt *s);
static void walk_expr(struct free_walk *w, const struct expr *e);
static void run_exits(struct free_walk *w, size_t base, bool failing);
static size_t pending_count(const struct free_walk *w);

static struct state state_live(void)
{
    struct state s = {true, NULL, 0};
    return s;
}

static struct state state_none(void)
{
    struct state s = {false, NULL, 0};
    return s;
}

static struct state state_copy(const struct state *from)
{
    struct state s = {from->live, NULL, 0};

    if (from->count > 0) {
        s.deaths = alloc_resize(NULL, from->count, sizeof *s.deaths);
        memcpy(s.deaths, from->deaths, from->count * sizeof *s.deaths);
        s.count = from->count;
    }
    return s;
}

static void state_free(struct state *s)
{
    free(s->deaths);
    s->deaths = NULL;
    s->count = 0;
    s->live = false;
}

/* Replace into with from, which it takes over. */
static void state_take(struct state *into, struct state *from)
{
    state_free(into);
    *into = *from;
    *from = state_none();
}

static const struct death *death_of(const struct state *s, size_t index)
{
    return index < s->count && s->deaths[index].dead ? &s->deaths[index]
                                                     : NULL;
}

static struct death *slot(struct state *s, size_t index)
{
    if (index >= s->count) {
        s->deaths = alloc_resize(s->deaths, alloc_sum(index, 1),
                                 sizeof *s->deaths);
        memset(&s->deaths[s->count], 0,
               (index + 1 - s->count) * sizeof *s->deaths);
        s->count = index + 1;
    }
    return &s->deaths[index];
}

/* The join of the path from into the path into. A value dead on one of
   the two is dead after the join, with the death of into first. */
static void state_join(struct state *into, const struct state *from)
{
    size_t count;
    size_t i;

    if (!from->live) {
        return;
    }
    if (!into->live) {
        state_free(into);
        *into = state_copy(from);
        return;
    }
    count = into->count > from->count ? into->count : from->count;
    for (i = 0; i < count; i++) {
        const struct death *a = death_of(into, i);
        const struct death *b = death_of(from, i);
        if (a != NULL && b != NULL) {
            slot(into, i)->mixed = a->mixed || b->mixed;
        } else if (a != NULL) {
            slot(into, i)->mixed = true;
        } else if (b != NULL) {
            struct death *d = slot(into, i);
            *d = *b;
            d->mixed = true;
        }
    }
}

/* The `own` field of the class value or the class behind the pointer t
   named name, in its class or a base, or NULL. An `own` field here is a
   pointer or a slice, whose buffer it owns. */
static const struct struct_field *own_field(const struct type *t,
                                            const struct name *name)
{
    const struct struct_field *f = NULL;

    if (t != NULL && t->kind == TYPE_POINTER) {
        t = t->element;
    }
    for (; t != NULL && f == NULL;
         t = t->kind == TYPE_CLASS ? t->base : NULL) {
        if (t->kind == TYPE_CLASS || t->kind == TYPE_STRUCT) {
            f = types_find_field(t, name);
        }
    }
    if (f == NULL || !f->owned || f->type == NULL ||
        (f->type->kind != TYPE_POINTER && f->type->kind != TYPE_SLICE)) {
        return NULL;
    }
    return f;
}

/* Whether e names a value the walk follows, written into out: a local
   or a parameter of the frame, or an `own` field of its `self`. The
   error a handler binds has rules of its own. */
static bool value_of(const struct free_walk *w, const struct expr *e,
                     struct value *out)
{
    const struct symbol *sym;
    const struct expr *base;

    if (e == NULL) {
        return false;
    }
    if (e->kind == EXPR_NAME) {
        sym = e->symbol;
        if (sym == NULL || sym->frame != w->frame || sym->caught ||
            (sym->kind != SYMBOL_LOCAL && sym->kind != SYMBOL_PARAM)) {
            return false;
        }
        out->symbol = sym;
        out->field = NULL;
        return true;
    }
    if (e->kind != EXPR_FIELD || e->symbol != NULL || w->frame->self == NULL) {
        return false;
    }
    base = e->as.field.base;
    if (base->kind != EXPR_NAME || base->symbol != w->frame->self) {
        return false;
    }
    out->field = own_field(base->type, &e->as.field.name);
    out->symbol = base->symbol;
    return out->field != NULL;
}

/* The index of v, which find adds when add is set, or the count of the
   values. */
static size_t find(struct values *list, const struct value *v, bool add)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i].symbol == v->symbol &&
            list->items[i].field == v->field) {
            return i;
        }
    }
    if (!add) {
        return list->count;
    }
    list->items = alloc_grow(list->items, &list->capacity, list->count,
                             sizeof *list->items);
    list->items[list->count] = *v;
    return list->count++;
}

/* The death of v on the path of w, or NULL while v is alive there. */
static struct death *dead(const struct free_walk *w, const struct value *v)
{
    size_t index = find(w->values, v, false);

    return index < w->now.count && w->now.deaths[index].dead
               ? &w->now.deaths[index]
               : NULL;
}

/* The message of a use at pos of v, which died as d says. A death is
   refused once on a path, so one mistake gives one message: the uses
   after the first, a second `delete` among them, follow from it. */
static void refuse(struct free_walk *w, struct pos pos,
                   const struct value *v, struct death *d)
{
    struct text value = {0};
    struct text how = {0};
    const char *where = d->turn_before ? " in the turn before"
                        : d->mixed     ? " on one of the paths here"
                                       : "";

    if (w->quiet || d->reported ||
        (w->deferred && sema_reported_at(w->c, pos, NAME_USE_AFTER_FREE))) {
        return;
    }
    d->reported = true;
    if (v->field != NULL) {
        text_appendf(&value, "self.%.*s", (int)v->field->name.length,
                     v->field->name.text);
    } else {
        text_appendf(&value, "%.*s", (int)v->symbol->name.length,
                     v->symbol->name.text);
    }
    switch (d->kind) {
    case DIED_DELETED:
        text_append(&how, "deleted");
        break;
    case DIED_DESTROYED:
        text_append(&how, "destroyed");
        break;
    case DIED_STORED:
        text_appendf(&how, "moved into the `own` field `%.*s`",
                     (int)d->into.length, d->into.text);
        break;
    case DIED_MOVED:
        if (d->into.length > 0 && d->by.length > 0) {
            text_appendf(&how, "moved into `%.*s` by `%.*s`",
                         (int)d->into.length, d->into.text,
                         (int)d->by.length, d->by.text);
        } else if (d->into.length > 0 || d->by.length > 0) {
            const struct name *to = d->by.length > 0 ? &d->by : &d->into;
            text_appendf(&how, "moved into `%.*s`", (int)to->length,
                         to->text);
        } else {
            text_append(&how, "moved");
        }
        break;
    }
    sema_check_at(w->c, NAME_USE_AFTER_FREE, pos, "`%s` was %s on line %d%s",
                  text_cstr(&value), text_cstr(&how), d->line, where);
    text_free(&value);
    text_free(&how);
}

/* A use of v at pos: refused when a path to it left v dead. */
static void use(struct free_walk *w, struct pos pos, const struct value *v)
{
    struct death *d = dead(w, v);

    if (d != NULL) {
        refuse(w, pos, v, d);
    }
}

/* Whether t is a pointer to bytes, `*byte` or `?*byte`. */
static bool bytes_pointer(const struct type *t)
{
    if (t != NULL && t->kind == TYPE_OPTIONAL) {
        t = t->element;
    }
    return t != NULL && t->kind == TYPE_POINTER && t->element != NULL &&
           t->element->kind == TYPE_U8;
}

/* DESIGN: `destroy` tears the value down and leaves its memory where it
   is, so the address of a destroyed value still names memory of the
   program. `free(p)` gives it back and `p as *byte` reads it as bytes,
   which is how a program ends memory that `destroy` left. Both are
   allowed after `destroy`, and refused after `delete` or a move, which
   left no memory of the program's there. */
static void use_address(struct free_walk *w, const struct expr *e)
{
    struct value v;
    struct death *d;

    if (!value_of(w, e, &v)) {
        walk_expr(w, e);
        return;
    }
    d = dead(w, &v);
    if (d != NULL && d->kind != DIED_DESTROYED) {
        refuse(w, e->kind == EXPR_FIELD ? e->as.field.base->pos : e->pos,
               &v, d);
    }
}

/* A use inside a closure of a value of a function around it, at pos,
   which is refused when the value is dead where the closure stands. */
static void use_outer(struct free_walk *w, const struct expr *e,
                      struct pos pos)
{
    const struct free_walk *o;
    struct value v;

    for (o = w->outer; o != NULL; o = o->outer) {
        if (value_of(o, e, &v)) {
            struct death *d = dead(o, &v);
            if (d != NULL) {
                refuse(w, pos, &v, d);
            }
            return;
        }
    }
}

static void kill(struct free_walk *w, const struct value *v,
                 enum death_kind kind, struct pos at,
                 const struct name *into, const struct name *by)
{
    static const struct name no_name = {"", 0};
    struct death *d = slot(&w->now, find(w->values, v, true));

    d->dead = true;
    d->mixed = false;
    d->turn_before = false;
    d->kind = kind;
    d->line = at.line;
    d->into = into != NULL ? *into : no_name;
    d->by = by != NULL ? *by : no_name;
}

static void revive(struct free_walk *w, const struct value *v)
{
    size_t index = find(w->values, v, false);

    if (index < w->now.count) {
        w->now.deaths[index].dead = false;
    }
}

/* The move of the name e that the checker marked, with the place and the
   function its note names. */
static void kill_moved(struct free_walk *w, const struct expr *e,
                       const struct value *v)
{
    const struct move_note *note = ptr_map_get(&w->c->moves, e);

    kill(w, v, DIED_MOVED, e->pos, note != NULL ? &note->into : NULL,
         note != NULL ? &note->by : NULL);
}

static void walk_list(struct free_walk *w, struct expr *const *list,
                      size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        walk_expr(w, list[i]);
    }
}

/* The name of the function sym without the class before it. */
static struct name bare_name(const struct symbol *sym)
{
    struct name n = sym->name;
    size_t i;

    for (i = n.length; i > 0; i--) {
        if (n.text[i - 1] == '.') {
            n.text += i;
            n.length -= i;
            break;
        }
    }
    return n;
}

/* DESIGN: a part of a literal moves once the literal has read every
   part, as the checker moves it, so a part after it may still read it.
   A part that names a value moved by an earlier part, `(k, k)`, is
   refused there. into names the `own` field a part stands in, and a
   part that names a pointer moves into it. */
struct part {
    const struct expr *e;
    struct value v;
    const struct struct_field *into;
};

static void walk_parts(struct free_walk *w, const struct expr *literal,
                       struct expr *const *parts,
                       const struct field_init *inits, size_t count)
{
    struct part *moved = NULL;
    size_t moved_count = 0;
    size_t capacity = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        const struct expr *e = parts != NULL ? parts[i] : inits[i].value;
        const struct struct_field *into =
            inits != NULL ? own_field(literal->type, &inits[i].name) : NULL;
        struct value v;
        if (e == NULL || !value_of(w, e, &v) ||
            !(e->moves || into != NULL)) {
            walk_expr(w, e);
            continue;
        }
        use(w, e->pos, &v);
        moved = alloc_grow(moved, &capacity, moved_count, sizeof *moved);
        moved[moved_count].e = e;
        moved[moved_count].v = v;
        moved[moved_count].into = e->moves ? NULL : into;
        moved_count++;
    }
    for (i = 0; i < moved_count && w->now.live; i++) {
        struct death *d = dead(w, &moved[i].v);
        if (d != NULL) {
            refuse(w, moved[i].e->pos, &moved[i].v, d);
        }
        if (moved[i].into != NULL) {
            kill(w, &moved[i].v, DIED_STORED, moved[i].e->pos,
                 &moved[i].into->name, NULL);
        } else {
            kill_moved(w, moved[i].e, &moved[i].v);
        }
    }
    free(moved);
}

/* A path that may not run, the right operand of `&&`, `||` and `??` and
   the access of `p?.x`: the walk joins the state before it. */
static void walk_maybe(struct free_walk *w, const struct expr *e)
{
    struct state before = state_copy(&w->now);

    walk_expr(w, e);
    state_join(&w->now, &before);
    state_free(&before);
}

/* A handler block: the error path from the state now, whose end and
   every `yield` join the path that goes on without an error. */
static void walk_handler(struct free_walk *w, const struct handler *h)
{
    struct state *yields = w->yields;
    struct state given = state_none();
    struct state ok;
    size_t yield_base = w->yield_base;

    if (h->kind == HANDLE_TRY) {
        run_exits(w, 0, true);
    }
    if (h->kind != HANDLE_BLOCK || h->body == NULL) {
        if (h->kind == HANDLE_ENCLOSING && w->tried != NULL) {
            run_exits(w, w->try_base, true);
            state_join(w->tried, &w->now);
        }
        return;
    }
    ok = state_copy(&w->now);
    w->yields = &given;
    w->yield_base = pending_count(w);
    walk_block(w, h->body);
    w->yields = yields;
    w->yield_base = yield_base;
    state_join(&ok, &w->now);
    state_join(&ok, &given);
    state_take(&w->now, &ok);
    state_free(&given);
}

static void walk_call(struct free_walk *w, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;
    const struct symbol *fn =
        callee->kind == EXPR_NAME ? callee->symbol : NULL;
    size_t i;

    walk_expr(w, callee);
    for (i = 0; i < e->as.call.arg_count && w->now.live; i++) {
        const struct expr *arg = e->as.call.args[i];
        struct value v;
        walk_expr(w, arg);
        /* An `own` field of `self` moves into an `own` parameter, which
           the checker does not mark. */
        if (fn != NULL && fn->owned != NULL && i < fn->owned_count &&
            fn->owned[i] && arg->kind == EXPR_FIELD && value_of(w, arg, &v)) {
            struct name by = bare_name(fn);
            if (fn->item != NULL && fn->item->has_self && i > 0) {
                struct name into = sema_place_name(e->as.call.args[0]);
                kill(w, &v, DIED_MOVED, arg->pos, &into, &by);
            } else {
                kill(w, &v, DIED_MOVED, arg->pos, &by, NULL);
            }
        }
    }
    walk_handler(w, &e->as.call.handler);
}

/* `delete(x)` and `destroy(x)` of a value the walk follows end it, and
   one already dead is refused once, as any other use. */
static void walk_object(struct free_walk *w, const struct expr *e)
{
    const struct expr *target = e->as.object.operand;
    struct value v;

    if (e->as.object.op == TOKEN_DUP) {
        walk_expr(w, target);
        return;
    }
    if (target->kind == EXPR_UNARY && target->as.unary.op == TOKEN_AMP) {
        target = target->as.unary.operand;
    }
    if (!value_of(w, target, &v)) {
        walk_expr(w, e->as.object.operand);
        walk_expr(w, e->as.object.from);
        return;
    }
    use(w, target->kind == EXPR_FIELD ? target->as.field.base->pos
                                      : target->pos,
        &v);
    walk_expr(w, e->as.object.from);
    kill(w, &v,
         e->as.object.op == TOKEN_DELETE ? DIED_DELETED : DIED_DESTROYED,
         e->pos, NULL, NULL);
}

/* An anonymous function is a frame of its own, which may run at any
   time, so it starts with every value of its own alive. */
static void walk_closure(struct free_walk *w, const struct item *fn)
{
    struct free_walk inner = *w;
    struct exits exits = {NULL, 0, 0, 0};

    if (fn == NULL || fn->body == NULL) {
        return;
    }
    inner.frame = fn;
    inner.outer = w;
    inner.now = state_live();
    inner.loop = NULL;
    inner.yields = NULL;
    inner.tried = NULL;
    inner.fall = NULL;
    inner.exits = w->exits != NULL ? &exits : NULL;
    walk_block(&inner, fn->body);
    state_free(&inner.now);
    free(exits.items);
}

static void walk_expr(struct free_walk *w, const struct expr *e)
{
    struct value v;
    size_t i;

    if (e == NULL || !w->now.live) {
        return;
    }
    switch (e->kind) {
    case EXPR_NAME:
        if (value_of(w, e, &v)) {
            use(w, e->pos, &v);
            if (e->moves) {
                kill_moved(w, e, &v);
            }
        } else {
            use_outer(w, e, e->pos);
        }
        return;
    case EXPR_FIELD:
        if (value_of(w, e, &v)) {
            use(w, e->as.field.base->pos, &v);
            return;
        }
        if (e->symbol == NULL && e->as.field.base->kind == EXPR_NAME &&
            w->outer != NULL) {
            use_outer(w, e, e->as.field.base->pos);
        }
        walk_expr(w, e->as.field.base);
        return;
    case EXPR_UNARY:
        walk_expr(w, e->as.unary.operand);
        return;
    case EXPR_BINARY:
        walk_expr(w, e->as.binary.left);
        if (e->as.binary.op == TOKEN_AND_AND ||
            e->as.binary.op == TOKEN_OR_OR ||
            e->as.binary.op == TOKEN_QUESTION_QUESTION) {
            walk_maybe(w, e->as.binary.right);
        } else {
            walk_expr(w, e->as.binary.right);
        }
        return;
    case EXPR_CAST:
        if (!e->as.cast.test && bytes_pointer(e->type)) {
            use_address(w, e->as.cast.operand);
        } else {
            walk_expr(w, e->as.cast.operand);
        }
        return;
    case EXPR_CALL:
        walk_call(w, e);
        return;
    case EXPR_INDEX:
        walk_expr(w, e->as.index.base);
        walk_expr(w, e->as.index.index);
        return;
    case EXPR_SLICE:
        walk_expr(w, e->as.slice.base);
        walk_expr(w, e->as.slice.low);
        walk_expr(w, e->as.slice.high);
        return;
    case EXPR_STRUCT_LIT:
        walk_parts(w, e, NULL, e->as.struct_lit.fields,
                   e->as.struct_lit.field_count);
        return;
    case EXPR_TUPLE:
        walk_parts(w, e, e->as.tuple.elements, NULL, e->as.tuple.count);
        return;
    case EXPR_ARRAY_LIT:
        walk_parts(w, e, e->as.array_lit.elements, NULL,
                   e->as.array_lit.count);
        return;
    case EXPR_SLICE_LIT:
        for (i = 0; i < e->as.slice_lit.field_count; i++) {
            walk_expr(w, e->as.slice_lit.fields[i].value);
        }
        return;
    case EXPR_ARRAY_REPEAT:
        walk_expr(w, e->as.array_repeat.value);
        walk_expr(w, e->as.array_repeat.count);
        return;
    case EXPR_ALLOC:
        walk_expr(w, e->as.alloc.count);
        walk_expr(w, e->as.alloc.value);
        return;
    case EXPR_FREE:
        use_address(w, e->as.free_pointer);
        return;
    case EXPR_OBJECT:
        walk_object(w, e);
        return;
    case EXPR_ATOMIC:
        walk_expr(w, e->as.atomic.place);
        walk_expr(w, e->as.atomic.a);
        walk_expr(w, e->as.atomic.b);
        return;
    case EXPR_PARALLEL:
        walk_expr(w, e->as.parallel.array);
        walk_expr(w, e->as.parallel.chunks);
        walk_expr(w, e->as.parallel.call);
        return;
    case EXPR_DISPATCH:
        walk_expr(w, e->as.dispatch.object);
        walk_expr(w, e->as.dispatch.call);
        return;
    case EXPR_JOIN:
        walk_expr(w, e->as.join.job);
        return;
    case EXPR_FORMAT:
        for (i = 0; i < e->as.format.count; i++) {
            walk_expr(w, e->as.format.parts[i].value);
        }
        return;
    case EXPR_IN:
        walk_expr(w, e->as.in.value);
        if (e->as.in.test != NULL) {
            walk_expr(w, e->as.in.test);
        } else {
            walk_expr(w, e->as.in.low);
            walk_expr(w, e->as.in.high);
        }
        return;
    case EXPR_OPTIONAL:
        walk_expr(w, e->as.optional.base);
        walk_maybe(w, e->as.optional.access);
        return;
    case EXPR_SYNC_OP:
        walk_expr(w, e->as.sync_op.target);
        walk_expr(w, e->as.sync_op.value);
        if (e->as.sync_op.op == SYNC_CHAN_DELETE &&
            value_of(w, e->as.sync_op.target, &v)) {
            kill(w, &v, DIED_DELETED, e->pos, NULL, NULL);
        }
        return;
    case EXPR_SIMD:
        walk_list(w, e->as.simd.args, e->as.simd.arg_count);
        return;
    case EXPR_COLLECT:
        walk_expr(w, e->as.collect.start);
        return;
    case EXPR_FN:
        walk_closure(w, e->as.fn);
        return;
    /* The field holds `none` after `take`, which is a value of its own,
       so `=` tears down the `none` and the field is alive again. */
    case EXPR_TAKE:
        walk_expr(w, e->as.take);
        if (value_of(w, e->as.take, &v)) {
            revive(w, &v);
        }
        return;
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_CHAR:
    case EXPR_STRING:
    case EXPR_BYTES:
    case EXPR_BOOL:
    case EXPR_NONE:
    case EXPR_SIZE_OF:
    case EXPR_HERE:
    case EXPR_PATTERN:
    case EXPR_DESCRIPTOR:
        return;
    }
}

/* A name a statement binds holds a new value. */
static void bind(struct free_walk *w, const struct symbol *sym)
{
    struct value v;

    if (sym == NULL) {
        return;
    }
    v.symbol = sym;
    v.field = NULL;
    revive(w, &v);
}

static void walk_let(struct free_walk *w, const struct stmt *s)
{
    size_t i;

    walk_expr(w, s->as.let.value);
    if (s->as.let.otherwise != NULL) {
        struct state ok = state_copy(&w->now);
        walk_block(w, s->as.let.otherwise);
        state_join(&ok, &w->now);
        state_take(&w->now, &ok);
    }
    walk_handler(w, &s->as.let.guard);
    bind(w, s->as.let.symbol);
    for (i = 0; i < s->as.let.name_count; i++) {
        bind(w, s->as.let.names[i].symbol);
    }
}

/* DESIGN: `=` gives a value the walk follows a new value. Into an `own`
   field of `self` that is dead on a path, it tears nothing down, since
   the old value is gone already and R074 tears down a live one alone.
   A value that `=` stores into an `own` field moves into it. */
static void walk_assign(struct free_walk *w, const struct stmt *s)
{
    const struct expr *target = s->as.assign.target;
    const struct expr *value = s->as.assign.value;
    const struct struct_field *into = NULL;
    struct value held;
    struct value moved;
    bool named = value_of(w, target, &held);

    if (!named || s->as.assign.op != TOKEN_ASSIGN) {
        walk_expr(w, target);
    }
    walk_expr(w, value);
    if (!w->now.live || s->as.assign.op != TOKEN_ASSIGN) {
        return;
    }
    if (target->kind == EXPR_FIELD && target->symbol == NULL) {
        into = own_field(target->as.field.base->type, &target->as.field.name);
    }
    if (into != NULL && value_of(w, value, &moved) && !value->moves) {
        kill(w, &moved, DIED_STORED, value->pos, &into->name, NULL);
    }
    if (!named) {
        return;
    }
    if (held.field != NULL && !w->quiet && !w->deferred &&
        dead(w, &held) != NULL) {
        ((struct stmt *)s)->as.assign.tears_nothing = true;
    }
    revive(w, &held);
}

/* The paths of an `if`: each condition runs once those before it were
   false, and the paths of the bodies join after the statement. */
static void walk_if(struct free_walk *w, const struct stmt *s)
{
    struct state after = state_none();
    size_t i;

    for (i = 0; i < s->as.if_chain.count; i++) {
        struct state otherwise;
        walk_expr(w, s->as.if_chain.branches[i].cond);
        otherwise = state_copy(&w->now);
        walk_block(w, s->as.if_chain.branches[i].body);
        state_join(&after, &w->now);
        state_take(&w->now, &otherwise);
    }
    walk_block(w, s->as.if_chain.else_body);
    state_join(&after, &w->now);
    state_take(&w->now, &after);
}

/* The arms of a `switch` and of a `select`, each from the state before
   it. A switch without `else` may match no arm, so the state before it
   joins the arms there. */
static void walk_arms(struct free_walk *w, const struct switch_arm *arms,
                      size_t count, const struct stmt *otherwise,
                      bool none_matched)
{
    struct state *fall = w->fall;
    size_t fall_base = w->fall_base;
    struct state before = state_copy(&w->now);
    struct state after = state_none();
    struct state fell = state_none();
    size_t i;

    w->fall = &fell;
    w->fall_base = pending_count(w);
    for (i = 0; i < count; i++) {
        struct state previous = fell;
        state_free(&w->now);
        w->now = state_copy(&before);
        fell = state_none();
        walk_expr(w, arms[i].test != NULL ? arms[i].test : arms[i].value);
        state_join(&w->now, &previous);
        state_free(&previous);
        bind(w, arms[i].bound);
        walk_stmt(w, arms[i].body);
        state_join(&after, &w->now);
    }
    state_free(&w->now);
    w->now = state_copy(&before);
    state_join(&w->now, &fell);
    if (otherwise != NULL) {
        walk_stmt(w, otherwise);
        state_join(&after, &w->now);
    } else if (none_matched) {
        state_join(&after, &w->now);
    }
    w->fall = fall;
    w->fall_base = fall_base;
    state_take(&w->now, &after);
    state_free(&before);
    state_free(&fell);
}

/* One turn of a loop from the state now: the head that runs before the
   body, the body and the tail that runs after it. exits gets the path
   that leaves at the head, and the loop flow the breaks and the ends of
   the turns. */
struct loop_parts {
    const struct expr *head;
    const struct expr *head_more;
    const struct block *body;
    const struct expr *tail;
    const struct stmt *loop;
    bool leaves_at_head;
    bool leaves_at_tail;
};

static void one_turn(struct free_walk *w, const struct loop_parts *p,
                     struct loop_flow *flow, struct state *exits)
{
    struct loop_flow *outer = w->loop;
    size_t i;

    walk_expr(w, p->head);
    walk_expr(w, p->head_more);
    if (p->leaves_at_head) {
        state_join(exits, &w->now);
    }
    if (p->loop != NULL && p->loop->kind == STMT_FOR) {
        for (i = 0; i < p->loop->as.for_loop.name_count; i++) {
            bind(w, p->loop->as.for_loop.names[i].symbol);
        }
    }
    w->loop = flow;
    walk_block(w, p->body);
    w->loop = outer;
    state_join(&w->now, &flow->turns);
    walk_expr(w, p->tail);
    if (p->leaves_at_tail) {
        state_join(exits, &w->now);
    }
}

/* DESIGN: the first walk of the body reports nothing and gives the state
   at the end of a turn. The second starts from its join with the state
   before the loop, and a value that only the end of a turn left dead is
   named as dead since the turn before. After the loop that value is dead
   on one of the paths. */
static void walk_loop(struct free_walk *w, const struct loop_parts *p)
{
    struct state entry = state_copy(&w->now);
    struct state exits = state_none();
    struct loop_flow flow;
    bool quiet = w->quiet;
    size_t i;

    flow.breaks = state_none();
    flow.turns = state_none();
    flow.base = pending_count(w);
    w->quiet = true;
    one_turn(w, p, &flow, &exits);
    w->quiet = quiet;
    for (i = 0; i < w->now.count; i++) {
        struct death *d = &w->now.deaths[i];
        if (d->dead && death_of(&entry, i) == NULL) {
            d->turn_before = true;
        }
    }
    state_join(&entry, &w->now);
    state_free(&w->now);
    state_free(&exits);
    state_free(&flow.breaks);
    state_free(&flow.turns);
    w->now = entry;
    flow.breaks = state_none();
    flow.turns = state_none();
    exits = state_none();
    one_turn(w, p, &flow, &exits);
    state_join(&exits, &flow.breaks);
    for (i = 0; i < exits.count; i++) {
        if (exits.deaths[i].turn_before) {
            exits.deaths[i].turn_before = false;
            exits.deaths[i].mixed = true;
        }
    }
    state_take(&w->now, &exits);
    state_free(&flow.breaks);
    state_free(&flow.turns);
}

static void walk_while(struct free_walk *w, const struct stmt *s)
{
    struct loop_parts p;
    const struct expr *cond = s->as.loop.cond;
    bool forever = cond->kind == EXPR_BOOL && cond->as.boolean;

    memset(&p, 0, sizeof p);
    p.body = s->as.loop.body;
    if (s->kind == STMT_WHILE) {
        p.head = cond;
        p.leaves_at_head = !forever;
    } else {
        p.tail = cond;
        p.leaves_at_tail = !forever;
    }
    walk_loop(w, &p);
}

static void walk_for(struct free_walk *w, const struct stmt *s)
{
    const struct iteration *it = &s->as.for_loop.hooks;
    struct loop_parts p;

    walk_expr(w, s->as.for_loop.low);
    walk_expr(w, s->as.for_loop.high);
    walk_expr(w, s->as.for_loop.step);
    walk_expr(w, it->start != NULL ? it->start : s->as.for_loop.over);
    memset(&p, 0, sizeof p);
    p.head = it->advance;
    p.head_more = it->current;
    p.body = s->as.for_loop.body;
    p.loop = s;
    p.leaves_at_head = true;
    walk_loop(w, &p);
}

/* `try { } catch e { }`: each failing call of the body reaches the
   handler with the state it has there, and the paths of the handler
   join the path of the body after the statement. */
static void walk_try(struct free_walk *w, const struct stmt *s)
{
    struct state *tried = w->tried;
    struct state *yields = w->yields;
    size_t try_base = w->try_base;
    size_t yield_base = w->yield_base;
    struct state failed = state_none();
    struct state given = state_none();
    struct state after;

    w->tried = &failed;
    w->try_base = pending_count(w);
    walk_block(w, s->as.try_block.body);
    w->tried = tried;
    w->try_base = try_base;
    after = state_copy(&w->now);
    if (failed.live && s->as.try_block.handler.body != NULL) {
        state_take(&w->now, &failed);
        w->yields = &given;
        w->yield_base = pending_count(w);
        walk_block(w, s->as.try_block.handler.body);
        w->yields = yields;
        w->yield_base = yield_base;
        state_join(&after, &w->now);
        state_join(&after, &given);
    }
    state_take(&w->now, &after);
    state_free(&failed);
    state_free(&given);
}

/* A `defer` or an `undo` inside a deferred statement runs where that
   one is left, from a copy of the state where it stands, which the path
   goes on without. */
static void walk_deferred(struct free_walk *w, const struct stmt *s)
{
    struct free_walk inner = *w;

    inner.now = state_copy(&w->now);
    inner.deferred = true;
    inner.loop = NULL;
    inner.yields = NULL;
    inner.tried = NULL;
    inner.fall = NULL;
    walk_stmt(&inner, s);
    state_free(&inner.now);
}

/* Run the pending statements above base at an exit, from a copy of the
   state now. failing marks an error exit. */
static void run_exits(struct free_walk *w, size_t base, bool failing)
{
    struct free_walk inner;
    const struct exits *x = w->exits;
    size_t end;
    size_t start;
    size_t i;

    if (x == NULL || !w->now.live || x->count <= base) {
        return;
    }
    inner = *w;
    inner.now = state_copy(&w->now);
    inner.exits = NULL;
    inner.deferred = true;
    inner.loop = NULL;
    inner.yields = NULL;
    inner.tried = NULL;
    inner.fall = NULL;
    for (end = x->count; end > base && inner.now.live; end = start) {
        for (start = end; start > base &&
                          x->items[start - 1].block == x->items[end - 1].block;
             start--) {
        }
        for (i = end; failing && i > start; i--) {
            if (x->items[i - 1].undo) {
                walk_stmt(&inner, x->items[i - 1].s);
            }
        }
        for (i = end; i > start; i--) {
            if (!x->items[i - 1].undo) {
                walk_stmt(&inner, x->items[i - 1].s);
            }
        }
    }
    state_free(&inner.now);
}

/* The count of the pending statements, the base of an exit. */
static size_t pending_count(const struct free_walk *w)
{
    return w->exits != NULL ? w->exits->count : 0;
}

/* The path ends here, and into, when given, takes it. */
static void leave(struct free_walk *w, struct state *into)
{
    if (into != NULL) {
        state_join(into, &w->now);
    }
    state_free(&w->now);
}

static void walk_stmt(struct free_walk *w, const struct stmt *s)
{
    if (s == NULL || !w->now.live) {
        return;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        walk_let(w, s);
        return;
    case STMT_EXPR:
        walk_expr(w, s->as.expr);
        if (sema_calls_never(s)) {
            leave(w, NULL);
        }
        return;
    case STMT_ASSIGN:
        walk_assign(w, s);
        return;
    case STMT_IF:
        walk_if(w, s);
        return;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        walk_while(w, s);
        return;
    case STMT_FOR:
        walk_for(w, s);
        return;
    case STMT_DEFER:
    case STMT_UNDO:
        if (w->exits != NULL) {
            struct exits *x = w->exits;
            x->items = alloc_grow(x->items, &x->capacity, x->count,
                                  sizeof *x->items);
            x->items[x->count].s = s->as.deferred;
            x->items[x->count].undo = s->kind == STMT_UNDO;
            x->items[x->count].block = x->depth;
            x->count++;
        } else {
            walk_deferred(w, s->as.deferred);
        }
        return;
    case STMT_FAIL:
        walk_expr(w, s->as.fail.value);
        run_exits(w, 0, true);
        leave(w, NULL);
        return;
    case STMT_SWITCH:
        walk_expr(w, s->as.switch_stmt.value);
        walk_arms(w, s->as.switch_stmt.arms, s->as.switch_stmt.count,
                  s->as.switch_stmt.otherwise, true);
        return;
    case STMT_SELECT:
        walk_arms(w, s->as.select.arms, s->as.select.count, NULL, false);
        return;
    case STMT_ASSERT:
        walk_expr(w, s->as.assertion.cond);
        return;
    case STMT_RETURN:
        walk_expr(w, s->as.return_value);
        run_exits(w, 0, false);
        leave(w, NULL);
        return;
    case STMT_YIELD:
        walk_expr(w, s->as.yielded);
        run_exits(w, w->yield_base, false);
        leave(w, w->yields);
        return;
    case STMT_TRY:
        walk_try(w, s);
        return;
    case STMT_BLOCK:
        walk_block(w, s->as.block);
        return;
    case STMT_SYNC:
        walk_expr(w, s->as.sync.mutex);
        walk_expr(w, s->as.sync.second);
        walk_block(w, s->as.sync.body);
        return;
    case STMT_BREAK:
    case STMT_CONTINUE:
        if (w->loop != NULL) {
            run_exits(w, w->loop->base, false);
        }
        leave(w, w->loop == NULL           ? NULL
                 : s->kind == STMT_BREAK ? &w->loop->breaks
                                         : &w->loop->turns);
        return;
    case STMT_FALLTHROUGH:
        run_exits(w, w->fall_base, false);
        leave(w, w->fall);
        return;
    }
}

/* The closing brace is an exit of the block, and never an error. */
static void walk_block(struct free_walk *w, const struct block *b)
{
    size_t base = pending_count(w);
    size_t i;

    if (w->exits != NULL) {
        w->exits->depth++;
    }
    for (i = 0; b != NULL && i < b->count; i++) {
        walk_stmt(w, b->stmts[i]);
    }
    if (w->exits != NULL) {
        run_exits(w, base, false);
        w->exits->count = base;
        w->exits->depth--;
    }
}

void sema_check_frees(struct checker *c, const struct item *it, size_t first)
{
    struct free_walk w;
    struct values values = {NULL, 0, 0};
    struct exits exits = {NULL, 0, 0, 0};
    size_t i;

    /* A body with an error has types the pass cannot trust. A safety
       check before it has its types in place. */
    for (i = first; i < c->diags->count; i++) {
        if (!c->diags->items[i].warning &&
            c->diags->items[i].name == NAME_NONE) {
            return;
        }
    }
    if (it->body == NULL || c->ctx.quiet > 0) {
        return;
    }
    memset(&w, 0, sizeof w);
    w.c = c;
    w.frame = it;
    w.values = &values;
    w.exits = &exits;
    w.now = state_live();
    walk_block(&w, it->body);
    state_free(&w.now);
    free(values.items);
    free(exits.items);
}
