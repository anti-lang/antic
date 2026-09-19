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

/* What the entries of the program reach. */
struct reach {
    bool *functions;
    bool *globals;
    uint32_t *work;                 /* functions, then globals + count */
    size_t work_count;
};

static void reach_function(struct reach *r, uint32_t f)
{
    if (!r->functions[f]) {
        r->functions[f] = true;
        r->work[r->work_count++] = f;
    }
}

static void reach_global(struct reach *r, const struct ir_module *m,
                         uint32_t g)
{
    if (!r->globals[g]) {
        r->globals[g] = true;
        r->work[r->work_count++] = (uint32_t)m->function_count + g;
    }
}

static void reach_const(struct reach *r, const struct ir_module *m,
                        const struct ir_const *c)
{
    size_t i;

    if (c->kind == IR_CONST_ADDR) {
        reach_global(r, m, c->global);
    } else if (c->kind == IR_CONST_FUNC) {
        reach_function(r, c->global);
    } else if (c->kind == IR_CONST_AGG) {
        for (i = 0; i < c->item_count; i++) {
            reach_const(r, m, &c->items[i]);
        }
    }
}

static void reach_operand(struct reach *r, const struct ir_module *m,
                          const struct ir_operand *o)
{
    if (o->kind == IR_FUNC) {
        reach_function(r, o->as.index);
    } else if (o->kind == IR_GLOBAL) {
        reach_global(r, m, o->as.index);
    }
}

/* DESIGN: the functions and globals that the entries of the program
   reach, as the optimizer's removal of unused functions marks them. The
   entry is main of the main module, or every function of that module
   when it has none, and every export fn and export global. What the
   entries do not reach never links, so a pass that asks whether the
   program uses something asks this. */
static void reach_program(struct reach *r, const struct ir_module *m,
                          const char *entry)
{
    bool has_main = false;
    size_t i;
    size_t b;
    size_t k;

    r->functions = allocate(m->function_count, sizeof *r->functions);
    r->globals = allocate(m->global_count, sizeof *r->globals);
    r->work = allocate(m->function_count + m->global_count, sizeof *r->work);
    for (i = 0; entry != NULL && i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        has_main = has_main || (!f->is_extern && f->module != NULL &&
                                strcmp(f->module, entry) == 0 &&
                                strcmp(f->name, "main") == 0);
    }
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!f->is_extern &&
            (entry == NULL || f->exported ||
             (f->module != NULL && strcmp(f->module, entry) == 0 &&
              (!has_main || strcmp(f->name, "main") == 0)))) {
            reach_function(r, (uint32_t)i);
        }
    }
    for (i = 0; i < m->global_count; i++) {
        if (m->globals[i]->exported) {
            reach_global(r, m, (uint32_t)i);
        }
    }
    while (r->work_count > 0) {
        uint32_t item = r->work[--r->work_count];
        if (item < m->function_count) {
            const struct ir_function *f = m->functions[item];
            for (b = 0; b < f->block_count; b++) {
                for (i = 0; i < f->blocks[b]->count; i++) {
                    const struct ir_inst *inst = &f->blocks[b]->insts[i];
                    reach_operand(r, m, &inst->a);
                    reach_operand(r, m, &inst->b);
                    reach_operand(r, m, &inst->c);
                    for (k = 0; k < inst->arg_count; k++) {
                        reach_operand(r, m, &inst->args[k]);
                    }
                }
            }
        } else {
            const struct ir_global *g =
                m->globals[item - m->function_count];
            if (g->value != NULL) {
                reach_const(r, m, g->value);
            }
            for (k = 0; k < g->reloc_count; k++) {
                if (g->relocs[k].fn) {
                    reach_function(r, g->relocs[k].global);
                } else {
                    reach_global(r, m, g->relocs[k].global);
                }
            }
        }
    }
}

static void reach_free(struct reach *r)
{
    free(r->functions);
    free(r->globals);
    free(r->work);
}

/* The runtime functions that read the registry. */
static const char *const registry_readers[] = {
    "anti_rt_reflect_new", "anti_rt_Object_deserialize"
};

static bool reads_registry(const struct ir_module *m, const struct reach *r)
{
    size_t i;
    size_t k;

    for (i = 0; i < m->function_count; i++) {
        for (k = 0; k < sizeof registry_readers / sizeof *registry_readers;
             k++) {
            if (r->functions[i] && m->functions[i]->module == NULL &&
                strcmp(m->functions[i]->name, registry_readers[k]) == 0) {
                return true;
            }
        }
    }
    return false;
}

static uint32_t struct_agg(struct ir_module *m, const char *name,
                           const char *const *names,
                           const enum ir_type *types, size_t count)
{
    struct ir_field fields[8];
    size_t i;

    memset(fields, 0, sizeof fields);
    for (i = 0; i < count; i++) {
        fields[i].name = names[i];
        fields[i].type = ir_scalar(types[i]);
    }
    return ir_struct_add(m, IR_AGG_STRUCT, name, fields, count, false, 0);
}

