/* The safety check `uninitialized-read`, C1 of "Compile-time checks of
   2026-10-20" in docs/decisions.md: an element of `alloc(T, n)` read
   before a path wrote it. The pass walks a function body once the
   checker has checked it, with the types and the symbols in place. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* DESIGN: the pass follows each path of the body in the order it runs,
   as sema_free.c does. A local that `let` or `=` gives the memory of
   `alloc(T, n)` is a buffer, and its elements are unwritten. A write at
   a constant index marks that index, and a write at the index a local
   names marks the local until `=` changes it. A loop writes the whole
   range when every turn of it writes an element at a computed index. A
   use of the buffer other than an index, a field of its first element
   or a comparison with `none` lets it leave the function, and from
   there on every element counts as written, since the function cannot
   see what the callee or the place does. Where paths join, an index is
   written when every path wrote it. A read at a constant index needs
   that index, and a read at a computed index needs the whole range or
   the same local. The check refuses only what it proves, so every
   case it cannot follow counts as written. */

enum key_kind {
    KEY_CONSTANT,                   /* the constant value */
    KEY_NAME,                       /* the index the local symbol names */
    KEY_ANY                         /* some computed index, for a loop */
};

struct key {
    enum key_kind kind;
    int64_t value;
    const struct symbol *symbol;
};

/* What one path wrote of a buffer. A buffer that is not followed counts
   as written. */
struct fill {
    bool followed;
    bool reported;                  /* a read of it was refused */
    int line;                       /* the `alloc` */
    struct key *keys;
    size_t count;
};

/* The buffers of one path. A buffer past count is not followed. */
struct state {
    bool live;                      /* the path reaches the walk */
    struct fill *fills;
    size_t count;
};

struct loop_flow {
    struct state breaks;
    struct state turns;             /* the ends of the turns, and `continue` */
    size_t base;                    /* the pending statements of the loop */
};

/* DESIGN: a `defer` or an `undo` runs where its block is left, so the
   walk reads it at each exit with the state of that exit, and not where
   it stands. An exit runs the pending statements of every block it
   leaves, innermost first and the last declared first in each block. An
   error exit, `fail`, `try` and a call that hands its error to a `try`
   block, runs the `undo` statements of a block before its `defer`
   statements, as lowering does. The walk of an exit goes on from a copy
   of the state, which the path goes on without. */
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

/* The locals the walk follows, which the walk of a closure shares. The
   index of a local is its index in every state. */
struct buffers {
    const struct symbol **items;
    size_t count;
    size_t capacity;
};

struct fill_walk {
    struct checker *c;
    const struct item *frame;       /* the function whose locals are followed */
    struct fill_walk *outer;        /* the walk a closure stands in */
    struct buffers *buffers;
    struct state now;
    bool quiet;                     /* the first walk of a loop body */
    struct loop_flow *loop;
    struct state *yields;           /* the handler a `yield` leaves */
    struct state *tried;            /* the handler of the `try` block */
    struct state *fall;             /* the arm a `fallthrough` enters */
    struct exits *exits;            /* NULL inside a deferred statement */
    size_t yield_base;              /* the pending statements of the handler */
    size_t try_base;                /* those of the body of the `try` block */
    size_t fall_base;               /* those of the arm */
    bool at_exit;                   /* the walk of the statements of an exit */
};

static void walk_block(struct fill_walk *w, const struct block *b);
static void walk_stmt(struct fill_walk *w, const struct stmt *s);
static void walk_expr(struct fill_walk *w, const struct expr *e);
static void run_exits(struct fill_walk *w, size_t base, bool failing);
static size_t pending_count(const struct fill_walk *w);

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

static void fill_clear(struct fill *f)
{
    free(f->keys);
    memset(f, 0, sizeof *f);
}

static void fill_copy(struct fill *into, const struct fill *from)
{
    *into = *from;
    into->keys = NULL;
    if (from->count > 0) {
        into->keys = alloc_resize(NULL, from->count, sizeof *into->keys);
        memcpy(into->keys, from->keys, from->count * sizeof *into->keys);
    }
}

static struct state state_copy(const struct state *from)
{
    struct state s = {from->live, NULL, 0};
    size_t i;

