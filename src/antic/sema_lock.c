/* The safety check `lock-order`, C4 of "Compile-time checks of
   2026-10-20" in docs/decisions.md: a cycle in the graph of the orders
   in which the program takes its locks. The pass reads the module once
   the checker has checked every function and made the copies of the
   generics, and the edges and the locks of the functions of every
   library the module reads. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* DESIGN: an edge leads from a lock to a lock wherever code that holds
   the first takes the second. Code holds the locks of the `sync` blocks
   around it, and a function of a synchronized class that takes the
   hidden lock holds that lock in its whole body. A `sync` takes its
   lock, and `sync a, b` takes both locks with no edge between them. A
   call takes every lock its callee takes, itself or through the calls
   it names. The pass follows a call of a function by its name: a
   function of the module, whose body it reads, and a function of a
   library, whose locks the library file carries. A call through the
   table of a class, an interface, a function value or a plugin is not
   followed, since the function that runs is not known. Such a call of
   a function of a synchronized class still takes the hidden lock of
   the object, which every function that may run there takes. The
   worker that `dispatch` and `parallel` start runs on a thread of its
   own and holds none of the locks of the code that starts it.

   A lock that the code holds already is taken again without waiting,
   or is a lock of another object of the same class, so it adds no
   edge, by the provisional entry on C4. An edge of the module that
   closes a cycle, read in the order of the text after the edges of the
   libraries, is refused. It then leaves the graph, so each cycle is
   refused once, and an `unchecked(lock-order)` that overrules it keeps
   it out of the library file as well. The copy of a generic of another
   module counts its locks for the calls of it, and its edges as edges
   of that module, since its lines name that module's source. */

/* What the walk saw at one place that takes locks while others are
   held: the lock of a `sync`, the locks of a callee of the module,
   which the pass knows once every body is read, or the locks of a
   library function. The held locks and the locks of a library lie in
   the pool of the walk. */
struct site {
    struct pos pos;
    const char *module;     /* NULL for a place of the module */
    size_t held;
    size_t held_count;
    size_t lock;
    size_t callee;
    size_t locks;
    size_t lock_count;
};

/* One function whose body the pass reads. */
struct function {
    const struct item *item;
    bool anonymous;
    /* The module of the generic whose copy it is, whose source its
       positions name, or NULL for a function of the module. */
    const char *home;
    size_t *direct;     /* the locks it takes itself */
    size_t direct_count;
    size_t direct_capacity;
    size_t *callees;    /* the functions of the module it calls */
    size_t callee_count;
    size_t callee_capacity;
};

/* One edge of the graph, of the module or of a library. module is NULL
   for an edge of the module. */
struct graph_edge {
    size_t held;
    size_t taken;
    struct pos pos;
    const char *module;
};

struct lock_walk {
    struct checker *c;
    struct lock_name *locks;
    size_t lock_count;
    size_t lock_capacity;
    struct function *fns;
    size_t fn_count;
    size_t fn_capacity;
    struct ptr_index fn_index;
    struct site *sites;
    size_t site_count;
    size_t site_capacity;
    size_t *pool;
    size_t pool_count;
    size_t pool_capacity;
    size_t *held;       /* the locks held where the walk stands */
    size_t held_count;
    size_t held_capacity;
    size_t current;     /* the function being read */
};

#define NO_INDEX ((size_t)-1)

/* The room of the name of a lock or of a place in a message. A
   diagnostic holds 160 bytes, so a longer one would be cut there. */
#define LOCK_TEXT 256

static bool same_text(const struct name *a, const struct name *b)
{
    return a->length == b->length &&
           (a->length == 0 || memcmp(a->text, b->text, a->length) == 0);
}

static bool same_lock(const struct lock_name *a, const struct lock_name *b)
{
    return same_text(&a->field, &b->field) && same_text(&a->type, &b->type) &&
           same_text(&a->module, &b->module);
}

