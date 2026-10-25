/* The safety check `index-out-of-range`, C2 of "Compile-time checks of
   2026-10-20" in docs/decisions.md: an index of an array, a slice or a
   `str` that is out of range on every path that reaches it. The pass
   reads a function body once the checker has checked it, with the
   types and the symbols in place. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "arith.h"
#include "sema_checker.h"

/* DESIGN: the pass knows the value of an integer expression as a range
   whose two ends are values some run takes. A constant, `.len` of a
   fixed array and of a literal `str`, and a local that holds one value
   on every path are a range of one value. A local holds one value when
   the function never assigns it or takes its address after its `let`,
   and its value is then that of its `let`. A parameter the function
   never changes holds its unknown value, and a slice or a `str` that the
   function never changes holds its unknown length, which is 0 or more.
   An end is therefore a number, or such an unknown plus a number. The
   variable of a `for` over a range takes every value of the range, so
   its range runs from the least to the greatest value of its turns.
   `+`, `-` and `*` by a constant move and stretch the ends, and every
   other expression is unknown.

   An index whose range reaches below zero or reaches the length of its
   base is refused when every path that reaches the index takes such a
   value. A range of one value is the same on every path, so its index
   is refused wherever it stands. A range of a loop variable is reached
   when every turn of the loop runs to its end and reaches the index:
   the body leaves no turn early and holds no loop that may not end, and
   the index stands outside every `if`, arm, handler and condition of
   the body. Every other index keeps the bounds check of dev mode. */

/* A value of every run: base plus add, base NULL for a number. base is
   a slice or a `str`, whose length the value then is, or an integer,
   whose value it is. */
struct end {
    const struct symbol *base;
    int64_t add;
};

/* The values of an expression, from lo to hi, both of which some run
   takes. loop names the `for` whose turns take them, NULL when the
   expression has one value on every path. */
struct range {
    bool known;
    const struct stmt *loop;
    struct end lo;
    struct end hi;
};

/* What the pass knows of a local, a parameter or the variable of a
   `for`. */
struct fact {
    const struct symbol *symbol;
    const struct expr *value;       /* the value of its `let`, or NULL */
    const struct stmt *loop;        /* the `for` whose variable it is */
    bool param;
    bool changed;
    bool busy;                      /* its value is being read */
};

/* A `for` of the function, whether every turn of it runs to its end,
   and the indices every turn reaches. */
struct turns {
    const struct stmt *loop;
    bool whole;
    const struct expr **sure;
    size_t count;
    size_t capacity;
};

struct index_walk {
    struct checker *c;
    struct fact *facts;
    size_t count;
    size_t capacity;
    struct turns *loops;
    size_t loop_count;
    size_t loop_capacity;
};

/* The pass refuses to follow a value past this bound, so no sum or
   product of two followed values leaves an int64_t. */
#define VALUE_BOUND ((int64_t)1 << 40)

static struct range range_of(struct index_walk *w, const struct expr *e);

static struct range unknown(void)
{
    struct range r;

    memset(&r, 0, sizeof r);
    return r;
}

static struct range number(int64_t v)
{
    struct range r = unknown();

    r.known = v > -VALUE_BOUND && v < VALUE_BOUND;
    r.lo.add = v;
    r.hi.add = v;
    return r;
}

static struct fact *fact_of(struct index_walk *w, const struct symbol *sym)
{
    size_t i;

    for (i = 0; i < w->count; i++) {
        if (w->facts[i].symbol == sym) {
            return &w->facts[i];
        }
    }
    return NULL;
}

static struct fact *add_fact(struct index_walk *w, const struct symbol *sym)
{
    struct fact *f;

    if (sym == NULL) {
        return NULL;
    }
    f = fact_of(w, sym);
    if (f != NULL) {
        return f;
    }
    if (w->count == w->capacity) {
        w->capacity = w->capacity == 0 ? 16 : w->capacity * 2;
        w->facts = alloc_resize(w->facts, w->capacity, sizeof *w->facts);
    }
    f = &w->facts[w->count++];
    memset(f, 0, sizeof *f);
    f->symbol = sym;
    return f;
}