    if (from->count > 0) {
        s.fills = alloc_resize(NULL, from->count, sizeof *s.fills);
        for (i = 0; i < from->count; i++) {
            fill_copy(&s.fills[i], &from->fills[i]);
        }
        s.count = from->count;
    }
    return s;
}

static void state_free(struct state *s)
{
    size_t i;

    for (i = 0; i < s->count; i++) {
        free(s->fills[i].keys);
    }
    free(s->fills);
    s->fills = NULL;
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

static struct fill *slot(struct state *s, size_t index)
{
    if (index >= s->count) {
        s->fills = alloc_resize(s->fills, alloc_sum(index, 1),
                                sizeof *s->fills);
        memset(&s->fills[s->count], 0,
               (index + 1 - s->count) * sizeof *s->fills);
        s->count = index + 1;
    }
    return &s->fills[index];
}

static bool same_key(const struct key *a, const struct key *b)
{
    return a->kind == b->kind && a->value == b->value &&
           a->symbol == b->symbol;
}

static bool has_key(const struct fill *f, const struct key *k)
{
    size_t i;

    for (i = 0; i < f->count; i++) {
        if (same_key(&f->keys[i], k)) {
            return true;
        }
    }
    return false;
}

static void add_key(struct fill *f, const struct key *k)
{
    if (has_key(f, k)) {
        return;
    }
    f->keys = alloc_resize(f->keys, alloc_sum(f->count, 1), sizeof *f->keys);
    f->keys[f->count++] = *k;
}

/* Remove the keys for which drop holds. */
static void drop_keys(struct fill *f, enum key_kind kind,
                      const struct symbol *symbol)
{
    size_t kept = 0;
    size_t i;

    for (i = 0; i < f->count; i++) {
        if (f->keys[i].kind != kind ||
            (symbol != NULL && f->keys[i].symbol != symbol)) {
            f->keys[kept++] = f->keys[i];
        }
    }
    f->count = kept;
}

/* The join of the path from into the path into. An index is written
   after the join when both paths wrote it, and a buffer one of the two
   does not follow takes what the other wrote. */
static void state_join(struct state *into, const struct state *from)
{
    size_t count;
    size_t i;
    size_t j;

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
        const struct fill *b = i < from->count ? &from->fills[i] : NULL;
        struct fill *a;
        size_t kept = 0;
        if (b == NULL || !b->followed) {
            continue;
        }
        a = slot(into, i);
        if (!a->followed) {
            fill_clear(a);
            fill_copy(a, b);
            continue;
        }
        for (j = 0; j < a->count; j++) {
            if (has_key(b, &a->keys[j])) {
                a->keys[kept++] = a->keys[j];
            }
        }
        a->count = kept;
        a->reported = a->reported || b->reported;
    }
}

/* The index of the buffer sym, which find adds when add is set, or the
   count of the buffers. */
static size_t find(struct buffers *list, const struct symbol *sym, bool add)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i] == sym) {
            return i;
        }
    }
    if (!add) {
        return list->count;
    }
    list->items = alloc_grow(list->items, &list->capacity, list->count,
                             sizeof *list->items);
    list->items[list->count] = sym;
    return list->count++;
}

/* The local of the frame of w that e names, or NULL. */
static const struct symbol *local_of(const struct fill_walk *w,
                                     const struct expr *e)
{
    const struct symbol *sym;

    if (e == NULL || e->kind != EXPR_NAME) {
        return NULL;
    }
    sym = e->symbol;
    if (sym == NULL || sym->frame != w->frame || sym->caught ||
        (sym->kind != SYMBOL_LOCAL && sym->kind != SYMBOL_PARAM)) {
        return NULL;
    }
    return sym;
}

/* The fill of the buffer e names on the path of w, or NULL when e names
   no buffer the path follows. */
static struct fill *buffer_of(struct fill_walk *w, const struct expr *e)
{
    const struct symbol *sym = local_of(w, e);
    size_t index;

    if (sym == NULL) {
        return NULL;
    }
    index = find(w->buffers, sym, false);
    if (index >= w->now.count || !w->now.fills[index].followed) {
        return NULL;
    }
    return &w->now.fills[index];
}

/* Whether e is the raw form `alloc(T, n)`, whose elements are
   unwritten. */
static bool raw_alloc(const struct expr *e)
{
    return e != NULL && e->kind == EXPR_ALLOC && e->as.alloc.count != NULL &&
           e->as.alloc.value == NULL;
}

