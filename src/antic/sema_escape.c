/* The safety check `escaping-local`, E1 of "Compile-time checks of
   2026-10-20" in docs/decisions.md: a reference to a local or a
   parameter that would outlive it. The pass walks a function body once
   the checker has checked it, with the types and the symbols in place. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* DESIGN: a local reference is the address of a local or a parameter,
   of a part of one, a slice of a local array, and every value made from
   one of those by a field access, an index, a slice or an `as` view. The
   pass records each local and parameter with the depth of its block and
   the line of the brace that ends it, and the local whose reference it
   holds. A local holds the reference the last `=` of the text gave it,
   on whatever path that `=` stands, so a later `=` of another value
   clears it. A local holds no reference of a block deeper than its own,
   since that `=` is refused, so a reference it holds is alive wherever
   the local is. A local that holds a copy of a parameter's pointer
   reaches the memory of that parameter, which outlives the call, and the
   record names the parameter as well. The check refuses only what it
   proves, and the run-time checks keep the rest. */
struct escape_local {
    const struct symbol *symbol;
    int depth;
    int ends;                       /* the line of the closing brace */
    size_t refers;                  /* the index of that local plus 1, or 0 */
    size_t reaches;                 /* the index of the parameter plus 1 */
};

struct escape_walk {
    struct checker *c;
    struct escape_local *locals;
    size_t count;
    size_t capacity;
    int depth;
    int ends;                       /* the end of the block walked now */
    const struct item *frame;       /* the function a `return` leaves */
    int handler;                    /* the depth of the handler, or -1 */
    const struct expr *joined;      /* a `dispatch` its block joins */
};

/* Where a local reference would outlive its local. */
enum escape_place {
    ESCAPE_RETURNED,
    ESCAPE_YIELDED,
    ESCAPE_OUTER,
    ESCAPE_FIELD,
    ESCAPE_ELEMENT,
    ESCAPE_THROUGH,
    ESCAPE_GLOBAL,
    ESCAPE_COLLECTION,
    ESCAPE_DISPATCH,
    ESCAPE_CONCURRENT
};

static void walk_block(struct escape_walk *w, const struct block *b,
                       bool deeper);
static void walk_stmt(struct escape_walk *w, const struct stmt *s);
static void walk_expr(struct escape_walk *w, const struct expr *e);

/* The record of sym, or NULL for a symbol the walk did not declare. The
   array is searched from the end, where the innermost locals stand. */
static struct escape_local *find(const struct escape_walk *w,
                                 const struct symbol *sym)
{
    size_t i;

    for (i = w->count; sym != NULL && i > 0; i--) {
        if (w->locals[i - 1].symbol == sym) {
            return &w->locals[i - 1];
        }
    }
    return NULL;
}

/* DESIGN: the records live in one array that grows, so a record names
   the record it refers to by its index, and declare returns the index of
   the new one. A pointer into the array lasts until the next declare. */
static size_t declare(struct escape_walk *w, const struct symbol *sym,
                      int depth, int ends)
{
    w->locals = alloc_grow(w->locals, &w->capacity, w->count,
                           sizeof *w->locals);
    w->locals[w->count].symbol = sym;
    w->locals[w->count].depth = depth;
    w->locals[w->count].ends = ends;
    w->locals[w->count].refers = 0;
    w->locals[w->count].reaches = 0;
    return w->count++;
}

/* The record r refers to, or NULL. */
static const struct escape_local *referred(const struct escape_walk *w,
                                           const struct escape_local *r)
{
    return r != NULL && r->refers > 0 ? &w->locals[r->refers - 1] : NULL;
}

static size_t index_of(const struct escape_walk *w,
                       const struct escape_local *r)
{
    return r != NULL ? (size_t)(r - w->locals) + 1 : 0;
}

/* Of two records, the one declared deeper, whose block ends first. */
static const struct escape_local *deeper(const struct escape_local *a,
                                         const struct escape_local *b)
{
    if (a == NULL) {
        return b;
    }
    if (b == NULL) {
        return a;
    }
    return b->depth > a->depth ? b : a;
}

/* Whether a value of type t can hold a reference: a pointer, a slice, or
   a value with a part that is one. A type parameter may be a pointer. */
static bool carries(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    switch (t->kind) {
    case TYPE_POINTER:
    case TYPE_SLICE:
    case TYPE_PARAM:
        return true;
    case TYPE_ARRAY:
    case TYPE_OPTIONAL:
        return carries(t->element);
    case TYPE_TUPLE:
    case TYPE_VARIANT:
        for (i = 0; i < t->param_count; i++) {
            if (carries(t->params[i])) {
                return true;
            }
        }
        return false;
    case TYPE_STRUCT:
    case TYPE_CLASS:
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].form != FIELD_TABLE &&
                carries(t->fields[i].type)) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

static const struct escape_local *root_of(const struct escape_walk *w,
                                          const struct expr *e);

