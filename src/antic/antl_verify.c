#include <stdint.h>

#include "antl_io.h"

/* DESIGN: the verifier of library files. The reader takes each record of
   a file as its encoding allows. The copy pass and lowering then take a
   tree the reader built as they take one the checker built, so every
   rule the checker makes true of a tree has to hold for it as well. The
   verifier holds each tree to those rules once the reader has read it,
   and a tree that breaks one is damaged. The rules of a library file
   stand here and nowhere else.

   The walk starts at the function of the tree and follows what the copy
   pass and lowering follow. It keeps one mark per record, so it takes
   each record once:

   - A tree nests at most ANTL_TREE_DEPTH_MAX records deep along any
     path, and a record that holds itself is refused. The copy pass and
     lowering recurse along the same paths.
   - A statement, a block, a written type and an anonymous function
     stand in one place, as the checker leaves them. An expression may
     stand in several, since the checker shares some, and the copy pass
     keeps it shared through its map.
   - A reference stands where the checker always sets one: the symbol of
     a name, the operands of an operator, the body of a loop, the type of
     an expression that lowering evaluates.
   - A number that indexes a type stays inside it: the case of a variant,
     the value of an enum, the lanes of a shuffle and the place of `else`
     among the arms of a `switch`.
   - A count matches what it counts: the parameters of a function against
     its type, the arguments of the copy of a generic against its type
     parameters, and the names of a pattern against its tuple.
   - `break` and `continue` stand inside a loop of their own function.

   The symbols of other items that the trees name are held to the rules
   of the tables as well: a function has the type of a function, a
   constant has its value, and a local or a parameter is never one. */

/* A record not reached yet, one the walk is inside, one walked, and the
   name of a case in an arm of a `switch`, which stands in that place
   alone. */
enum { MARK_NEW, MARK_BUSY, MARK_DONE, MARK_CASE };

/* Where an expression stands. Every expression has a type, and a name
   its symbol, but for three places. The callee of a call may lack a
   type, since lowering reads its symbol. The target `c[i]` of an
   assignment on a value of a type parameter has none, since the copy
   pass builds the call of `set_index` anew. The base of a field that is
   a value of an enum or a function names a type, a name or a field of a
   module, and needs neither, since lowering reads the number or the
   symbol of the field and never the base. */
enum role { AS_VALUE, AS_CALLEE, AS_TARGET, AS_TYPE };

/* Whether e, at role, has the type and the symbol that lowering reads. */
static bool stands_as(const struct expr *e, enum role role)
{
    if (role == AS_TYPE) {
        return e->kind == EXPR_NAME || e->kind == EXPR_FIELD;
    }
    if (e->kind == EXPR_NAME && e->symbol == NULL) {
        return false;
    }
    return e->type != NULL || role == AS_CALLEE ||
           (role == AS_TARGET && e->kind == EXPR_INDEX &&
            e->as.index.base != NULL && e->as.index.base->type != NULL &&
            e->as.index.base->type->kind == TYPE_PARAM);
}

/* What the walk learned of one record. */
struct mark {
    uint8_t state;
    /* It holds a `break` or a `continue` that no loop inside it takes. */
    bool loose;
    /* The records along the longest path down from it, itself included. */
    uint32_t height;
};

/* What the walk of one record gives back to the record that holds it. */
struct seen {
    uint32_t height;
    bool loose;
};

struct verify {
    const struct antl_tree *t;
    struct mark *fns;
    struct mark *blocks;
    struct mark *stmts;
    struct mark *exprs;
    struct mark *typexes;
    bool failed;
};

static void need(struct verify *v, bool ok)
{
    if (!ok) {
        v->failed = true;
    }
}

/* The mark of p among the count records of size bytes each from base, or
   NULL when p is none of them. */
static struct mark *mark_in(struct mark *marks, const void *base, size_t count,
                            size_t size, const void *p)
{
    uintptr_t start = (uintptr_t)base;
    uintptr_t at = (uintptr_t)p;

    if (base == NULL || at < start || (at - start) % size != 0 ||
        (at - start) / size >= count) {
        return NULL;
    }
    return &marks[(at - start) / size];
}

/* Begin the record whose mark is m at depth, the records above it.
   Returns whether the caller walks what it holds. A record walked before
   gives what it gave then in *s, when shared lets it stand in more than
   one place. */
