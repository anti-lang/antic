#include "whole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "modpath.h"
#include "reach.h"
#include "rt_abi.h"
#include "types.h"
#include "whole_parts.h"

/* DESIGN: a module that names a class of another module refers to its
   descriptor through a global of its own. That global is extern and
   carries the module and the name of the definition. The pass therefore
   knows a class by the module and the name of its descriptor, never by
   an index. One class keeps one identity whichever module names it. */

/* One table of a concrete class and the classes whose pointers may point
   at an object that holds it. Those are the class of a primary table and
   every class above it, or the interface of a sub-object's table and
   every class above that. The root serves every table and is not listed. */
struct served {
    uint32_t table;                 /* a global */
    uint32_t *classes;              /* indices of class records */
    size_t class_count;
};

/* The root has no class record. It stands for every class. */
#define ROOT (IR_NO_INDEX - 1)

static const char root_descriptor[] = RUNTIME_ROOT "descriptor";

/* A map from the module and the name of a global to a class record. */
struct slot {
    const char *module;
    const char *name;
    uint32_t record;
};

struct whole {
    const struct ir_module *m;
    struct slot *slots;
    size_t slot_count;              /* a power of two */
    struct served *tables;
    size_t table_count;
    uint32_t *entries;              /* the list whole_entries returns */
    size_t entry_count;
};

static bool same_text(const char *a, const char *b)
{
    return a == NULL ? b == NULL : b != NULL && strcmp(a, b) == 0;
}

static struct slot *slot_of(const struct whole *w, const char *module,
                            const char *name)
{
    size_t i = (size_t)ir_hash_name(module, name) & (w->slot_count - 1);

    while (w->slots[i].name != NULL &&
           !(same_text(w->slots[i].module, module) &&
             strcmp(w->slots[i].name, name) == 0)) {
        i = (i + 1) & (w->slot_count - 1);
    }
    return &w->slots[i];
}

/* The class record whose descriptor is global g, or IR_NO_INDEX for the
   root and for any global that describes no class. */
uint32_t whole_class_of(const struct whole *w, uint32_t g)
{
    const struct ir_global *global;
    const struct slot *s;

    if (g >= w->m->global_count) {
        return IR_NO_INDEX;
    }
    global = w->m->globals[g];
    s = slot_of(w, global->module, global->name);
    return s->name != NULL ? s->record : IR_NO_INDEX;
}

/* Add the table and every class from record up to the root. */
static void add_table(struct whole *w, uint32_t table, uint32_t record)
{
    struct served *t = &w->tables[w->table_count++];
    uint32_t up;
    size_t depth = 0;

    for (up = record; up != IR_NO_INDEX && depth <= w->m->class_count;
         up = whole_class_of(w, w->m->classes[up]->base)) {
        depth++;
    }
    t->table = table;
    t->classes = alloc_zeroed(depth, sizeof *t->classes);
    for (up = record; up != IR_NO_INDEX && t->class_count < depth;
         up = whole_class_of(w, w->m->classes[up]->base)) {
        t->classes[t->class_count++] = up;
    }
}

struct whole *whole_build(const struct ir_module *program)
{
    struct whole *w = alloc_zeroed(1, sizeof *w);
    size_t tables = 0;
    size_t i;
    size_t j;

    w->m = program;
    w->slot_count = 16;
    while (w->slot_count < 2 * program->class_count + 1) {
        w->slot_count *= 2;
    }
    w->slots = alloc_zeroed(w->slot_count, sizeof *w->slots);
    for (i = 0; i < program->class_count; i++) {
        const struct ir_class *c = program->classes[i];
        const struct ir_global *d = program->globals[c->descriptor];
        struct slot *s = slot_of(w, d->module, d->name);
        s->module = d->module;
        s->name = d->name;
        s->record = (uint32_t)i;
        if (c->table != IR_NO_INDEX) {
            tables += 1 + c->subtable_count;
        }
    }
    w->tables = alloc_zeroed(tables, sizeof *w->tables);
    for (i = 0; i < program->class_count; i++) {
        const struct ir_class *c = program->classes[i];
        if (c->table == IR_NO_INDEX) {
            continue;
        }
        add_table(w, c->table, (uint32_t)i);
        for (j = 0; j < c->subtable_count; j++) {
            add_table(w, c->subtables[j].table,
                      whole_class_of(w, c->subtables[j].interface));
        }
    }
    return w;
}