/* The index of the lock n, which the walk gives one on its first use. */
static size_t lock_id(struct lock_walk *w, const struct lock_name *n)
{
    size_t i;

    for (i = 0; i < w->lock_count; i++) {
        if (same_lock(&w->locks[i], n)) {
            return i;
        }
    }
    w->locks = alloc_grow(w->locks, &w->lock_capacity, w->lock_count,
                          sizeof *w->locks);
    w->locks[w->lock_count] = *n;
    return w->lock_count++;
}

static const struct name hidden_lock = {HIDDEN_LOCK, sizeof HIDDEN_LOCK - 1};

/* The class or struct of t or of its chain of bases that declares the
   field name. */
static const struct type *declaring(const struct type *t,
                                    const struct name *name)
{
    const struct type *s;

    for (s = t; s != NULL; s = s->kind == TYPE_CLASS ? s->base : NULL) {
        if (types_find_field(s, name) != NULL) {
            return s;
        }
    }
    return t;
}

/* The lock of the field name of t, or NO_INDEX when t has no name. */
static size_t field_lock(struct lock_walk *w, const struct type *t,
                         const struct name *name)
{
    struct lock_name n;

    if (t == NULL ||
        (t->kind != TYPE_STRUCT && t->kind != TYPE_CLASS) ||
        t->name.length == 0) {
        return NO_INDEX;
    }
    t = declaring(t, name);
    n.module = t->module;
    n.type = t->name;
    n.field = *name;
    return lock_id(w, &n);
}

/* The hidden lock of the synchronized object that e names, in place or
   through a pointer. */
static size_t object_lock(struct lock_walk *w, const struct expr *e)
{
    const struct type *t = e->type;

    if (t != NULL && t->kind == TYPE_POINTER) {
        t = t->element;
    }
    return field_lock(w, t, &hidden_lock);
}

/* The lock of the Mutex that e names: a Mutex field of a struct or a
   class, or a static field. A Mutex reached in any other way, a local,
   a parameter or a pointer, is no lock of the graph. */
static size_t mutex_lock(struct lock_walk *w, const struct expr *e)
{
    const struct expr *place = sema_bare_place(e);
    const struct item *it;
    const struct type *base;

    if (place->type == NULL || !types_is_mutex(place->type)) {
        return NO_INDEX;
    }
    it = place->symbol != NULL ? place->symbol->item : NULL;
    if ((place->kind == EXPR_NAME || place->kind == EXPR_FIELD) &&
        it != NULL && it->is_static) {
        return it->owner != NULL && it->owner->symbol != NULL
                   ? field_lock(w, it->owner->symbol->type, &it->name)
                   : NO_INDEX;
    }
    if (place->kind != EXPR_FIELD) {
        return NO_INDEX;
    }
    base = place->as.field.base->type;
    if (base != NULL && base->kind == TYPE_POINTER) {
        base = base->element;
    }
    return field_lock(w, base, &place->as.field.name);
}

/* The class whose hidden lock a call of the function of sym takes: a
   function of a synchronized class with `self` that is not private,
   `construct` and `destruct` apart, as lowering takes it. NULL for any
   other function. */
static const struct type *synchronized_class(const struct symbol *sym)
{
    const struct item *it = sym->item;
    const struct type *t;

    if (it == NULL || it->kind != ITEM_FN || !it->has_self ||
        it->vis == VIS_PRIVATE || sema_name_is(&it->name, "construct") ||
        sema_name_is(&it->name, "destruct") || sym->type == NULL ||
        sym->type->kind != TYPE_FN || sym->type->param_count == 0) {
        return NULL;
    }
    t = sym->type->params[0];
    if (t->kind == TYPE_POINTER) {
        t = t->element;
    }
    return t->kind == TYPE_CLASS && t->safety == SAFETY_SYNCHRONIZED ? t
                                                                    : NULL;
}