static bool enter(struct verify *v, struct mark *m, uint32_t depth,
                  bool shared, struct seen *s)
{
    if (v->failed) {
        return false;
    }
    if (m == NULL || m->state == MARK_BUSY || m->state == MARK_CASE ||
        depth >= ANTL_TREE_DEPTH_MAX ||
        (m->state == MARK_DONE &&
         (!shared || m->height > ANTL_TREE_DEPTH_MAX - depth))) {
        v->failed = true;
        return false;
    }
    if (m->state == MARK_DONE) {
        s->height = m->height;
        s->loose = m->loose;
        return false;
    }
    m->state = MARK_BUSY;
    return true;
}

/* End the record whose mark is m, given what the records it holds gave. */
static struct seen leave(struct mark *m, struct seen below)
{
    m->state = MARK_DONE;
    m->height = below.height + 1;
    m->loose = below.loose;
    below.height = m->height;
    return below;
}

static void join(struct seen *s, struct seen part)
{
    if (part.height > s->height) {
        s->height = part.height;
    }
    s->loose = s->loose || part.loose;
}

/* Whether number, a case of the variant t plus 1, names one of its
   cases. */
static bool case_of(const struct type *t, uint32_t number)
{
    return t != NULL && t->kind == TYPE_VARIANT && t->base != NULL &&
           number >= 1 && number <= t->base->field_count &&
           number <= t->param_count;
}

/* The type arguments of the copy that a call of callee reaches: those of
   the function and those of the type that holds it. 0 when callee names
   no function directly, as the copy pass reads a callee. */
static size_t copy_params(const struct expr *callee)
{
    const struct item *it;

    if (callee == NULL || callee->symbol == NULL ||
        callee->symbol->kind != SYMBOL_FN ||
        (callee->kind != EXPR_NAME && callee->kind != EXPR_FIELD)) {
        return 0;
    }
    it = callee->symbol->item;
    if (it == NULL) {
        return 0;
    }
    return it->type_param_count +
           (it->owner != NULL ? it->owner->type_param_count : 0);
}

/* Whether sym names a function, whose address lowering takes. */
static bool names_fn(const struct symbol *sym)
{
    return sym != NULL &&
           (sym->kind == SYMBOL_FN || sym->kind == SYMBOL_EXTERN_FN);
}

static bool all_set(struct expr *const *list, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (list[i] == NULL) {
            return false;
        }
    }
    return true;
}

static bool inits_set(const struct field_init *list, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (list[i].value == NULL) {
            return false;
        }
    }
    return true;
}

/* Whether every name of list has its symbol, with its type. */
static bool bindings_set(const struct binding *list, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (list[i].symbol == NULL || list[i].symbol->type == NULL) {
            return false;
        }
    }
    return true;
}