void whole_free(struct whole *w)
{
    size_t i;

    if (w == NULL) {
        return;
    }
    for (i = 0; i < w->table_count; i++) {
        free(w->tables[i].classes);
    }
    free(w->tables);
    free(w->slots);
    free(w->entries);
    free(w);
}

static bool serves(const struct served *t, uint32_t record)
{
    size_t i;

    if (record == ROOT) {
        return true;
    }
    for (i = 0; i < t->class_count; i++) {
        if (t->classes[i] == record) {
            return true;
        }
    }
    return false;
}

/* The function at entry slot of a table, or IR_NO_INDEX for an empty
   entry. */
static uint32_t entry_at(const struct whole *w, uint32_t table, uint32_t slot)
{
    const struct ir_const *value = w->m->globals[table]->value;

    if (value == NULL || value->kind != IR_CONST_AGG ||
        slot >= value->item_count ||
        value->items[slot].kind != IR_CONST_FUNC) {
        return IR_NO_INDEX;
    }
    return value->items[slot].global;
}

/* The class record of a descriptor, ROOT for the root, or IR_NO_INDEX. */
uint32_t whole_record_of(const struct whole *w, uint32_t descriptor)
{
    uint32_t record = whole_class_of(w, descriptor);

    if (record == IR_NO_INDEX && descriptor < w->m->global_count &&
        w->m->globals[descriptor]->module == NULL &&
        strcmp(w->m->globals[descriptor]->name, root_descriptor) == 0) {
        record = ROOT;
    }
    return record;
}

size_t whole_entries(struct whole *w, uint32_t descriptor, uint32_t slot,
                     const uint32_t **out)
{
    uint32_t record = whole_record_of(w, descriptor);
    size_t i;
    size_t k;

    free(w->entries);
    w->entries = alloc_zeroed(w->table_count, sizeof *w->entries);
    w->entry_count = 0;
    for (i = 0; i < w->table_count; i++) {
        uint32_t f;
        if (!serves(&w->tables[i], record)) {
            continue;
        }
        f = entry_at(w, w->tables[i].table, slot);
        k = 0;
        while (k < w->entry_count && w->entries[k] != f) {
            k++;
        }
        if (k == w->entry_count) {
            w->entries[w->entry_count++] = f;
        }
    }
    *out = w->entries;
    return w->entry_count;
}

/* DESIGN: release mode calls a function directly when every table that a
   call may read holds that one function at the slot. No class of the
   program then replaces it for the static type of the call. Dev mode
   compiles one module against objects it does not see, so it keeps the
   call through the table. A call with no concrete class to read keeps
   it as well.

   A program that can load a library, and the library itself, see no
   more than dev mode does: an object may come from a class of another
   library, whose table the IR does not hold. That class may inherit any
   class that is not final, concrete ones included, and replace its
   functions. whole_program therefore runs this pass on neither, and
   every call there that lowering left in the table stays in it. */
static void devirtualise(struct whole *w, struct ir_module *m)
{
    size_t i;
    size_t b;
    size_t k;

    for (i = 0; i < m->function_count; i++) {
        struct ir_function *f = m->functions[i];
        for (b = 0; b < f->block_count; b++) {
            for (k = 0; k < f->blocks[b]->count; k++) {
                struct ir_inst *inst = &f->blocks[b]->insts[k];
                const uint32_t *entries;
                if (inst->op != IR_CALL || inst->c.kind != IR_GLOBAL ||
                    whole_entries(w, inst->c.as.index, inst->field,
                                  &entries) != 1 ||
                    entries[0] == IR_NO_INDEX) {
                    continue;
                }
                inst->a = ir_func_op(m->functions[entries[0]]);
                memset(&inst->b, 0, sizeof inst->b);
                memset(&inst->c, 0, sizeof inst->c);
                inst->field = 0;
            }
        }
    }
}

