/* The checks of statements and function bodies: narrowing, assignment,
   `switch`, `sync`, the fields a `construct` sets, and the walk over
   every function a worker reaches. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

struct worker_walk;

static void walk_function(struct worker_walk *w, const struct item *it);

static size_t ptr_slot(const void *p, size_t capacity)
{
    uint64_t h = (uint64_t)(uintptr_t)p;

    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (size_t)(h & (capacity - 1));
}

/* Add p to s. True when p was not in s before. */
bool sema_ptr_set_add(struct ptr_set *s, const void *p)
{
    size_t i;

    if ((s->count + 1) * 2 > s->capacity) {
        size_t capacity = s->capacity == 0 ? 64 : s->capacity * 2;
        const void **slots = calloc(capacity, sizeof *slots);
        if (slots == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        for (i = 0; i < s->capacity; i++) {
            if (s->slots[i] != NULL) {
                size_t at = ptr_slot(s->slots[i], capacity);
                while (slots[at] != NULL) {
                    at = (at + 1) & (capacity - 1);
                }
                slots[at] = s->slots[i];
            }
        }
        free(s->slots);
        s->slots = slots;
        s->capacity = capacity;
    }
    for (i = ptr_slot(p, s->capacity); s->slots[i] != NULL;
         i = (i + 1) & (s->capacity - 1)) {
        if (s->slots[i] == p) {
            return false;
        }
    }
    s->slots[i] = p;
    s->count++;
    return true;
}

/* DESIGN: analysis that only reports runs over every function a worker
   can reach. A worker may not `delete` its object, and it may not touch
   a `mutable` field of a singleton, because another worker may hold the
   same one. The walk is over the checked tree of this compilation. */

/* DESIGN: the walk keeps the functions still to walk in a queue and not
   on the stack. A chain of calls of any length then costs no depth. A
   function is walked once per worker, after the body that calls it. */
struct worker_walk {
    struct checker *c;
    const struct item *worker;      /* the worker the path started at */
    struct ptr_set seen;
    const struct item **queue;      /* every function seen, in order */
    size_t queue_count;
    size_t queue_capacity;
};

static void walk_block(struct worker_walk *w, const struct block *b);

static void walk_expr(struct worker_walk *w, const struct expr *e)
{
    size_t i;

    if (e == NULL) {
        return;
    }
    switch (e->kind) {
    case EXPR_OBJECT:
        if (e->as.object.op == TOKEN_DELETE) {
            sema_error_at(w->c, e->pos, "`%.*s` is a `worker fn` and cannot "
                          "`delete` an object another one may hold",
                          (int)w->worker->name.length, w->worker->name.text);
        }
        walk_expr(w, e->as.object.operand);
        walk_expr(w, e->as.object.from);
        return;
    case EXPR_FIELD: {
        const struct type *owner =
            e->as.field.base != NULL && e->as.field.base->type != NULL
                ? sema_struct_of(e->as.field.base->type)
                : NULL;
        const struct struct_field *f =
            owner != NULL ? sema_find_field(owner, &e->as.field.name) : NULL;
        if (f != NULL && f->writable && sema_singleton_type(owner)) {
            sema_error_at(w->c, e->pos,
                          "`%.*s` is `mutable` in singleton `%s` and "
                          "`worker fn %.*s` reaches it",
                          (int)e->as.field.name.length, e->as.field.name.text,
                          sema_tn(owner), (int)w->worker->name.length,
                          w->worker->name.text);
        }
        walk_expr(w, e->as.field.base);
        return;
    }
    case EXPR_CALL:
        walk_expr(w, e->as.call.callee);
        for (i = 0; i < e->as.call.arg_count; i++) {
            walk_expr(w, e->as.call.args[i]);
        }
        if (e->as.call.handler.kind == HANDLE_BLOCK) {
            walk_block(w, e->as.call.handler.body);
        }
        if (e->as.call.callee != NULL && e->as.call.callee->symbol != NULL) {
            walk_function(w, e->as.call.callee->symbol->item);
        }
        return;
    case EXPR_UNARY:
        walk_expr(w, e->as.unary.operand);
        return;
    case EXPR_BINARY:
        walk_expr(w, e->as.binary.left);
        walk_expr(w, e->as.binary.right);
        return;
    case EXPR_INDEX:
        walk_expr(w, e->as.index.base);
        walk_expr(w, e->as.index.index);
        return;
    case EXPR_CAST:
        walk_expr(w, e->as.cast.operand);
        return;
    case EXPR_ALLOC:
        walk_expr(w, e->as.alloc.value);
        walk_expr(w, e->as.alloc.count);
        return;
    case EXPR_FREE:
        walk_expr(w, e->as.free_pointer);
        return;
    case EXPR_FORMAT:
        for (i = 0; i < e->as.format.count; i++) {
            walk_expr(w, e->as.format.parts[i].value);
            walk_expr(w, e->as.format.parts[i].value_call);
        }
        return;
    case EXPR_COLLECT:
        walk_expr(w, e->as.collect.start);
        walk_expr(w, e->as.collect.advance);
        walk_expr(w, e->as.collect.current);
        return;
    /* The test holds both bounds and any `lt` it calls. */
    case EXPR_IN:
        walk_expr(w, e->as.in.value);
        walk_expr(w, e->as.in.test);
        return;
    case EXPR_OPTIONAL:
        walk_expr(w, e->as.optional.base);
        walk_expr(w, e->as.optional.access);
        return;
    /* A channel is shared between workers as an object is, so a worker
       deletes neither. */
    case EXPR_SYNC_OP:
        if (e->as.sync_op.op == SYNC_CHAN_DELETE) {
            sema_error_at(w->c, e->pos, "`%.*s` is a `worker fn` and cannot "
                          "`delete` an object another one may hold",
                          (int)w->worker->name.length, w->worker->name.text);
        }
        walk_expr(w, e->as.sync_op.target);
        walk_expr(w, e->as.sync_op.value);
        return;
    case EXPR_SIMD:
        for (i = 0; i < e->as.simd.arg_count; i++) {
            walk_expr(w, e->as.simd.args[i]);
        }
        return;
    default:
        return;
    }
}

static void walk_stmt(struct worker_walk *w, const struct stmt *s)
{
    size_t i;

    if (s == NULL) {
        return;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        walk_expr(w, s->as.let.value);
        return;
    case STMT_EXPR:
        walk_expr(w, s->as.expr);
        return;
    case STMT_ASSIGN:
        walk_expr(w, s->as.assign.target);
        walk_expr(w, s->as.assign.value);
        return;
    case STMT_IF:
        for (i = 0; i < s->as.if_chain.count; i++) {
            walk_expr(w, s->as.if_chain.branches[i].cond);
            walk_block(w, s->as.if_chain.branches[i].body);
        }
        walk_block(w, s->as.if_chain.else_body);
        return;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        walk_expr(w, s->as.loop.cond);
        walk_block(w, s->as.loop.body);
        return;
    case STMT_FOR:
        walk_expr(w, s->as.for_loop.hooks.start != NULL
                         ? s->as.for_loop.hooks.start
                         : s->as.for_loop.over);
        walk_expr(w, s->as.for_loop.hooks.advance);
        walk_expr(w, s->as.for_loop.hooks.current);
        walk_block(w, s->as.for_loop.body);
        return;
    case STMT_DEFER:
    case STMT_UNDO:
        walk_stmt(w, s->as.deferred);
        return;
    case STMT_FAIL:
        walk_expr(w, s->as.fail.value);
        return;
    case STMT_RETURN:
        walk_expr(w, s->as.return_value);
        return;
    case STMT_YIELD:
        walk_expr(w, s->as.yielded);
        return;
    case STMT_TRY:
        walk_block(w, s->as.try_block.body);
        walk_block(w, s->as.try_block.handler.body);
        return;
    case STMT_BLOCK:
        walk_block(w, s->as.block);
        return;
    case STMT_SWITCH:
        walk_expr(w, s->as.switch_stmt.value);
        for (i = 0; i < s->as.switch_stmt.count; i++) {
            walk_stmt(w, s->as.switch_stmt.arms[i].body);
        }
        return;
    case STMT_SYNC:
        walk_expr(w, s->as.sync.mutex);
        walk_block(w, s->as.sync.body);
        return;
    case STMT_SELECT:
        for (i = 0; i < s->as.select.count; i++) {
            walk_expr(w, s->as.select.arms[i].value);
            walk_stmt(w, s->as.select.arms[i].body);
        }
        return;
    default:
        return;
    }
}

static void walk_block(struct worker_walk *w, const struct block *b)
{
    size_t i;

    for (i = 0; b != NULL && i < b->count; i++) {
        walk_stmt(w, b->stmts[i]);
    }
}

/* Queue it to be walked, once. */
static void walk_function(struct worker_walk *w, const struct item *it)
{
    if (it == NULL || it->kind != ITEM_FN || it->body == NULL ||
        !sema_ptr_set_add(&w->seen, it)) {
        return;
    }
    if (w->queue_count == w->queue_capacity) {
        size_t capacity = w->queue_capacity == 0 ? 16 : w->queue_capacity * 2;
        const struct item **queue =
            capacity <= SIZE_MAX / sizeof *queue
                ? realloc(w->queue, capacity * sizeof *queue)
                : NULL;
        if (queue == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        w->queue = queue;
        w->queue_capacity = capacity;
    }
    w->queue[w->queue_count++] = it;
}

/* Walk worker and every function it reaches. */
void sema_walk_worker(struct checker *c, const struct item *worker)
{
    struct worker_walk w;
    size_t next;

    memset(&w, 0, sizeof w);
    w.c = c;
    w.worker = worker;
    walk_function(&w, worker);
    for (next = 0; next < w.queue_count; next++) {
        walk_block(&w, w.queue[next]->body);
    }
    free(w.queue);
    free(w.seen.slots);
}

/* Whether some class of the program inherits t or implements it. */
bool sema_fills(const struct type *t, const struct type *abstract)
{
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS || t == abstract) {
        return false;
    }
    if (sema_descends_from(t, abstract) || sema_implemented_in(t, abstract)) {
        return true;
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].form == FIELD_IMPL &&
            sema_descends_from(t->fields[i].type, abstract)) {
            return true;
        }
    }
    return false;
}

bool sema_filled_somewhere(const struct checker *c, const struct type *t)
{
    size_t i;
    size_t j;

    for (i = 0; i < c->module->item_count; i++) {
        const struct item *it = c->module->items[i];
        if (it->kind == ITEM_CLASS && it->symbol != NULL && !it->is_abstract &&
            sema_fills(it->symbol->type, t)) {
            return true;
        }
    }
    for (i = 0; i < c->library_count; i++) {
        const struct interface *lib = c->libraries[i];
        for (j = 0; j < lib->item_count; j++) {
            const struct symbol *sym = lib->items[j];
            if (sym->kind == SYMBOL_STRUCT && sema_fills(sym->type, t)) {
                return true;
            }
        }
    }
    return false;
}

static bool block_returns(const struct block *b);

/* The missing-return rule of chapter 2 decides from the form alone. */
static bool stmt_returns(const struct stmt *s)
{
    size_t i;

    if (s->kind == STMT_RETURN || s->kind == STMT_FAIL) {
        return true;
    }
    /* A `sync` block returns when its last statement does. */
    if (s->kind == STMT_SYNC) {
        return block_returns(s->as.sync.body);
    }
    /* `if let` is an `if` with an `else`, whose two blocks are the arm
       and the `else` of its switch. */
    if (s->kind == STMT_SWITCH && s->as.switch_stmt.if_let) {
        return block_returns(s->as.switch_stmt.arms[0].body->as.block) &&
               block_returns(s->as.switch_stmt.otherwise->as.block);
    }
    if (s->kind == STMT_IF && s->as.if_chain.else_body != NULL) {
        for (i = 0; i < s->as.if_chain.count; i++) {
            if (!block_returns(s->as.if_chain.branches[i].body)) {
                return false;
            }
        }
        return block_returns(s->as.if_chain.else_body);
    }
    return false;
}

static bool block_returns(const struct block *b)
{
    return b->count > 0 && stmt_returns(b->stmts[b->count - 1]);
}

static bool block_leaves(const struct block *b);

/* DESIGN: whether s hands control out of the block it stands in, which
   is what `if p == none { return; }` and the `else` of a `let` need. It
   reads the form alone, as the missing-return rule does. `break` and
   `continue` leave the block as surely as `return` does, and `yield`
   ends the handler it stands in. */