static void mark_changed(struct index_walk *w, const struct expr *e)
{
    struct fact *f;

    if (e != NULL && e->kind == EXPR_NAME && e->symbol != NULL) {
        f = add_fact(w, e->symbol);
        f->changed = true;
    }
}

static bool collect_expr(void *data, const struct expr *e);

static bool collect_stmt(void *data, const struct stmt *s)
{
    struct index_walk *w = data;
    struct fact *f;
    size_t i;

    switch (s->kind) {
    case STMT_LET:
        for (i = 0; i < s->as.let.name_count; i++) {
            if (s->as.let.names[i].assigns) {
                f = add_fact(w, s->as.let.names[i].symbol);
                if (f != NULL) {
                    f->changed = true;
                }
            }
        }
        if (s->as.let.name_count == 0 && s->as.let.symbol != NULL) {
            f = add_fact(w, s->as.let.symbol);
            f->value = s->as.let.value;
            f->changed = f->changed || s->as.let.atomic;
        }
        break;
    case STMT_ASSIGN:
        mark_changed(w, s->as.assign.target);
        break;
    case STMT_FOR:
        if (s->as.for_loop.over == NULL && !s->as.for_loop.pattern &&
            s->as.for_loop.name_count == 1 &&
            s->as.for_loop.names[0].symbol != NULL) {
            f = add_fact(w, s->as.for_loop.names[0].symbol);
            f->loop = s;
        }
        break;
    default:
        break;
    }
    return true;
}

/* A closure may change a local it captures, so the walk that finds the
   changes enters the body of every anonymous function. The parameters of
   one hold one unknown value each, as those of the function do. */
static bool collect_expr(void *data, const struct expr *e)
{
    struct index_walk *w = data;
    struct ast_visitor v = {collect_expr, collect_stmt, NULL};
    struct fact *f;
    size_t i;

    v.data = w;
    for (i = 0; e->kind == EXPR_FN && e->as.fn != NULL &&
                i < e->as.fn->param_count;
         i++) {
        f = add_fact(w, e->as.fn->params[i].symbol);
        if (f != NULL) {
            f->param = true;
        }
    }
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_AMP) {
        mark_changed(w, e->as.unary.operand);
    }
    if (e->kind == EXPR_CALL) {
        mark_changed(w, e->as.call.out);
    }
    if (e->kind == EXPR_FN && e->as.fn != NULL) {
        ast_walk_block(&v, e->as.fn->body);
    }
    return true;
}

static bool is_length_base(const struct symbol *sym)
{
    return sym->type != NULL && (sym->type->kind == TYPE_SLICE ||
                                 sym->type->kind == TYPE_STR);
}

/* Whether a < b on every run. An unknown length is 0 or more. */
static bool below(struct end a, struct end b)
{
    if (a.base == b.base) {
        return a.add < b.add;
    }
    return a.base == NULL && is_length_base(b.base) && a.add < b.add;
}

static bool at_most(struct end a, struct end b)
{
    if (a.base == b.base) {
        return a.add <= b.add;
    }
    return a.base == NULL && is_length_base(b.base) && a.add <= b.add;
}

static bool end_shift(struct end *e, struct end by, bool minus)
{
    int64_t add = minus ? e->add - by.add : e->add + by.add;

    if (by.base != NULL) {
        if (minus && e->base == by.base) {
            e->base = NULL;
        } else if (!minus && e->base == NULL) {
            e->base = by.base;
        } else {
            return false;
        }
    }
    e->add = add;
    return add > -VALUE_BOUND && add < VALUE_BOUND;
}

/* The range of the variable of the `for` s: the least and the greatest
   value of its turns. A range with no turn, and one whose turns the
   pass cannot count, is unknown. */
static struct range loop_range(struct index_walk *w, const struct stmt *s)
{
    struct range low = range_of(w, s->as.for_loop.low);
    struct range high = range_of(w, s->as.for_loop.high);
    int64_t k = s->as.for_loop.step_value;
    struct range r = unknown();

