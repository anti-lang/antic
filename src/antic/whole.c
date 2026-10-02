#include "whole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "reach.h"
#include "rt_abi.h"

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
    t->classes = alloc_zeroed(depth, sizeof *t->classes);
    for (up = record; up != IR_NO_INDEX && t->class_count < depth;
         up = class_of(w, w->m->classes[up]->base)) {
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

/* The class record of a descriptor, ROOT for the root, or IR_NO_INDEX. */
static uint32_t record_of(const struct whole *w, uint32_t descriptor)
{
    uint32_t record = class_of(w, descriptor);

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
    uint32_t record = record_of(w, descriptor);
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

/* The runtime functions that read the registry. The loader is one of
   them. It adds the classes of a library to the registry, so a program
   that loads one carries the table those classes join. */
static const char *const registry_readers[] = {
    "anti_rt_reflect_new", RUNTIME_ROOT "deserialize", PLUGIN_LOAD
};

static bool reads_registry(const struct ir_module *m,
                           const struct ir_reach *r)
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
    globals[*count] = ir_global_add(m, RUNTIME_MODULE, name,
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
/* The list of the classes that `reflect.new` may build, as a global of
   an array of `anti.rt.Class`. Returns IR_NO_INDEX and 0 where the
   program lists none. */
static uint32_t write_class_list(struct ir_module *m, bool reflect,
                                 size_t *out_count)
{
    uint32_t class_agg = rt_record_agg(m, RT_RECORD_CLASS);
    size_t class_count = m->class_count;
    const char **seen = alloc_zeroed(class_count, sizeof *seen);
    uint32_t *texts = alloc_zeroed(class_count, sizeof *texts);
    struct ir_const *items =
        alloc_zeroed(class_count, sizeof *items);
    uint32_t list_global = IR_NO_INDEX;
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
        item = ir_const_agg(m, ir_aggregate(class_agg), RT_CLASS_ITEM_COUNT);
        const_addr(&item->items[RT_CLASS_DESCRIPTOR], c->descriptor);
        item->items[RT_CLASS_INIT].kind = IR_CONST_FUNC;
        item->items[RT_CLASS_INIT].scalar = IR_PTR;
        item->items[RT_CLASS_INIT].global = c->init;
        const_addr(&item->items[RT_CLASS_MODULE],
                   module_text(m, c->module, seen, texts, &modules));
        const_int(&item->items[RT_CLASS_MODULE_LENGTH], IR_I64, strlen(c->module));
        const_int(&item->items[RT_CLASS_FLAGS], IR_I64,
                  ((c->flags & IR_CLASS_ARGS) != 0 ? 1 : 0) |
                      ((c->flags & IR_CLASS_REQUIRED) != 0 ? 2 : 0));
        items[n++] = *item;
    }
    if (n > 0) {
        uint32_t array = ir_array_of(m, rt_record_name(RT_RECORD_CLASS),
                                     ir_aggregate(class_agg), n);
        struct ir_const *list = ir_const_agg(m, ir_aggregate(array), n);
        memcpy(list->items, items, n * sizeof *items);
        list_global = ir_global_add_value(m, RUNTIME_MODULE, "registry.classes",
                                          list)->index;
    }
    free(seen);
    free(texts);
    free(items);
    *out_count = n;
    return list_global;
}

static void write_registry(struct ir_module *m, bool reflect)
{
    uint32_t registry_agg = rt_record_agg(m, RT_RECORD_REGISTRY);
    struct ir_const *value;
    struct ir_global *g;
    size_t n = 0;
    uint32_t list = write_class_list(m, reflect, &n);

    value = ir_const_agg(m, ir_aggregate(registry_agg), RT_REGISTRY_ITEM_COUNT);
    const_int(&value->items[RT_REGISTRY_COUNT], IR_I64, n);
    if (list != IR_NO_INDEX) {
        const_addr(&value->items[RT_REGISTRY_CLASSES], list);
    } else {
        const_int(&value->items[RT_REGISTRY_CLASSES], IR_PTR, 0);
    }
    g = ir_global_add_value(m, NULL, "anti_rt_registry", value);
    g->exported = true;
}

/* The runtime function that every `fail` asks whether backtraces are
   on. */
static bool asks_backtrace(const struct ir_module *m,
                           const struct ir_reach *r)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        if (r->functions[i] && m->functions[i]->module == NULL &&
            strcmp(m->functions[i]->name, rt_name(RT_FN_BACKTRACE_ON)) ==
                0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: backtraces are on in dev mode and off in release. The build
   of the program decides, whichever build wrote the library file of a
   `fail`. The pass writes the default as `anti_rt_backtrace_default`
   into a program that reaches the runtime's reader of it, as it writes
   the registry. A library for C with the runtime bundled holds that
   reader and gets the default of its own build. A dev build writes it
   whatever the reach says, for the reason the `dev` option gives. */
static void write_backtrace_default(struct ir_module *m, bool release)
{
    uint32_t agg = rt_record_agg(m, RT_RECORD_BACKTRACE_DEFAULT);
    struct ir_const *value =
        ir_const_agg(m, ir_aggregate(agg), RT_BACKTRACE_DEFAULT_ITEM_COUNT);
    struct ir_global *g;

    const_int(&value->items[RT_BACKTRACE_DEFAULT_ON], IR_I64, release ? 0 : 1);
    g = ir_global_add_value(m, NULL, "anti_rt_backtrace_default", value);
    g->exported = true;
}

/* The kinds of anti.reflect.ValueKind, in its order. */
enum value_kind {
    VALUE_NONE, VALUE_INT, VALUE_UINT, VALUE_FLOAT, VALUE_BOOL, VALUE_CHAR,
    VALUE_STR, VALUE_PTR
};

/* One name of a signature. The type passes the value, the kind of
   reflect.Value carries it, and the Value holds it at the wide type. */
struct value_type {
    const char *name;
    enum ir_type type;
    enum ir_ext ext;
    enum value_kind kind;
    enum ir_type wide;
};

static const struct value_type value_types[] = {
    {"void", IR_VOID, IR_EXT_NONE, VALUE_NONE, IR_VOID},
    {"bool", IR_I8, IR_EXT_ZERO, VALUE_BOOL, IR_I8},
    {"char", IR_I32, IR_EXT_NONE, VALUE_CHAR, IR_I32},
    {"i8", IR_I8, IR_EXT_SIGN, VALUE_INT, IR_I64},
    {"i16", IR_I16, IR_EXT_SIGN, VALUE_INT, IR_I64},
    {"i32", IR_I32, IR_EXT_NONE, VALUE_INT, IR_I64},
    {"i64", IR_I64, IR_EXT_NONE, VALUE_INT, IR_I64},
    {"u8", IR_I8, IR_EXT_ZERO, VALUE_UINT, IR_I64},
    {"u16", IR_I16, IR_EXT_ZERO, VALUE_UINT, IR_I64},
    {"u32", IR_I32, IR_EXT_NONE, VALUE_UINT, IR_I64},
    {"u64", IR_I64, IR_EXT_NONE, VALUE_UINT, IR_I64},
    {"f32", IR_F32, IR_EXT_NONE, VALUE_FLOAT, IR_F64},
    {"f64", IR_F64, IR_EXT_NONE, VALUE_FLOAT, IR_F64},
    {"str", IR_AGG, IR_EXT_NONE, VALUE_STR, IR_AGG},
    {"ptr", IR_PTR, IR_EXT_NONE, VALUE_PTR, IR_PTR},
};

/* The most names a signature holds: the result and the parameters. */
#define SIGNATURE_MAX 64

/* The names of the signature text, result first. Returns the count, or
   0 for a text with a name that no Value carries. */
static size_t read_signature(const char *text,
                             const struct value_type **out)
{
    size_t count = 0;

    while (count < SIGNATURE_MAX) {
        size_t length = strcspn(text, ".");
        size_t k;
        for (k = 0; k < sizeof value_types / sizeof *value_types; k++) {
            if (strlen(value_types[k].name) == length &&
                strncmp(value_types[k].name, text, length) == 0) {
                break;
            }
        }
        if (k == sizeof value_types / sizeof *value_types ||
            (count > 0 && value_types[k].type == IR_VOID)) {
            return 0;
        }
        out[count++] = &value_types[k];
        if (text[length] == '\0') {
            return count;
        }
        text += length + 1;
    }
    return 0;
}

/* The argument of the type t passes, read from the payload of a Value at
   the pointer at. A str passes the address of its bytes in the Value. */
static struct ir_operand unpack(struct ir_function *f, struct ir_block *b,
                                const struct value_type *t,
                                struct ir_operand at)
{
    uint32_t wide;
    uint32_t narrow;

    if (t->type == IR_AGG) {
        return at;
    }
    wide = ir_load(f, b, t->wide, at);
    if (t->wide == t->type) {
        return ir_temp_op(f, wide);
    }
    narrow = ir_unary(f, b, t->kind == VALUE_FLOAT ? IR_FTRUNC : IR_TRUNC,
                      t->type, ir_temp_op(f, wide));
    return ir_temp_op(f, narrow);
}

/* Store the result r, of the type t passes, into the payload of a Value
   at the pointer at. It is widened to the type the Value holds it at. */
static void pack(struct ir_function *f, struct ir_block *b,
                 const struct value_type *t, uint32_t str_agg,
                 struct ir_operand r, struct ir_operand at)
{
    enum ir_op op = t->kind == VALUE_FLOAT ? IR_FEXT
                    : t->kind == VALUE_INT ? IR_SEXT
                                           : IR_ZEXT;
    uint32_t wide;

    if (t->type == IR_AGG) {
        ir_memcopy(f, b, at, r, ir_aggregate(str_agg));
        return;
    }
    if (t->wide == t->type) {
        ir_store(f, b, t->type, r, at);
        return;
    }
    wide = ir_unary(f, b, op, t->wide, r);
    ir_store(f, b, t->wide, ir_temp_op(f, wide), at);
}

/* DESIGN: a trampoline takes the entry of a table, the object, the
   Values of the arguments, their count and the Value of the result. It
   checks the count and the kind of every Value first, and gives 0 when
   one does not fit. It then reads each argument at the type of its
   parameter, calls the entry, writes the result as a Value and gives 1.
   The layout of a Value is the aggregate of anti.reflect, so the IR
   holds no size. */
static uint32_t write_trampoline(struct ir_module *m, const char *text,
                                 uint32_t value_agg, uint32_t str_agg)
{
    const struct value_type *types[SIGNATURE_MAX];
    struct ir_operand args[SIGNATURE_MAX];
    struct ir_operand at[SIGNATURE_MAX];
    size_t count = read_signature(text, types);
    size_t params = count > 0 ? count - 1 : 0;
    uint32_t stride = ir_sym_size_of(m, ir_aggregate(value_agg));
    uint32_t data = ir_sym_offset_of(m, value_agg, 1);
    struct ir_function *signature;
    struct ir_function *f;
    struct ir_block *b;
    struct ir_block *fail;
    struct ir_block *call;
    struct ir_operand entry;
    struct ir_operand object;
    struct ir_operand values;
    struct ir_operand result;
    struct ir_operand bad;
    struct text name = {0};
    uint32_t r;
    uint32_t test;
    size_t k;

    if (count == 0) {
        return IR_NO_INDEX;
    }
    text_appendf(&name, "signature.%s", text);
    signature = ir_declare_add(m, RUNTIME_MODULE, text_cstr(&name),
                               types[0]->type,
                               types[0]->type == IR_AGG ? str_agg
                                                        : IR_NO_AGG);
    ir_param_add(signature, IR_PTR, IR_NO_AGG);
    for (k = 1; k < count; k++) {
        ir_param_add(signature, types[k]->type,
                     types[k]->type == IR_AGG ? str_agg : IR_NO_AGG);
        signature->params[k].ext = types[k]->ext;
    }
    name.length = 0;
    text_appendf(&name, "trampoline.%s", text);
    f = ir_function_add(m, RUNTIME_MODULE, text_cstr(&name), IR_I8,
                        IR_NO_AGG);
    text_free(&name);
    entry = ir_temp_op(f, ir_param_add(f, IR_PTR, IR_NO_AGG));
    object = ir_temp_op(f, ir_param_add(f, IR_PTR, IR_NO_AGG));
    values = ir_temp_op(f, ir_param_add(f, IR_PTR, IR_NO_AGG));
    bad = ir_temp_op(f, ir_param_add(f, IR_I64, IR_NO_AGG));
    result = ir_temp_op(f, ir_param_add(f, IR_PTR, IR_NO_AGG));
    b = ir_block_add(f);
    fail = ir_block_add(f);
    ir_ret(f, fail, IR_I8, ir_int_op(IR_I8, 0));
    /* The count first, then the kind of each Value, its field 0. */
    test = ir_binary(f, b, IR_NE, IR_I64, bad, ir_int_op(IR_I64, params));
    bad = ir_temp_op(f, test);
    for (k = 0; k < params; k++) {
        struct ir_block *check = ir_block_add(f);
        uint32_t index = ir_sym_int(m, IR_I64, k);
        uint32_t offset = ir_sym_op(m, IR_MUL, IR_I64, stride, index);
        uint32_t kind;
        ir_branch(f, b, bad, fail, check);
        b = check;
        if (k == 0) {
            at[k] = values;
        } else {
            uint32_t moved =
                ir_ptradd(f, b, values, ir_sym_operand(m, offset));
            at[k] = ir_temp_op(f, moved);
        }
        kind = ir_load(f, b, IR_I8, at[k]);
        test = ir_binary(f, b, IR_NE, IR_I8, ir_temp_op(f, kind),
                         ir_int_op(IR_I8, (uint64_t)types[k + 1]->kind));
        bad = ir_temp_op(f, test);
    }
    call = ir_block_add(f);
    ir_branch(f, b, bad, fail, call);
    b = call;
    args[0] = object;
    for (k = 0; k < params; k++) {
        uint32_t payload = ir_ptradd(f, b, at[k], ir_sym_operand(m, data));
        args[k + 1] = unpack(f, b, types[k + 1], ir_temp_op(f, payload));
    }
    r = ir_call_indirect(f, b, types[0]->type, entry, signature, args,
                         count);
    ir_store(f, b, IR_I8, ir_int_op(IR_I8, (uint64_t)types[0]->kind), result);
    if (types[0]->type != IR_VOID) {
        uint32_t payload = ir_ptradd(f, b, result, ir_sym_operand(m, data));
        pack(f, b, types[0], str_agg, ir_temp_op(f, r),
             ir_temp_op(f, payload));
    }
    ir_ret(f, b, IR_I8, ir_int_op(IR_I8, 1));
    return f->index;
}

/* Whether the entries reach the runtime function of reflect.call. */
static bool calls_through_reflection(const struct ir_module *m,
                                     const struct ir_reach *r)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        if (r->functions[i] && m->functions[i]->module == NULL &&
            strcmp(m->functions[i]->name, "anti_rt_reflect_call") == 0) {
            return true;
        }
    }
    return false;
}

/* The text of the signature of the function record item, or NULL. */
static const char *signature_of(const struct ir_module *m,
                                const struct ir_const *item)
{
    const struct ir_global *g;

    if (item->kind != IR_CONST_AGG ||
        item->item_count <= RT_FUNCTION_SIGNATURE ||
        item->items[RT_FUNCTION_SIGNATURE].kind != IR_CONST_ADDR) {
        return NULL;
    }
    g = m->globals[item->items[RT_FUNCTION_SIGNATURE].global];
    return g->bytes != NULL && g->size > 0 && g->bytes[g->size - 1] == 0
               ? (const char *)g->bytes
               : NULL;
}

/* The function list of the descriptor of the class record c, or NULL
   when it has none. */
static const struct ir_const *function_list(const struct ir_module *m,
                                            const struct ir_class *c)
{
    const struct ir_const *descriptor = m->globals[c->descriptor]->value;

    if (descriptor == NULL || descriptor->kind != IR_CONST_AGG ||
        descriptor->item_count <= RT_DESCRIPTOR_FUNCTIONS ||
        descriptor->items[RT_DESCRIPTOR_FUNCTIONS].kind != IR_CONST_ADDR) {
        return NULL;
    }
    return m->globals[descriptor->items[RT_DESCRIPTOR_FUNCTIONS].global]
        ->value;
}

/* Add each signature of the function list of the class record c to
   seen, once. */
static void add_signatures(const struct ir_module *m, const struct ir_class *c,
                           const char **seen, size_t *count, size_t max)
{
    const struct ir_const *list = function_list(m, c);
    size_t i;
    size_t k;

    for (i = 0; list != NULL && i < list->item_count; i++) {
        const char *text = signature_of(m, &list->items[i]);
        for (k = 0; text != NULL && k < *count; k++) {
            if (strcmp(seen[k], text) == 0) {
                break;
            }
        }
        if (text != NULL && k == *count && *count < max) {
            seen[(*count)++] = text;
        }
    }
}

/* DESIGN: the table of trampolines maps the text of each signature to
   its trampoline, and `reflect.call` looks the text of a function up in
   it. The pass writes it as `anti_rt_trampolines` when the program
   reaches the runtime's call, and empty for a bundled runtime.
   `--no-reflect` empties it along with the function lists. */
static void write_trampolines(struct ir_module *m, bool full)
{
    uint32_t item_agg = rt_record_agg(m, RT_RECORD_TRAMPOLINE);
    uint32_t table_agg = rt_record_agg(m, RT_RECORD_TRAMPOLINES);
    uint32_t value_agg = ir_agg_find(m, "anti.reflect.Value");
    uint32_t str_agg = ir_agg_find(m, "str");
    size_t max = 0;
    const char **seen;
    struct ir_const *items;
    struct ir_const *value;
    struct ir_global *g;
    size_t count = 0;
    size_t n = 0;
    size_t i;

    /* The bound counts the items of each list, which lie in memory. The
       count of item 10 comes from a library file unchecked. */
    for (i = 0; i < m->class_count; i++) {
        const struct ir_const *list = function_list(m, m->classes[i]);
        size_t n_items = list != NULL ? list->item_count : 0;
        if (n_items > SIZE_MAX - max) {
            alloc_out_of_memory();
        }
        max += n_items;
    }
    seen = alloc_zeroed(max, sizeof *seen);
    items = alloc_zeroed(max, sizeof *items);
    for (i = 0; full && value_agg != IR_NO_AGG && str_agg != IR_NO_AGG &&
                i < m->class_count;
         i++) {
        add_signatures(m, m->classes[i], seen, &count, max);
    }
    for (i = 0; i < count; i++) {
        char name[32];
        uint32_t function = write_trampoline(m, seen[i], value_agg, str_agg);
        struct ir_const *item;
        uint32_t text;
        if (function == IR_NO_INDEX) {
            continue;
        }
        snprintf(name, sizeof name, "trampolines.%zu", n);
        text = ir_global_add(m, RUNTIME_MODULE, name, (const uint8_t *)seen[i],
                             strlen(seen[i]) + 1, 1)->index;
        item = ir_const_agg(m, ir_aggregate(item_agg),
                            RT_TRAMPOLINE_ITEM_COUNT);
        const_addr(&item->items[RT_TRAMPOLINE_SIGNATURE], text);
        item->items[RT_TRAMPOLINE_CALL].kind = IR_CONST_FUNC;
        item->items[RT_TRAMPOLINE_CALL].scalar = IR_PTR;
        item->items[RT_TRAMPOLINE_CALL].global = function;
        items[n++] = *item;
    }
    value = ir_const_agg(m, ir_aggregate(table_agg),
                         RT_TRAMPOLINES_ITEM_COUNT);
    const_int(&value->items[RT_TRAMPOLINES_COUNT], IR_I64, n);
    if (n > 0) {
        struct ir_const *list;
        uint32_t array;
        array = ir_array_of(m, rt_record_name(RT_RECORD_TRAMPOLINE),
                            ir_aggregate(item_agg), n);
        list = ir_const_agg(m, ir_aggregate(array), n);
        memcpy(list->items, items, n * sizeof *items);
        const_addr(&value->items[RT_TRAMPOLINES_ITEMS],
                   ir_global_add_value(m, RUNTIME_MODULE, "trampolines.list",
                                       list)->index);
    } else {
        const_int(&value->items[RT_TRAMPOLINES_ITEMS], IR_PTR, 0);
    }
    g = ir_global_add_value(m, NULL, "anti_rt_trampolines", value);
    g->exported = true;
    free(seen);
    free(items);
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
    bool *seen;
    uint32_t *work;
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
    seen = alloc_zeroed(m->function_count, sizeof *seen);
    work = alloc_zeroed(m->function_count, sizeof *work);
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *worker = m->functions[i];
        size_t pending = 0;
        size_t before = errors->length;
        if (!worker->worker || worker->is_extern) {
            continue;
        }
        memset(seen, 0, m->function_count * sizeof *seen);
        for (j = 0; j < count; j++) {
            fields[j].reported = false;
        }
        seen[i] = true;
        work[pending++] = (uint32_t)i;
        while (pending > 0) {
            const struct ir_function *f = m->functions[work[--pending]];
            size_t b;
            size_t k;
            check_accesses(m, f, worker, fields, count, errors);
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
                        if (g != IR_NO_INDEX && !seen[g] &&
                            !m->functions[g]->is_extern) {
                            seen[g] = true;
                            work[pending++] = g;
                        }
                    }
                }
            }
        }
        ok = ok && errors->length == before;
    }
    free(fields);
    free(seen);
    free(work);
    return ok;
}