/* The rules of one expression apart from what it holds. */
static void check_expr(struct verify *v, const struct expr *e)
{
    const struct type *simd;
    size_t i;

    switch (e->kind) {
    case EXPR_UNARY:
        need(v, e->as.unary.operand != NULL);
        break;
    case EXPR_BINARY:
        need(v, e->as.binary.left != NULL && e->as.binary.right != NULL &&
                    all_set(e->as.binary.eq_calls, e->as.binary.eq_count));
        break;
    case EXPR_CAST:
        need(v, e->as.cast.operand != NULL &&
                    (e->as.cast.variant_case == 0 ||
                     case_of(e->as.cast.operand->type,
                             e->as.cast.variant_case)));
        break;
    case EXPR_CALL:
        need(v, e->as.call.callee != NULL &&
                    all_set(e->as.call.args, e->as.call.arg_count) &&
                    all_set(e->as.call.hash_calls, e->as.call.hash_count));
        need(v, e->as.call.copy_count == 0 ||
                    e->as.call.copy_count == copy_params(e->as.call.callee));
        break;
    case EXPR_INDEX:
        need(v, e->as.index.base != NULL && e->as.index.index != NULL);
        break;
    case EXPR_SLICE:
        need(v, e->as.slice.base != NULL);
        break;
    case EXPR_FIELD:
        need(v, e->as.field.base != NULL);
        need(v, e->as.field.enum_value == 0 ||
                    (e->type != NULL && e->type->kind == TYPE_ENUM &&
                     e->as.field.enum_value <= e->type->field_count));
        break;
    case EXPR_STRUCT_LIT:
        need(v, inits_set(e->as.struct_lit.fields,
                          e->as.struct_lit.field_count));
        need(v, e->as.struct_lit.variant_case == 0 ||
                    case_of(e->type, e->as.struct_lit.variant_case));
        break;
    case EXPR_TUPLE:
        need(v, all_set(e->as.tuple.elements, e->as.tuple.count));
        break;
    case EXPR_SLICE_LIT:
        need(v, inits_set(e->as.slice_lit.fields,
                          e->as.slice_lit.field_count));
        break;
    case EXPR_ARRAY_LIT:
        need(v, all_set(e->as.array_lit.elements, e->as.array_lit.count));
        break;
    case EXPR_ARRAY_REPEAT:
        need(v, e->as.array_repeat.value != NULL &&
                    e->as.array_repeat.count != NULL);
        break;
    case EXPR_FREE:
        need(v, e->as.free_pointer != NULL);
        break;
    case EXPR_OBJECT:
        need(v, e->as.object.operand != NULL);
        break;
    case EXPR_ATOMIC:
        need(v, e->as.atomic.place != NULL);
        break;
    case EXPR_SIZE_OF:
        need(v, e->as.size_of != NULL);
        break;
    case EXPR_DISPATCH:
        need(v, e->as.dispatch.object != NULL && e->as.dispatch.call != NULL);
        break;
    case EXPR_IN:
        need(v, e->as.in.value != NULL && e->as.in.low != NULL &&
                    e->as.in.high != NULL);
        break;
    case EXPR_OPTIONAL:
        need(v, e->as.optional.base != NULL);
        break;
    case EXPR_SIMD:
        /* A shuffle has one lane per field of its simd struct, each a
           field of it. The reader gave it as many. */
        simd = e->as.simd.simd;
        need(v, simd != NULL &&
                    all_set(e->as.simd.args, e->as.simd.arg_count) &&
                    (e->as.simd.op == SIMD_OP_SHUFFLE) ==
                        (e->as.simd.lanes != NULL));
        for (i = 0; !v->failed && e->as.simd.lanes != NULL &&
                    i < simd->field_count;
             i++) {
            need(v, e->as.simd.lanes[i] < simd->field_count);
        }
        break;
    case EXPR_DESCRIPTOR:
        need(v, e->as.descriptor_of != NULL);
        break;
    case EXPR_FN:
        need(v, e->as.fn != NULL);
        break;
    default:
        break;
    }
}

/* The type parameter a `for` walks, directly or through a pointer that
   cannot be `none`, as the copy pass finds it. */
static bool walks_param(const struct type *t)
{
    if (t != NULL && t->kind == TYPE_POINTER && !t->nullable) {
        t = t->element;
    }
    return t != NULL && t->kind == TYPE_PARAM;
}

/* The rules of `for`: the names lowering binds, the tuple a pattern
   takes apart and the hooks of an iterator. */
static void check_for(struct verify *v, const struct stmt *s)
{
    const struct symbol *element = s->as.for_loop.element;
    const struct iteration *hooks = &s->as.for_loop.hooks;
    size_t names = s->as.for_loop.name_count;
    const struct expr *over = s->as.for_loop.over;

    need(v, s->as.for_loop.body != NULL &&
                bindings_set(s->as.for_loop.names, names));
    need(v, over != NULL || (s->as.for_loop.low != NULL &&
                             s->as.for_loop.high != NULL && names <= 1));
    if (v->failed) {
        return;
    }
    if (s->as.for_loop.pattern) {
        const struct type *tuple = element != NULL ? element->type : NULL;
        if (tuple != NULL && tuple->kind == TYPE_POINTER) {
            tuple = tuple->element;
        }
        need(v, tuple != NULL && tuple->kind == TYPE_TUPLE &&
                    tuple->param_count == names &&
                    tuple->field_count >= names);
    } else {
        need(v, names <= 2 &&
                    (names > 0 || (over == NULL && element == NULL)));
    }
    if (over != NULL && hooks->cursor == NULL) {
        need(v, over->type != NULL &&
                    (over->type->kind == TYPE_SLICE ||
                     over->type->kind == TYPE_ARRAY ||
                     walks_param(over->type)));
    }
    /* An iterator binds the element, or the one name, to the value that
       lowering reads: the place for a lent element, the current value
       for a copy. */
    if (!v->failed && hooks->cursor != NULL) {
        const struct symbol *sym;
        need(v, over != NULL && hooks->advance != NULL);
        if (v->failed) {
            return;
        }
        sym = element != NULL ? element : s->as.for_loop.names[0].symbol;
        need(v, sym->type != NULL &&
                    (s->as.for_loop.by_pointer || type_holds_lent(sym->type)
                         ? hooks->place
                         : hooks->current) != NULL);
    }
}

