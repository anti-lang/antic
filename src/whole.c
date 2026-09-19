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
    char name[256];
    uint32_t r;
    uint32_t test;
    size_t k;

    if (count == 0 || strlen(text) + 16 > sizeof name) {
        return IR_NO_INDEX;
    }
    snprintf(name, sizeof name, "signature.%s", text);
    signature = ir_declare_add(m, "anti.rt", name, types[0]->type,
                               types[0]->type == IR_AGG ? str_agg
                                                        : IR_NO_AGG);
    ir_param_add(signature, IR_PTR, IR_NO_AGG);
    for (k = 1; k < count; k++) {
        ir_param_add(signature, types[k]->type,
                     types[k]->type == IR_AGG ? str_agg : IR_NO_AGG);
        signature->params[k].ext = types[k]->ext;
    }
    snprintf(name, sizeof name, "trampoline.%s", text);
    f = ir_function_add(m, "anti.rt", name, IR_I8, IR_NO_AGG);
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
                         ir_int_op(IR_I8, types[k + 1]->kind));
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
    ir_store(f, b, IR_I8, ir_int_op(IR_I8, types[0]->kind), result);
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
                                     const struct reach *r)
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

    if (item->kind != IR_CONST_AGG || item->item_count < 5 ||
        item->items[4].kind != IR_CONST_ADDR) {
        return NULL;
    }
    g = m->globals[item->items[4].global];
    return g->bytes != NULL && g->size > 0 && g->bytes[g->size - 1] == 0
               ? (const char *)g->bytes
               : NULL;
}

/* Add each signature of the function list of the class record c to
   seen, once. */