/* The slots of one abstract class that the calls of the program reach,
   bit k of byte k / 8 for slot k. */
struct slots {
    uint8_t *bits;
    uint32_t count;                 /* the highest slot reached, plus one */
};

static void mark_slot(struct slots *s, uint32_t slot)
{
    if (slot >= s->count) {
        uint32_t bytes = slot / 8 + 1;
        uint8_t *grown = alloc_resize(s->bits, bytes, 1);
        memset(grown + (s->count + 7) / 8, 0, bytes - (s->count + 7) / 8);
        s->bits = grown;
        s->count = slot + 1;
    }
    s->bits[slot / 8] = (uint8_t)(s->bits[slot / 8] | (1u << (slot % 8)));
}

/* Whether the class record lies at or below the class above. */
static bool at_or_below(const struct whole *w, uint32_t record, uint32_t above)
{
    uint32_t up;
    size_t depth = 0;

    if (above == ROOT) {
        return true;
    }
    for (up = record; up != IR_NO_INDEX && depth <= w->m->class_count;
         up = class_of(w, w->m->classes[up]->base)) {
        if (up == above) {
            return true;
        }
        depth++;
    }
    return false;
}

/* DESIGN: a plugin that provides an interface must fill every slot the
   program can call through it. The program therefore carries the slots
   its calls reach, per abstract class. A call at a slot through a class
   reaches that slot in every abstract class at or below the class. A
   pointer to one converts to a pointer to the other at the same address.
   The pass counts the calls of the functions that the entries reach,
   before devirtualisation makes any of them direct. Every abstract class
   stands for an injectable interface until `inject` exists. The table
   `anti_rt_slots` lists each abstract class with a slot reached. Every
   program carries the table, empty where its calls reach none, because
   the loader of a library reads it. A library for C has none, because
   its host program carries it. */