    if (!low.known || !high.known || low.loop != NULL ||
        high.loop != NULL || !below(low.lo, high.lo)) {
        return r;
    }
    if (k < 0) {
        k = k == INT64_MIN ? 0 : -k;
    }
    r.lo = low.lo;
    r.hi = high.lo;
    if (low.lo.base == NULL && high.lo.base == NULL && k > 0) {
        r.hi.add = low.lo.add +
                   (high.lo.add - 1 - low.lo.add) / k * k;
    } else if (k == 1) {
        r.hi.add--;
    } else {
        return r;
    }
    r.known = true;
    r.loop = s;
    return r;
}

static struct range name_range(struct index_walk *w, const struct expr *e)
{
    const struct symbol *sym = e->symbol;
    struct fact *f;
    struct range r = unknown();
    struct const_value v;
    struct context saved;
    bool constant;

    if (sym == NULL) {
        return r;
    }
    if (sym->kind == SYMBOL_CONST) {
        sema_enter_quiet(w->c, &saved);
        constant = sema_eval_const(w->c, (struct expr *)e, &v);
        sema_leave(w->c, &saved);
        if (!constant || v.kind != CONST_INT) {
            return r;
        }
        return number(types_is_signed(e->type)
                          ? arith_signed(v.as.integer, types_bits(e->type))
                          : v.as.integer < (uint64_t)VALUE_BOUND
                                ? (int64_t)v.as.integer
                                : VALUE_BOUND);
    }
    f = fact_of(w, sym);
    if (f == NULL || f->changed || f->busy || sym->address_taken) {
        return r;
    }
    if (f->loop != NULL) {
        f->busy = true;
        r = loop_range(w, f->loop);
        f->busy = false;
        return r;
    }
    if (f->value != NULL) {
        f->busy = true;
        r = range_of(w, f->value);
        f->busy = false;
        if (r.known) {
            return r;
        }
    }
    if (f->value != NULL || f->param) {
        r.known = true;
        r.lo.base = sym;
        r.hi.base = sym;
    }
    return r;
}

/* The length of the slice or the `str` that sym names, which the
   function never changes, as an end. */
static bool length_of_symbol(struct index_walk *w, const struct symbol *sym,
                             struct end *out)
{
    struct fact *f = fact_of(w, sym);
    const struct expr *value;
    struct range low;
    struct range high;
    size_t i;

    if (f == NULL || f->changed || f->busy || sym->address_taken ||
        !is_length_base(sym) || (f->value == NULL && !f->param)) {
        return false;
    }
    out->base = sym;
    out->add = 0;
    value = f->value;
    if (value == NULL) {
        return true;
    }
    f->busy = true;
    if (value->kind == EXPR_STRING && sym->type->kind == TYPE_STR) {
        out->base = NULL;
        out->add = (int64_t)value->as.text.length;
    } else if (value->kind == EXPR_SLICE) {
        low = range_of(w, value->as.slice.low);
        high = range_of(w, value->as.slice.high);
        if (low.known && high.known && low.loop == NULL &&
            high.loop == NULL) {
            struct end n = high.lo;
            if (end_shift(&n, low.lo, true)) {
                *out = n;
            }
        }
    } else if (value->kind == EXPR_SLICE_LIT) {
        for (i = 0; i < value->as.slice_lit.field_count; i++) {
            if (sema_name_is(&value->as.slice_lit.fields[i].name, "len")) {
                high = range_of(w, value->as.slice_lit.fields[i].value);
                if (high.known && high.loop == NULL) {
                    *out = high.lo;
                }
            }
        }
    } else if (value->kind == EXPR_NAME && value->symbol != NULL) {
        struct end other;
        if (length_of_symbol(w, value->symbol, &other)) {
            *out = other;
        }
    }
    f->busy = false;
    /* A slice whose bounds cross stops a dev build where it is made, and
       its length names no element. */
    return out->base != NULL || out->add >= 0;
}

/* The length of the array, the slice or the `str` that base gives. */
static bool length_of(struct index_walk *w, const struct expr *base,
                      struct end *out)
{
    const struct type *t = base->type;

