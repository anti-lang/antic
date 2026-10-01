#include "reach.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

/* The walk over what the entries reach. The work list holds functions,
   then globals plus the count of functions, each once, so it never
   grows past the sum of the two counts. */
struct walk {
    const struct ir_module *m;
    struct ir_reach *out;
    const uint32_t *defined;
    uint32_t *work;
    size_t work_count;
};

static void reach_function(struct walk *w, uint32_t f)
{
    if (!w->out->functions[f]) {
        w->out->functions[f] = true;
        w->work[w->work_count++] = f;
    }
}

static void reach_global(struct walk *w, uint32_t g)
{
    if (!w->out->globals[g]) {
        w->out->globals[g] = true;
        w->work[w->work_count++] = (uint32_t)w->m->function_count + g;
    }
}

static void reach_const(struct walk *w, const struct ir_const *c)
{
    size_t i;

    if (c->kind == IR_CONST_ADDR) {
        reach_global(w, c->global);
    } else if (c->kind == IR_CONST_FUNC) {
        reach_function(w, c->global);
    } else if (c->kind == IR_CONST_AGG) {
        for (i = 0; i < c->item_count; i++) {
            reach_const(w, &c->items[i]);
        }
    }
}

static void reach_operand(struct walk *w, const struct ir_operand *o)
{
    if (o->kind == IR_FUNC) {
        reach_function(w, o->as.index);
    } else if (o->kind == IR_GLOBAL) {
        reach_global(w, o->as.index);
    }
}

static void walk_function(struct walk *w, const struct ir_function *f)
{
    size_t b;
    size_t i;
    size_t k;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b]->count; i++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[i];
            reach_operand(w, &inst->a);
            reach_operand(w, &inst->b);
            reach_operand(w, &inst->c);
            for (k = 0; k < inst->arg_count; k++) {
                reach_operand(w, &inst->args[k]);
            }
        }
    }
}

static void walk_global(struct walk *w, uint32_t index)
{
    const struct ir_global *g = w->m->globals[index];
    size_t k;

    if (g->value != NULL) {
        reach_const(w, g->value);
    }
    for (k = 0; k < g->reloc_count; k++) {
        if (g->relocs[k].fn) {
            reach_function(w, g->relocs[k].global);
        } else {
            reach_global(w, g->relocs[k].global);
        }
    }
    if (w->defined[index] != IR_NO_INDEX) {
        reach_global(w, w->defined[index]);
    }
}

static uint64_t global_hash(const struct ir_global *g)
{
    uint64_t h = 1469598103934665603u;
    const char *p;

    for (p = g->module; *p != '\0'; p++) {
        h = (h ^ (unsigned char)*p) * 1099511628211u;
    }
    h = (h ^ '.') * 1099511628211u;
    for (p = g->name; *p != '\0'; p++) {
        h = (h ^ (unsigned char)*p) * 1099511628211u;
    }
    return h;
}

/* DESIGN: a whole program holds the declaration that one module writes
   for a datum of another beside the definition that module wrote. The
   library files are read one after the other. A live declaration
   keeps the definition of the same module and name alive, which the
   object then holds under that name. The definition of each declaration
   by index, or IR_NO_INDEX, found through a table hashed by the name. */
static uint32_t *definitions_of(const struct ir_module *m)
{
    uint32_t *out = alloc_zeroed(m->global_count, sizeof *out);
    uint32_t *table;
    size_t capacity = 1;
    size_t i;

    while (capacity < 2 * m->global_count + 2) {
        capacity *= 2;
    }
    table = alloc_zeroed(capacity, sizeof *table);
    for (i = 0; i < capacity; i++) {
        table[i] = IR_NO_INDEX;
    }
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        size_t slot;
        out[i] = IR_NO_INDEX;
        if (g->is_extern || g->module == NULL) {
            continue;
        }
        slot = (size_t)global_hash(g) & (capacity - 1);
        while (table[slot] != IR_NO_INDEX) {
            slot = (slot + 1) & (capacity - 1);
        }
        table[slot] = (uint32_t)i;
    }
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        size_t slot;
        if (!g->is_extern || g->module == NULL) {
            continue;
        }
        slot = (size_t)global_hash(g) & (capacity - 1);
        while (table[slot] != IR_NO_INDEX) {
            const struct ir_global *d = m->globals[table[slot]];
            if (strcmp(d->module, g->module) == 0 &&
                strcmp(d->name, g->name) == 0) {
                out[i] = table[slot];
                break;
            }
            slot = (slot + 1) & (capacity - 1);
        }
    }
    free(table);
    return out;
}

/* Whether f is main of the unit entry. */
static bool is_main_of(const struct ir_function *f, const char *entry)
{
    return !f->is_extern && f->module != NULL &&
           ir_in_unit(f->module, f->unit, entry) &&
           strcmp(f->name, "main") == 0;
}

void ir_reach(const struct ir_module *m, const char *entry, bool all,
              struct ir_reach *out)
{
    struct walk w;
    uint32_t *defined = definitions_of(m);
    bool has_main = false;
    size_t i;

    out->functions = alloc_zeroed(m->function_count, sizeof *out->functions);
    out->globals = alloc_zeroed(m->global_count, sizeof *out->globals);
    w.m = m;
    w.out = out;
    w.defined = defined;
    w.work = alloc_zeroed(m->function_count + m->global_count, sizeof *w.work);
    w.work_count = 0;
    for (i = 0; entry != NULL && !all && i < m->function_count; i++) {
        has_main = has_main || is_main_of(m->functions[i], entry);
    }
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!f->is_extern &&
            (entry == NULL || f->exported || ir_is_patterns_start(f) ||
             (ir_in_unit(f->module, f->unit, entry) &&
              (!has_main || strcmp(f->name, "main") == 0)))) {
            reach_function(&w, (uint32_t)i);
        }
    }
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (g->exported ||
            (all && entry != NULL && !g->is_extern && g->module != NULL &&
             ir_in_unit(g->module, g->unit, entry))) {
            reach_global(&w, (uint32_t)i);
        }
    }
    while (w.work_count > 0) {
        uint32_t item = w.work[--w.work_count];
        if (item < m->function_count) {
            walk_function(&w, m->functions[item]);
        } else {
            walk_global(&w, item - (uint32_t)m->function_count);
        }
    }
    free(w.work);
    free(defined);
}

void ir_reach_free(struct ir_reach *r)
{
    free(r->functions);
    free(r->globals);
    r->functions = NULL;
    r->globals = NULL;
}