static void add_direct(struct lock_walk *w, size_t lock)
{
    struct function *f = &w->fns[w->current];
    size_t i;

    for (i = 0; i < f->direct_count; i++) {
        if (f->direct[i] == lock) {
            return;
        }
    }
    f->direct = alloc_grow(f->direct, &f->direct_capacity, f->direct_count,
                           sizeof *f->direct);
    f->direct[f->direct_count++] = lock;
}

static void add_callee(struct lock_walk *w, size_t callee)
{
    struct function *f = &w->fns[w->current];
    size_t i;

    for (i = 0; i < f->callee_count; i++) {
        if (f->callees[i] == callee) {
            return;
        }
    }
    f->callees = alloc_grow(f->callees, &f->callee_capacity, f->callee_count,
                            sizeof *f->callees);
    f->callees[f->callee_count++] = callee;
}

static size_t pool_put(struct lock_walk *w, size_t value)
{
    w->pool = alloc_grow(w->pool, &w->pool_capacity, w->pool_count,
                         sizeof *w->pool);
    w->pool[w->pool_count] = value;
    return w->pool_count++;
}

/* Record a place at pos that takes locks while the walk holds some.
   Exactly one of lock, callee and the count of library locks says what
   it takes. */
static void add_site(struct lock_walk *w, struct pos pos, size_t lock,
                     size_t callee, size_t locks, size_t lock_count)
{
    struct site *s;
    size_t first = w->pool_count;
    size_t i;

    if (w->held_count == 0) {
        return;
    }
    for (i = 0; i < w->held_count; i++) {
        pool_put(w, w->held[i]);
    }
    w->sites = alloc_grow(w->sites, &w->site_capacity, w->site_count,
                          sizeof *w->sites);
    s = &w->sites[w->site_count++];
    s->pos = pos;
    s->module = w->fns[w->current].home;
    s->held = first;
    s->held_count = w->held_count;
    s->lock = lock;
    s->callee = callee;
    s->locks = locks;
    s->lock_count = lock_count;
}

static void take_lock(struct lock_walk *w, struct pos pos, size_t lock)
{
    if (lock == NO_INDEX) {
        return;
    }
    add_direct(w, lock);
    add_site(w, pos, lock, NO_INDEX, 0, 0);
}

static void hold(struct lock_walk *w, size_t lock)
{
    w->held = alloc_grow(w->held, &w->held_capacity, w->held_count,
                         sizeof *w->held);
    w->held[w->held_count++] = lock;
}

/* The module of the generic whose copy it is, or of the function an
   anonymous function is written in, as lowering names its source. NULL
   for a function of the module. */
static const char *home_of(const struct item *it)
{
    while (it->enclosing != NULL) {
        it = it->enclosing;
    }
    return it->home_module;
}

/* The function of the walk for it, made on its first use. */
static size_t function_of(struct lock_walk *w, const struct item *it,
                          bool anonymous)
{
    uint32_t index;
    struct function *f;

    if (ptr_index_get(&w->fn_index, it, &index)) {
        return index;
    }
    w->fns = alloc_grow(w->fns, &w->fn_capacity, w->fn_count, sizeof *w->fns);
    f = &w->fns[w->fn_count];
    memset(f, 0, sizeof *f);
    f->item = it;
    f->anonymous = anonymous;
    f->home = home_of(it);
    ptr_index_put(&w->fn_index, it, (uint32_t)w->fn_count);
    return w->fn_count++;
}

/* The locks a library function of the call of sym takes, as its library
   file records them. Returns whether the file has a record of it. */
static bool library_locks(struct lock_walk *w, const struct symbol *sym,
                          size_t *first, size_t *count)
{
    const struct lock_facts *facts =
        sym->home != NULL ? sym->home->locks : NULL;
    size_t i;
    size_t j;

    for (i = 0; facts != NULL && i < facts->function_count; i++) {
        const struct lock_function *f = &facts->functions[i];
        if (!same_text(&f->symbol, &sym->name)) {
            continue;
        }
        *first = w->pool_count;
        for (j = 0; j < f->count; j++) {
            pool_put(w, lock_id(w, &f->locks[j]));
        }
        *count = f->count;
        return true;
    }
    return false;
}