/* DESIGN: `reflect.call` may reach any slot of any interface, because it
   takes the index at run time. The bitmaps hold the slots the calls of
   the program reach, and the flag `reflect` of the table says that
   reflection may reach every other one. The loader refuses a plugin that
   misses a slot of a bitmap, and fills a slot that reflection alone
   reaches with a stub. */
static void write_slots(struct whole *w, struct ir_module *m,
                        const struct ir_reach *r, bool every, bool force)
{
    size_t classes = m->class_count;
    struct slots *slots = alloc_zeroed(classes, sizeof *slots);
    size_t emitted = 0;
    size_t i;
    size_t b;
    size_t k;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!r->functions[i]) {
            continue;
        }
        for (b = 0; b < f->block_count; b++) {
            for (k = 0; k < f->blocks[b]->count; k++) {
                const struct ir_inst *inst = &f->blocks[b]->insts[k];
                uint32_t above;
                size_t c;
                if (inst->op != IR_CALL || inst->c.kind != IR_GLOBAL) {
                    continue;
                }
                above = record_of(w, inst->c.as.index);
                for (c = 0; above != IR_NO_INDEX && c < classes; c++) {
                    if ((m->classes[c]->flags & IR_CLASS_ABSTRACT) != 0 &&
                        at_or_below(w, (uint32_t)c, above)) {
                        mark_slot(&slots[c], inst->field);
                    }
                }
            }
        }
    }
    for (i = 0; i < classes; i++) {
        emitted += slots[i].count > 0 ? 1 : 0;
    }
    if (emitted > 0 || force) {
        uint32_t slot_agg = rt_record_agg(m, RT_RECORD_SLOTS);
        uint32_t table_agg = rt_record_agg(m, RT_RECORD_SLOT_TABLE);
        char name[48];
        struct ir_const *list;
        struct ir_const *value;
        struct ir_global *g;
        size_t n = 0;
        list = emitted > 0
                   ? ir_const_agg(m, ir_aggregate(ir_array_of(
                                         m, rt_record_name(RT_RECORD_SLOTS),
                                         ir_aggregate(slot_agg), emitted)),
                                  emitted)
                   : NULL;
        for (i = 0; list != NULL && i < classes; i++) {
            struct ir_const *item;
            uint32_t bits;
            if (slots[i].count == 0) {
                continue;
            }
            snprintf(name, sizeof name, "slots.%zu", n);
            bits = ir_global_add(m, RUNTIME_MODULE, name, slots[i].bits,
                                 (slots[i].count + 7) / 8, 1)->index;
            item = ir_const_agg(m, ir_aggregate(slot_agg),
                                RT_SLOTS_ITEM_COUNT);
            const_addr(&item->items[RT_SLOTS_DESCRIPTOR], m->classes[i]->descriptor);
            const_int(&item->items[RT_SLOTS_SLOT_COUNT], IR_I64, slots[i].count);
            const_addr(&item->items[RT_SLOTS_BITS], bits);
            list->items[n++] = *item;
        }
        value = ir_const_agg(m, ir_aggregate(table_agg),
                             RT_SLOT_TABLE_ITEM_COUNT);
        const_int(&value->items[RT_SLOT_TABLE_COUNT], IR_I64, emitted);
        if (list != NULL) {
            const_addr(&value->items[RT_SLOT_TABLE_INTERFACES],
                       ir_global_add_value(m, RUNTIME_MODULE, "slots.list",
                                           list)->index);
        } else {
            const_int(&value->items[RT_SLOT_TABLE_INTERFACES], IR_PTR, 0);
        }
        const_int(&value->items[RT_SLOT_TABLE_REFLECT], IR_I64, every ? 1 : 0);
        g = ir_global_add_value(m, NULL, "anti_rt_slots", value);
        g->exported = true;
    }
    for (i = 0; i < classes; i++) {
        free(slots[i].bits);
    }
    free(slots);
}