/* The local whose memory the place e lies in, or NULL. A place reached
   through a pointer or a slice lies in the local that pointer refers
   to. */
static const struct escape_local *place_root(const struct escape_walk *w,
                                             const struct expr *e)
{
    const struct expr *base;

    switch (e->kind) {
    case EXPR_NAME:
        return find(w, e->symbol);
    case EXPR_FIELD:
    case EXPR_INDEX:
        base = e->kind == EXPR_FIELD ? e->as.field.base : e->as.index.base;
        if (base->type != NULL && (base->type->kind == TYPE_POINTER ||
                                   base->type->kind == TYPE_SLICE)) {
            return root_of(w, base);
        }
        return place_root(w, base);
    case EXPR_UNARY:
        if (e->as.unary.op == TOKEN_STAR) {
            return root_of(w, e->as.unary.operand);
        }
        return NULL;
    default:
        return NULL;
    }
}

/* The local whose reference the value e is, or NULL. Of two, the one
   declared deeper, whose block ends first. */
static const struct escape_local *root_of(const struct escape_walk *w,
                                          const struct expr *e)
{
    const struct escape_local *r = NULL;
    const struct type *base;
    size_t i;

    if (e == NULL) {
        return NULL;
    }
    switch (e->kind) {
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_AMP ? place_root(w, e->as.unary.operand)
                                           : NULL;
    case EXPR_CAST:
        return e->as.cast.test ? NULL : root_of(w, e->as.cast.operand);
    case EXPR_SLICE:
        base = e->as.slice.base->type;
        if (base != NULL && base->kind == TYPE_ARRAY) {
            return place_root(w, e->as.slice.base);
        }
        return base != NULL && base->kind == TYPE_SLICE
                   ? root_of(w, e->as.slice.base)
                   : NULL;
    case EXPR_NAME:
        return referred(w, find(w, e->symbol));
    case EXPR_FIELD:
        base = e->as.field.base->type;
        if (sema_name_is(&e->as.field.name, "ptr") && base != NULL &&
            (base->kind == TYPE_SLICE || base->kind == TYPE_ARRAY)) {
            return base->kind == TYPE_ARRAY ? place_root(w, e->as.field.base)
                                            : root_of(w, e->as.field.base);
        }
        if (base == NULL || base->kind == TYPE_POINTER || !carries(e->type)) {
            return NULL;
        }
        return root_of(w, e->as.field.base);
    case EXPR_INDEX:
        base = e->as.index.base->type;
        if (base == NULL || base->kind != TYPE_ARRAY || !carries(e->type)) {
            return NULL;
        }
        return root_of(w, e->as.index.base);
    case EXPR_BINARY:
        /* `q ?? &x` is the value of either side. */
        if (e->as.binary.op != TOKEN_QUESTION_QUESTION) {
            return NULL;
        }
        return deeper(root_of(w, e->as.binary.left),
                      root_of(w, e->as.binary.right));
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            r = deeper(r, root_of(w, e->as.tuple.elements[i]));
        }
        return r;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            r = deeper(r, root_of(w, e->as.array_lit.elements[i]));
        }
        return r;
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            r = deeper(r, root_of(w, e->as.struct_lit.fields[i].value));
        }
        return r;
    default:
        return NULL;
    }
}

/* Whether sym is a parameter whose memory outlives the call: one that
   is not `own`, `self` among them. */
static bool outer_param(const struct symbol *sym)
{
    return sym != NULL && sym->kind == SYMBOL_PARAM && !sym->own_param;
}

/* The record of the parameter whose pointer the value e copies: the
   parameter itself, a local that holds a copy of it, or an `as` view of
   either. NULL for every other value. */
static const struct escape_local *reached_param(const struct escape_walk *w,
                                                const struct expr *e)
{
    const struct escape_local *r;
    const struct type *t;

    while (e != NULL && e->kind == EXPR_CAST && !e->as.cast.test) {
        e = e->as.cast.operand;
    }
    if (e == NULL || e->kind != EXPR_NAME || e->type == NULL) {
        return NULL;
    }
    t = e->type->kind == TYPE_OPTIONAL ? e->type->element : e->type;
    if (t == NULL || (t->kind != TYPE_POINTER && t->kind != TYPE_SLICE)) {
        return NULL;
    }
    r = find(w, e->symbol);
    if (r == NULL) {
        return NULL;
    }
    if (outer_param(r->symbol)) {
        return r;
    }
    return r->reaches > 0 ? &w->locals[r->reaches - 1] : NULL;
}

/* The name a message gives the place e: the variable, or the field, that
   the place names last. */
static const struct name *place_name(const struct expr *e)
{
    while (e != NULL) {
        switch (e->kind) {
        case EXPR_NAME:
            return &e->as.name;
        case EXPR_FIELD:
            return &e->as.field.name;
        case EXPR_INDEX:
            e = e->as.index.base;
            break;
        case EXPR_UNARY:
            e = e->as.unary.operand;
            break;
        default:
            return NULL;
        }
    }
    return NULL;
}