    out->base = NULL;
    out->add = 0;
    if (t == NULL) {
        return false;
    }
    /* The length of an array from `size_of` differs between targets, by
       the provisional entry on C2. */
    if (t->kind == TYPE_ARRAY) {
        if (t->length_of != NULL || t->length >= (uint64_t)VALUE_BOUND) {
            return false;
        }
        out->add = (int64_t)t->length;
        return true;
    }
    if (base->kind == EXPR_STRING) {
        out->add = (int64_t)base->as.text.length;
        return true;
    }
    if (base->kind == EXPR_NAME && base->symbol != NULL) {
        return length_of_symbol(w, base->symbol, out);
    }
    return false;
}

static struct range sum(struct range a, struct range b, bool minus)
{
    struct range r = unknown();
    struct end lo;
    struct end hi;

    if (!a.known || !b.known || (a.loop != NULL && b.loop != NULL)) {
        return r;
    }
    if (b.loop == NULL) {
        lo = a.lo;
        hi = a.hi;
        if (!end_shift(&lo, b.lo, minus) || !end_shift(&hi, b.lo, minus)) {
            return r;
        }
        r.loop = a.loop;
    } else if (!minus) {
        lo = b.lo;
        hi = b.hi;
        if (!end_shift(&lo, a.lo, false) || !end_shift(&hi, a.lo, false)) {
            return r;
        }
        r.loop = b.loop;
    } else {
        /* a - b of one value a and the turns of b: the ends swap. */
        if (b.lo.base != NULL || b.hi.base != NULL) {
            return r;
        }
        lo = a.lo;
        hi = a.lo;
        if (!end_shift(&lo, b.hi, true) || !end_shift(&hi, b.lo, true)) {
            return r;
        }
        r.loop = b.loop;
    }
    r.known = true;
    r.lo = lo;
    r.hi = hi;
    return r;
}

static struct range scale(struct range a, struct range by)
{
    int64_t k;
    struct range r = unknown();

    if (!a.known || !by.known || by.loop != NULL || by.lo.base != NULL ||
        a.lo.base != NULL || a.hi.base != NULL) {
        return r;
    }
    k = by.lo.add;
    if (k == 0) {
        return number(0);
    }
    /* Both ends and k lie inside the bound, so the test divides rather
       than multiplies, and no product leaves an int64_t. */
    if (llabs(a.lo.add) >= VALUE_BOUND / llabs(k) ||
        llabs(a.hi.add) >= VALUE_BOUND / llabs(k)) {
        return r;
    }
    r.known = true;
    r.loop = a.loop;
    r.lo.add = k > 0 ? a.lo.add * k : a.hi.add * k;
    r.hi.add = k > 0 ? a.hi.add * k : a.lo.add * k;
    return r;
}

/* Whether every value of r fits the integer type t. An unknown end
   fits a type of 64 bits alone, since the pass bounds the number added
   to it. */
static bool fits(struct range r, const struct type *t)
{
    int bits;
    int64_t least;
    int64_t most;

    if (t == NULL || !types_is_integer(t)) {
        return false;
    }
    bits = types_bits(t);
    if (r.lo.base != NULL || r.hi.base != NULL) {
        return bits == 64;
    }
    if (bits >= 64) {
        least = types_is_signed(t) ? INT64_MIN : 0;
        most = INT64_MAX;
    } else if (types_is_signed(t)) {
        least = -((int64_t)1 << (bits - 1));
        most = ((int64_t)1 << (bits - 1)) - 1;
    } else {
        least = 0;
        most = ((int64_t)1 << bits) - 1;
    }
    return r.lo.add >= least && r.hi.add <= most;
}

static struct range range_of(struct index_walk *w, const struct expr *e)
{
    struct range r = unknown();
    struct end n;