/* The rules of one statement apart from what it holds. */
static void check_stmt(struct verify *v, const struct stmt *s)
{
    size_t count;
    size_t i;

    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        need(v, (s->as.let.symbol != NULL || s->as.let.name_count > 0) &&
                    bindings_set(s->as.let.names, s->as.let.name_count));
        break;
    case STMT_EXPR:
        need(v, s->as.expr != NULL);
        break;
    case STMT_ASSIGN:
        need(v, s->as.assign.target != NULL && s->as.assign.value != NULL);
        break;
    case STMT_IF:
        need(v, s->as.if_chain.count > 0);
        for (i = 0; i < s->as.if_chain.count; i++) {
            need(v, s->as.if_chain.branches[i].cond != NULL &&
                        s->as.if_chain.branches[i].body != NULL);
        }
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        need(v, s->as.loop.cond != NULL && s->as.loop.body != NULL);
        break;
    case STMT_FOR:
        check_for(v, s);
        break;
    case STMT_DEFER:
    case STMT_UNDO:
        need(v, s->as.deferred != NULL);
        break;
    case STMT_FAIL:
        need(v, s->as.fail.value != NULL);
        break;
    case STMT_SWITCH:
        /* Lowering keeps one entry per arm and one for `else`, which
           stands after the arms before otherwise_at. */
        count = s->as.switch_stmt.count;
        need(v, s->as.switch_stmt.value != NULL &&
                    s->as.switch_stmt.otherwise_at <= count);
        for (i = 0; !v->failed && i < count; i++) {
            const struct switch_arm *a = &s->as.switch_stmt.arms[i];
            need(v, a->body != NULL &&
                        (a->test != NULL || a->variant_case != 0 ||
                         a->value != NULL));
            need(v, a->variant_case == 0 ||
                        case_of(s->as.switch_stmt.value->type,
                                a->variant_case));
            need(v, a->bound == NULL || a->variant_case != 0);
        }
        break;
    case STMT_ASSERT:
        need(v, s->as.assertion.cond != NULL);
        break;
    case STMT_TRY:
        need(v, s->as.try_block.body != NULL);
        break;
    case STMT_BLOCK:
        need(v, s->as.block != NULL);
        break;
    case STMT_SYNC:
        need(v, s->as.sync.mutex != NULL && s->as.sync.body != NULL);
        break;
    case STMT_SELECT:
        for (i = 0; i < s->as.select.count; i++) {
            need(v, s->as.select.arms[i].body != NULL);
        }
        break;
    default:
        break;
    }
}

/* The rules of a function: its type, its parameters and its body.
   Lowering takes the parameters of the function from its type, `self`
   first and the out pointer of `may fail` last, and gives each
   parameter of the item the one that stands in its place. */
static void check_fn(struct verify *v, const struct item *it)
{
    const struct type *type = it->symbol != NULL ? it->symbol->type : NULL;
    size_t first = it->has_self ? 1 : 0;
    size_t i;

    need(v, it->body != NULL && type != NULL && type->kind == TYPE_FN &&
                first + it->param_count + (type->has_out ? 1 : 0) ==
                    type->param_count);
    for (i = 0; !v->failed && i < it->param_count; i++) {
        const struct symbol *sym = it->params[i].symbol;
        need(v, sym != NULL && sym->type == type->params[first + i]);
    }
}

/* The walk */

static struct seen walk_at(struct verify *v, const struct expr *e,
                           uint32_t depth, enum role role);
static struct seen walk_stmt(struct verify *v, const struct stmt *s,
                             uint32_t depth);
static struct seen walk_block(struct verify *v, const struct block *b,
                              uint32_t depth);