static bool stmt_leaves(const struct stmt *s)
{
    size_t i;

    switch (s->kind) {
    case STMT_RETURN:
    case STMT_FAIL:
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_YIELD:
        return true;
    case STMT_BLOCK:
        return block_leaves(s->as.block);
    case STMT_SYNC:
        return block_leaves(s->as.sync.body);
    case STMT_SWITCH:
        return s->as.switch_stmt.if_let &&
               block_leaves(s->as.switch_stmt.arms[0].body->as.block) &&
               block_leaves(s->as.switch_stmt.otherwise->as.block);
    case STMT_IF:
        if (s->as.if_chain.else_body == NULL) {
            return false;
        }
        for (i = 0; i < s->as.if_chain.count; i++) {
            if (!block_leaves(s->as.if_chain.branches[i].body)) {
                return false;
            }
        }
        return block_leaves(s->as.if_chain.else_body);
    default:
        return false;
    }
}

static bool block_leaves(const struct block *b)
{
    return b->count > 0 && stmt_leaves(b->stmts[b->count - 1]);
}

static void check_condition(struct checker *c, struct expr *cond)
{
    struct type *t = sema_check_test(c, cond);

    if (!sema_is_error(t) && t->kind != TYPE_BOOL) {
        sema_error_at(c, cond->pos, "a condition has type `bool`, found `%s`",
                      sema_tn(t));
    }
}

/* DESIGN: a check narrows a name and nothing else. `p != none` proves
   something about the variable p, while `q.next != none` proves it about
   a field that any call in the block may write. The name is what the
   specification narrows, and a field keeps its declared type. */
static struct symbol *checked_against_none(const struct expr *cond,
                                           enum token_kind op)
{
    const struct expr *value;

    if (cond->kind != EXPR_BINARY || cond->as.binary.op != op) {
        return NULL;
    }
    if (cond->as.binary.left->kind == EXPR_NONE) {
        value = cond->as.binary.right;
    } else if (cond->as.binary.right->kind == EXPR_NONE) {
        value = cond->as.binary.left;
    } else {
        return NULL;
    }
    if (value->kind != EXPR_NAME || value->symbol == NULL ||
        !type_is_nullable(value->symbol->type)) {
        return NULL;
    }
    return (value->symbol->kind == SYMBOL_LOCAL ||
            value->symbol->kind == SYMBOL_PARAM)
               ? value->symbol
               : NULL;
}

/* DESIGN: the names cond proves hold a pointer, when it is true and when
   it is false. `a && b` proves both sides when it is true and neither
   when it is false, because either side may have failed. `a || b` is the
   mirror of it. That is why the narrowing of an `&&` chain reaches the
   body of the `if` and the narrowing of an `||` chain does not. */
size_t sema_proved_names(const struct expr *cond, bool want_true,
                         struct symbol **out, size_t count)
{
    struct symbol *one;
    size_t i;

    if (cond == NULL) {
        return count;
    }
    if (cond->kind == EXPR_UNARY && cond->as.unary.op == TOKEN_BANG) {
        return sema_proved_names(cond->as.unary.operand, !want_true, out,
                                 count);
    }
    if (cond->kind == EXPR_BINARY &&
        (cond->as.binary.op == TOKEN_AND_AND ||
         cond->as.binary.op == TOKEN_OR_OR)) {
        if ((cond->as.binary.op == TOKEN_AND_AND) != want_true) {
            return count;
        }
        count = sema_proved_names(cond->as.binary.left, want_true, out, count);
        return sema_proved_names(cond->as.binary.right, want_true, out, count);
    }
    one = checked_against_none(cond, want_true ? TOKEN_NE : TOKEN_EQ);
    if (one == NULL || count >= PROVED_MAX) {
        return count;
    }
    for (i = 0; i < count; i++) {
        if (out[i] == one) {
            return count;
        }
    }
    out[count++] = one;
    return count;
}

/* The `*T` that a check proved of sym. */
struct type *sema_proved_type(struct checker *c, const struct symbol *sym)
{
    return types_without_none(c->types, sym->type);
}

/* Check b with each of count names narrowed inside it, and nowhere
   else. */
static void check_block_narrowing(struct checker *c, struct block *b,
                                  struct symbol **proved, size_t count);

/* Whether a value of t owns memory: an `own` field anywhere in the chain
   of a class, or a class value held inline that does. An array holds its
   elements inline, and a `?T` its value, so one of them makes the array
   or the `?T` an owner too. */
bool sema_type_owns(const struct type *t)
{
    size_t i;

    while (t != NULL && (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL)) {
        t = t->element;
    }
    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if (f->owned || ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                             sema_type_owns(f->type))) {
                return true;
            }
        }
    }
    return false;
}

/* Whether e reads a value that already lives somewhere. A literal, a
   call and `*dup(p)` make a fresh one instead. */
bool sema_reads_existing(const struct expr *e)
{
    switch (e->kind) {
    case EXPR_NAME:
    case EXPR_FIELD:
    case EXPR_INDEX:
        return true;
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_STAR &&
               !(e->as.unary.operand->kind == EXPR_OBJECT &&
                 e->as.unary.operand->as.object.op == TOKEN_DUP);
    default:
        return false;
    }
}

/* DESIGN: an `own` parameter owns its value, so `=` and `let` move one
   that owns memory rather than copy it, and the function tears it down
   no more. A value of a type parameter may own memory in a copy, so it
   moves as well. into names the place. Returns whether value moved. */
static bool moves_own_param(struct checker *c, struct expr *value,
                            struct name into)
{
    static const struct name no_name = {"", 0};
    struct symbol *sym = value->kind == EXPR_NAME ? value->symbol : NULL;

    if (sym == NULL || !sym->own_param || value->type == NULL ||
        sema_is_error(value->type) ||
        !(sema_type_owns(value->type) || sema_has_params(value->type))) {
        return false;
    }
    sema_move_local(c, value, &into, &no_name);
    return true;
}

/* DESIGN: `=` refuses to copy an existing value that owns memory, since
   the bytes would give it two owners. A fresh value on the right has no
   other owner, so `=` moves it. The bytes it replaces are not destroyed:
   the element of an `alloc(T, n)` it fills has no value yet. */
void sema_refuse_owned_copy(struct checker *c, const struct expr *value,
                            struct type *t)
{
    if (!sema_is_error(t) && sema_type_owns(t) && sema_reads_existing(value)) {
        sema_error_at(c, value->pos, "`%s` has `own` fields, use `dup` instead "
                      "of `=`", sema_tn(t));
    }
    sema_refuse_lock_copy(c, value, t);
}

/* Whether a value of t holds a Mutex. It does when it is one, or a
   struct, a class, a tuple, a variant or an array with one inside it. A
   pointer holds none. */