/* DESIGN: the copies of a generic over types of one layout compile to
   the same code: `List<*Person>` and `List<*Order>` both hold pointers.
   A release build sees every copy in one IR. It keeps one of each set
   of copies whose code is identical, and that copy serves them all. Two
   functions are identical when their parameters, results, temporaries
   and instructions are. An aggregate compares by its layout, not its
   name, and a symbolic value by what it computes. A global compares as
   itself, or by its bytes where it is data without addresses, such as
   the text of a failed check. A call of another copy compares by the
   copy that stands for it, so a merge can make two callers identical
   in turn. A copy is known by its name, which carries its arguments.
   Only copies merge, so a function the program wrote keeps its own
   address. */

static bool is_copy(const struct ir_function *f)
{
    return !f->is_extern && !f->exported && f->module != NULL &&
           f->block_count > 0 && ir_is_copy_name(f->name);
}

static bool same_vtype(const struct ir_module *m, struct ir_vtype a,
                       struct ir_vtype b);

static bool same_agg(const struct ir_module *m, uint32_t a, uint32_t b);

static bool same_sym(const struct ir_module *m, uint32_t a, uint32_t b)
{
    const struct ir_sym *x;
    const struct ir_sym *y;

    if (a == b) {
        return true;
    }
    if (a == IR_NO_AGG || b == IR_NO_AGG || a >= m->sym_count ||
        b >= m->sym_count) {
        return false;
    }
    x = &m->syms[a];
    y = &m->syms[b];
    if (x->kind != y->kind || x->type != y->type) {
        return false;
    }
    switch (x->kind) {
    case IR_SYM_INT:
        return x->value == y->value;
    case IR_SYM_SIZE_OF:
        return same_vtype(m, x->of, y->of);
    case IR_SYM_OFFSET_OF:
        return x->field == y->field && same_vtype(m, x->of, y->of);
    case IR_SYM_OP:
        return x->op == y->op && same_sym(m, x->a, y->a) &&
               same_sym(m, x->b, y->b);
    }
    return false;
}

static bool same_agg(const struct ir_module *m, uint32_t a, uint32_t b)
{
    const struct ir_aggtype *x;
    const struct ir_aggtype *y;
    size_t i;

    if (a == b) {
        return true;
    }
    if (a == IR_NO_AGG || b == IR_NO_AGG) {
        return false;
    }
    x = m->aggs[a];
    y = m->aggs[b];
    if (x->kind != y->kind || x->field_count != y->field_count ||
        x->packed != y->packed || x->simd != y->simd || x->align != y->align) {
        return false;
    }
    if (x->kind == IR_AGG_ARRAY && !same_sym(m, x->length, y->length)) {
        return false;
    }
    for (i = 0; i < x->field_count; i++) {
        if (x->fields[i].bits != y->fields[i].bits ||
            x->fields[i].ext != y->fields[i].ext ||
            !same_vtype(m, x->fields[i].type, y->fields[i].type)) {
            return false;
        }
    }
    return true;
}

static bool same_vtype(const struct ir_module *m, struct ir_vtype a,
                       struct ir_vtype b)
{
    return a.type == b.type && (a.type != IR_AGG || same_agg(m, a.agg, b.agg));
}

/* Whether the globals a and b hold the same thing. They are one, or
   both are data of the same bytes that no program writes and that holds
   no address. */
static bool same_global(const struct ir_module *m, uint32_t a, uint32_t b)
{
    const struct ir_global *x = m->globals[a];
    const struct ir_global *y = m->globals[b];

    if (a == b) {
        return true;
    }
    if (x->value != NULL || y->value != NULL || x->reloc_count > 0 ||
        y->reloc_count > 0 || x->mutable || y->mutable || x->exported ||
        y->exported || x->is_extern || y->is_extern || x->size != y->size ||
        x->align != y->align) {
        return false;
    }
    return x->size == 0 || memcmp(x->bytes, y->bytes, x->size) == 0;
}

/* The copy that stands for function f. */
static uint32_t stands_for(const uint32_t *rep, uint32_t f)
{
    while (rep[f] != f) {
        f = rep[f];
    }
    return f;
}