static struct seen walk_typex(struct verify *v, const struct type_expr *x,
                              uint32_t depth);
static struct seen walk_fn(struct verify *v, const struct item *it,
                           uint32_t depth);

static struct seen walk_expr(struct verify *v, const struct expr *e,
                             uint32_t depth)
{
    return walk_at(v, e, depth, AS_VALUE);
}

/* The name of the case an arm of a `switch` on a variant takes, which
   lowering reads by its number and never evaluates. */
static struct seen walk_case(struct verify *v, const struct expr *e,
                             uint32_t depth)
{
    struct seen s = {1, false};
    struct mark *m = e != NULL ? mark_in(v->exprs, v->t->exprs,
                                         v->t->expr_count, sizeof *e, e)
                               : NULL;

    need(v, m != NULL && m->state == MARK_NEW && e->kind == EXPR_NAME &&
                depth < ANTL_TREE_DEPTH_MAX);
    if (!v->failed) {
        m->state = MARK_CASE;
    }
    return s;
}

static struct seen walk_list(struct verify *v, struct expr *const *list,
                             size_t count, uint32_t depth)
{
    struct seen s = {0, false};
    size_t i;

    for (i = 0; i < count; i++) {
        join(&s, walk_expr(v, list[i], depth));
    }
    return s;
}

static struct seen walk_inits(struct verify *v, const struct field_init *list,
                              size_t count, uint32_t depth)
{
    struct seen s = {0, false};
    size_t i;

    for (i = 0; i < count; i++) {
        join(&s, walk_expr(v, list[i].value, depth));
    }
    return s;
}

static struct seen walk_iteration(struct verify *v, const struct iteration *it,
                                  uint32_t depth)
{
    struct seen s = {0, false};

    join(&s, walk_expr(v, it->start, depth));
    join(&s, walk_expr(v, it->advance, depth));
    join(&s, walk_expr(v, it->current, depth));
    join(&s, walk_expr(v, it->place, depth));
    join(&s, walk_expr(v, it->changed, depth));
    join(&s, walk_expr(v, it->change_file, depth));
    join(&s, walk_expr(v, it->change_file_length, depth));
    join(&s, walk_expr(v, it->change_line, depth));
    return s;
}

static struct seen walk_arms(struct verify *v, const struct switch_arm *arms,
                             size_t count, uint32_t depth)
{
    struct seen s = {0, false};
    size_t i;

    for (i = 0; i < count; i++) {
        join(&s, arms[i].variant_case != 0
                     ? walk_case(v, arms[i].value, depth)
                     : walk_expr(v, arms[i].value, depth));
        join(&s, walk_expr(v, arms[i].test, depth));
        join(&s, walk_stmt(v, arms[i].body, depth));
    }
    return s;
}

/* An expression may stand in more than one place. The copy pass keeps it
   shared through its map. */