/* The local sym holds a new value, value: a buffer when it is the raw
   form of `alloc`, and a value the walk does not follow otherwise. The
   keys every buffer holds for the index sym named are gone. */
static void bind(struct fill_walk *w, const struct symbol *sym,
                 const struct expr *value)
{
    size_t index;
    size_t i;

    if (sym == NULL) {
        return;
    }
    for (i = 0; i < w->now.count; i++) {
        drop_keys(&w->now.fills[i], KEY_NAME, sym);
    }
    if (raw_alloc(value)) {
        struct fill *f = slot(&w->now, find(w->buffers, sym, true));
        fill_clear(f);
        f->followed = true;
        f->line = value->pos.line;
        return;
    }
    index = find(w->buffers, sym, false);
    if (index < w->now.count) {
        fill_clear(&w->now.fills[index]);
    }
}

/* The buffer e names leaves the function: every element counts as
   written from here on. A local of a function around a closure leaves
   on the path of that function. */
static void leave_buffer(struct fill_walk *w, const struct expr *e)
{
    struct fill_walk *o;

    for (o = w; o != NULL; o = o->outer) {
        struct fill *f = buffer_of(o, e);
        if (local_of(o, e) != NULL) {
            if (f != NULL) {
                fill_clear(f);
            }
            return;
        }
    }
}

/* The key of an index, written into out. False for a computed index
   that names no local. */
static bool key_of(struct fill_walk *w, const struct expr *index,
                   struct key *out)
{
    const struct symbol *sym = local_of(w, index);
    struct const_value v;
    struct context saved;
    bool constant;

    memset(out, 0, sizeof *out);
    if (sym != NULL) {
        out->kind = KEY_NAME;
        out->symbol = sym;
        return true;
    }
    sema_enter_quiet(w->c, &saved);
    constant = sema_eval_const(w->c, (struct expr *)index, &v);
    sema_leave(w->c, &saved);
    if (constant && v.kind == CONST_INT) {
        out->kind = KEY_CONSTANT;
        out->value = sema_signed_bits(v.as.integer);
        return true;
    }
    return false;
}

/* A read at pos of the element of the buffer base at index, NULL for
   the first element that `*p` and `p.x` read. A refusal stops the
   following of the buffer on the path, so one mistake gives one
   message. */
static void read_element(struct fill_walk *w, const struct expr *base,
                         const struct expr *index, struct pos pos)
{
    struct fill *f = buffer_of(w, base);
    struct key k;
    bool known;

    if (f == NULL || !w->now.live) {
        return;
    }
    if (index != NULL) {
        known = key_of(w, index, &k);
    } else {
        memset(&k, 0, sizeof k);
        k.kind = KEY_CONSTANT;
        known = true;
    }
    if (known && has_key(f, &k)) {
        return;
    }
    if (!w->quiet && !f->reported &&
        !(w->at_exit && sema_reported_at(w->c, pos, NAME_UNINITIALIZED_READ))) {
        const struct name *name = &base->symbol->name;
        if (known && k.kind == KEY_CONSTANT) {
            sema_check_at(w->c, NAME_UNINITIALIZED_READ, pos,
                          "element %lld of `%.*s` is read before every path "
                          "from `alloc` on line %d wrote it",
                          (long long)k.value, (int)name->length, name->text,
                          f->line);
        } else {
            sema_check_at(w->c, NAME_UNINITIALIZED_READ, pos,
                          "an element of `%.*s` is read at a computed index "
                          "before every path from `alloc` on line %d wrote "
                          "the whole range",
                          (int)name->length, name->text, f->line);
        }
    }
    fill_clear(f);
}

/* A write of the element of the buffer base at index, NULL for the
   first element. */
static void write_element(struct fill_walk *w, const struct expr *base,
                          const struct expr *index)
{
    struct fill *f = buffer_of(w, base);
    struct key k;
    bool known;

    if (f == NULL || !w->now.live) {
        return;
    }
    if (index != NULL) {
        known = key_of(w, index, &k);
    } else {
        memset(&k, 0, sizeof k);
        k.kind = KEY_CONSTANT;
        known = true;
    }
    if (known) {
        add_key(f, &k);
    }
    if (!known || k.kind != KEY_CONSTANT) {
        memset(&k, 0, sizeof k);
        k.kind = KEY_ANY;
        add_key(f, &k);
    }
}