    if (e == NULL || e->type == NULL) {
        return r;
    }
    switch (e->kind) {
    case EXPR_INT:
        if (types_is_signed(e->type)) {
            r = number(arith_signed(e->as.integer, types_bits(e->type)));
        } else if (e->as.integer < (uint64_t)VALUE_BOUND) {
            r = number((int64_t)e->as.integer);
        }
        break;
    case EXPR_NAME:
        r = name_range(w, e);
        break;
    case EXPR_UNARY:
        if (e->as.unary.op == TOKEN_MINUS) {
            r = scale(range_of(w, e->as.unary.operand), number(-1));
        }
        break;
    case EXPR_BINARY:
        if (e->as.binary.carry) {
            break;
        }
        if (e->as.binary.op == TOKEN_PLUS || e->as.binary.op == TOKEN_MINUS) {
            r = sum(range_of(w, e->as.binary.left),
                    range_of(w, e->as.binary.right),
                    e->as.binary.op == TOKEN_MINUS);
        } else if (e->as.binary.op == TOKEN_STAR) {
            struct range a = range_of(w, e->as.binary.left);
            struct range b = range_of(w, e->as.binary.right);
            r = b.loop == NULL && b.lo.base == NULL ? scale(a, b)
                                                    : scale(b, a);
        }
        break;
    case EXPR_FIELD:
        if (sema_name_is(&e->as.field.name, "len") &&
            e->as.field.base != NULL && e->as.field.base->type != NULL &&
            (e->as.field.base->type->kind == TYPE_ARRAY ||
             e->as.field.base->type->kind == TYPE_SLICE ||
             e->as.field.base->type->kind == TYPE_STR) &&
            length_of(w, e->as.field.base, &n)) {
            r.known = true;
            r.lo = n;
            r.hi = n;
        }
        break;
    default:
        break;
    }
    if (r.known && !fits(r, e->type)) {
        return unknown();
    }
    return r;
}

/* Whether every turn of the `for` s runs to the end of its body. A turn
   may end early by `break`, `continue`, `return`, `fail` or `yield`, by
   a call whose failure leaves the loop or ends the program, and by a
   call of a function that returns `never`. A `while`, a `do` and a walk
   of a collection may not end. A `for` over a range always ends, and a
   `break` or a `continue` inside one ends that loop alone. */
struct whole_walk {
    int nested;                     /* the ranges inside the loop */
    bool whole;
};

static bool whole_stmt(void *data, const struct stmt *s);

static bool whole_expr(void *data, const struct expr *e)
{
    struct whole_walk *ww = data;
    const struct type *callee;

    if (e->tried.line != 0) {
        ww->whole = false;
    }
    if (e->kind == EXPR_CALL) {
        callee = e->as.call.callee != NULL ? e->as.call.callee->type : NULL;
        if (e->as.call.handler.kind == HANDLE_TRY ||
            e->as.call.handler.kind == HANDLE_ENCLOSING ||
            e->as.call.handler.kind == HANDLE_FATAL ||
            (callee != NULL && callee->kind == TYPE_FN && callee->never)) {
            ww->whole = false;
        }
    }
    return ww->whole;
}

static bool whole_stmt(void *data, const struct stmt *s)
{
    struct whole_walk *ww = data;
    struct ast_visitor v = {whole_expr, whole_stmt, NULL};

    v.data = ww;
    switch (s->kind) {
    case STMT_BREAK:
    case STMT_CONTINUE:
        if (ww->nested == 0) {
            ww->whole = false;
        }
        break;
    case STMT_RETURN:
    case STMT_FAIL:
    case STMT_YIELD:
    case STMT_FALLTHROUGH:
    case STMT_WHILE:
    case STMT_DO_WHILE:
        ww->whole = false;
        break;
    case STMT_FOR:
        if (s->as.for_loop.over != NULL) {
            ww->whole = false;
            break;
        }
        ast_walk_expr(&v, s->as.for_loop.low);
        ast_walk_expr(&v, s->as.for_loop.high);
        ww->nested++;
        ast_walk_block(&v, s->as.for_loop.body);
        ww->nested--;
        return false;
    default:
        break;
    }
    return ww->whole;
}

static void add_sure(struct turns *t, const struct expr *e)
{
    if (t->count == t->capacity) {
        t->capacity = t->capacity == 0 ? 8 : t->capacity * 2;
        t->sure = alloc_resize(t->sure, t->capacity, sizeof *t->sure);
    }
    t->sure[t->count++] = e;
}

/* Gather the indices inside e that every evaluation of e reaches: the
   operands of an operator, a call and its arguments, a literal and the
   parts of an index, a slice and a field. The right of `&&`, `||` and
   `??`, a handler, a closure and every other form may not run. */