static bool same_operand(const struct ir_module *m, const uint32_t *rep,
                         const struct ir_operand *a,
                         const struct ir_operand *b)
{
    if (a->kind != b->kind || a->type != b->type) {
        return false;
    }
    switch (a->kind) {
    case IR_NONE:
        return true;
    case IR_TEMP:
        return a->as.temp == b->as.temp;
    case IR_INT:
        return a->as.integer == b->as.integer;
    case IR_FLOAT:
        return memcmp(&a->as.floating, &b->as.floating,
                      sizeof a->as.floating) == 0;
    case IR_GLOBAL:
        return same_global(m, a->as.index, b->as.index);
    case IR_FUNC:
        return stands_for(rep, a->as.index) == stands_for(rep, b->as.index);
    case IR_BLOCK:
        return a->as.index == b->as.index;
    case IR_SYM:
        return same_sym(m, a->as.index, b->as.index);
    }
    return false;
}

static bool same_function(const struct ir_module *m, const uint32_t *rep,
                          const struct ir_function *f,
                          const struct ir_function *g)
{
    size_t b;
    size_t i;
    size_t k;

    if (f->param_count != g->param_count || f->result != g->result ||
        f->block_count != g->block_count || f->temp_count != g->temp_count ||
        f->variadic != g->variadic || f->worker != g->worker ||
        (f->result == IR_AGG && !same_agg(m, f->result_agg, g->result_agg))) {
        return false;
    }
    for (i = 0; i < f->param_count; i++) {
        const struct ir_param *p = &f->params[i];
        const struct ir_param *q = &g->params[i];
        if (p->type != q->type || p->ext != q->ext || p->temp != q->temp ||
            (p->type == IR_AGG && !same_agg(m, p->agg, q->agg))) {
            return false;
        }
    }
    for (i = 0; i < f->temp_count; i++) {
        if (f->temps[i] != g->temps[i]) {
            return false;
        }
    }
    for (b = 0; b < f->block_count; b++) {
        const struct ir_block *x = f->blocks[b];
        const struct ir_block *y = g->blocks[b];
        if (x->count != y->count || x->fail != y->fail) {
            return false;
        }
        for (i = 0; i < x->count; i++) {
            const struct ir_inst *p = &x->insts[i];
            const struct ir_inst *q = &y->insts[i];
            if (p->op != q->op || p->type != q->type ||
                p->result != q->result || p->field != q->field ||
                p->arg_count != q->arg_count ||
                !same_vtype(m, p->of, q->of) ||
                !same_operand(m, rep, &p->a, &q->a) ||
                !same_operand(m, rep, &p->b, &q->b) ||
                !same_operand(m, rep, &p->c, &q->c)) {
                return false;
            }
            for (k = 0; k < p->arg_count; k++) {
                if (!same_operand(m, rep, &p->args[k], &q->args[k])) {
                    return false;
                }
            }
        }
    }
    return true;
}

static void redirect_operand(const uint32_t *rep, struct ir_operand *o)
{
    if (o->kind == IR_FUNC) {
        o->as.index = stands_for(rep, o->as.index);
    }
}

static void redirect_const(const uint32_t *rep, struct ir_const *c)
{
    size_t i;

    if (c->kind == IR_CONST_FUNC) {
        c->global = stands_for(rep, c->global);
    } else if (c->kind == IR_CONST_AGG) {
        for (i = 0; i < c->item_count; i++) {
            redirect_const(rep, &c->items[i]);
        }
    }
}