/* What the call e takes. */
static void take_call(struct lock_walk *w, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;
    const struct symbol *sym;
    const struct type *t;
    uint32_t index;
    size_t first;
    size_t count;
    size_t i;

    if (callee == NULL || callee->kind != EXPR_NAME) {
        return;
    }
    sym = callee->symbol;
    if (sym == NULL || sym->kind != SYMBOL_FN) {
        return;
    }
    if (e->as.call.dispatch == NULL && sym->item != NULL &&
        ptr_index_get(&w->fn_index, sym->item, &index)) {
        add_callee(w, index);
        add_site(w, e->pos, NO_INDEX, index, 0, 0);
        return;
    }
    if (e->as.call.dispatch == NULL && sym->home != NULL &&
        library_locks(w, sym, &first, &count)) {
        for (i = 0; i < count; i++) {
            add_direct(w, w->pool[first + i]);
        }
        add_site(w, e->pos, NO_INDEX, NO_INDEX, first, count);
        return;
    }
    t = synchronized_class(sym);
    if (t != NULL) {
        take_lock(w, e->pos, field_lock(w, t, &hidden_lock));
    }
}

static bool walk_expr(void *data, const struct expr *e);
static bool walk_stmt(void *data, const struct stmt *s);

static void walk_args(struct lock_walk *w, const struct expr *call)
{
    const struct ast_visitor v = {walk_expr, walk_stmt, w};
    size_t i;

    if (call == NULL || call->kind != EXPR_CALL) {
        return;
    }
    for (i = 0; i < call->as.call.arg_count; i++) {
        ast_walk_expr(&v, call->as.call.args[i]);
    }
}

static bool walk_expr(void *data, const struct expr *e)
{
    struct lock_walk *w = data;
    const struct ast_visitor v = {walk_expr, walk_stmt, w};

    switch (e->kind) {
    case EXPR_CALL:
        take_call(w, e);
        return true;
    case EXPR_FN:
        if (e->as.fn != NULL && e->as.fn->body != NULL) {
            function_of(w, e->as.fn, true);
        }
        return false;
    case EXPR_DISPATCH:
        ast_walk_expr(&v, e->as.dispatch.object);
        walk_args(w, e->as.dispatch.call);
        return false;
    case EXPR_PARALLEL:
        ast_walk_expr(&v, e->as.parallel.array);
        ast_walk_expr(&v, e->as.parallel.chunks);
        walk_args(w, e->as.parallel.call);
        return false;
    default:
        return true;
    }
}

static bool walk_stmt(void *data, const struct stmt *s)
{
    struct lock_walk *w = data;
    const struct ast_visitor v = {walk_expr, walk_stmt, w};
    size_t held = w->held_count;
    size_t first;
    size_t second = NO_INDEX;

    if (s->kind != STMT_SYNC) {
        return true;
    }
    ast_walk_expr(&v, s->as.sync.mutex);
    ast_walk_expr(&v, s->as.sync.second);
    if (s->as.sync.second != NULL) {
        first = object_lock(w, s->as.sync.mutex);
        second = object_lock(w, s->as.sync.second);
        take_lock(w, s->pos, first);
        take_lock(w, s->pos, second);
    } else {
        first = s->as.sync.object ? object_lock(w, s->as.sync.mutex)
                                  : mutex_lock(w, s->as.sync.mutex);
        take_lock(w, s->pos, first);
    }
    if (first != NO_INDEX) {
        hold(w, first);
    }
    if (second != NO_INDEX) {
        hold(w, second);
    }
    ast_walk_block(&v, s->as.sync.body);
    w->held_count = held;
    return false;
}