/* DESIGN: `inject name: *Interface` fills a field from the provider of
   its interface before `construct` runs. The program holds one slot per
   interface, a pointer that holds the provider, and every site calls
   through it. This pass runs where the program is whole. It sees every
   class of every module, every interface they inject and every function
   a provider may name. An interface the build named no provider for is
   refused here, which is the link-time error the specification asks
   for. */
struct injectable {
    const char *interface;          /* the path of the abstract class */
    const char *module;             /* the class that needs it */
    const char *name;
    const char *field;
    uint32_t descriptor;            /* the interface's descriptor */
    bool final;                     /* an `inject final` field names it */
    uint32_t slot;                  /* the global that holds the provider */
    uint32_t provider;              /* the function the slot points at */
    /* DESIGN: `plugin:<path>` and `discover` name a library rather than
       a function of the program. The slot is then zero at the link and
       the runtime fills it before `main`, from the library the path
       names or from the first one of the search directories that
       provides the interface. */
    const char *library;            /* the path, or NULL */
    bool discover;
    /* DESIGN: a provider that comes from a library is an object, and a
       slot holds a function. Every injectable interface therefore
       carries one holder and one thunk. The runtime stores the object
       in the holder and puts the thunk in the slot. A replacement at
       start then needs no code of its own. */
    uint32_t holder;                /* the global that holds the object */
    uint32_t thunk;                 /* the function that gives it */
};

/* The injectable interface of path, added to the list when it is new. */
static struct injectable *injectable_of(struct injectable *list, size_t *count,
                                        const char *path)
{
    size_t i;

    for (i = 0; i < *count; i++) {
        if (strcmp(list[i].interface, path) == 0) {
            return &list[i];
        }
    }
    memset(&list[*count], 0, sizeof list[*count]);
    list[*count].interface = path;
    list[*count].slot = IR_NO_INDEX;
    list[*count].provider = IR_NO_INDEX;
    return &list[(*count)++];
}

/* The `inject` fields of every class of the program, one entry per
   interface. The first class that names an interface is the one an
   error names. `inject final` anywhere makes the slot final: one slot
   serves every field of the interface, so a replacement that one field
   refuses is refused for all. */
static size_t collect_injectables(const struct ir_module *m,
                                  struct injectable *list)
{
    size_t count = 0;
    size_t i;
    size_t j;

    for (i = 0; i < m->class_count; i++) {
        const struct ir_class *c = m->classes[i];
        for (j = 0; j < c->inject_count; j++) {
            struct injectable *in =
                injectable_of(list, &count, c->injects[j].interface);
            if (in->module == NULL) {
                in->module = c->module;
                in->name = c->name;
                in->field = c->injects[j].field;
                in->descriptor = c->injects[j].descriptor;
            }
            in->final = in->final || c->injects[j].final;
        }
    }
    return count;
}

/* The slot of the interface, which lowering wrote where a module
   injects it. A program whose only injection came from a library file
   whose globals the reader dropped has none, and the pass adds it. */
static uint32_t slot_global(struct ir_module *m, const char *interface)
{
    struct text name = {0};
    uint32_t found = IR_NO_INDEX;
    size_t i;

    text_appendf(&name, INJECT_SLOT_PREFIX "%s", interface);
    for (i = 0; i < m->global_count && found == IR_NO_INDEX; i++) {
        if (m->globals[i]->module != NULL &&
            strcmp(m->globals[i]->module, RUNTIME_MODULE) == 0 &&
            strcmp(m->globals[i]->name, text_cstr(&name)) == 0) {
            found = (uint32_t)i;
        }
    }
    if (found == IR_NO_INDEX) {
        found = ir_global_add(m, RUNTIME_MODULE, text_cstr(&name), NULL, 0,
                              1)->index;
    }
    text_free(&name);
    return found;
}

/* The provider the build named for the interface, or NULL. */
static const char *provider_text(const struct whole_options *o,
                                 const char *interface)
{
    size_t length = strlen(interface);
    size_t i;

    for (i = 0; i < o->inject_count; i++) {
        if (strncmp(o->inject[i], interface, length) == 0 &&
            o->inject[i][length] == '=') {
            return o->inject[i] + length + 1;
        }
    }
    return NULL;
}

/* The function of the program that path names. A provider is written as
   one path, and the module of a function holds dots as its name may.
   Every split of the path at a dot is therefore tried. Returns
   IR_NO_INDEX when the program holds none, and sets ambiguous when two
   answer. */
static uint32_t function_named(const struct ir_module *m, const char *path,
                               bool *ambiguous)
{
    uint32_t found = IR_NO_INDEX;
    const char *dot;
    size_t i;

    for (dot = strchr(path, '.'); dot != NULL; dot = strchr(dot + 1, '.')) {
        size_t length = (size_t)(dot - path);
        for (i = 0; i < m->function_count; i++) {
            const struct ir_function *f = m->functions[i];
            if (f->module == NULL || strlen(f->module) != length ||
                strncmp(f->module, path, length) != 0 ||
                strcmp(f->name, dot + 1) != 0) {
                continue;
            }
            *ambiguous = *ambiguous || (found != IR_NO_INDEX &&
                                        found != (uint32_t)i);
            found = (uint32_t)i;
        }
    }
    return found;
}