static void merge_copies(struct ir_module *m)
{
    uint32_t *rep = alloc_zeroed(m->function_count, sizeof *rep);
    bool merged = false;
    bool changed = true;
    size_t i;
    size_t j;
    size_t b;
    size_t k;

    for (i = 0; i < m->function_count; i++) {
        rep[i] = (uint32_t)i;
    }
    while (changed) {
        changed = false;
        for (i = 0; i < m->function_count; i++) {
            if (rep[i] != i || !is_copy(m->functions[i])) {
                continue;
            }
            for (j = 0; j < i; j++) {
                if (rep[j] == j && is_copy(m->functions[j]) &&
                    same_function(m, rep, m->functions[j], m->functions[i])) {
                    rep[i] = (uint32_t)j;
                    changed = true;
                    merged = true;
                    break;
                }
            }
        }
    }
    if (!merged) {
        free(rep);
        return;
    }
    for (i = 0; i < m->function_count; i++) {
        struct ir_function *f = m->functions[i];
        for (b = 0; b < f->block_count; b++) {
            for (j = 0; j < f->blocks[b]->count; j++) {
                struct ir_inst *inst = &f->blocks[b]->insts[j];
                redirect_operand(rep, &inst->a);
                redirect_operand(rep, &inst->b);
                redirect_operand(rep, &inst->c);
                for (k = 0; k < inst->arg_count; k++) {
                    redirect_operand(rep, &inst->args[k]);
                }
            }
        }
    }
    for (i = 0; i < m->global_count; i++) {
        struct ir_global *g = m->globals[i];
        if (g->value != NULL) {
            redirect_const(rep, g->value);
        }
        for (k = 0; k < g->reloc_count; k++) {
            if (g->relocs[k].fn) {
                g->relocs[k].global = stands_for(rep, g->relocs[k].global);
            }
        }
    }
    for (i = 0; i < m->class_count; i++) {
        if (m->classes[i]->init != IR_NO_INDEX) {
            m->classes[i]->init = stands_for(rep, m->classes[i]->init);
        }
    }
    free(rep);
}

/* Whether the symbolic value sym is, or is built from, the offset of
   field of aggregate agg. */
static bool names_field(const struct ir_module *m, uint32_t sym, uint32_t agg,
                        uint32_t field, int depth)
{
    const struct ir_sym *s;

    if (sym >= m->sym_count || depth > 64) {
        return false;
    }
    s = &m->syms[sym];
    if (s->kind == IR_SYM_OFFSET_OF) {
        return s->of.type == IR_AGG && s->of.agg == agg && s->field == field;
    }
    if (s->kind == IR_SYM_OP) {
        return names_field(m, s->a, agg, field, depth + 1) ||
               (s->b != IR_NO_AGG &&
                names_field(m, s->b, agg, field, depth + 1));
    }
    return false;
}

/* A `mutable` field of a singleton. */
struct watched {
    uint32_t record;
    uint32_t agg;
    uint32_t field;
    bool reported;
};

/* Report every watched field that function f reaches through the offset
   of an address, once per worker. */
static void check_accesses(const struct ir_module *m,
                           const struct ir_function *f,
                           const struct ir_function *worker,
                           struct watched *fields, size_t count,
                           struct text *errors)
{
    size_t b;
    size_t i;
    size_t k;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b]->count; i++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[i];
            if (inst->op != IR_PTRADD || inst->b.kind != IR_SYM) {
                continue;
            }
            for (k = 0; k < count; k++) {
                const struct ir_class *c = m->classes[fields[k].record];
                if (fields[k].reported ||
                    !names_field(m, inst->b.as.index, fields[k].agg,
                                 fields[k].field, 0)) {
                    continue;
                }
                fields[k].reported = true;
                text_appendf(errors, "`%s` is `mutable` in singleton `%s` "
                                     "and `worker fn %s` reaches it\n",
                             m->aggs[fields[k].agg]
                                 ->fields[fields[k].field].name,
                             c->name, worker->name);
            }
        }
    }
}

/* Push every function a call of f reaches that the walk has not seen:
   the one a direct call names, and each one the class model lists for
   the slot of a call through a table. */
static void push_callees(struct whole *w, const struct ir_module *m,
                         const struct ir_function *f, struct whole_walk *walk)
{
    size_t b;
    size_t k;

    for (b = 0; b < f->block_count; b++) {
        for (k = 0; k < f->blocks[b]->count; k++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[k];
            const uint32_t *entries = NULL;
            size_t n = 0;
            size_t e;
            if (inst->op != IR_CALL) {
                continue;
            }
            if (inst->a.kind == IR_FUNC) {
                entries = &inst->a.as.index;
                n = 1;
            } else if (inst->c.kind == IR_GLOBAL) {
                n = whole_entries(w, inst->c.as.index, inst->field,
                                  &entries);
            }
            for (e = 0; e < n; e++) {
                uint32_t g = entries[e];
                if (g != IR_NO_INDEX && !walk->seen[g] &&
                    !m->functions[g]->is_extern) {
                    walk->seen[g] = true;
                    walk->work[walk->pending++] = g;
                }
            }
        }
    }
}