static void sure_expr(struct turns *t, const struct expr *e)
{
    size_t i;

    if (e == NULL) {
        return;
    }
    switch (e->kind) {
    case EXPR_UNARY:
        sure_expr(t, e->as.unary.operand);
        return;
    case EXPR_BINARY:
        sure_expr(t, e->as.binary.left);
        if (e->as.binary.op != TOKEN_AND_AND &&
            e->as.binary.op != TOKEN_OR_OR &&
            e->as.binary.op != TOKEN_QUESTION_QUESTION) {
            sure_expr(t, e->as.binary.right);
        }
        return;
    case EXPR_CAST:
        sure_expr(t, e->as.cast.operand);
        return;
    case EXPR_CALL:
        sure_expr(t, e->as.call.callee);
        for (i = 0; i < e->as.call.arg_count; i++) {
            sure_expr(t, e->as.call.args[i]);
        }
        return;
    case EXPR_INDEX:
        sure_expr(t, e->as.index.base);
        sure_expr(t, e->as.index.index);
        add_sure(t, e);
        return;
    case EXPR_SLICE:
        sure_expr(t, e->as.slice.base);
        sure_expr(t, e->as.slice.low);
        sure_expr(t, e->as.slice.high);
        return;
    case EXPR_FIELD:
        sure_expr(t, e->as.field.base);
        return;
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            sure_expr(t, e->as.tuple.elements[i]);
        }
        return;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            sure_expr(t, e->as.array_lit.elements[i]);
        }
        return;
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            sure_expr(t, e->as.struct_lit.fields[i].value);
        }
        return;
    default:
        return;
    }
}

static void sure_block(struct turns *t, const struct block *b);

/* The indices a statement reaches whenever it runs, in a body whose
   every statement runs to its end. */
static void sure_stmt(struct turns *t, const struct stmt *s)
{
    switch (s->kind) {
    case STMT_LET:
        sure_expr(t, s->as.let.value);
        return;
    case STMT_EXPR:
        sure_expr(t, s->as.expr);
        return;
    case STMT_ASSIGN:
        sure_expr(t, s->as.assign.target);
        sure_expr(t, s->as.assign.value);
        return;
    case STMT_IF:
        if (s->as.if_chain.count > 0) {
            sure_expr(t, s->as.if_chain.branches[0].cond);
        }
        return;
    case STMT_FOR:
        sure_expr(t, s->as.for_loop.low);
        sure_expr(t, s->as.for_loop.high);
        return;
    case STMT_SWITCH:
        sure_expr(t, s->as.switch_stmt.value);
        return;
    case STMT_BLOCK:
        sure_block(t, s->as.block);
        return;
    case STMT_SYNC:
        sure_expr(t, s->as.sync.mutex);
        sure_expr(t, s->as.sync.second);
        sure_block(t, s->as.sync.body);
        return;
    default:
        return;
    }
}

static void sure_block(struct turns *t, const struct block *b)
{
    size_t i;

    for (i = 0; b != NULL && i < b->count; i++) {
        sure_stmt(t, b->stmts[i]);
    }
}

static struct turns *turns_of(struct index_walk *w, const struct stmt *s)
{
    struct turns *t;
    struct whole_walk ww = {0, true};
    struct ast_visitor v = {whole_expr, whole_stmt, NULL};
    size_t i;

    for (i = 0; i < w->loop_count; i++) {
        if (w->loops[i].loop == s) {
            return &w->loops[i];
        }
    }
    if (w->loop_count == w->loop_capacity) {
        w->loop_capacity = w->loop_capacity == 0 ? 4 : w->loop_capacity * 2;
        w->loops = alloc_resize(w->loops, w->loop_capacity,
                                sizeof *w->loops);
    }
    t = &w->loops[w->loop_count++];
    memset(t, 0, sizeof *t);
    t->loop = s;
    v.data = &ww;
    ast_walk_block(&v, s->as.for_loop.body);
    t->whole = ww.whole;
    if (t->whole) {
        sure_block(t, s->as.for_loop.body);
    }
    return t;
}

static bool reached(struct index_walk *w, const struct expr *e,
                    const struct stmt *loop)
{
    struct turns *t;
    size_t i;

    if (loop == NULL) {
        return true;
    }
    t = turns_of(w, loop);
    for (i = 0; t->whole && i < t->count; i++) {
        if (t->sure[i] == e) {
            return true;
        }
    }
    return false;
}