/* The element of a buffer that e reads or writes: `p[i]`, `*p` or the
   field `p.x` of the first element. base and index get the buffer and
   the index, NULL for the first element. */
static bool element_of(struct fill_walk *w, const struct expr *e,
                       const struct expr **base, const struct expr **index)
{
    if (e->kind == EXPR_INDEX && buffer_of(w, e->as.index.base) != NULL) {
        *base = e->as.index.base;
        *index = e->as.index.index;
        return true;
    }
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_STAR &&
        buffer_of(w, e->as.unary.operand) != NULL) {
        *base = e->as.unary.operand;
        *index = NULL;
        return true;
    }
    if (e->kind == EXPR_FIELD && e->symbol == NULL &&
        buffer_of(w, e->as.field.base) != NULL) {
        *base = e->as.field.base;
        *index = NULL;
        return true;
    }
    return false;
}

/* The part of an element that a place names, as `p[i].x[2]`: the
   element, or NULL when the place lies in no buffer. The walk reads the
   indices on the way. */
static const struct expr *place_element(struct fill_walk *w,
                                        const struct expr *e)
{
    const struct expr *base;
    const struct expr *index;

    if (e == NULL) {
        return NULL;
    }
    if (element_of(w, e, &base, &index)) {
        walk_expr(w, index);
        return e;
    }
    if (e->kind == EXPR_FIELD && e->symbol == NULL) {
        return place_element(w, e->as.field.base);
    }
    if (e->kind == EXPR_INDEX) {
        const struct expr *inner = place_element(w, e->as.index.base);
        if (inner != NULL) {
            walk_expr(w, e->as.index.index);
        }
        return inner;
    }
    return NULL;
}

/* The address of a place: the buffer it lies in leaves. */
static void walk_address(struct fill_walk *w, const struct expr *e)
{
    const struct expr *element = place_element(w, e);
    const struct expr *base;
    const struct expr *index;

    if (element == NULL) {
        walk_expr(w, e);
        return;
    }
    if (element_of(w, element, &base, &index)) {
        leave_buffer(w, base);
    }
}

static void walk_list(struct fill_walk *w, struct expr *const *list,
                      size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        walk_expr(w, list[i]);
    }
}

/* A path that may not run, the right operand of `&&`, `||` and `??` and
   the access of `p?.x`: the walk joins the state before it. */
static void walk_maybe(struct fill_walk *w, const struct expr *e)
{
    struct state before = state_copy(&w->now);

    walk_expr(w, e);
    state_join(&w->now, &before);
    state_free(&before);
}

/* A handler block: the error path from the state now, whose end and
   every `yield` join the path that goes on without an error. */
static void walk_handler(struct fill_walk *w, const struct handler *h)
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

static void walk_call(struct fill_walk *w, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;

    /* The receiver of a function of a class is an argument. */
    if (callee->kind == EXPR_FIELD && callee->symbol != NULL) {
        walk_expr(w, callee->as.field.base);
        leave_buffer(w, callee->as.field.base);
    } else {
        walk_expr(w, callee);
    }
    walk_list(w, e->as.call.args, e->as.call.arg_count);
    walk_handler(w, &e->as.call.handler);
}

/* An anonymous function is a frame of its own, which may run at any
   time. A buffer of a function around it that it names leaves there. */