/* DESIGN: the singleton check follows every call a worker makes, direct
   or through a table, into every module of the program. A call through
   a table reaches each function that the class model lists for its
   slot. A call through a function pointer or a bound function names no
   function the pass can know, and the walk stops there. Each field is
   reported once per worker, because the IR holds no positions. */
static bool check_singletons(struct whole *w, const struct ir_module *m,
                             struct text *errors)
{
    size_t count = 0;
    struct watched *fields;
    struct whole_walk walk;
    size_t i;
    size_t j;
    bool ok = true;

    for (i = 0; i < m->class_count; i++) {
        count += m->classes[i]->mutable_count;
    }
    if (count == 0) {
        return true;
    }
    fields = alloc_zeroed(count, sizeof *fields);
    count = 0;
    for (i = 0; i < m->class_count; i++) {
        for (j = 0; j < m->classes[i]->mutable_count; j++) {
            fields[count].record = (uint32_t)i;
            fields[count].agg = m->classes[i]->agg;
            fields[count].field = m->classes[i]->mutable_fields[j];
            count++;
        }
    }
    walk.seen = alloc_zeroed(m->function_count, sizeof *walk.seen);
    walk.work = alloc_zeroed(m->function_count, sizeof *walk.work);
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *worker = m->functions[i];
        size_t before = errors->length;
        if (!worker->worker || worker->is_extern) {
            continue;
        }
        memset(walk.seen, 0, m->function_count * sizeof *walk.seen);
        for (j = 0; j < count; j++) {
            fields[j].reported = false;
        }
        walk.pending = 0;
        walk.seen[i] = true;
        walk.work[walk.pending++] = (uint32_t)i;
        while (walk.pending > 0) {
            const struct ir_function *f =
                m->functions[walk.work[--walk.pending]];
            check_accesses(m, f, worker, fields, count, errors);
            push_callees(w, m, f, &walk);
        }
        ok = ok && errors->length == before;
    }
    free(fields);
    free(walk.seen);
    free(walk.work);
    return ok;
}

/* Whether the class record lies at or below the class above. */
bool whole_at_or_below(const struct whole *w, uint32_t record, uint32_t above)
{
    uint32_t up;
    size_t depth = 0;

    if (above == ROOT) {
        return true;
    }
    for (up = record; up != IR_NO_INDEX && depth <= w->m->class_count;
         up = whole_class_of(w, w->m->classes[up]->base)) {
        if (up == above) {
            return true;
        }
        depth++;
    }
    return false;
}

/* Whether a value of the aggregate agg holds a class value, where
   classes marks the aggregate of every class. An aggregate holds its
   parts by value, so the walk ends. */
static bool agg_holds_class(const struct ir_module *m, const bool *classes,
                            uint32_t agg)
{
    const struct ir_aggtype *t = m->aggs[agg];
    size_t i;

    if (classes[agg]) {
        return true;
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].type.type == IR_AGG &&
            t->fields[i].type.agg < m->agg_count &&
            agg_holds_class(m, classes, t->fields[i].type.agg)) {
            return true;
        }
    }
    return false;
}

/* The class record of anti.mem.Allocator, or IR_NO_INDEX. */
static uint32_t allocator_class(const struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->class_count; i++) {
        if (same_text(m->classes[i]->module, MEM_MODULE) &&
            same_text(m->classes[i]->name, MEM_ALLOCATOR)) {
            return (uint32_t)i;
        }
    }
    return IR_NO_INDEX;
}

/* DESIGN: the table facts of "Tables and dispatch" in
   docs/work-order-llvm-optimization.md say that every load and store of a
   table pointer through one pointer reads or writes one table. That holds
   where the table pointer of an object is written once, when the object
   is made, and no pointer that reached an object reaches an object of
   another class later. A whole program in release mode holds every class
   and every write, and it holds unless:
   - a function writes tables, see writes_tables of struct ir_function;
   - a union holds a class value, a variant's union of its cases among
     them, since its memory holds objects of several classes in turn;
   - a class outside the standard library inherits anti.mem.Allocator, which
     may hand out memory that held an object of another class again.
   A dev object, a plugin, a library for C and a program that may load a
   plugin share their objects with code the pass does not see. */