/* The class record whose module and name the function belongs to: the
   part of a name before its last dot, which is `C` of `C.get`. Returns
   IR_NO_INDEX for a module function. */
static uint32_t class_of_function(const struct ir_module *m,
                                  uint32_t function)
{
    const struct ir_function *f = m->functions[function];
    const char *dot = strrchr(f->name, '.');
    size_t length;
    size_t i;

    if (dot == NULL || f->module == NULL) {
        return IR_NO_INDEX;
    }
    length = (size_t)(dot - f->name);
    for (i = 0; i < m->class_count; i++) {
        const struct ir_class *c = m->classes[i];
        if (strcmp(c->module, f->module) == 0 && strlen(c->name) == length &&
            strncmp(c->name, f->name, length) == 0) {
            return (uint32_t)i;
        }
    }
    return IR_NO_INDEX;
}

/* The symbolic offset of the interface inside the class, IR_NO_INDEX
   when the class is no such interface. A class at or below the
   interface holds it at offset 0, because a base is field 0 of the
   class below it. An interface it implements is a sub-object at the
   offset of its field. */
#define INJECT_AT_ZERO (IR_NO_INDEX - 1)

static uint32_t interface_offset(const struct whole *w, struct ir_module *m,
                                 uint32_t record, uint32_t descriptor)
{
    const struct ir_class *c = m->classes[record];
    size_t i;

    if (at_or_below(w, record, class_of(w, descriptor))) {
        return INJECT_AT_ZERO;
    }
    for (i = 0; i < c->subtable_count; i++) {
        if (c->subtables[i].interface == descriptor) {
            return ir_sym_offset_of(m, c->subtables[i].agg,
                                    c->subtables[i].field);
        }
    }
    return IR_NO_INDEX;
}

/* A provider that gives a pointer to the class calls that provider and
   moves the pointer to the interface's sub-object. The slot then holds
   one signature, `fn() -> *Interface`, whatever the provider gives. */
static uint32_t write_provider_thunk(struct ir_module *m, uint32_t provider,
                                     uint32_t offset, const char *interface)
{
    struct ir_function *f;
    struct ir_block *b;
    struct text name = {0};
    uint32_t call;
    uint32_t at;

    text_appendf(&name, "provider.%s", interface);
    f = ir_function_add(m, RUNTIME_MODULE, text_cstr(&name), IR_PTR,
                        IR_NO_AGG);
    text_free(&name);
    b = ir_block_add(f);
    call = ir_call(f, b, IR_PTR, ir_func_op(m->functions[provider]), NULL, 0);
    at = ir_ptradd(f, b, ir_temp_op(f, call), ir_sym_operand(m, offset));
    ir_ret(f, b, IR_PTR, ir_temp_op(f, at));
    return f->index;
}

/* Write the slot of in as a mutable `anti.rt.InjectSlot` that holds the
   function provider, or a null pointer for IR_NO_INDEX. */
static void write_slot(struct ir_module *m, struct injectable *in,
                       uint32_t provider)
{
    uint32_t agg = rt_record_agg(m, RT_RECORD_INJECT_SLOT);
    struct ir_const *value =
        ir_const_agg(m, ir_aggregate(agg), RT_INJECT_SLOT_ITEM_COUNT);
    struct ir_global *g = m->globals[in->slot];

    if (provider == IR_NO_INDEX) {
        const_int(&value->items[RT_INJECT_SLOT_PROVIDER], IR_PTR, 0);
    } else {
        value->items[RT_INJECT_SLOT_PROVIDER].kind = IR_CONST_FUNC;
        value->items[RT_INJECT_SLOT_PROVIDER].scalar = IR_PTR;
        value->items[RT_INJECT_SLOT_PROVIDER].global = provider;
    }
    g->bytes = NULL;
    g->size = 0;
    g->align = 0;
    g->value = value;
    g->mutable = true;
    g->is_extern = false;
}

/* Resolve one provider, named by the build or by the interface itself,
   and fill the slot. `own` says the name came from the interface, which
   is what the message of a missing provider tells apart. */
static void resolve_named_provider(struct whole *w, struct ir_module *m,
                                   struct injectable *in, const char *text,
                                   bool own, struct text *errors)
{
    bool ambiguous = false;
    uint32_t function;
    uint32_t record;

    function = function_named(m, text, &ambiguous);
    /* A class name alone names the `get` of its singleton, which is the
       second form the specification gives a provider. */
    if (function == IR_NO_INDEX) {
        struct text get = {0};
        text_appendf(&get, "%s.get", text);
        function = function_named(m, text_cstr(&get), &ambiguous);
        text_free(&get);
    }
    if (function == IR_NO_INDEX && own) {
        text_appendf(errors, "`%s` has no provider, and `%s.%s` injects it "
                             "as `%s`. Name one under `[inject]` of the "
                             "manifest, or give `%s` a static `default`\n",
                     in->interface, in->module, in->name, in->field,
                     in->interface);
        return;
    }
    if (function == IR_NO_INDEX) {
        text_appendf(errors, "the provider `%s` of `%s` names no function of "
                             "the program, and no singleton with a `get`\n",
                     text, in->interface);
        return;
    }
    if (ambiguous) {
        text_appendf(errors, "the provider `%s` of `%s` names more than one "
                             "function of the program\n", text,
                     in->interface);
        return;
    }
    if (m->functions[function]->param_count != 0 ||
        m->functions[function]->result != IR_PTR) {
        text_appendf(errors, "the provider `%s` of `%s` takes arguments or "
                             "gives no pointer. A provider is written "
                             "`fn() -> *%s`\n", text, in->interface,
                     in->interface);
        return;
    }
    record = class_of_function(m, function);
    if (record != IR_NO_INDEX) {
        uint32_t offset = interface_offset(w, m, record, in->descriptor);
        if (offset == IR_NO_INDEX) {
            text_appendf(errors, "the provider `%s` of `%s` gives "
                                 "`%s.%s`, which is no `%s`\n", text,
                         in->interface, m->classes[record]->module,
                         m->classes[record]->name, in->interface);
            return;
        }
        if (offset != INJECT_AT_ZERO) {
            function = write_provider_thunk(m, function, offset,
                                            in->interface);
        }
    }
    in->provider = function;
    write_slot(m, in, function);
}

/* DESIGN: an interface carries its own default provider in a static
   function `default` of the interface, which the build's table
   overrides. The six standard interfaces are written that way, so
   `inject log: *Logger` needs no entry in the manifest. A library ships
   its own interface with a default the same way. */
/* The holder of the object a library provides and the thunk that gives
   it, which a replacement at start puts in the slot. */
static void write_replacement(struct ir_module *m, struct injectable *in)
{
    struct text name = {0};
    struct ir_function *f;
    struct ir_block *b;
    struct ir_global *g;
    uint32_t load;
    uint32_t at;

    text_appendf(&name, "plugin.%s", in->interface);
    g = ir_global_add_value(m, RUNTIME_MODULE, text_cstr(&name),
                            ir_const_int(m, IR_PTR, 0));
    g->mutable = true;
    in->holder = g->index;
    name.length = 0;
    text_appendf(&name, "provided.%s", in->interface);
    f = ir_function_add(m, RUNTIME_MODULE, text_cstr(&name), IR_PTR,
                        IR_NO_AGG);
    text_free(&name);
    b = ir_block_add(f);
    at = ir_addr(f, b, ir_global_op(g));
    load = ir_load(f, b, IR_PTR, ir_temp_op(f, at));
    ir_ret(f, b, IR_PTR, ir_temp_op(f, load));
    in->thunk = f->index;
}

/* Write the slot as a mutable pointer holding nothing, which the
   runtime fills before `main`. */
static void empty_slot(struct ir_module *m, struct injectable *in)
{
    write_slot(m, in, IR_NO_INDEX);
}

