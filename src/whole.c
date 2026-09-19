#include "whole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char root_descriptor[] = "anti_rt_Object_descriptor";

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

static void *allocate(size_t count, size_t size)
{
    void *p = calloc(count == 0 ? 1 : count, size);

    if (p == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    return p;
}

static uint64_t hash_text(uint64_t h, const char *s)
{
    for (; s != NULL && *s != '\0'; s++) {
        h = (h ^ (unsigned char)*s) * 1099511628211u;
    }
    return (h ^ 0xff) * 1099511628211u;
}

static bool same_text(const char *a, const char *b)
{
    return a == NULL ? b == NULL : b != NULL && strcmp(a, b) == 0;
}

static struct slot *slot_of(const struct whole *w, const char *module,
                            const char *name)
{
    size_t i = (size_t)hash_text(hash_text(1469598103934665603u, module),
                                 name) &
               (w->slot_count - 1);

    while (w->slots[i].name != NULL &&
           !(same_text(w->slots[i].module, module) &&
             strcmp(w->slots[i].name, name) == 0)) {
        i = (i + 1) & (w->slot_count - 1);
    }
    return &w->slots[i];
}

/* The class record whose descriptor is global g, or IR_NO_INDEX for the
   root and for any global that describes no class. */
static uint32_t class_of(const struct whole *w, uint32_t g)
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
         up = class_of(w, w->m->classes[up]->base)) {
        depth++;
    }
    t->table = table;
    t->classes = allocate(depth, sizeof *t->classes);
    for (up = record; up != IR_NO_INDEX && t->class_count < depth;
         up = class_of(w, w->m->classes[up]->base)) {
        t->classes[t->class_count++] = up;
    }
}

struct whole *whole_build(const struct ir_module *program)
{
    struct whole *w = allocate(1, sizeof *w);
    size_t tables = 0;
    size_t i;
    size_t j;

    w->m = program;
    w->slot_count = 16;
    while (w->slot_count < 2 * program->class_count + 1) {
        w->slot_count *= 2;
    }
    w->slots = allocate(w->slot_count, sizeof *w->slots);
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
    w->tables = allocate(tables, sizeof *w->tables);
    for (i = 0; i < program->class_count; i++) {
        const struct ir_class *c = program->classes[i];
        if (c->table == IR_NO_INDEX) {
            continue;
        }
        add_table(w, c->table, (uint32_t)i);
        for (j = 0; j < c->subtable_count; j++) {
            add_table(w, c->subtables[j].table,
                      class_of(w, c->subtables[j].interface));
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

size_t whole_entries(struct whole *w, uint32_t descriptor, uint32_t slot,
                     const uint32_t **out)
{
    uint32_t record = class_of(w, descriptor);
    size_t i;
    size_t k;

    if (record == IR_NO_INDEX && descriptor < w->m->global_count &&
        w->m->globals[descriptor]->module == NULL &&
        strcmp(w->m->globals[descriptor]->name, root_descriptor) == 0) {
        record = ROOT;
    }
    free(w->entries);
    w->entries = allocate(w->table_count, sizeof *w->entries);
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
   it as well. */
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

bool whole_program(struct ir_module *program,
                   const struct whole_options *options, struct text *errors)
{
    struct whole *w = whole_build(program);

    (void)errors;
    if (options->release) {
        devirtualise(w, program);
    }
    whole_free(w);
    return true;
}