bool sema_holds_mutex(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    if (types_is_mutex(t)) {
        return true;
    }
    if (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        return sema_holds_mutex(t->element);
    }
    if (t->kind != TYPE_STRUCT && t->kind != TYPE_CLASS &&
        t->kind != TYPE_TUPLE && t->kind != TYPE_VARIANT) {
        return false;
    }
    for (i = 0; i < t->field_count; i++) {
        if (sema_holds_mutex(t->fields[i].type)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a Mutex is the lock word itself, so a copy would be a second
   lock that guards nothing. A value that holds one is never copied out
   of the place it lives in: not by `=`, not into a parameter and not by
   `return`. A fresh value, a literal, a call or `Mutex.new()`, moves. */
void sema_refuse_lock_copy(struct checker *c, const struct expr *value,
                           const struct type *t)
{
    if (!sema_is_error(t) && sema_holds_mutex(t) &&
        sema_reads_existing(value)) {
        if (types_is_mutex(t)) {
            sema_error_at(c, value->pos, "a `" LANG_MUTEX "` cannot be "
                          "copied, pass a pointer to it");
        } else {
            sema_error_at(c, value->pos, "`%s` holds a `" LANG_MUTEX "` and "
                          "cannot be copied, pass a pointer to it",
                          sema_tn(t));
        }
    }
}

static void check_flags_assign(struct checker *c, struct stmt *s);

/* DESIGN: `e[i] = v` on a type with the `index` or the `set_index` hook
   is the call `e.set_index(i, v)`, and the statement becomes that call.
   A type with `index` alone is read-only through `e[i]`. A compound
   assignment would compute e and i once for the read and once for the
   write, so it is refused, and `e[i] = e[i] + v` says what runs. */
static bool check_set_index(struct checker *c, struct stmt *s)
{
    struct expr *target = s->as.assign.target;
    struct expr *base = target->as.index.base;
    struct expr *args[2];
    struct type *t = sema_check_expr(c, base, NULL);
    char spelling[OP_TEXT];

    if (!sema_is_error(t) && t->kind == TYPE_PARAM) {
        if (!sema_is_error(sema_param_index(c, target, t, true)) &&
            s->as.assign.op != TOKEN_ASSIGN) {
            const char *o = sema_op_text(s->as.assign.op, spelling);
            sema_error_at(c, s->pos, "`%s` takes no `operator fn set_index`, "
                          "and `e[i] = e[i] %.*s v` writes it", o,
                          (int)strlen(o) - 1, o);
        }
        sema_check_expr(c, s->as.assign.value, NULL);
        return true;
    }
    if (sema_is_error(t) || (sema_hook(c, t, LANG_HOOK_INDEX) == NULL &&
                             sema_hook(c, t, LANG_HOOK_SET_INDEX) == NULL)) {
        return false;
    }
    if (sema_hook(c, t, LANG_HOOK_SET_INDEX) == NULL) {
        sema_error_at(c, target->pos, "`%s` has no `operator fn set_index`, "
                      "which `e[i] = v` calls", sema_tn(t));
        sema_check_expr(c, s->as.assign.value, NULL);
        return true;
    }
    if (s->as.assign.op != TOKEN_ASSIGN) {
        /* Every compound spelling is its binary operator and `=`. */
        const char *o = sema_op_text(s->as.assign.op, spelling);
        sema_error_at(c, s->pos, "`%s` takes no `operator fn set_index`, "
                      "and `e[i] = e[i] %.*s v` writes it", o,
                      (int)strlen(o) - 1, o);
        sema_check_expr(c, s->as.assign.value, NULL);
        return true;
    }
    args[0] = target->as.index.index;
    args[1] = s->as.assign.value;
    s->kind = STMT_EXPR;
    s->as.expr = sema_hook_call(c, base, LANG_HOOK_SET_INDEX, args, 2);
    sema_check_expr(c, s->as.expr, NULL);
    return true;
}

bool sema_set_index(struct checker *c, struct stmt *s)
{
    return check_set_index(c, s);
}

/* The captured variable declared deepest of the closure that the value e
   gives, or NULL when e gives none or one that captures nothing. */
static const struct symbol *held_deepest(const struct expr *e)
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
static void check_closure_lifetime(struct checker *c, const struct expr *target,
                                   const struct expr *value)
{
    const struct symbol *deepest = held_deepest(value);
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

/* Mark sym, the element variable of the `for` s over a sequence. It is
   a copy of each element in the form `for x in e`. It is lent for one
   turn in the form `for x in &e`. */
static void mark_walked(const struct stmt *s, struct symbol *sym)
{
    if (sym == NULL || s->as.for_loop.over == NULL) {
        return;
    }
    if (s->as.for_loop.by_pointer) {
        sym->lent_turn = true;
        return;
    }
    sym->copy_of.text = s->as.for_loop.over_text.bytes;
    sym->copy_of.length = s->as.for_loop.over_text.length;
}

/* DESIGN: the variable of `for x in e` is a copy of each element, so a
   change reaches the element only through a pointer the copy holds. This
   gives the variable whose copy alone the place target would change. The
   place is the variable itself, or a field, an element of an array or an
   element of a tuple held inline in it. NULL where the path passes a
   pointer or a slice, whose change reaches what they point at. A call of
   a function on the copy is not refused, since no signature says whether
   it changes `self`. */
static struct symbol *walked_copy(const struct expr *target)
{
    for (;;) {
        const struct expr *base;
        if (target->kind == EXPR_NAME) {
            return target->symbol != NULL &&
                           target->symbol->copy_of.length > 0
                       ? target->symbol
                       : NULL;
        }
        if (target->kind == EXPR_FIELD) {
            base = target->as.field.base;
        } else if (target->kind == EXPR_INDEX) {
            base = target->as.index.base;
            if (base->type == NULL || base->type->kind != TYPE_ARRAY) {
                return NULL;
            }
        } else {
            return NULL;
        }
        if (base->type == NULL || base->type->kind == TYPE_POINTER) {
            return NULL;
        }
        target = base;
    }
}

/* Refuse a change of the variable sym of a `for` at pos. */
static void refuse_read_only(struct checker *c, struct pos pos,
                             const struct symbol *sym)
{
    if (sym->copy_of.length > 0) {
        sema_error_at(c, pos, "`%.*s` is a copy of each element of `%.*s`. "
                      "Walk with `&%.*s` to change the elements",
                      (int)sym->name.length, sym->name.text,
                      (int)sym->copy_of.length, sym->copy_of.text,
                      (int)sym->copy_of.length, sym->copy_of.text);
        return;
    }
    sema_error_at(c, pos, "`%.*s` is the variable of a `for` and is read-only",
                  (int)sym->name.length, sym->name.text);
}

static void check_assign(struct checker *c, struct stmt *s)
{
    struct expr *target = s->as.assign.target;
    struct symbol *copy;
    struct type *t;
    struct type *v;
    enum token_kind op = s->as.assign.op;

    if (target->kind == EXPR_TUPLE) {
        check_flags_assign(c, s);
        return;
    }
    if (target->kind == EXPR_INDEX && check_set_index(c, s)) {
        return;
    }
    t = sema_check_storage(c, target);
    if (sema_is_error(t)) {
        sema_check_expr(c, s->as.assign.value, NULL);
        return;
    }
    /* A narrowed name is still a `?*T` variable, so an assignment to it
       takes the declared type and any pointer the program has. A `?T`
       narrowed to its value is the same, and `+=` and the other compound
       forms change the value it holds, which stays there. */
    if (target->kind == EXPR_NAME && target->symbol != NULL &&
        type_is_nullable(target->symbol->type) &&
        (op == TOKEN_ASSIGN || target->symbol->type->kind != TYPE_OPTIONAL)) {
        t = target->symbol->type;
        target->type = t;
    }
    /* A Mutex cannot be assigned, as it cannot be copied: the lock a
       thread holds would change under it. */
    if (sema_holds_mutex(t)) {
        if (types_is_mutex(t)) {
            sema_error_at(c, target->pos, "a `" LANG_MUTEX "` cannot be "
                          "assigned");
        } else {
            sema_error_at(c, target->pos, "`%s` holds a `" LANG_MUTEX "` and "
                          "cannot be assigned", sema_tn(t));
        }
        sema_check_expr(c, s->as.assign.value, NULL);
        return;
    }
    /* The variable of a `for` is read-only, so the loop keeps its step
       and the sequence it walks. A change that reaches the copy of an
       element alone is refused with it. */
    copy = walked_copy(target);
    if (copy != NULL || (target->kind == EXPR_NAME &&
                         target->symbol != NULL &&
                         target->symbol->read_only)) {
        refuse_read_only(c, target->pos,
                         copy != NULL ? copy : target->symbol);
        sema_check_expr(c, s->as.assign.value, NULL);
        return;
    }
    /* DESIGN: a field of a singleton is read-only after creation, so
       that a program has one place to look for the value. `construct`
       sets it, and `mutable` marks the fields the program may write
       anywhere. An atomic field has its own operations. */
    if (target->kind == EXPR_FIELD) {
        const struct type *owner = sema_struct_of(target->as.field.base->type);
        const struct struct_field *f =
            owner != NULL ? sema_find_field(owner,
                                            &target->as.field.name) : NULL;
        if (f != NULL && !f->writable && sema_singleton_type(owner) &&
            (c->function == NULL ||
             !sema_name_is(&c->function->name, "construct"))) {
            sema_error_at(c, target->pos,
                          "`%.*s` of singleton `%s` is read-only "
                          "after creation, and `mutable` marks a field the "
                          "program writes", (int)target->as.field.name.length,
                          target->as.field.name.text, sema_tn(owner));
            sema_check_expr(c, s->as.assign.value, NULL);
            return;
        }
    }
    if (target->kind == EXPR_FIELD &&
        sema_struct_of(target->as.field.base->type) != NULL &&
        sema_struct_of(target->as.field.base->type)->kind == TYPE_VARIANT) {
        sema_error_at(c, target->pos, "the `tag` of a variant is read-only");
        return;
    }
    if (!sema_is_place(target) ||
        (target->kind == EXPR_INDEX &&
         target->as.index.base->type->kind == TYPE_STR)) {
        sema_error_at(c, target->pos, "cannot assign to this expression");
        return;
    }
    if (op != TOKEN_ASSIGN && sema_refuses_half(c, s->pos, t)) {
        sema_check_expr(c, s->as.assign.value, NULL);
        return;
    }
    sema_note_write(c, target);
    v = sema_check_expr(c, s->as.assign.value, t);
    /* The value is checked first, so that `p = p.next` still reads the
       `p` the check proved. Then the narrowing ends: what it proved is
       about the value the assignment replaced. */
    if (target->kind == EXPR_NAME && target->symbol != NULL &&
        target->type == target->symbol->type) {
        sema_end_narrowing(c, target->symbol);
    }
    if (!sema_require(c, s->as.assign.value, v, t)) {
        return;
    }
    if (op == TOKEN_ASSIGN) {
        if (!moves_own_param(c, s->as.assign.value, sema_place_name(target))) {
            sema_refuse_owned_copy(c, s->as.assign.value, t);
        }
        check_closure_lifetime(c, target, s->as.assign.value);
        return;
    }
    if ((op == TOKEN_PLUS_ASSIGN || op == TOKEN_MINUS_ASSIGN ||
         op == TOKEN_STAR_ASSIGN || op == TOKEN_SLASH_ASSIGN)
            ? !type_is_numeric(t)
            : !type_is_integer(t)) {
        char spelling[OP_TEXT];
        sema_error_at(c, s->pos, "`%s` does not apply to `%s`",
                      sema_op_text(op, spelling), sema_tn(t));
    }
}

/* Every value of an enum needs an arm when a switch has no else. The
   message names the ones that have none. */
static void check_switch_covers(struct checker *c, const struct stmt *s,
                                const struct type *over)
{
    struct text missing = {0};
    size_t found = 0;
    size_t i;
    size_t j;

    for (i = 0; i < over->field_count; i++) {
        bool covered = false;
        for (j = 0; j < s->as.switch_stmt.count && !covered; j++) {
            struct const_value v;
            covered = sema_eval_const(c, s->as.switch_stmt.arms[j].value, &v) &&
                      v.as.integer == over->fields[i].number;
        }
        if (covered) {
            continue;
        }
        text_appendf(&missing, "%s`%.*s`", found++ > 0 ? ", " : "",
                     (int)over->fields[i].name.length,
                     over->fields[i].name.text);
    }
    if (found > 0) {
        sema_error_at(c, s->pos, "this `switch` on `%s` has no arm for %s",
                      sema_tn(over), text_cstr(&missing));
    }
    text_free(&missing);
}

/* DESIGN: an arm of a `switch` on a variant names one of its cases. It
   may bind the fields of that case to a name, which holds a copy of them
   in the arm alone. Returns whether the arm opened the scope of that
   name, which the caller closes after the body. */
static bool check_variant_arm(struct checker *c, struct stmt *s, size_t index,
                              const struct type *over, struct scope *scope)
{
    struct switch_arm *arm = &s->as.switch_stmt.arms[index];
    size_t found;
    size_t j;

    if (arm->value->kind != EXPR_NAME) {
        sema_error_at(c, arm->pos, "an arm of a `switch` on `%s` names one of "
                      "its cases", sema_tn(over));
        return false;
    }
    if (!types_case_index(over, &arm->value->as.name, &found)) {
        sema_error_at(c, arm->pos, "`%s` has no case `%.*s`", sema_tn(over),
                      (int)arm->value->as.name.length,
                      arm->value->as.name.text);
        return false;
    }
    for (j = 0; j < index; j++) {
        if ((size_t)s->as.switch_stmt.arms[j].variant_case == found + 1) {
            sema_error_at(c, arm->pos, "this case already has an arm");
            break;
        }
    }
    arm->variant_case = (uint32_t)(found + 1);
    arm->value->type = over->base;
    if (arm->binds.length == 0) {
        return false;
    }
    if (over->params[found] == NULL) {
        sema_error_at(c, arm->binds_pos, "case `%.*s` of `%s` has no fields to "
                      "bind", (int)arm->value->as.name.length,
                      arm->value->as.name.text, sema_tn(over));
        return false;
    }
    sema_enter_scope(c, scope);
    arm->bound = sema_declare(c, SYMBOL_LOCAL, &arm->binds, arm->binds_pos,
                              "`%.*s` is already declared in this block");
    if (arm->bound != NULL) {
        arm->bound->type = over->params[found];
    }
    return true;
}

/* The `fallthrough;` that ends an arm enters the arm at position next,
   which binds nothing. */
static void refuse_fallthrough_binding(struct checker *c, const struct stmt *s,
                                       size_t next)
{
    const struct switch_arm *arm;

    if (s->as.switch_stmt.otherwise != NULL &&
        next == s->as.switch_stmt.otherwise_at) {
        return;
    }
    if (s->as.switch_stmt.otherwise != NULL &&
        next > s->as.switch_stmt.otherwise_at) {
        next--;
    }
    arm = &s->as.switch_stmt.arms[next];
    if (arm->binds.length > 0) {
        sema_error_at(c, c->fallthrough->pos, "`fallthrough` into an arm that "
                      "binds the fields of `%.*s`",
                      (int)arm->value->as.name.length,
                      arm->value->as.name.text);
    }
}

/* A switch on a variant without `else` names every case, and the message
   names the ones it misses in the order of the declaration. */
static void check_cases_covered(struct checker *c, const struct stmt *s,
                                const struct type *over)
{
    struct text missing = {0};
    size_t found = 0;
    size_t i;
    size_t j;

    for (i = 0; i < over->param_count; i++) {
        for (j = 0; j < s->as.switch_stmt.count &&
                    (size_t)s->as.switch_stmt.arms[j].variant_case != i + 1;
             j++) {
        }
        if (j < s->as.switch_stmt.count) {
            continue;
        }
        text_appendf(&missing, "%s`%.*s`", found++ > 0 ? ", " : "",
                     (int)sema_case_name(over, i)->length,
                     sema_case_name(over, i)->text);
    }
    if (found > 0) {
        sema_error_at(c, s->pos, "this `switch` on `%s` has no arm for %s",
                      sema_tn(over), text_cstr(&missing));
    }
    text_free(&missing);
}

/* Whether two arms of a `switch` name one value. */
static bool same_arm_value(const struct const_value *a,
                           const struct const_value *b)
{
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == CONST_TEXT) {
        return a->as.text.length == b->as.text.length &&
               (a->as.text.length == 0 ||
                memcmp(a->as.text.bytes, b->as.text.bytes,
                       a->as.text.length) == 0);
    }
    return a->as.integer == b->as.integer;
}

/* The function `anti.text.equal`, which a `switch` on a `str` calls for
   each arm, or NULL after the message. */
static struct symbol *text_equal(struct checker *c, struct pos pos)
{
    static const struct name module = {TEXT_MODULE, sizeof TEXT_MODULE - 1};
    static const struct name name = {TEXT_EQUAL, sizeof TEXT_EQUAL - 1};
    const struct interface *lib = sema_find_library(c, &module);
    struct symbol *fn = lib != NULL ? sema_library_item(c, lib, &name) : NULL;
    struct type *str = sema_builtin(c, TYPE_STR);

    if (lib == NULL) {
        sema_error_at(c, pos,
                      "a `switch` on a `str` compares with `" TEXT_MODULE
                      "." TEXT_EQUAL "`, so the module imports `" TEXT_MODULE
                      "`");
        return NULL;
    }
    if (fn == NULL || fn->kind != SYMBOL_FN || fn->type == NULL ||
        fn->type->kind != TYPE_FN || fn->type->param_count != 2 ||
        fn->type->params[0] != str || fn->type->params[1] != str ||
        fn->type->result != sema_builtin(c, TYPE_BOOL)) {
        sema_error_at(c, pos, "a `switch` on a `str` calls `" TEXT_MODULE "."
                      TEXT_EQUAL "`, which this `" TEXT_MODULE "` lacks");
        return NULL;
    }
    return fn;
}

/* DESIGN: a `switch` on a `str` is a chain of calls of `text.equal`, one
   per arm in the order of the text, and not a table. The value is
   computed once into a local that no scope holds, and each arm's test
   reads it. */
static struct expr *equal_test(struct checker *c, struct symbol *equal,
                               struct symbol *bound, struct expr *value)
{
    struct expr *call = sema_new_node(c, EXPR_CALL, value->pos);
    struct expr *callee = sema_new_node(c, EXPR_NAME, value->pos);
    struct expr *over = sema_new_node(c, EXPR_NAME, value->pos);
    struct expr **args = arena_alloc(c->arena, 2 * sizeof *args);

    callee->symbol = equal;
    callee->type = equal->type;
    callee->as.name = equal->name;
    over->symbol = bound;
    over->type = bound->type;
    over->as.name = bound->name;
    args[0] = over;
    args[1] = value;
    call->as.call.callee = callee;
    call->as.call.args = args;
    call->as.call.arg_count = 2;
    call->type = equal->type->result;
    return call;
}

/* Whether e holds a call anywhere inside it. */
static bool expr_calls(const struct expr *e)
{
    size_t i;

    if (e == NULL) {
        return false;
    }
    switch (e->kind) {
    case EXPR_CALL:
    case EXPR_ALLOC:
    case EXPR_FREE:
    case EXPR_OBJECT:
    case EXPR_ATOMIC:
    case EXPR_PARALLEL:
    case EXPR_DISPATCH:
    case EXPR_JOIN:
    case EXPR_FORMAT:
    case EXPR_COLLECT:
    case EXPR_SYNC_OP:
    case EXPR_SIMD:
        return true;
    case EXPR_UNARY:
        return expr_calls(e->as.unary.operand);
    case EXPR_BINARY:
        return expr_calls(e->as.binary.left) ||
               expr_calls(e->as.binary.right);
    case EXPR_CAST:
        return expr_calls(e->as.cast.operand);
    case EXPR_IN:
        return expr_calls(e->as.in.value) || expr_calls(e->as.in.test);
    case EXPR_OPTIONAL:
        return expr_calls(e->as.optional.base) ||
               expr_calls(e->as.optional.access);
    case EXPR_FIELD:
        return expr_calls(e->as.field.base);
    case EXPR_INDEX:
        return expr_calls(e->as.index.base) || expr_calls(e->as.index.index);
    case EXPR_SLICE:
        return expr_calls(e->as.slice.base) || expr_calls(e->as.slice.low) ||
               expr_calls(e->as.slice.high);
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            if (expr_calls(e->as.struct_lit.fields[i].value)) {
                return true;
            }
        }
        return false;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            if (expr_calls(e->as.array_lit.elements[i])) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

/* Refuse a step of zero, which would never leave the range.
   https://foundingfuture.com/programming/writing-a-compiler/02-the-anti-language/#fn:heederik
   names the loop this check remembers. */
static void heederik_guardrail(struct checker *c, struct expr *step,
                               const struct const_value *v)
{
    if (v->as.integer == 0) {
        sema_error_at(c, step->pos, "`by 0` never advances");
    }
}

/* DESIGN: the step of a `for` range is a constant. The back end then
   knows which way the loop walks, and the checker can refuse a step of
   zero. A variable would hide both until the program hangs. */
static void check_step(struct checker *c, struct stmt *s, struct type *element)
{
    struct expr *step = s->as.for_loop.step;
    struct const_value v;
    struct type *type = sema_check_expr(c, step, element);

    if (sema_is_error(type)) {
        return;
    }
    if (!type_is_integer(type)) {
        sema_error_at(c, step->pos, "a `for` step is an integer, found `%s`",
                      sema_tn(type));
        return;
    }
    if (!sema_eval_const(c, step, &v)) {
        return;
    }
    s->as.for_loop.step_value = sema_signed_bits(v.as.integer);
    heederik_guardrail(c, step, &v);
}

/* Check the `catch` that guards a `?*T` in a `let`, and give the type
   the binding holds. The handler runs when the pointer is `none`, with
   an `anti.lang.NoneDereference` in hand, and it leaves the block or ends
   with `yield`, as every handler does. */
static struct type *check_pointer_guard(struct checker *c, struct stmt *s,
                                        struct type *value)
{
    struct handler *h = &s->as.let.guard;
    struct type *error;
    struct scope scope;
    struct type *outer_yield = c->yields;
    int outer_depth = c->handler_depth;

    if (sema_is_error(value)) {
        return value;
    }
    if (h->none) {
        sema_error_at(c, h->pos, "`catch none` counts a failure as `none`, "
                      "and a `catch` on a pointer guards no failure");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!type_is_nullable(value) || types_is_maybe_match(value)) {
        sema_error_at(c, h->pos, "`catch` here guards a `?*T`, found `%s`",
                      sema_tn(value));
        return sema_builtin(c, TYPE_ERROR);
    }
    if ((s->as.let.guard_make = sema_null_pointer_maker(c, h->pos)) == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    error = s->as.let.guard_make->type->result;
    if (h->kind != HANDLE_BLOCK) {
        return types_without_none(c->types, value);
    }
    sema_enter_scope(c, &scope);
    sema_declare_caught(c, h, error);
    c->yields = types_without_none(c->types, value);
    c->handler_depth++;
    sema_check_block(c, h->body);
    c->handler_depth = outer_depth;
    c->yields = outer_yield;
    sema_leave_scope(c, &scope);
    return types_without_none(c->types, value);
}

/* DESIGN: a `may fail` function writes what it computes through its out
   pointer and keeps `?*Error` for the error channel. `return` therefore
   names the declared result and not the result of the ABI. */
static struct type *declared_result(struct checker *c)
{
    const struct item *it = c->function;
    const struct type *t = it->symbol->type;

    if (!it->may_fail) {
        return t->result;
    }
    /* An anonymous function may take its result from the target, so the
       result is read from the out pointer of its type. */
    return t->has_out ? t->params[t->param_count - 1]->element
                      : sema_builtin(c, TYPE_VOID);
}

/* DESIGN: `let (a, b) = e;` and `for i, x in items` are one rule. The
   names take the elements of a tuple in order and each becomes a local
   of the type of its element. Nothing else destructures: not a parameter
   list, and not a name inside another pair of parentheses. read_only
   marks the names of a `for`, which its body may not write. */
static void bind_elements(struct checker *c, struct binding *names,
                          size_t count, struct type *t, struct pos pos,
                          bool read_only)
{
    size_t i;

    if (!sema_is_error(t) && t->kind != TYPE_TUPLE) {
        sema_error_at(c, pos, "a destructuring takes a tuple, found `%s`",
                      sema_tn(t));
        t = sema_builtin(c, TYPE_ERROR);
    } else if (!sema_is_error(t) && t->param_count != count) {
        sema_error_at(c, pos,
                      "`%s` has %d elements, and the destructuring names "
                      "%d", sema_tn(t), (int)t->param_count, (int)count);
        t = sema_builtin(c, TYPE_ERROR);
    }
    for (i = 0; i < count; i++) {
        struct type *element = sema_is_error(t) ? t : t->fields[i].type;
        struct symbol *sym =
            sema_declare(c, SYMBOL_LOCAL, &names[i].name, names[i].pos,
                         "`%.*s` is already declared in this block");
        if (sema_refuse_abstract_value(c, names[i].pos, "this local",
                                       element)) {
            element = sema_builtin(c, TYPE_ERROR);
        }
        if (sym != NULL) {
            sym->type = element;
            sym->read_only = read_only;
            names[i].symbol = sym;
        }
    }
}

/* DESIGN: the flags form takes one arithmetic operation on an integer
   type: `+`, `-`, `*`, `<<`, `>>` or unary `-`. A `-` directly before a
   literal forms a constant rather than an operation, so it has no
   flags. Returns false after a message. */
static bool flags_operation(struct checker *c, const struct expr *value,
                            const struct type *t)
{
    char spelling[OP_TEXT];
    enum token_kind op = value->kind == EXPR_BINARY ? value->as.binary.op
                         : value->kind == EXPR_UNARY ? value->as.unary.op
                                                     : TOKEN_EOF;
    bool known = value->kind == EXPR_BINARY
                     ? op == TOKEN_PLUS || op == TOKEN_MINUS ||
                           op == TOKEN_STAR || op == TOKEN_SHL ||
                           op == TOKEN_SHR
                     : op == TOKEN_MINUS;

    if (sema_is_error(t)) {
        return false;
    }
    if (op == TOKEN_EOF || !known) {
        sema_error_at(c, value->pos, "the flags form takes one `+`, `-`, `*`, "
                      "`<<`, `>>` or unary `-`, found %s%s%s",
                      op != TOKEN_EOF ? "`" : "",
                      op != TOKEN_EOF ? sema_op_text(op, spelling)
                      : value->kind == EXPR_TUPLE ? "a tuple"
                                                  : "another expression",
                      op != TOKEN_EOF ? "`" : "");
        return false;
    }
    if (value->kind == EXPR_UNARY &&
        (value->as.unary.operand->kind == EXPR_INT ||
         value->as.unary.operand->kind == EXPR_FLOAT)) {
        sema_error_at(c, value->pos, "a `-` before a literal forms a constant, "
                      "which has no flags");
        return false;
    }
    if (!type_is_integer(t)) {
        sema_error_at(c, value->pos,
                      "the flags form takes an integer type, found "
                      "`%s`", sema_tn(t));
        return false;
    }
    return true;
}

/* `let (result, flags) = e;` destructures the `(T, Flags)` of the flags
   form. The result name is always new. The flags name takes a Flags
   variable in scope, which the statement then assigns, and is new
   otherwise. The statement's own symbol holds the pair for the dump and
   no place. */
static void check_flags_let(struct checker *c, struct stmt *s, struct type *t)
{
    struct binding *names = s->as.let.names;
    struct type *flags = types_flags(c->types);
    struct type *pair[2];
    struct symbol *value;
    struct symbol *sym;

    if (!flags_operation(c, s->as.let.value, t)) {
        t = sema_builtin(c, TYPE_ERROR);
    }
    sym = sema_declare(c, SYMBOL_LOCAL, &names[0].name, names[0].pos,
                       "`%.*s` is already declared in this block");
    if (sym != NULL) {
        sym->type = t;
        names[0].symbol = sym;
    }
    sym = sema_lookup(c, &names[1].name);
    if (sym != NULL &&
        (sym->kind == SYMBOL_LOCAL || sym->kind == SYMBOL_PARAM) &&
        sym->type == flags) {
        if (sym->read_only) {
            refuse_read_only(c, names[1].pos, sym);
        }
        names[1].symbol = sym;
        names[1].assigns = true;
    } else {
        sym = sema_declare(c, SYMBOL_LOCAL, &names[1].name, names[1].pos,
                           "`%.*s` is already declared in this block");
        if (sym != NULL) {
            sym->type = flags;
            names[1].symbol = sym;
        }
    }
    value = arena_alloc(c->arena, sizeof *value);
    value->kind = SYMBOL_LOCAL;
    value->pos = s->as.let.name_pos;
    pair[0] = t;
    pair[1] = flags;
    value->type = sema_is_error(t) ? t : types_tuple(c->types, pair, 2);
    s->as.let.symbol = value;
}

/* `(result, flags) = e;` assigns the flags form to two names that exist:
   a variable of the operand type and a Flags variable. */
static void check_flags_assign(struct checker *c, struct stmt *s)
{
    struct expr *target = s->as.assign.target;
    struct expr *value = s->as.assign.value;
    struct type *places[2];
    struct type *t;
    size_t i;

    if (s->as.assign.op != TOKEN_ASSIGN || target->as.tuple.count != 2 ||
        target->as.tuple.elements[0]->kind != EXPR_NAME ||
        target->as.tuple.elements[1]->kind != EXPR_NAME) {
        sema_error_at(c, target->pos, "the flags form assigns to two names");
        return;
    }
    for (i = 0; i < 2; i++) {
        struct expr *name = target->as.tuple.elements[i];
        struct symbol *sym = sema_lookup(c, &name->as.name);
        if (sym == NULL) {
            sema_error_at(c, name->pos, "unknown name `%.*s`",
                          (int)name->as.name.length, name->as.name.text);
            return;
        }
        if ((sym->kind != SYMBOL_LOCAL && sym->kind != SYMBOL_PARAM) ||
            sym->type == NULL) {
            sema_error_at(c, name->pos, "cannot assign to this expression");
            return;
        }
        if (sym->read_only) {
            refuse_read_only(c, name->pos, sym);
            return;
        }
        name->symbol = sym;
        name->type = sym->type;
        places[i] = sym->type;
    }
    if (places[1] != types_flags(c->types)) {
        sema_error_at(c, target->as.tuple.elements[1]->pos,
                      "expected `%s`, found `%s`", LANG_FLAGS,
                      sema_tn(places[1]));
        return;
    }
    t = sema_check_expr(c, value, places[0]);
    if (flags_operation(c, value, t)) {
        sema_require(c, value, t, places[0]);
    }
}

/* `let (a, b) = e;`. The value goes into a place of its own, which the
   statement's own symbol names and no scope holds. The names take the
   elements from there. */
static void check_destructuring_let(struct checker *c, struct stmt *s)
{
    struct symbol *value;
    struct type *t;

    c->target_sized = true;
    t = sema_check_expr(c, s->as.let.value, NULL);
    c->target_sized = false;
    if (s->as.let.name_count == 2 && s->as.let.guard.kind == HANDLE_NONE &&
        !sema_is_error(t) && t->kind != TYPE_TUPLE &&
        (s->as.let.value->kind == EXPR_BINARY ||
         s->as.let.value->kind == EXPR_UNARY)) {
        check_flags_let(c, s, t);
        return;
    }
    if (s->as.let.guard.kind != HANDLE_NONE) {
        t = check_pointer_guard(c, s, t);
    }
    sema_refuse_escaping_error(c, s->as.let.value);
    if (!sema_is_error(t) && t->kind == TYPE_VOID) {
        sema_error_at(c, s->as.let.value->pos,
                      "a destructuring takes a tuple, found `%s`", sema_tn(t));
        t = sema_builtin(c, TYPE_ERROR);
    } else {
        sema_refuse_owned_copy(c, s->as.let.value, t);
    }
    value = arena_alloc(c->arena, sizeof *value);
    value->kind = SYMBOL_LOCAL;
    value->pos = s->as.let.name_pos;
    value->type = t;
    value->address_taken = true;
    s->as.let.symbol = value;
    bind_elements(c, s->as.let.names, s->as.let.name_count, t,
                  s->as.let.name_pos, false);
}

static void check_stmt(struct checker *c, struct stmt *s);

/* The mutex of a `sync` without the `&` or the `*` before it, so `m`,
   `&m` and `*&m` name one mutex. */
static const struct expr *mutex_place(const struct expr *e)
{
    while (e->kind == EXPR_UNARY && (e->as.unary.op == TOKEN_AMP ||
                                     e->as.unary.op == TOKEN_STAR)) {
        e = e->as.unary.operand;
    }
    return e;
}

/* DESIGN: two `sync` blocks hold one mutex when their operands name one
   place: the same variable, or the same path of fields from it. That is
   what one function can prove. Two places that hold copies of one
   handle deadlock at run time instead. */
static bool same_mutex(const struct expr *a, const struct expr *b)
{
    a = mutex_place(a);
    b = mutex_place(b);
    if (a->kind == EXPR_NAME && b->kind == EXPR_NAME) {
        return a->symbol != NULL && a->symbol == b->symbol;
    }
    if (a->kind == EXPR_FIELD && b->kind == EXPR_FIELD) {
        return sema_same_name(&a->as.field.name, &b->as.field.name) &&
               same_mutex(a->as.field.base, b->as.field.base);
    }
    return false;
}

/* The operand of a `sync` as the program wrote it. */
static void spell_mutex(struct text *out, const struct expr *e)
{
    while (e->kind == EXPR_UNARY && (e->as.unary.op == TOKEN_AMP ||
                                     e->as.unary.op == TOKEN_STAR)) {
        text_append(out, e->as.unary.op == TOKEN_AMP ? "&" : "*");
        e = e->as.unary.operand;
    }
    sema_spell(out, e);
}

/* `sync m { }` holds m, a Mutex or a pointer to one, for the block. A
   `sync` on the mutex that an enclosing one of the function holds would
   wait for itself. `sync obj { }` holds the hidden lock of a
   synchronized object, which its thread takes again without waiting. */
static void check_sync(struct checker *c, struct stmt *s)
{
    struct type *t = sema_check_expr(c, s->as.sync.mutex, NULL);
    const struct held_mutex *h;
    struct held_mutex here;
    bool pointer = !sema_is_error(t) && t->kind == TYPE_POINTER;

    if (pointer) {
        t = sema_usable_pointer(c, s->as.sync.mutex, t)->element;
    }
    if (!sema_is_error(t) && t->kind == TYPE_CLASS &&
        t->safety == SAFETY_SYNCHRONIZED) {
        s->as.sync.object = true;
    } else if (!sema_is_error(t) && !types_is_mutex(t)) {
        sema_error_at(c, s->as.sync.mutex->pos, "`sync` takes a `" LANG_MUTEX
                      "`, a synchronized object or a pointer to either, "
                      "found `%s`", sema_tn(t));
    } else if (!sema_is_error(t) && !pointer &&
               !sema_reads_existing(s->as.sync.mutex)) {
        sema_error_at(c, s->as.sync.mutex->pos, "`sync` takes a `" LANG_MUTEX
                      "` that lives in a place, or a pointer to one");
    }
    for (h = c->held; h != NULL && !s->as.sync.object; h = h->outer) {
        if (same_mutex(h->mutex, s->as.sync.mutex)) {
            struct text inner = {0};
            struct text outer = {0};
            spell_mutex(&inner, s->as.sync.mutex);
            spell_mutex(&outer, h->mutex);
            sema_error_at(c, s->pos, "`sync %s` inside `sync %s` deadlocks",
                          text_cstr(&inner), text_cstr(&outer));
            text_free(&inner);
            text_free(&outer);
            break;
        }
    }
    here.mutex = s->as.sync.mutex;
    here.outer = c->held;
    c->held = &here;
    sema_check_block(c, s->as.sync.body);
    c->held = here.outer;
}

/* DESIGN: an arm of `select` names a channel, and the name it binds
   holds what `recv` of that channel would give: a `?*T`, `none` once
   the channel is closed and empty. The name lives in the arm alone. */
static void check_select(struct checker *c, struct stmt *s)
{
    struct stmt *outer = c->fallthrough;
    size_t i;

    c->fallthrough = NULL;
    for (i = 0; i < s->as.select.count; i++) {
        struct switch_arm *arm = &s->as.select.arms[i];
        struct type *t = sema_check_expr(c, arm->value, NULL);
        struct scope scope;

        if (!sema_is_error(t) && !types_is_chan(t)) {
            sema_error_at(c, arm->value->pos, "an arm of a `select` names a "
                          "channel, found `%s`", sema_tn(t));
            t = sema_builtin(c, TYPE_ERROR);
        }
        if (arm->binds.length == 0) {
            check_stmt(c, arm->body);
            continue;
        }
        sema_enter_scope(c, &scope);
        arm->bound = sema_declare(c, SYMBOL_LOCAL, &arm->binds, arm->binds_pos,
                                  "`%.*s` is already declared in this block");
        if (arm->bound != NULL) {
            arm->bound->type = sema_is_error(t)
                                   ? t
                                   : types_pointer_nullable(c->types,
                                                            t->element);
        }
        check_stmt(c, arm->body);
        sema_leave_scope(c, &scope);
    }
    c->fallthrough = outer;
}

static void check_stmt(struct checker *c, struct stmt *s)
{
    struct type *t;
    struct symbol *sym;
    size_t i;
    struct type *result = declared_result(c);

    switch (s->kind) {
    case STMT_LET: {
        struct type *declared;
        if (s->as.let.name_count > 0) {
            check_destructuring_let(c, s);
            return;
        }
        c->target_sized = true;
        declared = s->as.let.type != NULL ? sema_resolve_type(c, s->as.let.type)
                                          : NULL;
        /* `let m: *T = p else { }` names the type the binding has, and
           the value beside it is the `?*T` of the same element. */
        if (s->as.let.otherwise != NULL && declared != NULL &&
            (declared->kind == TYPE_POINTER || declared->kind == TYPE_FN) &&
            !declared->nullable) {
            t = sema_check_expr(c, s->as.let.value,
                                types_with_none(c->types, declared));
        } else {
            t = sema_check_expr(c, s->as.let.value, declared);
        }
        c->target_sized = false;
        /* `let m = p catch fatal` and `let m = p catch e { }` guard the
           pointer with the error forms. A call that gives a `?*T` keeps
           its handler, and the `let` takes it over here. So does the
           call after a `?.`, which gives a `?*T` in every case. */
        if (s->as.let.guard.kind == HANDLE_NONE) {
            struct expr *guarded = s->as.let.value;
            if (guarded->kind == EXPR_OPTIONAL) {
                guarded = guarded->as.optional.access;
            }
            if (guarded->kind == EXPR_CALL &&
                guarded->as.call.guards_pointer) {
                s->as.let.guard = guarded->as.call.handler;
                memset(&guarded->as.call.handler, 0,
                       sizeof guarded->as.call.handler);
            }
        }
        if (s->as.let.guard.kind != HANDLE_NONE) {
            t = check_pointer_guard(c, s, t);
        }
        /* `let m = p else { }` binds m as the `*T` of p's `?*T`. The
           block runs when p is `none` and leaves, so below the `let` the
           name holds a pointer on every path. */
        if (s->as.let.otherwise != NULL) {
            if (!sema_is_error(t) && !type_is_nullable(t)) {
                sema_error_at(c, s->as.let.value->pos,
                              "the `else` of a `let` follows a value of type "
                              "`?*T` or `?T`, found `%s`", sema_tn(t));
                t = sema_builtin(c, TYPE_ERROR);
            } else if (!sema_is_error(t)) {
                t = types_without_none(c->types, t);
            }
            sema_check_block(c, s->as.let.otherwise);
            if (!block_leaves(s->as.let.otherwise)) {
                sema_error_at(c, s->as.let.otherwise->pos,
                              "the `else` of a `let` leaves the block it "
                              "stands in");
            }
            declared = NULL;
        }
        /* A local of function type holds a closure in the form of two
           words, as a parameter that does not keep its argument does. */
        if (declared != NULL && !sema_is_error(t) && t->kind == TYPE_FN &&
            t->context && declared->kind == TYPE_FN && !declared->context &&
            types_fn_form(c->types, t, false, false) ==
                types_fn_form(c->types, declared, false, false)) {
            declared = t;
        }
        /* A local never owns a function. It borrows an `own fn` in the
           form of two words, and its owner frees the snapshot. */
        if (!sema_is_error(t) && t->kind == TYPE_FN && t->owned &&
            (declared == NULL || declared == t)) {
            declared = types_fn_form(c->types, t, true, true);
        }
        if (declared != NULL) {
            if (sema_require(c, s->as.let.value, t, declared) &&
                !moves_own_param(c, s->as.let.value, s->as.let.name)) {
                sema_refuse_owned_copy(c, s->as.let.value, declared);
            }
            t = declared;
        } else if (!sema_is_error(t) && t->kind == TYPE_VOID) {
            sema_require(c, s->as.let.value, t, sema_builtin(c, TYPE_I64));
            t = sema_builtin(c, TYPE_ERROR);
        } else if (!moves_own_param(c, s->as.let.value, s->as.let.name)) {
            sema_refuse_owned_copy(c, s->as.let.value, t);
        }
        if (sema_refuse_abstract_value(c, s->as.let.name_pos, "this local",
                                       t)) {
            t = sema_builtin(c, TYPE_ERROR);
        }
        sym = sema_declare(c, SYMBOL_LOCAL, &s->as.let.name, s->as.let.name_pos,
                           "`%.*s` is already declared in this block");
        sema_refuse_escaping_error(c, s->as.let.value);
        if (sym != NULL) {
            sym->type = t;
            s->as.let.symbol = sym;
            sym->holds = held_deepest(s->as.let.value);
            sym->into_fields = sema_points_into_fields(s->as.let.value,
                                                       c->function, t);
            /* DESIGN: an atomic local lives in memory, where the atomic
               operations reach it through its address. It holds what
               one operation of the runtime moves: an integer, a `bool`,
               a `char` or a pointer. */
            if (s->as.let.atomic) {
                sym->atomic = true;
                sym->address_taken = true;
                if (!sema_is_error(t) && !type_is_integer(t) &&
                    t->kind != TYPE_BOOL && t->kind != TYPE_CHAR &&
                    t->kind != TYPE_POINTER) {
                    sema_error_at(c, s->as.let.name_pos, "an atomic local "
                                  "holds an integer, a `bool`, a `char` or "
                                  "a pointer, found `%s`", sema_tn(t));
                }
            }
            if (s->as.let.value->kind == EXPR_FN) {
                sym->closure = s->as.let.value->as.fn;
            }
            /* A failing call writes its result through a pointer, so
               the local it writes to needs a place of its own. So does
               the pointer of `alloc T(args)`, which a handler may
               replace. */
            if (s->as.let.value->kind == EXPR_CALL &&
                s->as.let.value->as.call.out != NULL) {
                sym->address_taken = true;
            }
            if (s->as.let.value->kind == EXPR_ALLOC &&
                s->as.let.value->as.alloc.value != NULL &&
                s->as.let.value->as.alloc.value->kind == EXPR_CALL &&
                s->as.let.value->as.alloc.value->as.call.builds != NULL) {
                sym->address_taken = true;
            }
            /* A `catch` handler may put another pointer in the binding
               with `yield`, so the binding needs a place of its own. So
               does the value `let ... else` takes out of a `?T`, which
               the path where it is there writes. */
            if (s->as.let.guard.kind != HANDLE_NONE ||
                (s->as.let.otherwise != NULL &&
                 s->as.let.value->type != NULL &&
                 s->as.let.value->type->kind == TYPE_OPTIONAL)) {
                sym->address_taken = true;
            }
        }
        return;
    }
    case STMT_CONST:
        sym = sema_declare(c, SYMBOL_CONST, &s->as.let.name, s->as.let.name_pos,
                           "`%.*s` is already declared in this block");
        if (sym != NULL) {
            sym->stmt = s;
            s->as.let.symbol = sym;
            sema_const_symbol(c, sym, s->as.let.name_pos);
        }
        return;
    case STMT_EXPR:
        sema_check_expr(c, s->as.expr, NULL);
        return;
    case STMT_ASSIGN:
        sema_refuse_escaping_error(c, s->as.assign.value);
        check_assign(c, s);
        return;
    case STMT_IF: {
        struct symbol *leaves[PROVED_MAX];
        size_t leave_count = 0;
        for (i = 0; i < s->as.if_chain.count; i++) {
            struct expr *cond = s->as.if_chain.branches[i].cond;
            struct symbol *proved[PROVED_MAX];
            size_t count;
            check_condition(c, cond);
            count = sema_proved_names(cond, true, proved, 0);
            check_block_narrowing(c, s->as.if_chain.branches[i].body, proved,
                                  count);
            /* `if p == none { return; }` proves the rest of the
               enclosing block runs with p bound, so the narrowing
               outlives the branch. One branch alone can prove it, and
               only when it leaves. */
            if (s->as.if_chain.count == 1 && leave_count == 0 &&
                block_leaves(s->as.if_chain.branches[i].body)) {
                leave_count = sema_proved_names(cond, false, leaves, 0);
            }
        }
        if (s->as.if_chain.else_body != NULL) {
            /* The `else` of `if p == none` runs with p bound. */
            struct symbol *bound[PROVED_MAX];
            size_t count =
                s->as.if_chain.count == 1
                    ? sema_proved_names(s->as.if_chain.branches[0].cond, false,
                                        bound, 0)
                    : 0;
            check_block_narrowing(c, s->as.if_chain.else_body, bound, count);
        }
        if (s->as.if_chain.else_body == NULL) {
            for (i = 0; i < leave_count; i++) {
                sema_narrow(c, leaves[i], sema_proved_type(c, leaves[i]));
            }
        }
        return;
    }
    case STMT_WHILE:
    case STMT_DO_WHILE:
        c->loop_depth++;
        if (s->kind == STMT_WHILE) {
            struct symbol *proved[PROVED_MAX];
            size_t count;
            check_condition(c, s->as.loop.cond);
            count = sema_proved_names(s->as.loop.cond, true, proved, 0);
            check_block_narrowing(c, s->as.loop.body, proved, count);
        } else {
            sema_check_block(c, s->as.loop.body);
            check_condition(c, s->as.loop.cond);
        }
        c->loop_depth--;
        return;
    /* DESIGN: `for i in lo..hi` counts over an integer range, and
       `for x in slice` walks a slice or an array. The variable is
       read-only in the body, so the loop cannot lose its step. */
    /* `yield v` gives the value that takes the place of the result the
       failing call would have written. */
    case STMT_YIELD:
        if (c->handler_depth == 0) {
            sema_error_at(c, s->pos, "`yield` stands in a `catch` handler");
            return;
        }
        if (s->as.yielded == NULL) {
            if (c->yields->kind != TYPE_VOID) {
                sema_error_at(c, s->pos, "`yield` gives a value of type `%s`",
                              sema_tn(c->yields));
            }
            return;
        }
        if (c->yields->kind == TYPE_VOID) {
            sema_error_at(c, s->pos,
                          "the call writes no result, so `yield` takes "
                          "no value");
            sema_check_expr(c, s->as.yielded, NULL);
            return;
        }
        sema_require(c, s->as.yielded,
                     sema_check_expr(c, s->as.yielded, c->yields), c->yields);
        return;
    /* `try { } catch e { }` handles every failing call of the block. */
    case STMT_TRY: {
        struct scope try_scope;
        struct handler *h = &s->as.try_block.handler;
        struct block *outer_try = c->try_block;
        struct type *outer_error = c->error_type;
        if (h->none) {
            sema_error_at(c, h->pos, "`catch none` needs a result that can "
                          "be `none`, and a `try` block gives none");
            return;
        }
        c->try_block = s->as.try_block.body;
        c->error_type = NULL;
        sema_check_block(c, s->as.try_block.body);
        c->try_block = outer_try;
        if (h->kind != HANDLE_BLOCK) {
            return;
        }
        sema_enter_scope(c, &try_scope);
        sema_declare_caught(c, h,
                            c->error_type != NULL
                                ? sema_caught_error(c, c->error_type)
                                : sema_builtin(c, TYPE_ERROR));
        sema_check_block(c, h->body);
        sema_leave_scope(c, &try_scope);
        c->error_type = outer_error;
        return;
    }
    case STMT_FOR: {
        struct scope for_scope;
        struct type *element = NULL;
        struct symbol *loop_var;
        size_t names = s->as.for_loop.name_count;
        sema_enter_scope(c, &for_scope);
        if (s->as.for_loop.over != NULL) {
            struct type *over = sema_check_expr(c, s->as.for_loop.over, NULL);
            /* A range without a name repeats its block. A slice has an
               element to read, so it names one. */
            if (names == 0) {
                sema_error_at(c, s->pos,
                              "a `for` over a slice names its element");
            }
            if (!sema_is_error(over) && over->kind != TYPE_SLICE &&
                over->kind != TYPE_ARRAY) {
                if (!sema_iterate(c, s->as.for_loop.over, over,
                                  &s->as.for_loop.hooks, &element)) {
                    sema_error_at(c, s->as.for_loop.over->pos,
                                  "`for` walks a range, a slice, an array, a "
                                  "collection or an iterator, found `%s`",
                                  sema_tn(over));
                    element = sema_builtin(c, TYPE_ERROR);
                } else if (s->as.for_loop.by_pointer &&
                           s->as.for_loop.hooks.place != NULL) {
                    element = s->as.for_loop.hooks.place->type;
                } else if (s->as.for_loop.by_pointer) {
                    const struct symbol *cursor = s->as.for_loop.hooks.cursor;
                    const struct type *walker =
                        cursor != NULL ? cursor->type : over;
                    if (walker->kind == TYPE_POINTER) {
                        walker = walker->element;
                    }
                    if (!sema_is_error(element)) {
                        sema_error_at(c, s->as.for_loop.over->pos,
                                      "`for x in &e` takes each element in "
                                      "place, and the `operator fn value` of "
                                      "`%s` gives a copy of `%s`",
                                      sema_tn(walker), sema_tn(element));
                    }
                    element = sema_builtin(c, TYPE_ERROR);
                } else if (types_is_match(element) &&
                           s->as.for_loop.over->kind == EXPR_CALL &&
                           s->as.for_loop.over->as.call.pattern != NULL) {
                    /* The matches of a pattern literal know its
                       groups. */
                    element = types_match(c->types,
                                          s->as.for_loop.over->as.call.pattern);
                }
            } else if (!sema_is_error(over)) {
                element = over->element;
                if (s->as.for_loop.by_pointer) {
                    element = types_lent(c->types,
                                         types_pointer(c->types, element));
                }
            } else {
                element = over;
            }
        } else {
            struct type *low = sema_check_expr(c, s->as.for_loop.low, NULL);
            struct type *high =
                sema_check_expr(c, s->as.for_loop.high,
                                sema_is_error(low) ? NULL : low);
            if (!sema_is_error(low) && !type_is_integer(low)) {
                sema_error_at(c, s->as.for_loop.low->pos,
                              "a `for` range counts over an integer, found "
                              "`%s`",
                              sema_tn(low));
                low = sema_builtin(c, TYPE_ERROR);
            } else if (!sema_is_error(low)) {
                sema_require(c, s->as.for_loop.high, high, low);
            }
            element = low;
        }
        s->as.for_loop.step_value = 1;
        if (s->as.for_loop.step != NULL) {
            check_step(c, s, element);
        }
        /* DESIGN: `for i, x in items` is the destructuring of the
           `(int, T)` of each element, and `for i, x in &items` of an
           `(int, *T)`, so the two names follow the rule that
           `let (a, b) = e;` follows and bind_elements gives both. One
           name binds the element itself, and a range without a name
           repeats its block and counts in a temporary that no body can
           read. */
        if (names > 1) {
            struct type *pair[2];
            if (names > 2 || s->as.for_loop.over == NULL ||
                s->as.for_loop.hooks.cursor != NULL) {
                sema_error_at(c, s->as.for_loop.names[0].pos,
                              "`for i, x` binds the index and the element of a "
                              "slice or an array");
                element = sema_builtin(c, TYPE_ERROR);
            }
            pair[0] = sema_builtin(c, TYPE_I64);
            pair[1] = element;
            bind_elements(c, s->as.for_loop.names, names,
                          sema_is_error(element)
                              ? element
                              : types_tuple(c->types, pair, 2),
                          s->pos, true);
            mark_walked(s, s->as.for_loop.names[names - 1].symbol);
        } else if (names == 1) {
            loop_var = sema_declare(c, SYMBOL_LOCAL,
                                    &s->as.for_loop.names[0].name,
                                    s->as.for_loop.names[0].pos,
                                    "`%.*s` is already declared in this block");
            if (loop_var != NULL) {
                loop_var->type = element;
                loop_var->read_only = true;
                s->as.for_loop.names[0].symbol = loop_var;
                mark_walked(s, loop_var);
            }
        }
        c->loop_depth++;
        sema_check_block(c, s->as.for_loop.body);
        c->loop_depth--;
        sema_leave_scope(c, &for_scope);
        return;
    }
    /* DESIGN: `switch` runs one arm, and it enters the next only where
       the arm ends in `fallthrough;`. The arms are checked in the order
       of the text, `else` in its place, so that an assignment that ends
       a narrowing reaches the arm a `fallthrough` enters. A switch on an
       enum without `else` covers every value, and the message names the
       ones it misses. */
    case STMT_SWITCH: {
        struct type *over = sema_check_expr(c, s->as.switch_stmt.value, NULL);
        struct stmt *outer = c->fallthrough;
        struct symbol *equal = NULL;
        size_t arms = s->as.switch_stmt.count +
                      (s->as.switch_stmt.otherwise != NULL ? 1 : 0);
        bool variant;
        size_t k;
        size_t j;
        if (s->as.switch_stmt.if_let && !sema_is_error(over) &&
            type_is_nullable(over) &&
            s->as.switch_stmt.arms[0].binds.length == 0) {
            sema_if_let_none(c, s, over);
            return;
        }
        if (s->as.switch_stmt.if_let && !sema_is_error(over) &&
            over->kind != TYPE_VARIANT) {
            sema_error_at(c, s->as.switch_stmt.value->pos, "`if let` takes a "
                          "variant or a value that may be `none`, found `%s`",
                          sema_tn(over));
            over = sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_is_error(over) && over->kind == TYPE_STR) {
            struct symbol *bound;
            equal = text_equal(c, s->as.switch_stmt.value->pos);
            if (equal == NULL) {
                over = sema_builtin(c, TYPE_ERROR);
            } else {
                bound = arena_alloc(c->arena, sizeof *bound);
                bound->kind = SYMBOL_LOCAL;
                bound->name = sema_hidden_value;
                bound->pos = s->as.switch_stmt.value->pos;
                bound->type = over;
                s->as.switch_stmt.bound = bound;
            }
        } else if (!sema_is_error(over) && !type_is_integer(over) &&
                   over->kind != TYPE_ENUM && over->kind != TYPE_VARIANT) {
            sema_error_at(c, s->as.switch_stmt.value->pos,
                          "`switch` takes an enum, an integer, a `str` or a "
                          "variant, found `%s`", sema_tn(over));
            over = sema_builtin(c, TYPE_ERROR);
        }
        variant = !sema_is_error(over) && over->kind == TYPE_VARIANT;
        for (k = 0, i = 0; k < arms; k++) {
            struct stmt *body;
            struct const_value v;
            struct scope arm_scope;
            bool scoped = false;
            if (s->as.switch_stmt.otherwise != NULL &&
                k == s->as.switch_stmt.otherwise_at) {
                body = s->as.switch_stmt.otherwise;
            } else if (variant) {
                body = s->as.switch_stmt.arms[i].body;
                scoped = check_variant_arm(c, s, i, over, &arm_scope);
                i++;
            } else {
                struct switch_arm *arm = &s->as.switch_stmt.arms[i];
                body = arm->body;
                if (arm->binds.length > 0) {
                    sema_error_at(c, arm->binds_pos,
                                  "an arm binds the fields of "
                                  "a case in a `switch` on a variant alone");
                }
                if (sema_require(c, arm->value,
                                 sema_check_expr(c, arm->value, over),
                                 over) &&
                    !sema_is_error(over) &&
                    sema_eval_const(c, arm->value, &v)) {
                    for (j = 0; j < i; j++) {
                        struct const_value other;
                        if (sema_eval_const(c, s->as.switch_stmt.arms[j].value,
                                            &other) &&
                            same_arm_value(&other, &v)) {
                            sema_error_at(c, arm->pos,
                                          "this value already has an arm");
                        }
                    }
                    if (equal != NULL) {
                        arm->test = equal_test(c, equal,
                                               s->as.switch_stmt.bound,
                                               arm->value);
                    }
                }
                i++;
            }
            /* The block of an `if let` is no arm that `fallthrough`
               leaves. */
            c->fallthrough = s->as.switch_stmt.if_let
                                 ? NULL
                                 : sema_arm_fallthrough(body);
            if (c->fallthrough != NULL && k + 1 == arms) {
                sema_error_at(c, c->fallthrough->pos,
                              "`fallthrough` in the last arm");
            } else if (c->fallthrough != NULL && variant) {
                refuse_fallthrough_binding(c, s, k + 1);
            }
            check_stmt(c, body);
            if (scoped) {
                sema_leave_scope(c, &arm_scope);
            }
        }
        c->fallthrough = outer;
        if (s->as.switch_stmt.otherwise == NULL && !sema_is_error(over) &&
            over->kind == TYPE_ENUM) {
            check_switch_covers(c, s, over);
        }
        if (s->as.switch_stmt.otherwise == NULL && variant) {
            check_cases_covered(c, s, over);
        }
        return;
    }
    /* DESIGN: the switch above names the one `fallthrough;` of each arm
       that stands where the rule allows it, the last statement of the
       arm's block. It refuses one into an arm that binds the fields of a
       case, since nothing would fill them. Every other one is refused
       here, inside a nested block, an `if`, a loop or a `defer` as
       well. */
    case STMT_FALLTHROUGH:
        if (s != c->fallthrough) {
            sema_error_at(c, s->pos, "`fallthrough` is allowed as the last "
                          "statement of a `switch` arm only");
        }
        return;
    /* DESIGN: a release build removes the whole statement, so a call in
       the condition does not run there. The warning says so, because the
       program would behave differently in the two builds. */
    case STMT_ASSERT:
        sema_require(c, s->as.assertion.cond,
                     sema_check_expr(c, s->as.assertion.cond,
                                     sema_builtin(c, TYPE_BOOL)),
                     sema_builtin(c, TYPE_BOOL));
        if (expr_calls(s->as.assertion.cond)) {
            diagnostics_warn(c->diags, NAME_ASSERT_CALL,
                             s->as.assertion.cond->pos.line,
                             s->as.assertion.cond->pos.column,
                             "this `assert` condition calls a function, "
                             "which a release build does not run");
        }
        return;
    case STMT_DEFER:
    case STMT_UNDO:
        c->deferring++;
        check_stmt(c, s->as.deferred);
        c->deferring--;
        return;
    case STMT_FAIL: {
        struct type *error;
        if (!sema_in_failing_function(c)) {
            sema_error_at(c, s->pos, "`fail` outside a function that may fail");
            sema_check_expr(c, s->as.fail.value, NULL);
            return;
        }
        s->error_exit = true;
        c->saw_fail = true;
        sema_resolve_origin(c, s);
        /* `fail "text";` builds the error from the text. Every other
           value is an error the program has in hand. */
        if (s->as.fail.value->kind == EXPR_STRING) {
            s->as.fail.make = sema_error_maker(c, s->pos);
            t = sema_builtin(c, TYPE_STR);
            sema_require(c, s->as.fail.value,
                         sema_check_expr(c, s->as.fail.value, t), t);
            return;
        }
        error = c->function->symbol->type->result;
        t = sema_caught_error(c, error);
        sema_require(c, s->as.fail.value,
                     sema_check_expr(c, s->as.fail.value, t), t);
        return;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
        if (c->loop_depth == 0) {
            sema_error_at(c, s->pos, "`%s` outside a loop",
                          s->kind == STMT_BREAK ? "break" : "continue");
        }
        return;
    case STMT_RETURN:
        if (s->as.return_value == NULL) {
            if (result->kind != TYPE_VOID) {
                sema_error_at(c, s->pos, "`return` needs a value of type `%s`",
                              sema_tn(result));
            }
            return;
        }
        if (result->kind == TYPE_VOID) {
            sema_check_expr(c, s->as.return_value, NULL);
            sema_error_at(c, s->as.return_value->pos, "`%.*s` returns no value",
                          (int)c->function->name.length,
                          c->function->name.text);
            return;
        }
        {
            struct type *given =
                sema_check_expr(c, s->as.return_value, result);
            bool held;
            c->lent_use = LENT_RETURNED;
            held = sema_require(c, s->as.return_value, given, result);
            c->lent_use = LENT_STORED;
            if (held) {
                sema_refuse_lock_copy(c, s->as.return_value, result);
                sema_check_leak_return(c, s->as.return_value, result);
            }
        }
        return;
    case STMT_BLOCK:
        sema_check_block(c, s->as.block);
        return;
    case STMT_SYNC:
        check_sync(c, s);
        return;
    case STMT_SELECT:
        check_select(c, s);
        return;
    }
}

void sema_check_block(struct checker *c, struct block *b)
{
    check_block_narrowing(c, b, NULL, 0);
}

static void check_block_narrowing(struct checker *c, struct block *b,
                                  struct symbol **proved, size_t count)
{
    struct scope scope;
    size_t i;

    sema_enter_scope(c, &scope);
    for (i = 0; i < count; i++) {
        sema_narrow(c, proved[i], sema_proved_type(c, proved[i]));
    }
    for (i = 0; i < b->count; i++) {
        check_stmt(c, b->stmts[i]);
    }
    sema_leave_scope(c, &scope);
}

/* DESIGN: a `construct` with arguments makes the object without a
   literal, so it sets every field that has no default. A path that
   succeeds ends at a `return;` or at the closing brace. Each such path
   assigns each such field of the chain, or calls the `construct` of a
   base, which sets the fields from that base up. The checker refuses
   the construct and names the field otherwise. A path that fails leaves
   no object behind and needs nothing. It is definite assignment, as a
   local has it, applied to the fields of self. A loop body may not run,
   so what it assigns counts only inside it. */
struct required {
    const struct item *fn;
    const struct type *owner;
    const struct struct_field **fields;
    const struct type **levels;
    size_t count;
};

static bool sets_block(struct checker *c, const struct required *r,
                       const struct block *b, bool *set);

static bool sets_stmt(struct checker *c, const struct required *r,
                      const struct stmt *s, bool *set);

/* Report the first field that a path which succeeds at pos leaves
   unset. */
static void require_set(struct checker *c, const struct required *r,
                        const bool *set, struct pos pos)
{
    size_t i;

    for (i = 0; i < r->count && set[i]; i++) {
    }
    if (i < r->count) {
        sema_error_at(c, pos,
                      "`construct` of `%s` returns `none` before it sets "
                      "`%.*s`", sema_tn(r->owner),
                      (int)r->fields[i]->name.length, r->fields[i]->name.text);
    }
}

/* Mark the field that the target of `=` names on self. */
static void set_target(const struct required *r, const struct expr *target,
                       bool *set)
{
    const struct expr *base;
    size_t i;

    if (target->kind != EXPR_FIELD) {
        return;
    }
    for (base = target->as.field.base;
         base->kind == EXPR_FIELD &&
         sema_name_is(&base->as.field.name, "super");
         base = base->as.field.base) {
    }
    if (base->kind != EXPR_NAME || base->symbol == NULL ||
        base->symbol != r->fn->self) {
        return;
    }
    for (i = 0; i < r->count; i++) {
        if (sema_same_name(&r->fields[i]->name, &target->as.field.name)) {
            set[i] = true;
        }
    }
}

/* Mark every field from the class whose `construct` e calls up, when e
   is the call of a base's `construct`. */
static void set_by_base(const struct required *r, const struct expr *e,
                        bool *set)
{
    const struct item *callee;
    const struct type *from;
    const struct type *up;
    size_t i;

    if (e == NULL || e->kind != EXPR_CALL ||
        e->as.call.callee->kind != EXPR_NAME ||
        e->as.call.callee->symbol == NULL) {
        return;
    }
    callee = e->as.call.callee->symbol->item;
    if (callee == NULL || callee == r->fn ||
        !sema_name_is(&callee->name, "construct")) {
        return;
    }
    from = sema_declaring_class(callee);
    for (up = from; up != NULL && up->kind == TYPE_CLASS; up = up->base) {
        for (i = 0; i < r->count; i++) {
            if (r->levels[i] == up) {
                set[i] = true;
            }
        }
    }
}

/* The call that carries the handler of a statement: the value itself,
   or the call after a `?.`. */
static const struct expr *handled_call(const struct expr *e)
{
    if (e->kind == EXPR_OPTIONAL) {
        e = e->as.optional.access;
    }
    return e->kind == EXPR_CALL ? e : NULL;
}

/* The handler of a failing call runs on a path of its own. What it
   assigns does not count after the call. */
static void sets_handler(struct checker *c, const struct required *r,
                         const struct handler *h, const bool *set)
{
    bool *copy;

    if (h->kind != HANDLE_BLOCK || h->body == NULL) {
        return;
    }
    copy = arena_alloc(c->arena, r->count + 1);
    memcpy(copy, set, r->count);
    sets_block(c, r, h->body, copy);
}

/* Fold a path that goes on into the meet of the paths so far. */
static void meet(const struct required *r, bool *into, const bool *path,
                 bool *any)
{
    size_t i;

    for (i = 0; i < r->count; i++) {
        into[i] = (*any ? into[i] : true) && path[i];
    }
    *any = true;
}

static bool sets_stmt(struct checker *c, const struct required *r,
                      const struct stmt *s, bool *set)
{
    bool *copy = arena_alloc(c->arena, r->count + 1);
    bool *out = arena_alloc(c->arena, r->count + 1);
    bool any = false;
    size_t i;

    switch (s->kind) {
    case STMT_ASSIGN:
        if (s->as.assign.op == TOKEN_ASSIGN) {
            set_target(r, s->as.assign.target, set);
        }
        return true;
    case STMT_EXPR:
        set_by_base(r, s->as.expr, set);
        if (handled_call(s->as.expr) != NULL) {
            sets_handler(c, r, &handled_call(s->as.expr)->as.call.handler,
                         set);
        }
        return true;
    case STMT_LET:
        if (s->as.let.value != NULL && handled_call(s->as.let.value) != NULL) {
            sets_handler(c, r, &handled_call(s->as.let.value)->as.call.handler,
                         set);
        }
        return true;
    case STMT_IF:
        for (i = 0; i < s->as.if_chain.count; i++) {
            memcpy(copy, set, r->count);
            if (sets_block(c, r, s->as.if_chain.branches[i].body, copy)) {
                meet(r, out, copy, &any);
            }
        }
        memcpy(copy, set, r->count);
        if (s->as.if_chain.else_body == NULL ||
            sets_block(c, r, s->as.if_chain.else_body, copy)) {
            meet(r, out, copy, &any);
        }
        break;
    /* An arm that ends in `fallthrough;` goes on into the next arm and
       not past the switch. The next arm starts from what the switch
       started from, since its test reaches it with that much set. */
    case STMT_SWITCH:
        for (i = 0; i < s->as.switch_stmt.count; i++) {
            const struct stmt *body = s->as.switch_stmt.arms[i].body;
            memcpy(copy, set, r->count);
            if (sets_stmt(c, r, body, copy) &&
                sema_arm_fallthrough(body) == NULL) {
                meet(r, out, copy, &any);
            }
        }
        memcpy(copy, set, r->count);
        if (s->as.switch_stmt.otherwise == NULL ||
            (sets_stmt(c, r, s->as.switch_stmt.otherwise, copy) &&
             sema_arm_fallthrough(s->as.switch_stmt.otherwise) == NULL)) {
            meet(r, out, copy, &any);
        }
        break;
    case STMT_TRY:
        memcpy(copy, set, r->count);
        if (sets_block(c, r, s->as.try_block.body, copy)) {
            meet(r, out, copy, &any);
        }
        memcpy(copy, set, r->count);
        if (s->as.try_block.handler.kind != HANDLE_BLOCK ||
            sets_block(c, r, s->as.try_block.handler.body, copy)) {
            meet(r, out, copy, &any);
        }
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        memcpy(copy, set, r->count);
        sets_block(c, r, s->as.loop.body, copy);
        return true;
    case STMT_FOR:
        memcpy(copy, set, r->count);
        sets_block(c, r, s->as.for_loop.body, copy);
        return true;
    case STMT_BLOCK:
        return sets_block(c, r, s->as.block, set);
    case STMT_SYNC:
        return sets_block(c, r, s->as.sync.body, set);
    /* One arm of a `select` runs, and any of them may. */
    case STMT_SELECT:
        for (i = 0; i < s->as.select.count; i++) {
            memcpy(copy, set, r->count);
            if (sets_stmt(c, r, s->as.select.arms[i].body, copy)) {
                meet(r, out, copy, &any);
            }
        }
        break;
    case STMT_RETURN:
        if (s->as.return_value == NULL) {
            require_set(c, r, set, s->pos);
        }
        return false;
    case STMT_FAIL:
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_YIELD:
        return false;
    default:
        return true;
    }
    if (!any) {
        return false;
    }
    memcpy(set, out, r->count);
    return true;
}

/* A statement after one that leaves the block is never reached. */
static bool sets_block(struct checker *c, const struct required *r,
                       const struct block *b, bool *set)
{
    size_t i;

    for (i = 0; i < b->count; i++) {
        if (!sets_stmt(c, r, b->stmts[i], set)) {
            return false;
        }
    }
    return true;
}

static void check_construct_sets(struct checker *c, const struct item *it)
{
    const struct type *owner = sema_declaring_class(it);
    const struct type *up;
    struct required r;
    bool *set;
    size_t n = 0;
    size_t i;

    if (owner == NULL || owner->kind != TYPE_CLASS || !it->has_self ||
        it->param_count == 0 || !sema_name_is(&it->name, "construct") ||
        it->body == NULL) {
        return;
    }
    for (up = owner; up != NULL && up->kind == TYPE_CLASS; up = up->base) {
        n += up->field_count;
    }
    r.fn = it;
    r.owner = owner;
    r.fields = types_alloc_array(c->arena, n + 1, sizeof *r.fields);
    r.levels = types_alloc_array(c->arena, n + 1, sizeof *r.levels);
    r.count = 0;
    for (up = owner; up != NULL && up->kind == TYPE_CLASS; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *f = &up->fields[i];
            if ((f->form != FIELD_PLAIN && f->form != FIELD_USE) ||
                type_field_is_unit_break(f) || f->value != NULL ||
                f->constant != NULL || sema_field_takes_literal(f)) {
                continue;
            }
            r.fields[r.count] = f;
            r.levels[r.count++] = up;
        }
    }
    if (r.count == 0) {
        return;
    }
    set = arena_alloc(c->arena, r.count + 1);
    if (sets_block(c, &r, it->body, set)) {
        require_set(c, &r, set, it->body->end);
    }
}

void sema_check_function(struct checker *c, struct item *it)
{
    struct scope params;
    size_t i;

    const struct item *within = c->within;

    if (sema_is_error(it->symbol->type)) {
        return;
    }
    c->function = it;
    c->within = sema_within(it);
    sema_enter_scope(c, &params);
    if (it->has_self) {
        static const struct name self_name = {"self", 4};
        struct symbol *sym =
            sema_declare(c, SYMBOL_PARAM, &self_name, it->pos,
                         "`%.*s` is already declared in this block");
        if (sym != NULL) {
            sym->type = it->symbol->type->params[0];
            it->self = sym;
        }
    }
    for (i = 0; i < it->param_count; i++) {
        struct symbol *sym =
            sema_declare(c, SYMBOL_PARAM, &it->params[i].name,
                         it->params[i].pos,
                         "`%.*s` is already declared in this block");
        if (sym != NULL) {
            sym->type = it->symbol->type->params[i + (it->has_self ? 1 : 0)];
            sym->own_param = it->params[i].owned;
            it->params[i].symbol = sym;
        }
    }
    c->saw_fail = false;
    c->top_call = NULL;
    if (it->body != NULL && it->body->count > 0 &&
        it->body->stmts[0]->kind == STMT_EXPR &&
        it->body->stmts[0]->as.expr->kind == EXPR_CALL) {
        c->top_call = it->body->stmts[0]->as.expr;
    }
    sema_check_block(c, it->body);
    check_construct_sets(c, it);
    sema_leave_scope(c, &params);
    if (declared_result(c)->kind != TYPE_VOID && !block_returns(it->body)) {
        if (it->enclosing != NULL) {
            sema_error_at(c, it->name_pos, "the anonymous function can reach "
                          "its end without `return`");
        } else {
            sema_error_at(c, it->name_pos,
                          "`%.*s` can reach its end without `return`",
                          (int)it->name.length, it->name.text);
        }
    }
    /* DESIGN: a `may fail` function without a `fail` and without a
       `try` is a warning and not an error. An interface function may
       fail in one implementation and not in another. An anonymous
       function that takes `may fail` from its target wrote no position,
       and no warning names it. */
    if (it->may_fail && !c->saw_fail && !sema_is_error(it->symbol->type) &&
        it->may_fail_pos.line != 0) {
        diagnostics_warn(c->diags, NAME_NEVER_FAILS, it->may_fail_pos.line,
                         it->may_fail_pos.column,
                         "`%.*s` may fail and never does",
                         (int)it->name.length, it->name.text);
    }
    c->saw_fail = false;
    c->function = NULL;
    c->within = within;
}

/* Anonymous functions and closures */

/* The named function whose body is checked, around every anonymous
   function that stands in it. NULL outside a function. */
const struct item *sema_named_function(const struct checker *c)
{
    const struct item *it = c->function;

    while (it != NULL && it->enclosing != NULL) {
        it = it->enclosing;
    }
    return it;
}

/* DESIGN: a type is thread-safe when it is built to be changed from more
   than one thread at once. Such are a `Mutex`, a channel, a synchronized
   class and a concurrent class. An atomic is a field or a local marked
   `atomic`, so sema_thread_safe_symbol asks the variable as well. */
bool sema_thread_safe(const struct type *t)
{
    return t != NULL &&
           (types_is_mutex(t) || types_is_chan(t) ||
            ((t->kind == TYPE_CLASS || t->kind == TYPE_STRUCT) &&
             t->safety != SAFETY_NONE));
}

bool sema_thread_safe_symbol(const struct symbol *sym)
{
    return sym->atomic || sema_thread_safe(sym->type);
}

/* The capture of sym in the anonymous function it, added when it is not
   there yet. */
static struct capture *capture_in(struct item *it, struct symbol *sym)
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
            capacity <= SIZE_MAX / sizeof *grown
                ? realloc(it->captures, capacity * sizeof *grown)
                : NULL;
        if (grown == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
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
    for (it = c->function; it != NULL && it != sym->frame;
         it = it->enclosing) {
        capture_in(it, sym);
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
        e->symbol->frame == NULL || e->symbol->frame == c->function) {
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
    for (it = c->function; sym != NULL && it != NULL && it != sym->frame;
         it = it->enclosing) {
        struct capture *cap = capture_in(it, sym);
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
    for (it = c->function; it != NULL && it != sym->frame;
         it = it->enclosing) {
        struct capture *cap = capture_in(it, sym);
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

/* The checker's state of one function body, which an anonymous function
   sets aside while its own body is checked. */
struct body_state {
    struct item *function;
    int loop_depth;
    struct stmt *fallthrough;
    struct type *yields;
    int handler_depth;
    struct block *try_block;
    struct type *error_type;
    bool saw_fail;
    const struct expr *top_call;
    int deferring;
    const struct expr *field_base;
    const struct held_mutex *held;
};

static void set_aside(struct checker *c, struct body_state *s)
{
    s->function = c->function;
    s->loop_depth = c->loop_depth;
    s->fallthrough = c->fallthrough;
    s->yields = c->yields;
    s->handler_depth = c->handler_depth;
    s->try_block = c->try_block;
    s->error_type = c->error_type;
    s->saw_fail = c->saw_fail;
    s->top_call = c->top_call;
    s->deferring = c->deferring;
    s->field_base = c->field_base;
    s->held = c->held;
    c->loop_depth = 0;
    c->fallthrough = NULL;
    c->yields = NULL;
    c->handler_depth = 0;
    c->try_block = NULL;
    c->error_type = NULL;
    c->deferring = 0;
    c->field_base = NULL;
    c->held = NULL;
}

static void take_back(struct checker *c, const struct body_state *s)
{
    c->function = s->function;
    c->loop_depth = s->loop_depth;
    c->fallthrough = s->fallthrough;
    c->yields = s->yields;
    c->handler_depth = s->handler_depth;
    c->try_block = s->try_block;
    c->error_type = s->error_type;
    c->saw_fail = s->saw_fail;
    c->top_call = s->top_call;
    c->deferring = s->deferring;
    c->field_base = s->field_base;
    c->held = s->held;
}

/* The type of the anonymous function it, from the types it writes and
   the target where it leaves one out. NULL after an error. */
static struct type *anonymous_type(struct checker *c, struct item *it,
                                   const struct type *target)
{
    size_t shown = target != NULL
                       ? target->param_count - (target->has_out ? 1 : 0)
                       : 0;
    struct type **params = arena_alloc(
        c->arena, (it->param_count + 2) * sizeof *params);
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
   captured value and each capture of another type are refused. */
static void check_snapshot(struct checker *c, const struct expr *e)
{
    const struct item *it = e->as.fn;
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        const struct capture *cap = &it->captures[i];
        const struct symbol *sym = cap->symbol;
        if (sym->type == NULL || sema_is_error(sym->type)) {
            continue;
        }
        if (!snapshot_copies(sym->type, true)) {
            sema_error_at(c, e->pos, "a snapshot holds values that copy "
                          "fully, and `%.*s` is `%s`",
                          (int)sym->name.length, sym->name.text,
                          sema_tn(sym->type));
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
    struct scope params;
    struct symbol *sym;
    struct type *fn;
    size_t i;
    bool safe = true;

    if (c->function == NULL) {
        sema_error_at(c, e->pos, "an anonymous function stands in the body "
                      "of a function");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (expected != NULL && expected->kind == TYPE_FN && !expected->bound) {
        target = expected;
    }
    it->enclosing = c->function;
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
    c->function = it;
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
    if (declared_result(c)->kind != TYPE_VOID && !block_returns(it->body)) {
        sema_error_at(c, it->pos, "the anonymous function can reach its end "
                      "without `return`");
    }
    if (it->may_fail && !c->saw_fail && it->may_fail_pos.line != 0) {
        diagnostics_warn(c->diags, NAME_NEVER_FAILS, it->may_fail_pos.line,
                         it->may_fail_pos.column,
                         "the anonymous function may fail and never does");
    }
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

/* main takes one of the three forms of chapter 2. */
void sema_check_main(struct checker *c, struct item *it)
{
    struct type *t = it->symbol->type;
    struct type *strs = types_slice(c->types, sema_builtin(c, TYPE_STR));
    size_t i;
    bool ok = t->result->kind == TYPE_I64 && t->param_count <= 2;

    for (i = 0; ok && i < t->param_count; i++) {
        ok = t->params[i] == strs;
    }
    if (!ok) {
        sema_error_at(c, it->name_pos,
                      "`main` must be fn main() -> int, fn main("
                      "args: []str) -> int or fn main(args: []str, "
                      "env: []str) -> int");
    }
}

/* DESIGN: every `fn` in `tests` is a test, so it takes no parameters and
   returns nothing: the runner calls it and reads its asserts. A `fn` in
   `fixtures` may take and return anything and is never run by itself.
   `setup` and `teardown` run before and after every test of the module,
   which only `fixtures` declares. */
void sema_check_test_block(struct checker *c, struct item *it)
{
    struct type *t = it->symbol->type;

    if (it->block == BLOCK_TESTS &&
        (t->param_count != 0 || t->result->kind != TYPE_VOID)) {
        sema_error_at(c, it->name_pos, "a `tests` function takes no parameters "
                      "and returns nothing, because the runner calls it");
        return;
    }
    if (it->block == BLOCK_TESTS &&
        (sema_name_is(&it->name, "setup") ||
         sema_name_is(&it->name, "teardown"))) {
        sema_error_at(c, it->name_pos, "`setup` and `teardown` belong to "
                      "`fixtures`, which runs them around every test");
    }
}