static struct seen walk_at(struct verify *v, const struct expr *e,
                           uint32_t depth, enum role role)
{
    struct seen s = {0, false};
    struct mark *m;
    uint32_t d = depth + 1;
    size_t i;

    if (e == NULL) {
        return s;
    }
    need(v, stands_as(e, role));
    m = mark_in(v->exprs, v->t->exprs, v->t->expr_count, sizeof *e, e);
    if (!enter(v, m, depth, true, &s)) {
        return s;
    }
    check_expr(v, e);
    switch (e->kind) {
    case EXPR_UNARY:
        join(&s, walk_expr(v, e->as.unary.operand, d));
        break;
    case EXPR_BINARY:
        join(&s, walk_expr(v, e->as.binary.left, d));
        join(&s, walk_expr(v, e->as.binary.right, d));
        join(&s, walk_list(v, e->as.binary.eq_calls, e->as.binary.eq_count,
                           d));
        break;
    case EXPR_CAST:
        join(&s, walk_expr(v, e->as.cast.operand, d));
        join(&s, walk_typex(v, e->as.cast.type, d));
        break;
    case EXPR_CALL:
        join(&s, walk_at(v, e->as.call.callee, d, AS_CALLEE));
        join(&s, walk_list(v, e->as.call.args, e->as.call.arg_count, d));
        join(&s, walk_block(v, e->as.call.handler.body, d));
        /* The call that writes its result through itself names itself
           as out, which the copy pass keeps. */
        if (e->as.call.out != e) {
            join(&s, walk_expr(v, e->as.call.out, d));
        }
        join(&s, walk_expr(v, e->as.call.pattern, d));
        join(&s, walk_list(v, e->as.call.hash_calls, e->as.call.hash_count,
                           d));
        break;
    case EXPR_INDEX:
        join(&s, walk_expr(v, e->as.index.base, d));
        join(&s, walk_expr(v, e->as.index.index, d));
        break;
    case EXPR_SLICE:
        join(&s, walk_expr(v, e->as.slice.base, d));
        join(&s, walk_expr(v, e->as.slice.low, d));
        join(&s, walk_expr(v, e->as.slice.high, d));
        break;
    case EXPR_FIELD:
        join(&s, walk_at(v, e->as.field.base, d,
                         role == AS_TYPE || e->as.field.enum_value != 0 ||
                                 names_fn(e->symbol)
                             ? AS_TYPE
                             : AS_VALUE));
        break;
    case EXPR_STRUCT_LIT:
        join(&s, walk_inits(v, e->as.struct_lit.fields,
                            e->as.struct_lit.field_count, d));
        break;
    case EXPR_TUPLE:
        join(&s, walk_list(v, e->as.tuple.elements, e->as.tuple.count, d));
        break;
    case EXPR_SLICE_LIT:
        join(&s, walk_typex(v, e->as.slice_lit.element, d));
        join(&s, walk_inits(v, e->as.slice_lit.fields,
                            e->as.slice_lit.field_count, d));
        break;
    case EXPR_ARRAY_LIT:
        join(&s, walk_list(v, e->as.array_lit.elements, e->as.array_lit.count,
                           d));
        break;
    case EXPR_ARRAY_REPEAT:
        join(&s, walk_expr(v, e->as.array_repeat.value, d));
        join(&s, walk_expr(v, e->as.array_repeat.count, d));
        break;
    case EXPR_ALLOC:
        join(&s, walk_typex(v, e->as.alloc.type, d));
        join(&s, walk_expr(v, e->as.alloc.count, d));
        join(&s, walk_expr(v, e->as.alloc.value, d));
        break;
    case EXPR_FREE:
        join(&s, walk_expr(v, e->as.free_pointer, d));
        break;
    case EXPR_OBJECT:
        join(&s, walk_expr(v, e->as.object.operand, d));
        join(&s, walk_expr(v, e->as.object.from, d));
        break;
    case EXPR_ATOMIC:
        join(&s, walk_expr(v, e->as.atomic.place, d));
        join(&s, walk_expr(v, e->as.atomic.a, d));
        join(&s, walk_expr(v, e->as.atomic.b, d));
        break;
    case EXPR_SIZE_OF:
        join(&s, walk_typex(v, e->as.size_of, d));
        break;
    case EXPR_PARALLEL:
        join(&s, walk_expr(v, e->as.parallel.array, d));
        join(&s, walk_expr(v, e->as.parallel.chunks, d));
        join(&s, walk_expr(v, e->as.parallel.call, d));
        break;
    case EXPR_DISPATCH:
        join(&s, walk_expr(v, e->as.dispatch.object, d));
        join(&s, walk_expr(v, e->as.dispatch.call, d));
        break;
    case EXPR_JOIN:
        join(&s, walk_expr(v, e->as.join.job, d));
        break;
    case EXPR_FORMAT:
        for (i = 0; i < e->as.format.count; i++) {
            const struct format_part *p = &e->as.format.parts[i];
            join(&s, walk_expr(v, p->value, d));
            join(&s, walk_expr(v, p->text_call, d));
            join(&s, walk_expr(v, p->value_call, d));
        }
        join(&s, walk_expr(v, e->as.format.start, d));
        join(&s, walk_expr(v, e->as.format.take, d));
        break;
    case EXPR_IN:
        join(&s, walk_expr(v, e->as.in.value, d));
        join(&s, walk_expr(v, e->as.in.low, d));
        join(&s, walk_expr(v, e->as.in.high, d));
        join(&s, walk_expr(v, e->as.in.test, d));
        break;
    case EXPR_OPTIONAL:
        join(&s, walk_expr(v, e->as.optional.base, d));
        join(&s, walk_expr(v, e->as.optional.access, d));
        break;
    case EXPR_SYNC_OP:
        join(&s, walk_expr(v, e->as.sync_op.target, d));
        join(&s, walk_expr(v, e->as.sync_op.value, d));
        join(&s, walk_typex(v, e->as.sync_op.element, d));
        break;
    case EXPR_SIMD:
        join(&s, walk_list(v, e->as.simd.args, e->as.simd.arg_count, d));
        break;
    case EXPR_COLLECT:
        join(&s, walk_iteration(v, &e->as.collect, d));
        break;
    case EXPR_FN:
        join(&s, walk_fn(v, e->as.fn, d));
        break;
    default:
        break;
    }
    return leave(m, s);
}