static void resolve_provider(struct whole *w, struct ir_module *m,
                             const struct whole_options *o,
                             struct injectable *in, struct text *errors)
{
    const char *named = provider_text(o, in->interface);
    bool own = named == NULL;
    struct text fallback = {0};

    /* A library is named where a function of the program would be. */
    if (named != NULL && (strcmp(named, PROVIDER_DISCOVER) == 0 ||
                          strncmp(named, PROVIDER_PLUGIN,
                                  sizeof PROVIDER_PLUGIN - 1) == 0) &&
        o->closed) {
        text_appendf(errors, "the provider `%s` of `%s` names a library, and "
                             "`--closed` loads none\n", named, in->interface);
        return;
    }
    if (named != NULL && strcmp(named, PROVIDER_DISCOVER) == 0) {
        in->discover = true;
        in->library = "";
        empty_slot(m, in);
        return;
    }
    if (named != NULL &&
        strncmp(named, PROVIDER_PLUGIN, sizeof PROVIDER_PLUGIN - 1) == 0) {
        in->library = named + sizeof PROVIDER_PLUGIN - 1;
        if (in->library[0] == '\0') {
            text_appendf(errors, "the provider `%s` of `%s` names no "
                                 "library\n", named, in->interface);
            return;
        }
        empty_slot(m, in);
        return;
    }
    if (own) {
        text_appendf(&fallback, "%s.default", in->interface);
        named = text_cstr(&fallback);
    }
    resolve_named_provider(w, m, in, named, own, errors);
    text_free(&fallback);
}

/* DESIGN: the provider graph is the code the providers run. An edge
   from one interface to another stands where the provider of the first,
   or a function it calls, reads the slot of the second. Only the direct
   calls are followed. A call through a table names no function here,
   and a graph that guessed would refuse a program that has no cycle.
   The walk marks every function a provider may run. */
static void reach_calls(const struct ir_module *m, uint32_t from, bool *seen,
                        uint32_t *work, bool *globals)
{
    size_t count = 0;
    size_t b;
    size_t k;

    if (seen[from]) {
        return;
    }
    seen[from] = true;
    work[count++] = from;
    while (count > 0) {
        const struct ir_function *f = m->functions[work[--count]];
        for (b = 0; b < f->block_count; b++) {
            for (k = 0; k < f->blocks[b]->count; k++) {
                const struct ir_inst *inst = &f->blocks[b]->insts[k];
                const struct ir_operand *o[2];
                size_t n;
                o[0] = &inst->a;
                o[1] = &inst->b;
                for (n = 0; n < 2; n++) {
                    if (o[n]->kind == IR_GLOBAL) {
                        globals[o[n]->as.index] = true;
                    } else if (o[n]->kind == IR_FUNC &&
                               !seen[o[n]->as.index]) {
                        seen[o[n]->as.index] = true;
                        work[count++] = o[n]->as.index;
                    }
                }
            }
        }
    }
}

/* Refuse a cycle through the providers. The graph has one node per
   injectable interface. Depth-first search over it names the interfaces
   of the first cycle it closes. */
static bool cycle_from(const bool *edges, size_t count, size_t node,
                       uint8_t *state, size_t *stack, size_t *depth)
{
    size_t i;

    state[node] = 1;
    stack[(*depth)++] = node;
    for (i = 0; i < count; i++) {
        if (!edges[node * count + i]) {
            continue;
        }
        if (state[i] == 1) {
            stack[(*depth)++] = i;
            return true;
        }
        if (state[i] == 0 &&
            cycle_from(edges, count, i, state, stack, depth)) {
            return true;
        }
    }
    state[node] = 2;
    (*depth)--;
    return false;
}

static void check_cycles(const struct ir_module *m, struct injectable *list,
                         size_t count, struct text *errors)
{
    bool *edges = alloc_zeroed(alloc_product(count, count), sizeof *edges);
    bool *seen = alloc_zeroed(m->function_count, sizeof *seen);
    bool *globals = alloc_zeroed(m->global_count, sizeof *globals);
    uint32_t *work = alloc_zeroed(m->function_count, sizeof *work);
    uint8_t *state = alloc_zeroed(count, sizeof *state);
    size_t *stack = alloc_zeroed(count + 1, sizeof *stack);
    size_t depth = 0;
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        if (list[i].provider == IR_NO_INDEX) {
            continue;
        }
        memset(seen, 0, m->function_count * sizeof *seen);
        memset(globals, 0, m->global_count * sizeof *globals);
        reach_calls(m, list[i].provider, seen, work, globals);
        for (j = 0; j < count; j++) {
            edges[i * count + j] = globals[list[j].slot];
        }
    }
    for (i = 0; i < count; i++) {
        if (state[i] != 0 ||
            !cycle_from(edges, count, i, state, stack, &depth)) {
            depth = 0;
            continue;
        }
        /* The walk may reach the cycle through nodes that stand
           outside it. The repeated node closes it, so the report starts
           where that node first stands. */
        for (j = 0; stack[j] != stack[depth - 1]; j++) {
        }
        text_appendf(errors, "the providers make a cycle: `%s` needs `%s`",
                     list[stack[j]].interface, list[stack[j + 1]].interface);
        for (j += 2; j < depth; j++) {
            text_appendf(errors, ", which needs `%s`",
                         list[stack[j]].interface);
        }
        text_append(errors, "\n");
        break;
    }
    free(edges);
    free(seen);
    free(globals);
    free(work);
    free(state);
    free(stack);
}

/* DESIGN: the program names the interfaces it injects. The runtime then
   reports what a line of `[injections]` may replace, and refuses one
   that names an interface the program has not or an `inject final`
   field. The table stands in every program, empty where nothing
   injects, because the runtime reads it before `main`. */
static void write_injectable(struct ir_module *m,
                             const struct injectable *list, size_t count)
{
    uint32_t item_agg = rt_record_agg(m, RT_RECORD_INJECTABLE);
    uint32_t table_agg = rt_record_agg(m, RT_RECORD_INJECTABLES);
    struct ir_const *value =
        ir_const_agg(m, ir_aggregate(table_agg), RT_INJECTABLES_ITEM_COUNT);
    struct ir_global *g;
    size_t i;

    const_int(&value->items[RT_INJECTABLES_COUNT], IR_I64, count);
    if (count > 0) {
        struct ir_const *list_value;
        uint32_t array;
        array = ir_array_of(m, rt_record_name(RT_RECORD_INJECTABLE),
                            ir_aggregate(item_agg), count);
        list_value = ir_const_agg(m, ir_aggregate(array), count);
        for (i = 0; i < count; i++) {
            struct ir_const *item = ir_const_agg(m, ir_aggregate(item_agg),
                                                 RT_INJECTABLE_ITEM_COUNT);
            struct text name = {0};
            char global[48];
            uint32_t text;
            snprintf(global, sizeof global, "injectable.%zu.name", i);
            text = ir_global_add(m, RUNTIME_MODULE, global,
                                 (const uint8_t *)list[i].interface,
                                 strlen(list[i].interface) + 1, 1)->index;
            const_addr(&item->items[RT_INJECTABLE_NAME], text);
            text_appendf(&name, "%s.%s", list[i].module, list[i].name);
            snprintf(global, sizeof global, "injectable.%zu.class", i);
            text = ir_global_add(m, RUNTIME_MODULE, global,
                                 (const uint8_t *)text_cstr(&name),
                                 name.length + 1, 1)->index;
            text_free(&name);
            const_addr(&item->items[RT_INJECTABLE_OWNER], text);
            snprintf(global, sizeof global, "injectable.%zu.field", i);
            text = ir_global_add(m, RUNTIME_MODULE, global,
                                 (const uint8_t *)list[i].field,
                                 strlen(list[i].field) + 1, 1)->index;
            const_addr(&item->items[RT_INJECTABLE_FIELD], text);
            const_int(&item->items[RT_INJECTABLE_FINAL], IR_I64, list[i].final ? 1 : 0);
            const_addr(&item->items[RT_INJECTABLE_SLOT], list[i].slot);
            if (list[i].library != NULL && list[i].library[0] != '\0') {
                snprintf(global, sizeof global, "injectable.%zu.library", i);
                text = ir_global_add(m, RUNTIME_MODULE, global,
                                     (const uint8_t *)list[i].library,
                                     strlen(list[i].library) + 1, 1)->index;
                const_addr(&item->items[RT_INJECTABLE_LIBRARY], text);
                const_int(&item->items[RT_INJECTABLE_LIBRARY_LENGTH], IR_I64, strlen(list[i].library));
            } else {
                const_int(&item->items[RT_INJECTABLE_LIBRARY], IR_PTR, 0);
                const_int(&item->items[RT_INJECTABLE_LIBRARY_LENGTH], IR_I64, 0);
            }
            const_int(&item->items[RT_INJECTABLE_DISCOVER], IR_I64,
                      list[i].discover || list[i].library != NULL ? 1 : 0);
            if (list[i].holder != IR_NO_INDEX) {
                const_addr(&item->items[RT_INJECTABLE_HOLDER], list[i].holder);
                item->items[RT_INJECTABLE_THUNK].kind = IR_CONST_FUNC;
                item->items[RT_INJECTABLE_THUNK].scalar = IR_PTR;
                item->items[RT_INJECTABLE_THUNK].global = list[i].thunk;
            } else {
                const_int(&item->items[RT_INJECTABLE_HOLDER], IR_PTR, 0);
                const_int(&item->items[RT_INJECTABLE_THUNK], IR_PTR, 0);
            }
            /* `--anti.inspect` reads the version and the used slots of
               the interface through its descriptor. */
            const_addr(&item->items[RT_INJECTABLE_DESCRIPTOR], list[i].descriptor);
            list_value->items[i] = *item;
        }
        const_addr(&value->items[RT_INJECTABLES_INTERFACES],
                   ir_global_add_value(m, RUNTIME_MODULE, "injectable.list",
                                       list_value)->index);
    } else {
        const_int(&value->items[RT_INJECTABLES_INTERFACES], IR_PTR, 0);
    }
    g = ir_global_add_value(m, NULL, "anti_rt_injectable", value);
    g->exported = true;
}