static void walk_closure(struct fill_walk *w, const struct item *fn)
{
    struct fill_walk inner = *w;
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

/* Run the pending statements above base at an exit, from a copy of the
   state now. failing marks an error exit. */
static void run_exits(struct fill_walk *w, size_t base, bool failing)
{
    struct fill_walk inner;
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
    inner.at_exit = true;
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
static size_t pending_count(const struct fill_walk *w)
{
    return w->exits != NULL ? w->exits->count : 0;
}

/* Whether e compares a pointer with `none`, which reads no element. */
static bool none_test(const struct expr *e)
{
    return e->kind == EXPR_BINARY &&
           (e->as.binary.op == TOKEN_EQ || e->as.binary.op == TOKEN_NE) &&
           (e->as.binary.left->kind == EXPR_NONE ||
            e->as.binary.right->kind == EXPR_NONE);
}

static void walk_expr(struct fill_walk *w, const struct expr *e)
{
    const struct expr *base;
    const struct expr *index;
    size_t i;

    if (e == NULL || !w->now.live) {
        return;
    }
    if (element_of(w, e, &base, &index)) {
        walk_expr(w, index);
        read_element(w, base, index, base->pos);
        return;
    }
    switch (e->kind) {
    case EXPR_NAME:
        leave_buffer(w, e);
        return;
    case EXPR_FIELD:
        walk_expr(w, e->as.field.base);
        return;
    case EXPR_UNARY:
        if (e->as.unary.op == TOKEN_AMP) {
            walk_address(w, e->as.unary.operand);
        } else {
            walk_expr(w, e->as.unary.operand);
        }
        return;
    case EXPR_BINARY:
        if (none_test(e)) {
            if (e->as.binary.left->kind != EXPR_NAME) {
                walk_expr(w, e->as.binary.left);
            }
            if (e->as.binary.right->kind != EXPR_NAME) {
                walk_expr(w, e->as.binary.right);
            }
            return;
        }
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
        walk_expr(w, e->as.cast.operand);
        return;
    case EXPR_CALL:
        walk_call(w, e);
        return;
    case EXPR_INDEX:
        walk_expr(w, e->as.index.base);
        walk_expr(w, e->as.index.index);
        return;
    case EXPR_SLICE:
        /* A slice reads no element. It is a second way to the elements,
           so the buffer it lies in leaves. */
        walk_address(w, e->as.slice.base);
        walk_expr(w, e->as.slice.low);
        walk_expr(w, e->as.slice.high);
        return;
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            walk_expr(w, e->as.struct_lit.fields[i].value);
        }
        return;
    case EXPR_TUPLE:
        walk_list(w, e->as.tuple.elements, e->as.tuple.count);
        return;
    case EXPR_ARRAY_LIT:
        walk_list(w, e->as.array_lit.elements, e->as.array_lit.count);
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
        walk_expr(w, e->as.free_pointer);
        return;
    case EXPR_OBJECT:
        walk_expr(w, e->as.object.operand);
        walk_expr(w, e->as.object.from);
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
    case EXPR_TAKE:
        walk_expr(w, e->as.take);
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

static void walk_let(struct fill_walk *w, const struct stmt *s)
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
    bind(w, s->as.let.symbol, s->as.let.value);
    for (i = 0; i < s->as.let.name_count; i++) {
        bind(w, s->as.let.names[i].symbol, NULL);
    }
}

/* DESIGN: `=` and a compound assignment to an element of a buffer write
   it, and a compound one reads it first. A write of a part of an
   element, `p[i].x = v`, counts as a write of the element, since the
   check refuses only what it proves. `=` to a local gives it a new
   value. */
static void walk_assign(struct fill_walk *w, const struct stmt *s)
{
    const struct expr *target = s->as.assign.target;
    const struct expr *value = s->as.assign.value;
    const struct symbol *local = local_of(w, target);
    const struct expr *element;
    const struct expr *base;
    const struct expr *index;

    if (local != NULL) {
        if (s->as.assign.op != TOKEN_ASSIGN) {
            walk_expr(w, target);
        }
        walk_expr(w, value);
        if (w->now.live) {
            bind(w, local, s->as.assign.op == TOKEN_ASSIGN ? value : NULL);
        }
        return;
    }
    element = place_element(w, target);
    if (element == NULL) {
        walk_expr(w, target);
        walk_expr(w, value);
        return;
    }
    walk_expr(w, value);
    if (!element_of(w, element, &base, &index)) {
        return;
    }
    if (s->as.assign.op != TOKEN_ASSIGN) {
        read_element(w, base, index, base->pos);
    }
    write_element(w, base, index);
}

/* The paths of an `if`: each condition runs once those before it were
   false, and the paths of the bodies join after the statement. */
static void walk_if(struct fill_walk *w, const struct stmt *s)
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
static void walk_arms(struct fill_walk *w, const struct switch_arm *arms,
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
        bind(w, arms[i].bound, NULL);
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
   that leaves at the head or at the tail, and the loop flow the breaks
   and the ends of the turns. */
struct loop_parts {
    const struct expr *head;
    const struct expr *head_more;
    const struct block *body;
    const struct expr *tail;
    const struct stmt *loop;
    bool leaves_at_head;
    bool leaves_at_tail;
};

static void one_turn(struct fill_walk *w, const struct loop_parts *p,
                     struct loop_flow *flow, struct state *exits,
                     struct state *ends)
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
            bind(w, p->loop->as.for_loop.names[i].symbol, NULL);
        }
    }
    /* A computed write counts for the turn that makes it. */
    for (i = 0; i < w->now.count; i++) {
        drop_keys(&w->now.fills[i], KEY_ANY, NULL);
    }
    w->loop = flow;
    walk_block(w, p->body);
    w->loop = outer;
    state_join(&w->now, &flow->turns);
    state_free(ends);
    *ends = state_copy(&w->now);
    walk_expr(w, p->tail);
    if (p->leaves_at_tail) {
        state_join(exits, &w->now);
    }
}