static void const_int(struct ir_const *c, enum ir_type type, uint64_t value)
{
    c->kind = IR_CONST_INT;
    c->scalar = type;
    c->integer = value;
}

static void const_addr(struct ir_const *c, uint32_t global)
{
    c->kind = IR_CONST_ADDR;
    c->scalar = IR_PTR;
    c->global = global;
}

/* The global that holds the bytes of a module path, one per module. */
static uint32_t module_text(struct ir_module *m, const char *module,
                            const char **seen, uint32_t *globals,
                            size_t *count)
{
    char name[32];
    size_t i;

    for (i = 0; i < *count; i++) {
        if (strcmp(seen[i], module) == 0) {
            return globals[i];
        }
    }
    snprintf(name, sizeof name, "registry.%zu", *count);
    seen[*count] = module;
    globals[*count] = ir_global_add(m, "anti.rt", name,
                                    (const uint8_t *)module,
                                    strlen(module) + 1, 1)->index;
    return globals[(*count)++];
}

/* DESIGN: the registry lists every class that `reflect.new` may build.
   That is each complete class that is not a singleton, with its
   descriptor, the function that prepares an object and the path of its
   module. The runtime reads it as `anti_rt_registry`. A program whose
   entries reach no reader of it gets none, so the classes it never names
   stay out of the link. `--no-reflect` drops the list and keeps an empty
   registry for a reader that remains. A library for C with the runtime
   bundled holds every file of the runtime, the readers among them. It
   gets an empty registry when nothing reads it. */
static void write_registry(struct ir_module *m, bool reflect)
{
    static const char *const class_names[] = {
        "descriptor", "init", "module", "module_length", "flags"
    };
    static const enum ir_type class_types[] = {
        IR_PTR, IR_PTR, IR_PTR, IR_I64, IR_I64
    };
    static const char *const registry_names[] = {"count", "classes"};
    static const enum ir_type registry_types[] = {IR_I64, IR_PTR};
    uint32_t class_agg = struct_agg(m, "anti.rt.Class", class_names,
                                    class_types, 5);
    uint32_t registry_agg = struct_agg(m, "anti.rt.Registry", registry_names,
                                       registry_types, 2);
    size_t class_count = m->class_count;
    const char **seen = allocate(class_count, sizeof *seen);
    uint32_t *texts = allocate(class_count, sizeof *texts);
    struct ir_const *items =
        allocate(class_count, sizeof *items);
    struct ir_const *value;
    struct ir_global *g;
    size_t modules = 0;
    size_t n = 0;
    size_t i;

    for (i = 0; reflect && i < class_count; i++) {
        const struct ir_class *c = m->classes[i];
        struct ir_const *item;
        if (c->table == IR_NO_INDEX || c->init == IR_NO_INDEX ||
            (c->flags & IR_CLASS_SINGLETON) != 0) {
            continue;
        }
        item = ir_const_agg(m, ir_aggregate(class_agg), 5);
        const_addr(&item->items[0], c->descriptor);
        item->items[1].kind = IR_CONST_FUNC;
        item->items[1].scalar = IR_PTR;
        item->items[1].global = c->init;
        const_addr(&item->items[2],
                   module_text(m, c->module, seen, texts, &modules));
        const_int(&item->items[3], IR_I64, strlen(c->module));
        const_int(&item->items[4], IR_I64,
                  (c->flags & IR_CLASS_ARGS) != 0 ? 1 : 0);
        items[n++] = *item;
    }
    value = ir_const_agg(m, ir_aggregate(registry_agg), 2);
    const_int(&value->items[0], IR_I64, n);
    if (n > 0) {
        char length[32];
        struct ir_const *list;
        snprintf(length, sizeof length, "[%zu]anti.rt.Class", n);
        list = ir_const_agg(m, ir_aggregate(ir_array_add(
                                   m, length, ir_aggregate(class_agg),
                                   ir_sym_int(m, IR_I64, n), NULL)),
                            n);
        memcpy(list->items, items, n * sizeof *items);
        const_addr(&value->items[1],
                   ir_global_add_value(m, "anti.rt", "registry.classes",
                                       list)->index);
    } else {
        const_int(&value->items[1], IR_PTR, 0);
    }
    g = ir_global_add_value(m, NULL, "anti_rt_registry", value);
    g->exported = true;
    free(seen);
    free(texts);
    free(items);
}

bool whole_program(struct ir_module *program,
                   const struct whole_options *options, struct text *errors)
{
    struct whole *w = whole_build(program);
    struct reach reach;

    (void)errors;
    memset(&reach, 0, sizeof reach);
    reach_program(&reach, program, options->entry);
    if (reads_registry(program, &reach)) {
        write_registry(program, options->reflect);
    } else if (options->bundled) {
        write_registry(program, false);
    }
    reach_free(&reach);
    if (options->release) {
        devirtualise(w, program);
    }
    whole_free(w);
    return true;
}