/* The whole of the injection pass: the interfaces, their slots, their
   providers and the cycle check. */
static void write_injections(struct whole *w, struct ir_module *m,
                             const struct whole_options *o,
                             struct text *errors)
{
    struct injectable *list = NULL;
    size_t total = 0;
    size_t count;
    size_t i;

    for (i = 0; i < m->class_count; i++) {
        total += m->classes[i]->inject_count;
    }
    list = alloc_zeroed(total, sizeof *list);
    count = collect_injectables(m, list);
    for (i = 0; i < count; i++) {
        list[i].slot = slot_global(m, list[i].interface);
        list[i].holder = IR_NO_INDEX;
        list[i].thunk = IR_NO_INDEX;
        /* A closed program takes no provider from a library, so it
           carries no place to put one. */
        if (!o->closed) {
            write_replacement(m, &list[i]);
        }
    }
    for (i = 0; i < count; i++) {
        resolve_provider(w, m, o, &list[i], errors);
    }
    check_cycles(m, list, count, errors);
    /* DESIGN: the table of the interfaces belongs to the program the
       runtime starts. A library for C has one only where it carries the
       runtime. The configuration reads the table before `main`, and
       that copy of the runtime is the one that runs. */
    if (!o->library || o->bundled) {
        write_injectable(m, list, count);
    }
    free(list);
}

/* DESIGN: a plugin carries one table of what it provides. An entry
   names the interface by its path. It holds the descriptor of the
   interface and of the class, the function that prepares an object and
   the offset of the interface sub-object. The loader allocates the size
   the class descriptor gives. It calls that function and moves the
   pointer by the offset. The table carries the version of the runtime
   the library was built against. It carries the classes of the library
   as well, which the host's registry takes over. */
/* The constant of the global g, or NULL where it has none. A plugin
   reads the descriptor of an interface of another module, which its
   library file carries. */
static const struct ir_const *global_value(const struct ir_module *m,
                                           uint32_t g)
{
    return g < m->global_count ? m->globals[g]->value : NULL;
}

/* Item at of the descriptor global, or NULL where the build cannot read
   it. */
static const struct ir_const *descriptor_item(const struct ir_module *m,
                                              uint32_t descriptor, size_t at)
{
    const struct ir_const *value = global_value(m, descriptor);

    if (value == NULL || value->kind != IR_CONST_AGG ||
        value->item_count <= at) {
        return NULL;
    }
    return &value->items[at];
}

/* Whether c is an integer, or a symbolic value where sym is set. */
static bool is_integer(const struct ir_const *c, bool sym)
{
    return c->kind == IR_CONST_INT || (sym && c->kind == IR_CONST_SYM);
}

/* The chain of hashes the version record names, or NULL where the
   library file that carries it is damaged. It is damaged where the record
   is no address and a length, or the length is zero. It is damaged as
   well where the global holds no array of that many integers. */
static const struct ir_const *chain_of(const struct ir_module *m,
                                       const struct ir_const *record)
{
    const struct ir_const *from;
    uint64_t n;
    size_t i;

    if (record->kind != IR_CONST_AGG || record->item_count <= RT_VERSIONS_CHAIN_LENGTH ||
        record->items[RT_VERSIONS_CHAIN].kind != IR_CONST_ADDR ||
        record->items[RT_VERSIONS_CHAIN_LENGTH].kind != IR_CONST_INT) {
        return NULL;
    }
    from = global_value(m, record->items[RT_VERSIONS_CHAIN].global);
    n = record->items[RT_VERSIONS_CHAIN_LENGTH].integer;
    if (from == NULL || from->kind != IR_CONST_AGG || n == 0 ||
        n > from->item_count) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        if (from->items[i].kind != IR_CONST_INT) {
            return NULL;
        }
    }
    return from;
}

/* DESIGN: a plugin copies the chain and the version of the interface it
   was built against into its own image. A reference to the interface's
   own globals would resolve against the host at load. It would then give
   the host's numbers, which are what the check compares against. */
static uint32_t copy_chain(struct ir_module *m, const struct ir_const *from,
                           size_t n, size_t at)
{
    struct ir_const *value;
    char name[40];
    size_t i;

    value = ir_const_agg(
        m, ir_aggregate(ir_array_of(m, "i64", ir_scalar(IR_I64), n)), n);
    for (i = 0; i < n; i++) {
        const_int(&value->items[i], IR_I64, from->items[i].integer);
    }
    snprintf(name, sizeof name, "provides.chain.%zu", at);
    return ir_global_add_value(m, RUNTIME_MODULE, name, value)->index;
}

static void write_provides(struct whole *w, struct ir_module *m,
                           struct text *errors)
{
    uint32_t entry_agg = rt_record_agg(m, RT_RECORD_PROVIDES);
    uint32_t table_agg = rt_record_agg(m, RT_RECORD_PROVIDED);
    size_t total = 0;
    struct ir_const *items;
    struct ir_const *value;
    struct ir_global *g;
    size_t classes = 0;
    uint32_t list_global;
    size_t n = 0;
    size_t i;
    size_t j;