/* Whether it or a class around it has type parameters, so its body is
   no code of the program and only its copies are. */
static bool generic_item(const struct item *it)
{
    const struct item *o;

    for (o = it; o != NULL; o = o->outer) {
        if (o->type_param_count > 0) {
            return true;
        }
    }
    return it->owner != NULL && it->owner->type_param_count > 0;
}

static void collect(struct lock_walk *w, const struct item *it)
{
    size_t i;

    if (generic_item(it)) {
        return;
    }
    if (it->kind == ITEM_FN && it->body != NULL && it->symbol != NULL &&
        !sema_is_error(it->symbol->type)) {
        function_of(w, it, false);
    }
    for (i = 0; i < it->member_count; i++) {
        collect(w, it->members[i]);
    }
}

static void read_function(struct lock_walk *w, size_t index)
{
    const struct ast_visitor v = {walk_expr, walk_stmt, w};
    const struct item *it = w->fns[index].item;
    const struct type *t =
        it->symbol != NULL ? synchronized_class(it->symbol) : NULL;

    w->current = index;
    w->held_count = 0;
    if (t != NULL) {
        size_t lock = field_lock(w, t, &hidden_lock);
        if (lock != NO_INDEX) {
            add_direct(w, lock);
            hold(w, lock);
        }
    }
    ast_walk_block(&v, it->body);
}

/* The locks each function takes, itself or through its callees, as a
   table of fn_count rows of lock_count flags. */
static bool *locks_taken(const struct lock_walk *w)
{
    bool *takes = alloc_zeroed(alloc_product(w->fn_count, w->lock_count),
                               sizeof *takes);
    bool changed = true;
    size_t f;
    size_t i;
    size_t l;

    for (f = 0; f < w->fn_count; f++) {
        for (i = 0; i < w->fns[f].direct_count; i++) {
            takes[f * w->lock_count + w->fns[f].direct[i]] = true;
        }
    }
    while (changed) {
        changed = false;
        for (f = 0; f < w->fn_count; f++) {
            for (i = 0; i < w->fns[f].callee_count; i++) {
                size_t callee = w->fns[f].callees[i];
                for (l = 0; l < w->lock_count; l++) {
                    if (takes[callee * w->lock_count + l] &&
                        !takes[f * w->lock_count + l]) {
                        takes[f * w->lock_count + l] = true;
                        changed = true;
                    }
                }
            }
        }
    }
    return takes;
}

struct graph {
    struct graph_edge *edges;
    size_t count;
    size_t capacity;
    bool *added;
};

static void graph_put(struct graph *g, size_t held, size_t taken,
                      struct pos pos, const char *module)
{
    g->edges = alloc_grow(g->edges, &g->capacity, g->count, sizeof *g->edges);
    g->edges[g->count].held = held;
    g->edges[g->count].taken = taken;
    g->edges[g->count].pos = pos;
    g->edges[g->count].module = module;
    g->count++;
}

/* Add to g the edges of every site: from each lock it holds to each lock
   it takes that it does not hold already. */
static void site_edges(const struct lock_walk *w, const bool *takes,
                       struct graph *g)
{
    size_t s;
    size_t h;
    size_t l;

    for (s = 0; s < w->site_count; s++) {
        const struct site *site = &w->sites[s];
        const size_t *held = &w->pool[site->held];
        for (l = 0; l < w->lock_count; l++) {
            bool taken;
            bool held_already = false;
            if (site->lock != NO_INDEX) {
                taken = site->lock == l;
            } else if (site->callee != NO_INDEX) {
                taken = takes[site->callee * w->lock_count + l];
            } else {
                size_t i;
                taken = false;
                for (i = 0; i < site->lock_count; i++) {
                    taken = taken || w->pool[site->locks + i] == l;
                }
            }
            for (h = 0; h < site->held_count; h++) {
                held_already = held_already || held[h] == l;
            }
            if (!taken || held_already) {
                continue;
            }
            for (h = 0; h < site->held_count; h++) {
                size_t k;
                bool twice = false;
                for (k = 0; k < h; k++) {
                    twice = twice || held[k] == held[h];
                }
                if (!twice) {
                    graph_put(g, held[h], l, site->pos, site->module);
                }
            }
        }
    }
}