static bool tables_written_once(const struct whole *w)
{
    const struct ir_module *m = w->m;
    uint32_t allocator = allocator_class(m);
    bool *classes;
    bool once = true;
    size_t i;

    for (i = 0; i < m->function_count && once; i++) {
        once = !m->functions[i]->writes_tables;
    }
    classes = alloc_zeroed(m->agg_count + 1, sizeof *classes);
    for (i = 0; i < m->class_count; i++) {
        if (m->classes[i]->agg < m->agg_count) {
            classes[m->classes[i]->agg] = true;
        }
    }
    for (i = 0; i < m->agg_count && once; i++) {
        once = !(m->aggs[i]->kind == IR_AGG_UNION &&
                 agg_holds_class(m, classes, (uint32_t)i));
    }
    free(classes);
    for (i = 0; i < m->class_count && once && allocator != IR_NO_INDEX; i++) {
        once = (m->classes[i]->module != NULL &&
                modpath_reserved(m->classes[i]->module)) ||
               !whole_at_or_below(w, (uint32_t)i, allocator);
    }
    return once;
}

/* DESIGN: a program that injects an interface or loads a library is a
   host. Its symbols are what a plugin resolves against, so the link
   exports them and the emitter writes every name global. `--closed`
   turns both off, and the program then loads no plugin. */
bool whole_hosts_plugins(const struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->class_count; i++) {
        if (m->classes[i]->inject_count > 0) {
            return true;
        }
    }
    for (i = 0; i < m->function_count; i++) {
        if (strcmp(m->functions[i]->name, PLUGIN_LOAD) == 0) {
            return true;
        }
    }
    return false;
}

bool whole_program(struct ir_module *program,
                   const struct whole_options *options, struct text *errors)
{
    struct whole *w = whole_build(program);
    struct ir_reach reach;
    size_t inject_errors;
    bool partial;
    bool calls;
    bool ok;

    /* DESIGN: a dev build links one object per module. An object
       carries every function of its module, reached or not. A table the
       runtime reads is then pulled into the link by a function this
       program never calls. The pass therefore writes each of them
       whatever the reach says. A release build sees the whole program
       in one IR. It writes what that program reaches. */
    partial = options->dev;
    ok = check_singletons(w, program, errors);
    /* What the program reaches, as the optimizer that follows decides
       it. */
    ir_reach(program, options->entry, false, &reach);
    if (options->plugin) {
        /* The host holds the registry, the default of the backtraces and
           the slots. A plugin reads all three through the host. */
    } else if (whole_reads_registry(program, &reach) || partial) {
        whole_write_registry(program, options->reflect);
    } else if (options->bundled) {
        whole_write_registry(program, false);
    }
    if (!options->plugin &&
        (whole_asks_backtrace(program, &reach) || options->bundled ||
         partial)) {
        whole_write_backtrace_default(program, options->release);
    }
    calls = whole_calls_through_reflection(program, &reach);
    /* The loader reads the table, and the runtime archive puts the
       loader in every program. Every program therefore carries a table,
       empty where its calls reach no slot. A library for C with the
       runtime bundled carries the loader too. */
    if (!options->library && !options->plugin) {
        whole_write_slots(w, program, &reach, calls && options->reflect, true);
    } else if (options->bundled && !options->plugin) {
        whole_write_slots(w, program, &reach, false, true);
    }
    /* The providers of a library for C are resolved as a program's are.
       Its host is C and cannot fill a slot. */
    inject_errors = errors->length;
    /* A plugin fills no slot: the host holds them, and its own `inject`
       fields read the host's. It carries the table of what it provides
       instead. */
    if (options->plugin) {
        whole_write_provides(w, program, errors);
    } else {
        whole_write_injections(w, program, options, errors);
    }
    ok = ok && errors->length == inject_errors;
    ir_reach_free(&reach);
    /* The trampolines come after every reader of the reach, because they
       add functions that it does not cover. */
    if (calls || options->bundled || partial) {
        whole_write_trampolines(program,
                                (calls || partial) && options->reflect);
    }
    if (options->release) {
        if (!options->plugin &&
            (options->closed || !whole_hosts_plugins(program))) {
            devirtualise(w, program);
            program->table_facts = !options->library &&
                                   tables_written_once(w);
        }
        merge_copies(program);
    }
    whole_free(w);
    return ok;
}