static void refuse(struct escape_walk *w, struct pos pos,
                   const struct escape_local *root, enum escape_place place,
                   const struct name *other)
{
    struct checker *c = w->c;
    const struct name *n = &root->symbol->name;
    int len = (int)n->length;
    int olen = other != NULL ? (int)other->length : 0;
    const char *o = other != NULL ? other->text : "";

    switch (place) {
    case ESCAPE_RETURNED:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "returned, and `%.*s` ends at line %d", len, n->text,
                      len, n->text, root->ends);
        return;
    case ESCAPE_YIELDED:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "yielded, and `%.*s` ends at line %d", len, n->text,
                      len, n->text, root->ends);
        return;
    case ESCAPE_OUTER:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored into `%.*s` of an outer block, and `%.*s` ends "
                      "at line %d", len, n->text, olen, o, len, n->text,
                      root->ends);
        return;
    case ESCAPE_FIELD:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored into the field `%.*s`, and `%.*s` ends at line "
                      "%d", len, n->text, olen, o, len, n->text, root->ends);
        return;
    case ESCAPE_ELEMENT:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored into an element of `%.*s`, and `%.*s` ends at "
                      "line %d", len, n->text, olen, o, len, n->text,
                      root->ends);
        return;
    case ESCAPE_THROUGH:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored through the parameter `%.*s`, and `%.*s` ends "
                      "at line %d", len, n->text, olen, o, len, n->text,
                      root->ends);
        return;
    case ESCAPE_GLOBAL:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored into the global `%.*s`, and `%.*s` ends at "
                      "line %d", len, n->text, olen, o, len, n->text,
                      root->ends);
        return;
    case ESCAPE_COLLECTION:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "stored into the collection `%.*s`, and `%.*s` ends at "
                      "line %d", len, n->text, olen, o, len, n->text,
                      root->ends);
        return;
    case ESCAPE_DISPATCH:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a reference to `%.*s` is "
                      "passed to `dispatch`, and `%.*s` ends at line %d", len,
                      n->text, len, n->text, root->ends);
        return;
    case ESCAPE_CONCURRENT:
        sema_check_at(c, NAME_ESCAPING_LOCAL, pos, "a closure passed to the "
                      "concurrent parameter `%.*s` captures a reference to "
                      "`%.*s`, and `%.*s` ends at line %d", olen, o, len,
                      n->text, len, n->text, root->ends);
        return;
    }
}

/* What the place a `=` writes lives as long as. */
enum anchor_kind {
    ANCHOR_UNKNOWN,     /* memory the function cannot prove the end of */
    ANCHOR_LOCAL,       /* a local or a parameter of the walk */
    ANCHOR_OUTLIVES     /* memory that outlives every local of the call */
};

struct anchor {
    enum anchor_kind kind;
    size_t local;                   /* the index of ANCHOR_LOCAL */
    const struct name *through;     /* the parameter a place is reached by */
    bool global;
};

/* DESIGN: a place outlives every local of the function when a parameter
   that is not `own` reaches it, since the caller holds that memory before
   the call and after it, and when it is a static field, a global. A
   local that holds a copy of such a parameter reaches the same memory. A place
   in a local, or reached through a reference to one, lives as long as
   that local. Memory the function allocated, or reached through a pointer
   it read, has an end the function cannot prove, so it keeps the run-time
   checks. */
static struct anchor anchor_of(const struct escape_walk *w,
                               const struct expr *e)
{
    struct anchor a = {ANCHOR_UNKNOWN, 0, NULL, false};
    const struct escape_local *r;
    const struct expr *base;

    if (e->symbol != NULL && e->symbol->kind == SYMBOL_GLOBAL) {
        a.kind = ANCHOR_OUTLIVES;
        a.global = true;
        return a;
    }
    switch (e->kind) {
    case EXPR_NAME:
        r = find(w, e->symbol);
        if (r != NULL) {
            a.kind = ANCHOR_LOCAL;
            a.local = (size_t)(r - w->locals);
        }
        return a;
    case EXPR_FIELD:
    case EXPR_INDEX:
    case EXPR_UNARY:
        if (e->kind == EXPR_UNARY) {
            if (e->as.unary.op != TOKEN_STAR) {
                return a;
            }
            base = e->as.unary.operand;
        } else {
            base = e->kind == EXPR_FIELD ? e->as.field.base
                                         : e->as.index.base;
            if (base->type == NULL || (base->type->kind != TYPE_POINTER &&
                                       base->type->kind != TYPE_SLICE)) {
                return anchor_of(w, base);
            }
        }
        r = base->kind == EXPR_NAME ? reached_param(w, base) : NULL;
        if (r != NULL) {
            a.kind = ANCHOR_OUTLIVES;
            a.through = &r->symbol->name;
            return a;
        }
        r = root_of(w, base);
        if (r != NULL) {
            a.kind = ANCHOR_LOCAL;
            a.local = (size_t)(r - w->locals);
        }
        return a;
    default:
        return a;
    }
}