static int pos_order(struct pos a, struct pos b)
{
    if (a.line != b.line) {
        return a.line < b.line ? -1 : 1;
    }
    return a.column < b.column ? -1 : a.column > b.column;
}

static int edge_order(const void *pa, const void *pb)
{
    const struct graph_edge *a = pa;
    const struct graph_edge *b = pb;
    int by_module;

    if ((a->module == NULL) != (b->module == NULL)) {
        return a->module != NULL ? -1 : 1;
    }
    by_module = a->module != NULL ? strcmp(a->module, b->module) : 0;
    if (by_module != 0) {
        return by_module;
    }
    if (pos_order(a->pos, b->pos) != 0) {
        return pos_order(a->pos, b->pos);
    }
    if (a->held != b->held) {
        return a->held < b->held ? -1 : 1;
    }
    return a->taken < b->taken ? -1 : a->taken > b->taken;
}

/* The edges of the graph that lead from from back to to, through the
   edges added so far, as indices of g in order, into path. Returns
   their count, 0 when no such path stands. */
static size_t find_path(const struct graph *g, size_t lock_count,
                        size_t from, size_t to, size_t *path)
{
    size_t *via = alloc_zeroed(lock_count, sizeof *via);
    size_t *queue = alloc_zeroed(lock_count, sizeof *queue);
    bool *seen = alloc_zeroed(lock_count, sizeof *seen);
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t at;
    size_t e;

    seen[from] = true;
    queue[tail++] = from;
    while (head < tail && !seen[to]) {
        at = queue[head++];
        for (e = 0; e < g->count; e++) {
            if (g->added[e] && g->edges[e].held == at &&
                !seen[g->edges[e].taken]) {
                seen[g->edges[e].taken] = true;
                via[g->edges[e].taken] = e;
                queue[tail++] = g->edges[e].taken;
            }
        }
    }
    if (seen[to] && from != to) {
        for (at = to; at != from; at = g->edges[via[at]].held) {
            count++;
        }
        e = count;
        for (at = to; at != from; at = g->edges[via[at]].held) {
            path[--e] = via[at];
        }
    }
    free(via);
    free(queue);
    free(seen);
    return count;
}

/* Write the name of lock as a message names it into out: `Type` for the
   hidden lock of a synchronized class, `Type.field` for a Mutex. */
static void lock_text(char *out, size_t size, const struct lock_name *lock)
{
    if (same_text(&lock->field, &hidden_lock)) {
        text_format_name(out, size, "`%.*s`", (int)lock->type.length,
                         lock->type.text);
    } else {
        text_format_name(out, size, "`%.*s.%.*s`", (int)lock->type.length,
                         lock->type.text, (int)lock->field.length,
                         lock->field.text);
    }
}

/* Write where edge stands into out: its line, and its module when it
   is an edge of a library. */
static void place_text(char *out, size_t size, const struct graph_edge *e)
{
    if (e->module == NULL) {
        text_format_name(out, size, "line %d", e->pos.line);
    } else {
        text_format_name(out, size, "line %d of `%s`", e->pos.line,
                         e->module);
    }
}

/* Refuse the edge e of the graph, which the path of count edges closes
   into a cycle, at pos. */