/* DESIGN: a loop whose every turn writes an element of a buffer at a
   computed index, or lets the buffer leave, writes its whole range. The
   check compares neither the index with the variable of the loop nor
   the bounds with the count of `alloc`, since one function cannot
   always prove them equal, and the check refuses only what it proves.
   For the same reason the path that leaves the loop at its head or its
   tail has what the end of a turn wrote, as though the loop ran once at
   least. A loop over the range of a buffer runs no turn only when the
   buffer has no element to read. The first walk of the body reports
   nothing and gives the state at the end of a turn, and the second
   starts from its join with the state before the loop, so a read in the
   next turn of what a turn changed is refused. */
static void walk_loop(struct fill_walk *w, const struct loop_parts *p)
{
    struct state entry = state_copy(&w->now);
    struct state exits = state_none();
    struct state ends = state_none();
    struct loop_flow flow;
    bool quiet = w->quiet;
    size_t i;

    flow.breaks = state_none();
    flow.turns = state_none();
    flow.base = pending_count(w);
    w->quiet = true;
    one_turn(w, p, &flow, &exits, &ends);
    w->quiet = quiet;
    state_join(&entry, &w->now);
    state_free(&w->now);
    state_free(&exits);
    state_free(&flow.breaks);
    state_free(&flow.turns);
    w->now = entry;
    flow.breaks = state_none();
    flow.turns = state_none();
    exits = state_none();
    one_turn(w, p, &flow, &exits, &ends);
    if (ends.live) {
        struct key any = {KEY_ANY, 0, NULL};
        for (i = 0; i < exits.count; i++) {
            const struct fill *end = i < ends.count ? &ends.fills[i] : NULL;
            if (!exits.fills[i].followed) {
                continue;
            }
            fill_clear(&exits.fills[i]);
            if (end != NULL && end->followed && !has_key(end, &any)) {
                fill_copy(&exits.fills[i], end);
            }
        }
    }
    state_join(&exits, &flow.breaks);
    state_take(&w->now, &exits);
    state_free(&ends);
    state_free(&flow.breaks);
    state_free(&flow.turns);
}

static void walk_while(struct fill_walk *w, const struct stmt *s)
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

static void walk_for(struct fill_walk *w, const struct stmt *s)
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
static void walk_try(struct fill_walk *w, const struct stmt *s)
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

/* The path ends here, and into, when given, takes it. */
static void leave(struct fill_walk *w, struct state *into)
{
    if (into != NULL) {
        state_join(into, &w->now);
    }
    state_free(&w->now);
}

static void walk_stmt(struct fill_walk *w, const struct stmt *s)
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
        /* A deferred statement inside another one is read nowhere. */
        if (w->exits != NULL) {
            struct exits *x = w->exits;
            x->items = alloc_grow(x->items, &x->capacity, x->count,
                                  sizeof *x->items);
            x->items[x->count].s = s->as.deferred;
            x->items[x->count].undo = s->kind == STMT_UNDO;
            x->items[x->count].block = x->depth;
            x->count++;
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
static void walk_block(struct fill_walk *w, const struct block *b)
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

void sema_check_fills(struct checker *c, const struct item *it, size_t first)
{
    struct fill_walk w;
    struct buffers buffers = {NULL, 0, 0};
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
    w.buffers = &buffers;
    w.exits = &exits;
    w.now = state_live();
    walk_block(&w, it->body);
    state_free(&w.now);
    free(buffers.items);
    free(exits.items);
}