/* The message of a store into the place target, which a reference to
   root would outlive. */
static void refuse_store(struct escape_walk *w, const struct expr *target,
                         const struct expr *value, const struct anchor *a,
                         const struct escape_local *root)
{
    if (a->global) {
        refuse(w, value->pos, root, ESCAPE_GLOBAL, place_name(target));
    } else if (target->kind == EXPR_FIELD) {
        refuse(w, value->pos, root, ESCAPE_FIELD, &target->as.field.name);
    } else if (target->kind == EXPR_INDEX) {
        refuse(w, value->pos, root, ESCAPE_ELEMENT,
               place_name(target->as.index.base));
    } else if (a->through != NULL) {
        refuse(w, value->pos, root, ESCAPE_THROUGH, a->through);
    } else if (a->kind == ANCHOR_LOCAL) {
        refuse(w, value->pos, root, ESCAPE_OUTER,
               &w->locals[a->local].symbol->name);
    }
}

/* A `=` of value to target, or an atomic store of value into it. */
static void check_store(struct escape_walk *w, const struct expr *target,
                        const struct expr *value)
{
    const struct escape_local *root = root_of(w, value);
    struct anchor a = anchor_of(w, target);
    struct escape_local *held;

    if (target->kind == EXPR_NAME && a.kind == ANCHOR_LOCAL) {
        w->locals[a.local].reaches = index_of(w, reached_param(w, value));
    }
    if (root == NULL) {
        /* A new value that holds no reference clears a local. */
        if (target->kind == EXPR_NAME && a.kind == ANCHOR_LOCAL) {
            w->locals[a.local].refers = 0;
        }
        return;
    }
    if (a.kind == ANCHOR_OUTLIVES) {
        refuse_store(w, target, value, &a, root);
        return;
    }
    if (a.kind != ANCHOR_LOCAL) {
        return;
    }
    held = &w->locals[a.local];
    if (held->depth < root->depth) {
        refuse_store(w, target, value, &a, root);
        return;
    }
    held->refers = index_of(w, target->kind == EXPR_NAME
                                   ? root
                                   : deeper(referred(w, held), root));
}

/* Whether t is a collection of `anti.collection`, or a class that
   inherits one. */