static void refuse_cycle(struct lock_walk *w, const struct graph *g,
                         const struct graph_edge *e, const size_t *path,
                         size_t count, struct pos pos)
{
    const struct graph_edge *next = &g->edges[path[0]];
    char taken[LOCK_TEXT];
    char held[LOCK_TEXT];
    char other[LOCK_TEXT];
    char here[LOCK_TEXT];
    char there[LOCK_TEXT];

    lock_text(taken, sizeof taken, &w->locks[e->taken]);
    lock_text(held, sizeof held, &w->locks[e->held]);
    lock_text(other, sizeof other, &w->locks[next->taken]);
    if (e->module == NULL) {
        text_format_name(here, sizeof here, "here");
    } else {
        text_format_name(here, sizeof here, "on line %d of `%s`", e->pos.line,
                         e->module);
    }
    place_text(there, sizeof there, next);
    if (count == 1) {
        sema_check_at(w->c, NAME_LOCK_ORDER, pos,
                      "%s is locked %s while %s is held, and %s locks them "
                      "in the other order",
                      taken, here, held, there);
    } else {
        sema_check_at(w->c, NAME_LOCK_ORDER, pos,
                      "%s is locked %s while %s is held, and %s locks %s "
                      "while %s is held, on a cycle of %zu locks",
                      taken, here, held, there, other, taken, count + 1);
    }
}

/* The position of `main`, where a cycle of the libraries alone stands,
   or NULL in a library. */
static const struct item *main_of(const struct checker *c)
{
    size_t i;

    for (i = 0; c->program && i < c->module->item_count; i++) {
        const struct item *it = c->module->items[i];
        if (it->kind == ITEM_FN && it->owner == NULL &&
            sema_name_is(&it->name, "main")) {
            return it;
        }
    }
    return NULL;
}

/* Add the edges of the libraries to g, each after the module name it
   belongs to. */
static void library_edges(struct lock_walk *w, struct graph *g)
{
    size_t i;
    size_t j;

    for (i = 0; i < w->c->library_count; i++) {
        const struct interface *lib = w->c->libraries[i];
        if (lib->locks == NULL) {
            continue;
        }
        for (j = 0; j < lib->locks->edge_count; j++) {
            const struct lock_edge *e = &lib->locks->edges[j];
            struct pos pos;
            pos.line = e->line;
            pos.column = e->column;
            graph_put(g, lock_id(w, &e->held), lock_id(w, &e->taken), pos,
                      lib->module);
        }
    }
}

/* Whether the graph kept an edge before e from the same lock to the
   same lock. The kept edges of the module close no cycle, so another
   such edge closes none either. */
static bool kept_before(const struct graph *g, size_t e)
{
    size_t k;

    for (k = 0; k < e; k++) {
        if (g->added[k] && g->edges[k].held == g->edges[e].held &&
            g->edges[k].taken == g->edges[e].taken) {
            return true;
        }
    }
    return false;
}

/* Add each edge of g in order and refuse one that closes a cycle. An
   edge of a library that closes one with the libraries alone stands at
   `main` of a program, and in a library it is added unrefused. */
static void find_cycles(struct lock_walk *w, struct graph *g)
{
    const struct item *main_fn = main_of(w->c);
    size_t *path = alloc_zeroed(w->lock_count, sizeof *path);
    size_t e;

    g->added = alloc_zeroed(g->count, sizeof *g->added);
    for (e = 0; e < g->count; e++) {
        const struct graph_edge *edge = &g->edges[e];
        size_t count;
        if (edge->held == edge->taken) {
            continue;
        }
        if (kept_before(g, e)) {
            g->added[e] = true;
            continue;
        }
        count = find_path(g, w->lock_count, edge->taken, edge->held, path);
        if (count == 0 || (edge->module != NULL && main_fn == NULL)) {
            g->added[e] = true;
            continue;
        }
        refuse_cycle(w, g, edge, path, count,
                     edge->module == NULL ? edge->pos : main_fn->pos);
    }
    free(path);
}