/* A loop takes the `break` and `continue` of its body. Those of the
   condition of `while` count as its own too, as the checker counts
   them. */
static struct seen walk_stmt(struct verify *v, const struct stmt *s,
                             uint32_t depth)
{
    struct seen r = {0, false};
    struct seen loop = {0, false};
    struct mark *m;
    uint32_t d = depth + 1;
    size_t i;

    if (s == NULL) {
        return r;
    }
    m = mark_in(v->stmts, v->t->stmts, v->t->stmt_count, sizeof *s, s);
    if (!enter(v, m, depth, false, &r)) {
        return r;
    }
    check_stmt(v, s);
    if (v->failed) {
        return r;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        join(&r, walk_typex(v, s->as.let.type, d));
        join(&r, walk_expr(v, s->as.let.value, d));
        join(&r, walk_block(v, s->as.let.otherwise, d));
        join(&r, walk_block(v, s->as.let.guard.body, d));
        break;
    case STMT_EXPR:
        join(&r, walk_expr(v, s->as.expr, d));
        break;
    case STMT_ASSIGN:
        join(&r, walk_at(v, s->as.assign.target, d, AS_TARGET));
        join(&r, walk_expr(v, s->as.assign.value, d));
        break;
    case STMT_IF:
        for (i = 0; i < s->as.if_chain.count; i++) {
            join(&r, walk_expr(v, s->as.if_chain.branches[i].cond, d));
            join(&r, walk_block(v, s->as.if_chain.branches[i].body, d));
        }
        join(&r, walk_block(v, s->as.if_chain.else_body, d));
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        join(&loop, walk_expr(v, s->as.loop.cond, d));
        join(&loop, walk_block(v, s->as.loop.body, d));
        r.height = loop.height;
        break;
    case STMT_FOR:
        join(&r, walk_expr(v, s->as.for_loop.low, d));
        join(&r, walk_expr(v, s->as.for_loop.high, d));
        join(&r, walk_expr(v, s->as.for_loop.over, d));
        join(&r, walk_expr(v, s->as.for_loop.step, d));
        join(&r, walk_iteration(v, &s->as.for_loop.hooks, d));
        loop = walk_block(v, s->as.for_loop.body, d);
        loop.loose = false;
        join(&r, loop);
        break;
    case STMT_DEFER:
    case STMT_UNDO:
        join(&r, walk_stmt(v, s->as.deferred, d));
        break;
    case STMT_FAIL:
        join(&r, walk_expr(v, s->as.fail.value, d));
        break;
    case STMT_SWITCH:
        join(&r, walk_expr(v, s->as.switch_stmt.value, d));
        join(&r, walk_arms(v, s->as.switch_stmt.arms, s->as.switch_stmt.count,
                           d));
        join(&r, walk_stmt(v, s->as.switch_stmt.otherwise, d));
        break;
    case STMT_ASSERT:
        join(&r, walk_expr(v, s->as.assertion.cond, d));
        break;
    case STMT_RETURN:
        join(&r, walk_expr(v, s->as.return_value, d));
        break;
    case STMT_YIELD:
        join(&r, walk_expr(v, s->as.yielded, d));
        break;
    case STMT_TRY:
        join(&r, walk_block(v, s->as.try_block.body, d));
        join(&r, walk_block(v, s->as.try_block.handler.body, d));
        break;
    case STMT_BLOCK:
        join(&r, walk_block(v, s->as.block, d));
        break;
    case STMT_SYNC:
        join(&r, walk_expr(v, s->as.sync.mutex, d));
        join(&r, walk_expr(v, s->as.sync.second, d));
        join(&r, walk_block(v, s->as.sync.body, d));
        break;
    case STMT_SELECT:
        join(&r, walk_arms(v, s->as.select.arms, s->as.select.count, d));
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        r.loose = true;
        break;
    case STMT_FALLTHROUGH:
        break;
    }
    return leave(m, r);
}