    for (i = 0; i < m->class_count; i++) {
        total += m->classes[i]->provides_count;
    }
    items = alloc_zeroed(total + 1, sizeof *items);
    for (i = 0; i < m->class_count; i++) {
        const struct ir_class *c = m->classes[i];
        for (j = 0; j < c->provides_count; j++) {
            uint32_t offset = interface_offset(w, m, (uint32_t)i,
                                               c->provides[j].descriptor);
            struct ir_const *item;
            char name[32];
            uint32_t text;
            if (c->init == IR_NO_INDEX || offset == IR_NO_INDEX) {
                text_appendf(errors, "`%s.%s` provides `%s` and is no such "
                             "interface\n", c->module, c->name,
                             c->provides[j].interface);
                continue;
            }
            snprintf(name, sizeof name, "provides.%zu", n);
            text = ir_global_add(m, RUNTIME_MODULE, name,
                                 (const uint8_t *)c->provides[j].interface,
                                 strlen(c->provides[j].interface) + 1,
                                 1)->index;
            item = ir_const_agg(m, ir_aggregate(entry_agg),
                                RT_PROVIDES_ITEM_COUNT);
            const_addr(&item->items[RT_PROVIDES_PATH], text);
            const_int(&item->items[RT_PROVIDES_PATH_LENGTH], IR_I64,
                      strlen(c->provides[j].interface));
            const_addr(&item->items[RT_PROVIDES_DESCRIPTOR], c->provides[j].descriptor);
            const_addr(&item->items[RT_PROVIDES_CLASS_OF], c->descriptor);
            item->items[RT_PROVIDES_INIT].kind = IR_CONST_FUNC;
            item->items[RT_PROVIDES_INIT].scalar = IR_PTR;
            item->items[RT_PROVIDES_INIT].global = c->init;
            if (offset == INJECT_AT_ZERO) {
                const_int(&item->items[RT_PROVIDES_OFFSET], IR_I64, 0);
            } else {
                item->items[RT_PROVIDES_OFFSET].kind = IR_CONST_SYM;
                item->items[RT_PROVIDES_OFFSET].scalar = IR_I64;
                item->items[RT_PROVIDES_OFFSET].sym = offset;
            }
            const_int(&item->items[RT_PROVIDES_FLAGS], IR_I64,
                      ((c->flags & IR_CLASS_ARGS) != 0 ? 1 : 0) |
                          ((c->flags & IR_CLASS_REQUIRED) != 0 ? 2 : 0));
            /* What the interface looked like where the library was
               built: its chain, its fields, its size and the version of
               its package. The loader compares each with the host's. */
            {
                const struct ir_const *record =
                    descriptor_item(m, c->provides[j].descriptor, RT_DESCRIPTOR_VERSIONS);
                const struct ir_const *fields =
                    descriptor_item(m, c->provides[j].descriptor, RT_DESCRIPTOR_FIELD_COUNT);
                const struct ir_const *size =
                    descriptor_item(m, c->provides[j].descriptor, RT_DESCRIPTOR_SIZE);
                const struct ir_const *version =
                    descriptor_item(m, c->provides[j].descriptor, RT_DESCRIPTOR_VERSION);
                const struct ir_const *length =
                    descriptor_item(m, c->provides[j].descriptor, RT_DESCRIPTOR_VERSION_LENGTH);
                const struct ir_const *chain =
                    record != NULL && record->kind == IR_CONST_ADDR
                        ? global_value(m, record->global)
                        : NULL;
                const struct ir_global *built =
                    version != NULL && version->kind == IR_CONST_ADDR &&
                            version->global < m->global_count
                        ? m->globals[version->global]
                        : NULL;
                const struct ir_const *hashes;
                if (chain == NULL || chain->item_count <= RT_VERSIONS_CHAIN_LENGTH ||
                    fields == NULL || size == NULL || length == NULL ||
                    built == NULL || built->bytes == NULL) {
                    text_appendf(errors, "`%s.%s` provides `%s`, and this "
                                 "build reads no version of it\n", c->module,
                                 c->name, c->provides[j].interface);
                    continue;
                }
                /* The descriptor comes from the library file of another
                   module, so each value is checked before it is read. */
                hashes = chain_of(m, chain);
                if (hashes == NULL || !is_integer(fields, false) ||
                    !is_integer(size, true) || !is_integer(length, false) ||
                    length->integer > built->size) {
                    text_appendf(errors, "`%s.%s` provides `%s`, and the "
                                 "library file that describes it is "
                                 "damaged\n", c->module, c->name,
                                 c->provides[j].interface);
                    continue;
                }
                const_addr(&item->items[RT_PROVIDES_CHAIN],
                           copy_chain(m, hashes,
                                      (size_t)chain->items[RT_VERSIONS_CHAIN_LENGTH].integer, n));
                const_int(&item->items[RT_PROVIDES_CHAIN_LENGTH], IR_I64,
                          (uint64_t)chain->items[RT_VERSIONS_CHAIN_LENGTH].integer);
                item->items[RT_PROVIDES_FIELDS] = *fields;
                item->items[RT_PROVIDES_FIELDS].scalar = IR_I64;
                item->items[RT_PROVIDES_SIZE] = *size;
                item->items[RT_PROVIDES_SIZE].scalar = IR_I64;
                snprintf(name, sizeof name, "provides.built.%zu", n);
                const_addr(&item->items[RT_PROVIDES_BUILT],
                           ir_global_add(m, RUNTIME_MODULE, name,
                                         built->bytes, built->size,
                                         1)->index);
                item->items[RT_PROVIDES_BUILT_LENGTH] = *length;
                item->items[RT_PROVIDES_BUILT_LENGTH].scalar = IR_I64;
            }
            items[n++] = *item;
        }
    }
    if (n == 0) {
        text_append(errors, "a plugin has at least one `provides` line\n");
    }
    list_global = write_class_list(m, true, &classes);
    value = ir_const_agg(m, ir_aggregate(table_agg), RT_PROVIDED_ITEM_COUNT);
    const_int(&value->items[RT_PROVIDED_COUNT], IR_I64, n);
    if (n > 0) {
        uint32_t array = ir_array_of(m, rt_record_name(RT_RECORD_PROVIDES),
                                     ir_aggregate(entry_agg), n);
        struct ir_const *list = ir_const_agg(m, ir_aggregate(array), n);
        memcpy(list->items, items, n * sizeof *items);
        const_addr(&value->items[RT_PROVIDED_ENTRIES],
                   ir_global_add_value(m, RUNTIME_MODULE, "provides.entries",
                                       list)->index);
    } else {
        const_int(&value->items[RT_PROVIDED_ENTRIES], IR_PTR, 0);
    }
    const_addr(&value->items[RT_PROVIDED_VERSION],
               ir_global_add(m, RUNTIME_MODULE, "provides.version",
                             (const uint8_t *)ANTIC_VERSION,
                             strlen(ANTIC_VERSION) + 1, 1)->index);
    const_int(&value->items[RT_PROVIDED_VERSION_LENGTH], IR_I64, strlen(ANTIC_VERSION));
    const_int(&value->items[RT_PROVIDED_CLASS_COUNT], IR_I64, classes);
    if (list_global != IR_NO_INDEX) {
        const_addr(&value->items[RT_PROVIDED_CLASSES], list_global);
    } else {
        const_int(&value->items[RT_PROVIDED_CLASSES], IR_PTR, 0);
    }
    g = ir_global_add_value(m, NULL, "anti_rt_provides", value);
    g->exported = true;
    free(items);
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
    } else if (reads_registry(program, &reach) || partial) {
        write_registry(program, options->reflect);
    } else if (options->bundled) {
        write_registry(program, false);
    }
    if (!options->plugin &&
        (asks_backtrace(program, &reach) || options->bundled || partial)) {
        write_backtrace_default(program, options->release);
    }
    calls = calls_through_reflection(program, &reach);
    /* The loader reads the table, and the runtime archive puts the
       loader in every program. Every program therefore carries a table,
       empty where its calls reach no slot. A library for C with the
       runtime bundled carries the loader too. */
    if (!options->library && !options->plugin) {
        write_slots(w, program, &reach, calls && options->reflect, true);
    } else if (options->bundled && !options->plugin) {
        write_slots(w, program, &reach, false, true);
    }
    /* The providers of a library for C are resolved as a program's are.
       Its host is C and cannot fill a slot. */
    inject_errors = errors->length;
    /* A plugin fills no slot: the host holds them, and its own `inject`
       fields read the host's. It carries the table of what it provides
       instead. */
    if (options->plugin) {
        write_provides(w, program, errors);
    } else {
        write_injections(w, program, options, errors);
    }
    ok = ok && errors->length == inject_errors;
    ir_reach_free(&reach);
    /* The trampolines come after every reader of the reach, because they
       add functions that it does not cover. */
    if (calls || options->bundled || partial) {
        write_trampolines(program, (calls || partial) && options->reflect);
    }
    if (options->release) {
        if (!options->plugin &&
            (options->closed || !whole_hosts_plugins(program))) {
            devirtualise(w, program);
        }
        merge_copies(program);
    }
    whole_free(w);
    return ok;
}