static bool is_collection(const struct type *t)
{
    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        const struct type *home = t->generic != NULL ? t->generic : t;
        if (sema_name_is(&home->module, COLLECTION_MODULE) &&
            sema_name_is(&home->name, COLLECTION_BASE)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a function of a collection that takes an argument as `own`
   stores it in the collection, so the argument lives as long as the
   collection does. The checker writes `all.push(x)` as the call of
   `List.push` with `&all` first, and the collection is the place that
   first argument reaches. */
static void check_collection(struct escape_walk *w, const struct expr *call)
{
    const struct expr *callee = call->as.call.callee;
    const struct symbol *fn = callee->symbol;
    const struct expr *receiver;
    const struct type *t;
    size_t i;

    if (callee->kind != EXPR_NAME || fn == NULL || fn->item == NULL ||
        !fn->item->has_self || fn->owned == NULL ||
        call->as.call.arg_count == 0) {
        return;
    }
    receiver = call->as.call.args[0];
    t = receiver->type != NULL && receiver->type->kind == TYPE_POINTER
            ? receiver->type->element
            : NULL;
    if (!is_collection(t)) {
        return;
    }
    for (i = 1; i < call->as.call.arg_count && i < fn->owned_count; i++) {
        const struct escape_local *root;
        struct anchor a;
        const struct expr *place;

        if (!fn->owned[i]) {
            continue;
        }
        root = root_of(w, call->as.call.args[i]);
        if (root == NULL) {
            continue;
        }
        place = receiver;
        if (place->kind == EXPR_UNARY && place->as.unary.op == TOKEN_AMP) {
            place = place->as.unary.operand;
            a = anchor_of(w, place);
        } else {
            a.kind = ANCHOR_UNKNOWN;
            a.through = NULL;
            a.global = false;
            a.local = 0;
            if (place->kind == EXPR_NAME && reached_param(w, place) != NULL) {
                a.kind = ANCHOR_OUTLIVES;
            }
        }
        if (a.kind == ANCHOR_OUTLIVES ||
            (a.kind == ANCHOR_LOCAL &&
             w->locals[a.local].depth < root->depth)) {
            refuse(w, call->as.call.args[i]->pos, root, ESCAPE_COLLECTION,
                   place_name(place));
        }
    }
}

/* The local whose reference a closure captures, through a local that
   holds one, or NULL. */
static const struct escape_local *captured_root(const struct escape_walk *w,
                                                const struct item *fn)
{
    const struct escape_local *r = NULL;
    size_t i;

    if (fn == NULL || fn->snapshot) {
        return NULL;
    }
    for (i = 0; i < fn->capture_count; i++) {
        const struct escape_local *held = find(w, fn->captures[i].symbol);
        if (held != NULL) {
            r = deeper(r, referred(w, held));
        }
    }
    return r;
}

/* DESIGN: a closure at a `concurrent` parameter goes on to `parallel` or
   `dispatch`, so a reference it captures reaches other threads, as one
   passed to either does. */
static void check_concurrent(struct escape_walk *w, const struct expr *call)
{
    const struct type *t = call->as.call.callee->type;
    const struct name *params = NULL;
    const struct symbol *fn = call->as.call.callee->symbol;
    size_t i;

    if (t == NULL || t->kind != TYPE_FN) {
        return;
    }
    for (i = 0; i < call->as.call.arg_count && i < t->param_count; i++) {
        const struct expr *arg = call->as.call.args[i];
        const struct type *p = t->params[i];
        const struct item *closure = NULL;
        const struct escape_local *root;
        const struct item *it;
        size_t at;

        if (p == NULL || p->kind != TYPE_FN || !p->concurrent) {
            continue;
        }
        if (arg->kind == EXPR_FN) {
            closure = arg->as.fn;
        } else if (arg->kind == EXPR_NAME && arg->symbol != NULL) {
            closure = arg->symbol->closure;
        }
        root = captured_root(w, closure);
        if (root == NULL) {
            continue;
        }
        it = fn != NULL ? fn->item : NULL;
        at = it != NULL && it->has_self ? i - 1 : i;
        params = it != NULL && at < it->param_count ? &it->params[at].name
                                                    : NULL;
        refuse(w, arg->pos, root, ESCAPE_CONCURRENT, params);
    }
}

/* DESIGN: E1 as "Compile-time checks of 2026-10-20" in docs/decisions.md
   corrects it on 2026-10-25. `parallel` waits for every chunk before it
   gives its value, so its array and the arguments of its worker are lent
   for the length of the expression and never refused. A job of
   `dispatch` runs on until it is joined, so a local reference passed to
   it as the object or as an argument of the worker is refused, unless
   the block joins the job while the local lives: walk_block finds that
   join and marks the `dispatch` in w->joined. */
static void check_dispatch(struct escape_walk *w, const struct expr *e)
{
    const struct expr *call = e->as.dispatch.call;
    const struct escape_local *root = root_of(w, e->as.dispatch.object);
    size_t i;

    if (root != NULL) {
        refuse(w, e->as.dispatch.object->pos, root, ESCAPE_DISPATCH, NULL);
        return;
    }
    for (i = 0; call != NULL && call->kind == EXPR_CALL &&
                i < call->as.call.arg_count;
         i++) {
        root = root_of(w, call->as.call.args[i]);
        if (root != NULL) {
            refuse(w, call->as.call.args[i]->pos, root, ESCAPE_DISPATCH,
                   NULL);
            return;
        }
    }
}

/* Whether e joins the job sym whenever it is evaluated: `join(sym)`, or
   an operand, a cast, an argument or a part of an `f"..."` that does.
   The right side of `&&`, `||` and `??` and a call after `?.` may not
   run. */
static bool joins(const struct expr *e, const struct symbol *sym)
{
    size_t i;

    if (e == NULL) {
        return false;
    }
    switch (e->kind) {
    case EXPR_JOIN:
        return !e->as.join.all && e->as.join.job->kind == EXPR_NAME &&
               e->as.join.job->symbol == sym;
    case EXPR_BINARY:
        if (joins(e->as.binary.left, sym)) {
            return true;
        }
        return e->as.binary.op != TOKEN_AND_AND &&
               e->as.binary.op != TOKEN_OR_OR &&
               e->as.binary.op != TOKEN_QUESTION_QUESTION &&
               joins(e->as.binary.right, sym);
    case EXPR_UNARY:
        return joins(e->as.unary.operand, sym);
    case EXPR_CAST:
        return joins(e->as.cast.operand, sym);
    case EXPR_CALL:
        for (i = 0; !e->as.call.optional && i < e->as.call.arg_count; i++) {
            if (joins(e->as.call.args[i], sym)) {
                return true;
            }
        }
        return false;
    case EXPR_FORMAT:
        for (i = 0; i < e->as.format.count; i++) {
            if (joins(e->as.format.parts[i].value, sym)) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

/* What a scan for a way out of a block has met. */
struct leave_scan {
    const struct symbol *job;
    bool leaves;                    /* a way out of the block, or a use */
    bool in_loop;                   /* inside a loop the scan entered */
    bool in_handler;                /* inside a handler the scan entered */
};

static bool leave_stmt(void *data, const struct stmt *s);

static bool leave_expr(void *data, const struct expr *e)
{
    struct leave_scan *scan = data;

    if (e->kind == EXPR_NAME && scan->job != NULL && e->symbol == scan->job) {
        scan->leaves = true;
    }
    if (e->kind == EXPR_CALL && (e->as.call.handler.kind == HANDLE_TRY)) {
        scan->leaves = true;
    }
    if (e->kind == EXPR_CALL && e->as.call.handler.kind == HANDLE_BLOCK &&
        !scan->in_handler) {
        struct leave_scan inner = *scan;
        struct ast_visitor v = {leave_expr, leave_stmt, NULL};
        size_t i;

        v.data = scan;
        ast_walk_expr(&v, e->as.call.callee);
        for (i = 0; i < e->as.call.arg_count; i++) {
            ast_walk_expr(&v, e->as.call.args[i]);
        }
        inner.in_handler = true;
        v.data = &inner;
        ast_walk_block(&v, e->as.call.handler.body);
        scan->leaves = scan->leaves || inner.leaves;
        return false;
    }
    return !scan->leaves;
}

static bool leave_stmt(void *data, const struct stmt *s)
{
    struct leave_scan *scan = data;

    switch (s->kind) {
    case STMT_RETURN:
    case STMT_FAIL:
    case STMT_FALLTHROUGH:
        scan->leaves = true;
        break;
    case STMT_YIELD:
        scan->leaves = scan->leaves || !scan->in_handler;
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        scan->leaves = scan->leaves || !scan->in_loop;
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
    case STMT_FOR:
        if (!scan->in_loop) {
            struct leave_scan inner = *scan;
            struct ast_visitor v = {leave_expr, leave_stmt, NULL};

            inner.in_loop = true;
            v.data = &inner;
            ast_walk_stmt(&v, s);
            scan->leaves = scan->leaves || inner.leaves;
            return false;
        }
        break;
    case STMT_LET:
        if (s->as.let.guard.kind == HANDLE_BLOCK && !scan->in_handler) {
            struct leave_scan inner = *scan;
            struct ast_visitor v = {leave_expr, leave_stmt, NULL};

            v.data = scan;
            ast_walk_expr(&v, s->as.let.value);
            ast_walk_block(&v, s->as.let.otherwise);
            inner.in_handler = true;
            v.data = &inner;
            ast_walk_block(&v, s->as.let.guard.body);
            scan->leaves = scan->leaves || inner.leaves;
            return false;
        }
        break;
    default:
        break;
    }
    return !scan->leaves;
}

/* Whether the statement s may leave the block it stands in, or names the
   job sym, which may then be given away before its join. A NULL sym
   names no job. A loop or a handler s holds keeps its own `break`,
   `continue` and `yield`, and a closure its own `return`. A `return`
   that joins the job leaves after the join, so its value alone is
   read. */
static bool may_leave(const struct stmt *s, const struct symbol *sym)
{
    struct leave_scan scan = {sym, false, false, false};
    struct ast_visitor v = {leave_expr, leave_stmt, NULL};

    v.data = &scan;
    if (s->kind == STMT_RETURN && sym == NULL) {
        ast_walk_expr(&v, s->as.return_value);
    } else {
        ast_walk_stmt(&v, s);
    }
    return scan.leaves;
}

/* Whether the statement s joins the job sym whenever it runs. */
static bool joins_stmt(const struct stmt *s, const struct symbol *sym)
{
    switch (s->kind) {
    case STMT_LET:
        return joins(s->as.let.value, sym);
    case STMT_EXPR:
        return joins(s->as.expr, sym);
    case STMT_ASSIGN:
        return joins(s->as.assign.value, sym);
    case STMT_RETURN:
        return joins(s->as.return_value, sym);
    default:
        return false;
    }
}

/* Whether the `let` b->stmts[at] binds a job of `dispatch` that a later
   statement of b joins on every path. No statement between the two may
   leave the block or name the job, and a closure may not capture it. */
static bool joined_later(const struct block *b, size_t at)
{
    const struct stmt *s = b->stmts[at];
    const struct symbol *job = s->as.let.symbol;
    size_t i;

    if (s->kind != STMT_LET || s->as.let.name_count != 0 || job == NULL ||
        job->captured || s->as.let.value == NULL ||
        s->as.let.value->kind != EXPR_DISPATCH) {
        return false;
    }
    for (i = at + 1; i < b->count; i++) {
        if (joins_stmt(b->stmts[i], job)) {
            return !may_leave(b->stmts[i], NULL);
        }
        if (may_leave(b->stmts[i], job)) {
            return false;
        }
    }
    return false;
}

/* An anonymous function is a frame of its own: its `return` leaves it,
   and no `yield` crosses into it. */
static void walk_function(struct escape_walk *w, const struct item *it)
{
    const struct item *frame = w->frame;
    int handler = w->handler;
    int ends = it->body != NULL ? it->body->end.line : 0;
    size_t i;

    w->frame = it;
    w->handler = -1;
    w->depth++;
    if (it->self != NULL) {
        declare(w, it->self, w->depth, ends);
    }
    for (i = 0; i < it->param_count; i++) {
        if (it->params[i].symbol != NULL) {
            declare(w, it->params[i].symbol, w->depth, ends);
        }
    }
    walk_block(w, it->body, false);
    w->depth--;
    w->frame = frame;
    w->handler = handler;
}

static void walk_handler(struct escape_walk *w, const struct handler *h)
{
    int handler = w->handler;

    if (h->kind != HANDLE_BLOCK || h->body == NULL) {
        return;
    }
    w->handler = w->depth + 1;
    if (h->symbol != NULL) {
        declare(w, h->symbol, w->depth + 1, h->body->end.line);
    }
    walk_block(w, h->body, true);
    w->handler = handler;
}

static void walk_list(struct escape_walk *w, struct expr *const *list,
                      size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        walk_expr(w, list[i]);
    }
}

static bool visit_expr(void *data, const struct expr *e)
{
    struct escape_walk *w = data;

    switch (e->kind) {
    case EXPR_CALL:
        check_collection(w, e);
        check_concurrent(w, e);
        walk_expr(w, e->as.call.callee);
        walk_list(w, e->as.call.args, e->as.call.arg_count);
        walk_list(w, e->as.call.hash_calls, e->as.call.hash_count);
        walk_handler(w, &e->as.call.handler);
        return false;
    case EXPR_FN:
        walk_function(w, e->as.fn);
        return false;
    case EXPR_DISPATCH:
        if (e != w->joined) {
            check_dispatch(w, e);
        }
        return true;
    case EXPR_ATOMIC:
        if (e->as.atomic.op == ATOMIC_STORE ||
            e->as.atomic.op == ATOMIC_SWAP) {
            check_store(w, e->as.atomic.place, e->as.atomic.a);
        } else if (e->as.atomic.b != NULL) {
            check_store(w, e->as.atomic.place, e->as.atomic.b);
        }
        return true;
    default:
        return true;
    }
}

static void walk_expr(struct escape_walk *w, const struct expr *e)
{
    struct ast_visitor v = {visit_expr, NULL, NULL};

    v.data = w;
    ast_walk_expr(&v, e);
}

/* Declare the names a `let` binds, each holding the reference its part
   of the value holds. */
static void walk_let(struct escape_walk *w, const struct stmt *s)
{
    const struct expr *value = s->as.let.value;
    size_t i;
    size_t at;

    walk_expr(w, value);
    if (s->as.let.otherwise != NULL) {
        walk_block(w, s->as.let.otherwise, true);
    }
    walk_handler(w, &s->as.let.guard);
    if (s->as.let.name_count == 0) {
        if (s->as.let.symbol != NULL) {
            at = declare(w, s->as.let.symbol, w->depth, w->ends);
            w->locals[at].refers = index_of(w, root_of(w, value));
            w->locals[at].reaches = index_of(w, reached_param(w, value));
        }
        return;
    }
    for (i = 0; i < s->as.let.name_count; i++) {
        const struct expr *part =
            value != NULL && value->kind == EXPR_TUPLE &&
                    i < value->as.tuple.count
                ? value->as.tuple.elements[i]
                : value;
        if (s->as.let.names[i].symbol == NULL ||
            s->as.let.names[i].assigns) {
            continue;
        }
        at = declare(w, s->as.let.names[i].symbol, w->depth, w->ends);
        w->locals[at].refers = index_of(w, root_of(w, part));
        w->locals[at].reaches = index_of(w, reached_param(w, part));
    }
}

/* The arms of a `switch` and of a `select`, each with the name it binds
   declared for its body. The name an arm of a `switch` binds is a part of
   value, the value switched on, and holds what value holds, as the name
   of `let ... else` does. A `select` binds a value of a channel, which
   holds no reference, and passes NULL. */
static void walk_arms(struct escape_walk *w, const struct switch_arm *arms,
                      size_t count, const struct expr *value)
{
    size_t i;
    size_t at;

    for (i = 0; arms != NULL && i < count; i++) {
        const struct stmt *body = arms[i].body;
        walk_expr(w, arms[i].test != NULL ? arms[i].test : arms[i].value);
        if (arms[i].bound != NULL && body != NULL) {
            at = declare(w, arms[i].bound, w->depth + 1,
                         body->kind == STMT_BLOCK ? body->as.block->end.line
                                                  : body->pos.line);
            if (carries(arms[i].bound->type)) {
                w->locals[at].refers = index_of(w, root_of(w, value));
                w->locals[at].reaches = index_of(w, reached_param(w, value));
            }
        }
        w->depth++;
        walk_stmt(w, body);
        w->depth--;
    }
}

static void walk_for(struct escape_walk *w, const struct stmt *s)
{
    const struct iteration *it = &s->as.for_loop.hooks;
    const struct block *body = s->as.for_loop.body;
    size_t i;

    walk_expr(w, s->as.for_loop.low);
    walk_expr(w, s->as.for_loop.high);
    walk_expr(w, s->as.for_loop.step);
    if (it->start == NULL) {
        walk_expr(w, s->as.for_loop.over);
    }
    walk_expr(w, it->start);
    walk_expr(w, it->advance);
    walk_expr(w, it->current);
    for (i = 0; body != NULL && i < s->as.for_loop.name_count; i++) {
        if (s->as.for_loop.names[i].symbol != NULL) {
            declare(w, s->as.for_loop.names[i].symbol, w->depth + 1,
                    body->end.line);
        }
    }
    walk_block(w, body, true);
}

static void walk_return(struct escape_walk *w, const struct stmt *s)
{
    const struct escape_local *root;

    walk_expr(w, s->as.return_value);
    root = root_of(w, s->as.return_value);
    if (root != NULL && root->symbol->frame == w->frame) {
        refuse(w, s->as.return_value->pos, root, ESCAPE_RETURNED, NULL);
    }
}

/* DESIGN: a `yield` gives its value to the expression of the handler,
   which stands outside the handler's block. A reference to a local of
   that block outlives it. One to a local outside the handler goes on
   with the value, and the pass follows no value of a call. */
static void walk_yield(struct escape_walk *w, const struct stmt *s)
{
    const struct escape_local *root;

    walk_expr(w, s->as.yielded);
    root = root_of(w, s->as.yielded);
    if (root != NULL && w->handler >= 0 && root->depth >= w->handler) {
        refuse(w, s->as.yielded->pos, root, ESCAPE_YIELDED, NULL);
    }
}

static void walk_stmt(struct escape_walk *w, const struct stmt *s)
{
    size_t i;

    if (s == NULL) {
        return;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        walk_let(w, s);
        return;
    case STMT_EXPR:
        walk_expr(w, s->as.expr);
        return;
    case STMT_ASSIGN:
        walk_expr(w, s->as.assign.target);
        walk_expr(w, s->as.assign.value);
        if (s->as.assign.op == TOKEN_ASSIGN) {
            check_store(w, s->as.assign.target, s->as.assign.value);
        }
        return;
    case STMT_IF:
        for (i = 0; i < s->as.if_chain.count; i++) {
            walk_expr(w, s->as.if_chain.branches[i].cond);
            walk_block(w, s->as.if_chain.branches[i].body, true);
        }
        walk_block(w, s->as.if_chain.else_body, true);
        return;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        walk_expr(w, s->as.loop.cond);
        walk_block(w, s->as.loop.body, true);
        return;
    case STMT_FOR:
        walk_for(w, s);
        return;
    case STMT_DEFER:
    case STMT_UNDO:
        walk_stmt(w, s->as.deferred);
        return;
    case STMT_FAIL:
        walk_expr(w, s->as.fail.value);
        return;
    case STMT_SWITCH:
        walk_expr(w, s->as.switch_stmt.value);
        walk_arms(w, s->as.switch_stmt.arms, s->as.switch_stmt.count,
                  s->as.switch_stmt.value);
        w->depth++;
        walk_stmt(w, s->as.switch_stmt.otherwise);
        w->depth--;
        return;
    case STMT_ASSERT:
        walk_expr(w, s->as.assertion.cond);
        return;
    case STMT_RETURN:
        walk_return(w, s);
        return;
    case STMT_YIELD:
        walk_yield(w, s);
        return;
    case STMT_TRY:
        walk_block(w, s->as.try_block.body, true);
        walk_handler(w, &s->as.try_block.handler);
        return;
    case STMT_BLOCK:
        walk_block(w, s->as.block, true);
        return;
    case STMT_SYNC:
        walk_expr(w, s->as.sync.mutex);
        walk_expr(w, s->as.sync.second);
        walk_block(w, s->as.sync.body, true);
        return;
    case STMT_SELECT:
        walk_arms(w, s->as.select.arms, s->as.select.count, NULL);
        return;
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_FALLTHROUGH:
        return;
    }
}

/* Walk the statements of b. The body of a function stands at the depth
   of its parameters, which end with it, and every other block one deeper
   than the block around it. */
static void walk_block(struct escape_walk *w, const struct block *b,
                       bool deep)
{
    int ends = w->ends;
    size_t i;

    if (b == NULL) {
        return;
    }
    if (deep) {
        w->depth++;
    }
    w->ends = b->end.line;
    for (i = 0; i < b->count; i++) {
        const struct expr *joined = w->joined;

        if (joined_later(b, i)) {
            w->joined = b->stmts[i]->as.let.value;
        }
        walk_stmt(w, b->stmts[i]);
        w->joined = joined;
    }
    w->ends = ends;
    if (deep) {
        w->depth--;
    }
}

void sema_check_escapes(struct checker *c, const struct item *it,
                        size_t first)
{
    struct escape_walk w;
    size_t i;

    /* A body with an error has types the pass cannot trust. */
    for (i = first; i < c->diags->count; i++) {
        if (!c->diags->items[i].warning) {
            return;
        }
    }
    if (it->body == NULL || c->ctx.quiet > 0) {
        return;
    }
    memset(&w, 0, sizeof w);
    w.c = c;
    w.handler = -1;
    walk_function(&w, it);
    free(w.locals);
}