static void add_signatures(const struct ir_module *m, const struct ir_class *c,
                           const char **seen, size_t *count, size_t max)
{
    const struct ir_const *descriptor = m->globals[c->descriptor]->value;
    const struct ir_const *list;
    size_t i;
    size_t k;

    if (descriptor == NULL || descriptor->kind != IR_CONST_AGG ||
        descriptor->item_count < 12 ||
        descriptor->items[11].kind != IR_CONST_ADDR) {
        return;
    }
    list = m->globals[descriptor->items[11].global]->value;
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
    static const char *const item_names[] = {"signature", "call"};
    static const enum ir_type item_types[] = {IR_PTR, IR_PTR};
    static const char *const table_names[] = {"count", "items"};
    static const enum ir_type table_types[] = {IR_I64, IR_PTR};
    uint32_t item_agg = struct_agg(m, "anti.rt.Trampoline", item_names,
                                   item_types, 2);
    uint32_t table_agg = struct_agg(m, "anti.rt.Trampolines", table_names,
                                    table_types, 2);
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

    for (i = 0; i < m->class_count; i++) {
        const struct ir_const *d = m->globals[m->classes[i]->descriptor]->value;
        max += d != NULL && d->item_count >= 12 &&
                       d->items[10].kind == IR_CONST_INT
                   ? (size_t)d->items[10].integer
                   : 0;
    }
    seen = allocate(max, sizeof *seen);
    items = allocate(max, sizeof *items);
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
        text = ir_global_add(m, "anti.rt", name, (const uint8_t *)seen[i],
                             strlen(seen[i]) + 1, 1)->index;
        item = ir_const_agg(m, ir_aggregate(item_agg), 2);
        const_addr(&item->items[0], text);
        item->items[1].kind = IR_CONST_FUNC;
        item->items[1].scalar = IR_PTR;
        item->items[1].global = function;
        items[n++] = *item;
    }
    value = ir_const_agg(m, ir_aggregate(table_agg), 2);
    const_int(&value->items[0], IR_I64, n);
    if (n > 0) {
        char length[48];
        struct ir_const *list;
        uint32_t array;
        snprintf(length, sizeof length, "[%zu]anti.rt.Trampoline", n);
        array = ir_array_add(m, length, ir_aggregate(item_agg),
                             ir_sym_int(m, IR_I64, n), NULL);
        list = ir_const_agg(m, ir_aggregate(array), n);
        memcpy(list->items, items, n * sizeof *items);
        const_addr(&value->items[1],
                   ir_global_add_value(m, "anti.rt", "trampolines.list",
                                       list)->index);
    } else {
        const_int(&value->items[1], IR_PTR, 0);
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
    fields = allocate(count, sizeof *fields);
    count = 0;
    for (i = 0; i < m->class_count; i++) {
        for (j = 0; j < m->classes[i]->mutable_count; j++) {
            fields[count].record = (uint32_t)i;
            fields[count].agg = m->classes[i]->agg;
            fields[count].field = m->classes[i]->mutable_fields[j];
            count++;
        }
    }
    seen = allocate(m->function_count, sizeof *seen);
    work = allocate(m->function_count, sizeof *work);
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
        uint8_t *grown = realloc(s->bits, bytes);
        if (grown == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
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
   `anti_rt_slots` lists each abstract class with a slot reached, and a
   program with none has no table. A library for C has none, because its
   host program carries it. */
/* The number of public functions in the list of the class whose
   descriptor is the global descriptor, or 0 without a list. */
static uint32_t function_count(const struct ir_module *m, uint32_t descriptor)
{
    const struct ir_const *value = m->globals[descriptor]->value;

    if (value == NULL || value->kind != IR_CONST_AGG ||
        value->item_count < 12 || value->items[10].kind != IR_CONST_INT) {
        return 0;
    }
    return (uint32_t)value->items[10].integer;
}

/* DESIGN: `reflect.call` may reach any slot of any interface, because it
   takes the index at run time. A program that calls through reflection
   therefore reaches every slot of every abstract class, slot 1 up to its
   last public function. */
static void write_slots(struct whole *w, struct ir_module *m,
                        const struct reach *r, bool every)
{
    static const char *const slot_names[] = {"descriptor", "slot_count",
                                             "bits"};
    static const enum ir_type slot_types[] = {IR_PTR, IR_I64, IR_PTR};
    static const char *const table_names[] = {"count", "interfaces"};
    static const enum ir_type table_types[] = {IR_I64, IR_PTR};
    size_t classes = m->class_count;
    struct slots *slots = allocate(classes, sizeof *slots);
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
    for (i = 0; every && i < classes; i++) {
        uint32_t last = (m->classes[i]->flags & IR_CLASS_ABSTRACT) != 0
                            ? function_count(m, m->classes[i]->descriptor)
                            : 0;
        uint32_t slot;
        for (slot = 1; slot <= last; slot++) {
            mark_slot(&slots[i], slot);
        }
    }
    for (i = 0; i < classes; i++) {
        emitted += slots[i].count > 0 ? 1 : 0;
    }
    if (emitted > 0) {
        uint32_t slot_agg = struct_agg(m, "anti.rt.Slots", slot_names,
                                       slot_types, 3);
        uint32_t table_agg = struct_agg(m, "anti.rt.SlotTable", table_names,
                                        table_types, 2);
        char name[48];
        struct ir_const *list;
        struct ir_const *value;
        struct ir_global *g;
        size_t n = 0;
        snprintf(name, sizeof name, "[%zu]anti.rt.Slots", emitted);
        list = ir_const_agg(m, ir_aggregate(ir_array_add(
                                   m, name, ir_aggregate(slot_agg),
                                   ir_sym_int(m, IR_I64, emitted), NULL)),
                            emitted);
        for (i = 0; i < classes; i++) {
            struct ir_const *item;
            uint32_t bits;
            if (slots[i].count == 0) {
                continue;
            }
            snprintf(name, sizeof name, "slots.%zu", n);
            bits = ir_global_add(m, "anti.rt", name, slots[i].bits,
                                 (slots[i].count + 7) / 8, 1)->index;
            item = ir_const_agg(m, ir_aggregate(slot_agg), 3);
            const_addr(&item->items[0], m->classes[i]->descriptor);
            const_int(&item->items[1], IR_I64, slots[i].count);
            const_addr(&item->items[2], bits);
            list->items[n++] = *item;
        }
        value = ir_const_agg(m, ir_aggregate(table_agg), 2);
        const_int(&value->items[0], IR_I64, emitted);
        const_addr(&value->items[1],
                   ir_global_add_value(m, "anti.rt", "slots.list",
                                       list)->index);
        g = ir_global_add_value(m, NULL, "anti_rt_slots", value);
        g->exported = true;
    }
    for (i = 0; i < classes; i++) {
        free(slots[i].bits);
    }
    free(slots);
}

bool whole_program(struct ir_module *program,
                   const struct whole_options *options, struct text *errors)
{
    struct whole *w = whole_build(program);
    struct reach reach;
    bool calls;
    bool ok;

    ok = check_singletons(w, program, errors);
    memset(&reach, 0, sizeof reach);
    reach_program(&reach, program, options->entry);
    if (reads_registry(program, &reach)) {
        write_registry(program, options->reflect);
    } else if (options->bundled) {
        write_registry(program, false);
    }
    calls = calls_through_reflection(program, &reach);
    if (!options->library) {
        write_slots(w, program, &reach, calls && options->reflect);
    }
    reach_free(&reach);
    /* The trampolines come after every reader of the reach, because they
       add functions that it does not cover. */
    if (calls || options->bundled) {
        write_trampolines(program, calls && options->reflect);
    }
    if (options->release) {
        devirtualise(w, program);
    }
    whole_free(w);
    return ok;
}