static struct lock_name *keep_locks(struct checker *c, const bool *row,
                                    const struct lock_walk *w, size_t *count)
{
    struct lock_name *out;
    size_t l;

    *count = 0;
    for (l = 0; l < w->lock_count; l++) {
        *count += row[l] ? 1u : 0u;
    }
    out = arena_alloc(c->arena, alloc_product(*count + 1, sizeof *out));
    *count = 0;
    for (l = 0; l < w->lock_count; l++) {
        if (row[l]) {
            out[(*count)++] = w->locks[l];
        }
    }
    return out;
}

/* Whether the graph kept an edge of the module before e from the same
   lock to the same lock, which the library file carries in its place. */
static bool own_before(const struct graph *g, size_t e)
{
    size_t k;

    for (k = 0; k < e; k++) {
        if (g->added[k] && g->edges[k].module == NULL &&
            g->edges[k].held == g->edges[e].held &&
            g->edges[k].taken == g->edges[e].taken) {
            return true;
        }
    }
    return false;
}

/* What the module keeps for the modules that import it: the locks of
   each named function that takes any, and the edges of the module that
   the graph kept. */
static void keep_facts(struct lock_walk *w, const bool *takes,
                       const struct graph *g)
{
    struct checker *c = w->c;
    struct lock_facts *facts = arena_alloc(c->arena, sizeof *facts);
    size_t f;
    size_t e;

    facts->functions = arena_alloc(
        c->arena, alloc_product(w->fn_count + 1, sizeof *facts->functions));
    for (f = 0; f < w->fn_count; f++) {
        struct lock_function *out = &facts->functions[facts->function_count];
        if (w->fns[f].anonymous || w->fns[f].item->symbol == NULL) {
            continue;
        }
        out->locks = keep_locks(c, &takes[f * w->lock_count], w, &out->count);
        if (out->count > 0) {
            out->symbol = w->fns[f].item->symbol->name;
            facts->function_count++;
        }
    }
    facts->edges = arena_alloc(
        c->arena, alloc_product(g->count + 1, sizeof *facts->edges));
    for (e = 0; e < g->count; e++) {
        const struct graph_edge *edge = &g->edges[e];
        struct lock_edge *out = &facts->edges[facts->edge_count];
        if (edge->module != NULL || !g->added[e] || own_before(g, e)) {
            continue;
        }
        out->held = w->locks[edge->held];
        out->taken = w->locks[edge->taken];
        out->line = edge->pos.line;
        out->column = edge->pos.column;
        facts->edge_count++;
    }
    if (facts->function_count > 0 || facts->edge_count > 0) {
        c->module->locks = facts;
    }
}

/* DESIGN: an edge of the module that an `unchecked(lock-order)` covers
   is refused all the same, so the clause counts as used, and
   warnings_apply drops the refusal. The edge stays out of the graph,
   as every refused edge does, and so out of the library file. */
void sema_check_lock_order(struct checker *c)
{
    struct lock_walk w;
    struct graph g;
    bool *takes;
    size_t i;

    memset(&w, 0, sizeof w);
    memset(&g, 0, sizeof g);
    w.c = c;
    for (i = 0; i < c->module->item_count; i++) {
        collect(&w, c->module->items[i]);
    }
    for (i = 0; i < w.fn_count; i++) {
        read_function(&w, i);
    }
    /* The edges of the libraries name their locks first, so the table
       of the locks each function takes has a column for every lock. */
    library_edges(&w, &g);
    takes = locks_taken(&w);
    site_edges(&w, takes, &g);
    if (g.count > 0) {
        qsort(g.edges, g.count, sizeof *g.edges, edge_order);
    }
    find_cycles(&w, &g);
    keep_facts(&w, takes, &g);
    free(takes);
    free(g.edges);
    free(g.added);
    for (i = 0; i < w.fn_count; i++) {
        free(w.fns[i].direct);
        free(w.fns[i].callees);
    }
    free(w.fns);
    free(w.locks);
    free(w.sites);
    free(w.pool);
    free(w.held);
    ptr_index_free(&w.fn_index);
}