static struct seen walk_block(struct verify *v, const struct block *b,
                              uint32_t depth)
{
    struct seen s = {0, false};
    struct mark *m;
    size_t i;

    if (b == NULL) {
        return s;
    }
    m = mark_in(v->blocks, v->t->blocks, v->t->block_count, sizeof *b, b);
    if (!enter(v, m, depth, false, &s)) {
        return s;
    }
    for (i = 0; i < b->count; i++) {
        join(&s, walk_stmt(v, b->stmts[i], depth + 1));
    }
    return leave(m, s);
}

static struct seen walk_typex(struct verify *v, const struct type_expr *x,
                              uint32_t depth)
{
    struct seen s = {0, false};
    struct mark *m;
    uint32_t d = depth + 1;
    size_t i;

    if (x == NULL) {
        return s;
    }
    m = mark_in(v->typexes, v->t->typexes, v->t->typex_count, sizeof *x, x);
    if (!enter(v, m, depth, false, &s)) {
        return s;
    }
    join(&s, walk_typex(v, x->element, d));
    join(&s, walk_expr(v, x->length, d));
    for (i = 0; i < x->param_count; i++) {
        join(&s, walk_typex(v, x->params[i], d));
    }
    join(&s, walk_typex(v, x->result, d));
    return leave(m, s);
}

/* The parameters, the result and the body of a function. A `break` of
   the body that no loop of the body takes has no loop to leave. */
static struct seen walk_body(struct verify *v, const struct item *it,
                             uint32_t depth)
{
    struct seen s = {0, false};
    size_t i;

    check_fn(v, it);
    if (v->failed) {
        return s;
    }
    for (i = 0; i < it->param_count; i++) {
        join(&s, walk_typex(v, it->params[i].type, depth));
    }
    join(&s, walk_typex(v, it->result, depth));
    join(&s, walk_block(v, it->body, depth));
    need(v, !s.loose);
    return s;
}

static struct seen walk_fn(struct verify *v, const struct item *it,
                           uint32_t depth)
{
    struct seen s = {0, false};
    struct mark *m;

    if (it == NULL) {
        return s;
    }
    m = mark_in(v->fns, v->t->fns, v->t->fn_count, sizeof *it, it);
    if (!enter(v, m, depth, false, &s)) {
        return s;
    }
    return leave(m, walk_body(v, it, depth + 1));
}

bool antl_verify_extern(const struct symbol *sym)
{
    switch (sym->kind) {
    case SYMBOL_LOCAL:
    case SYMBOL_PARAM:
        /* The table of the tree holds these. */
        return false;
    case SYMBOL_FN:
    case SYMBOL_EXTERN_FN:
        /* Lowering calls it by its type, as the member reader of the
           tables requires. */
        return sym->type != NULL && sym->type->kind == TYPE_FN;
    case SYMBOL_CONST:
        /* Lowering reads the value of a constant where it is named. */
        return sym->value != NULL;
    default:
        return true;
    }
}

bool antl_verify_tree(struct reader *r, const struct antl_tree *t)
{
    struct verify v;
    size_t i;

    v.t = t;
    v.fns = antl_allocate(r, t->fn_count, sizeof *v.fns);
    v.blocks = antl_allocate(r, t->block_count, sizeof *v.blocks);
    v.stmts = antl_allocate(r, t->stmt_count, sizeof *v.stmts);
    v.exprs = antl_allocate(r, t->expr_count, sizeof *v.exprs);
    v.typexes = antl_allocate(r, t->typex_count, sizeof *v.typexes);
    v.failed = false;
    /* A local and a parameter have a type, which lowering lays out. */
    for (i = 0; i < t->sym_count; i++) {
        const struct symbol *sym = &t->syms[i];
        need(&v, (sym->kind != SYMBOL_LOCAL && sym->kind != SYMBOL_PARAM) ||
                     sym->type != NULL);
    }
    if (!v.failed) {
        walk_body(&v, t->fn, 0);
    }
    return !v.failed;
}