static void end_text(struct text *out, struct end e)
{
    if (e.base == NULL) {
        text_appendf(out, "%lld", (long long)e.add);
        return;
    }
    text_appendf(out, "`%.*s%s", (int)e.base->name.length,
                 e.base->name.text, is_length_base(e.base) ? ".len" : "");
    if (e.add > 0) {
        text_appendf(out, " + %lld", (long long)e.add);
    } else if (e.add < 0) {
        text_appendf(out, " - %lld", -(long long)e.add);
    }
    text_appendf(out, "`");
}

static void refuse(struct index_walk *w, const struct expr *e,
                   const struct range *r, struct end value,
                   const struct end *length)
{
    const struct expr *base = e->as.index.base;
    const struct expr *index = e->as.index.index;
    struct text at = {0};
    struct text where = {0};

    end_text(&at, value);
    if (r->loop != NULL) {
        text_appendf(&at, " on a turn of the `for` on line %d",
                     r->loop->pos.line);
    }
    if (length == NULL) {
        text_appendf(&where, "below zero");
    } else {
        if (base->kind == EXPR_NAME) {
            text_appendf(&where, "outside `%.*s`",
                         (int)base->as.name.length, base->as.name.text);
        } else {
            text_appendf(&where, "outside the %s",
                         base->type->kind == TYPE_ARRAY   ? "array"
                         : base->type->kind == TYPE_SLICE ? "slice"
                                                          : "`str`");
        }
        if (length->base == NULL) {
            text_appendf(&where, " of length %lld", (long long)length->add);
        }
    }
    sema_check_at(w->c, NAME_INDEX_OUT_OF_RANGE, index->pos,
                  "the index %s %s, %s", r->loop != NULL ? "reaches" : "is",
                  text_cstr(&at), text_cstr(&where));
    text_free(&at);
    text_free(&where);
}

static void check_index(struct index_walk *w, const struct expr *e)
{
    const struct expr *base = e->as.index.base;
    struct range r;
    struct end length;
    struct end zero = {NULL, 0};

    if (e->as.index.several || base == NULL || base->type == NULL ||
        (base->type->kind != TYPE_ARRAY && base->type->kind != TYPE_SLICE &&
         base->type->kind != TYPE_STR)) {
        return;
    }
    r = range_of(w, e->as.index.index);
    if (!r.known || !reached(w, e, r.loop)) {
        return;
    }
    if (below(r.lo, zero)) {
        refuse(w, e, &r, r.lo, NULL);
        return;
    }
    if (length_of(w, base, &length) && at_most(length, r.hi)) {
        refuse(w, e, &r, r.hi, &length);
    }
}

/* DESIGN: the body of an anonymous function is checked where it stands,
   with the facts of the function around it, T196. An index there that a
   `for` of the closure reaches on every turn is refused. A `for` of the
   function around it does not count the closure as reached, since the
   closure may never run. */
static bool visit_index(void *data, const struct expr *e)
{
    struct ast_visitor v = {visit_index, NULL, NULL};

    if (e->kind == EXPR_INDEX) {
        check_index(data, e);
    }
    if (e->kind == EXPR_FN && e->as.fn != NULL) {
        v.data = data;
        ast_walk_block(&v, e->as.fn->body);
    }
    return true;
}

void sema_check_indices(struct checker *c, const struct item *it,
                        size_t first)
{
    struct index_walk w;
    struct ast_visitor collect = {collect_expr, collect_stmt, NULL};
    struct ast_visitor indices = {visit_index, NULL, NULL};
    struct fact *f;
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
    if (it->self != NULL) {
        f = add_fact(&w, it->self);
        f->param = true;
    }
    for (i = 0; i < it->param_count; i++) {
        f = add_fact(&w, it->params[i].symbol);
        if (f != NULL) {
            f->param = true;
        }
    }
    collect.data = &w;
    ast_walk_block(&collect, it->body);
    indices.data = &w;
    ast_walk_block(&indices, it->body);
    for (i = 0; i < w.loop_count; i++) {
        free(w.loops[i].sure);
    }
    free(w.loops);
    free(w.facts);
}
