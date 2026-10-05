#include "ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

/* Growable arrays of the module are plain heap arrays, released by
   ir_module_free. Names and global bytes go into the memory pool. */

static const char *keep(struct arena *arena, const char *s)
{
    size_t n;
    char *copy;

    if (s == NULL) {
        return NULL;
    }
    n = strlen(s);
    copy = arena_alloc(arena, n + 1);
    memcpy(copy, s, n + 1);
    return copy;
}

void ir_module_init(struct ir_module *m, struct arena *arena,
                    const char *name)
{
    memset(m, 0, sizeof *m);
    m->arena = arena;
    m->name = keep(arena, name);
}

void ir_block_free(struct ir_block *b)
{
    size_t i;

    for (i = 0; i < b->count; i++) {
        free(b->insts[i].args);
    }
    free(b->insts);
    free(b);
}

void ir_function_free_body(struct ir_function *f)
{
    size_t i;

    for (i = 0; i < f->block_count; i++) {
        ir_block_free(f->blocks[i]);
    }
    free(f->blocks);
    free(f->temps);
    f->blocks = NULL;
    f->block_count = 0;
    f->block_capacity = 0;
    f->temps = NULL;
    f->temp_count = 0;
    f->temp_capacity = 0;
}

void ir_function_free(struct ir_function *f)
{
    ir_function_free_body(f);
    free(f->params);
    f->params = NULL;
    f->param_count = 0;
    f->param_capacity = 0;
}

void ir_class_free(struct ir_class *c)
{
    free(c->subtables);
    free(c->mutable_fields);
    free(c->injects);
    free(c->provides);
    c->subtables = NULL;
    c->subtable_count = 0;
    c->mutable_fields = NULL;
    c->mutable_count = 0;
    c->injects = NULL;
    c->inject_count = 0;
    c->provides = NULL;
    c->provides_count = 0;
}

void ir_module_free(struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        ir_function_free(m->functions[i]);
    }
    for (i = 0; i < m->class_count; i++) {
        ir_class_free(m->classes[i]);
    }
    free(m->classes);
    free(m->functions);
    free(m->globals);
    free(m->aggs);
    free(m->syms);
    free(m->files);
    memset(m, 0, sizeof *m);
}

struct ir_vtype ir_scalar(enum ir_type type)
{
    struct ir_vtype v;

    v.type = type;
    v.agg = IR_NO_AGG;
    return v;
}

struct ir_vtype ir_aggregate(uint32_t agg)
{
    struct ir_vtype v;

    v.type = IR_AGG;
    v.agg = agg;
    return v;
}

uint32_t ir_index(size_t count)
{
    if (count >= UINT32_MAX) {
        alloc_out_of_memory();
    }
    return (uint32_t)count;
}

bool ir_is_copy_name(const char *name)
{
    return strchr(name, '<') != NULL;
}

uint64_t ir_hash_name(const char *module, const char *name)
{
    uint64_t h = 1469598103934665603u;
    const char *p;

    for (p = module; p != NULL && *p != '\0'; p++) {
        h = (h ^ (unsigned char)*p) * 1099511628211u;
    }
    h = (h ^ 0xff) * 1099511628211u;
    for (p = name; *p != '\0'; p++) {
        h = (h ^ (unsigned char)*p) * 1099511628211u;
    }
    return h;
}

uint32_t ir_agg_find(const struct ir_module *m, const char *name)
{
    size_t i;

    for (i = 0; i < m->agg_count; i++) {
        if (strcmp(m->aggs[i]->name, name) == 0) {
            return ir_index(i);
        }
    }
    return IR_NO_AGG;
}

/* DESIGN: an aggregate is identified by its name. A struct name carries
   its module, and an array name its length expression, so two entries
   with one name describe the same type. */
static uint32_t add_agg(struct ir_module *m, const struct ir_aggtype *key,
                        const struct ir_field *fields, size_t count)
{
    uint32_t found = ir_agg_find(m, key->name);
    struct ir_aggtype *t;
    size_t i;

    if (found != IR_NO_AGG) {
        return found;
    }
    t = arena_alloc(m->arena, sizeof *t);
    *t = *key;
    t->name = keep(m->arena, key->name);
    t->length_text = keep(m->arena, key->length_text);
    t->fields = arena_alloc(m->arena,
                            alloc_product(count + 1, sizeof *t->fields));
    for (i = 0; i < count; i++) {
        t->fields[i] = fields[i];
        t->fields[i].name = keep(m->arena, fields[i].name);
    }
    t->field_count = count;
    m->aggs = alloc_grow(m->aggs, &m->agg_capacity, m->agg_count,
                         sizeof *m->aggs);
    m->aggs[m->agg_count] = t;
    return ir_index(m->agg_count++);
}

uint32_t ir_file_add(struct ir_module *m, const char *path)
{
    size_t i;

    for (i = 0; i < m->file_count; i++) {
        if (strcmp(m->files[i], path) == 0) {
            return ir_index(i);
        }
    }
    m->files = alloc_grow(m->files, &m->file_capacity, m->file_count,
                    sizeof *m->files);
    m->files[m->file_count] = keep(m->arena, path);
    return ir_index(m->file_count++);
}

uint32_t ir_struct_add(struct ir_module *m, enum ir_agg_kind kind,
                       const char *name, const struct ir_field *fields,
                       size_t count, bool packed, uint64_t align)
{
    struct ir_aggtype key;

    memset(&key, 0, sizeof key);
    key.kind = kind;
    key.name = name;
    key.packed = packed;
    key.align = align;
    return add_agg(m, &key, fields, count);
}

/* DESIGN: a lane is a scalar of one width on every target, as the
   checker requires of a lane of a simd struct. The width is part of the
   type's name, so the IR stays free of sizes the target chooses. */
unsigned ir_lane_bytes(enum ir_type type)
{
    switch (type) {
    case IR_I8: return 1;
    case IR_I16: return 2;
    case IR_I32:
    case IR_F32: return 4;
    case IR_I64:
    case IR_F64: return 8;
    default: return 0;
    }
}

uint32_t ir_simd_add(struct ir_module *m, const char *name,
                     const struct ir_field *fields, size_t count)
{
    struct ir_aggtype key;

    memset(&key, 0, sizeof key);
    key.kind = IR_AGG_STRUCT;
    key.name = name;
    key.simd = true;
    return add_agg(m, &key, fields, count);
}

uint32_t ir_array_add(struct ir_module *m, const char *name,
                      struct ir_vtype element, uint32_t length,
                      const char *length_text)
{
    struct ir_aggtype key;
    struct ir_field field;

    memset(&key, 0, sizeof key);
    memset(&field, 0, sizeof field);
    key.kind = IR_AGG_ARRAY;
    key.name = name;
    key.length = length;
    key.length_text = length_text;
    field.type = element;
    return add_agg(m, &key, &field, 1);
}

uint32_t ir_array_of(struct ir_module *m, const char *element_name,
                     struct ir_vtype element, size_t n)
{
    struct text name = {0};
    char digits[24];
    uint32_t agg;

    text_appendf(&name, "[%zu]%s", n, element_name);
    agg = ir_agg_find(m, text_cstr(&name));
    if (agg == IR_NO_AGG) {
        text_format(digits, sizeof digits, "%zu", n);
        agg = ir_array_add(m, text_cstr(&name), element,
                           ir_sym_int(m, IR_I64, (uint64_t)n), digits);
    }
    text_free(&name);
    return agg;
}

static uint32_t add_sym(struct ir_module *m, const struct ir_sym *key)
{
    size_t i;

    for (i = 0; i < m->sym_count; i++) {
        const struct ir_sym *s = &m->syms[i];
        if (s->kind == key->kind && s->type == key->type &&
            s->value == key->value && s->of.type == key->of.type &&
            s->of.agg == key->of.agg && s->field == key->field &&
            s->op == key->op && s->a == key->a && s->b == key->b) {
            return ir_index(i);
        }
    }
    m->syms = alloc_grow(m->syms, &m->sym_capacity, m->sym_count,
                         sizeof *m->syms);
    m->syms[m->sym_count] = *key;
    return ir_index(m->sym_count++);
}

static struct ir_sym sym_key(enum ir_sym_kind kind, enum ir_type type)
{
    struct ir_sym key;

    memset(&key, 0, sizeof key);
    key.kind = kind;
    key.type = type;
    key.of = ir_scalar(IR_VOID);
    key.a = IR_NO_AGG;
    key.b = IR_NO_AGG;
    return key;
}

uint32_t ir_sym_int(struct ir_module *m, enum ir_type type, uint64_t value)
{
    struct ir_sym key = sym_key(IR_SYM_INT, type);

    key.value = ir_int_op(type, value).as.integer;
    return add_sym(m, &key);
}

uint32_t ir_sym_size_of(struct ir_module *m, struct ir_vtype of)
{
    struct ir_sym key = sym_key(IR_SYM_SIZE_OF, IR_I64);

    key.of = of;
    return add_sym(m, &key);
}

uint32_t ir_sym_offset_of(struct ir_module *m, uint32_t agg, uint32_t field)
{
    struct ir_sym key = sym_key(IR_SYM_OFFSET_OF, IR_I64);

    key.of = ir_aggregate(agg);
    key.field = field;
    return add_sym(m, &key);
}

uint32_t ir_sym_op(struct ir_module *m, enum ir_op op, enum ir_type type,
                   uint32_t a, uint32_t b)
{
    struct ir_sym key = sym_key(IR_SYM_OP, type);

    key.op = (int)op;
    key.a = a;
    key.b = b;
    return add_sym(m, &key);
}

struct ir_operand ir_sym_operand(const struct ir_module *m, uint32_t sym)
{
    struct ir_operand o = {IR_SYM, IR_VOID, {0}};
    const struct ir_sym *s = &m->syms[sym];

    if (s->kind == IR_SYM_INT) {
        return ir_int_op(s->type, s->value);
    }
    o.type = s->type;
    o.as.index = sym;
    return o;
}

static struct ir_function *new_function(struct ir_module *m,
                                        const char *module, const char *name,
                                        enum ir_type result)
{
    struct ir_function *f = arena_alloc(m->arena, sizeof *f);

    m->functions = alloc_grow(m->functions, &m->function_capacity,
                        m->function_count, sizeof *m->functions);
    f->index = ir_index(m->function_count);
    f->file = IR_NO_INDEX;
    f->module = keep(m->arena, module);
    f->name = keep(m->arena, name);
    f->result = result;
    f->result_agg = IR_NO_AGG;
    m->functions[m->function_count++] = f;
    return f;
}

bool ir_in_unit(const char *module, const char *unit, const char *entry)
{
    return unit != NULL ? strcmp(unit, entry) == 0
                        : module != NULL && strcmp(module, entry) == 0;
}

bool ir_is_patterns_start(const struct ir_function *f)
{
    return !f->is_extern && f->module != NULL &&
           strcmp(f->name, IR_PATTERNS_START) == 0;
}

struct ir_function *ir_function_add(struct ir_module *m, const char *module,
                                    const char *name, enum ir_type result,
                                    uint32_t result_agg)
{
    struct ir_function *f = new_function(m, module, name, result);
    f->result_agg = result_agg;
    return f;
}

struct ir_function *ir_extern_add(struct ir_module *m, const char *name,
                                  enum ir_type result, bool variadic)
{
    struct ir_function *f = new_function(m, NULL, name, result);
    f->is_extern = true;
    f->variadic = variadic;
    return f;
}

struct ir_function *ir_declare_add(struct ir_module *m, const char *module,
                                   const char *name, enum ir_type result,
                                   uint32_t result_agg)
{
    struct ir_function *f = new_function(m, module, name, result);
    f->result_agg = result_agg;
    f->is_extern = true;
    return f;
}

uint32_t ir_temp(struct ir_function *f, enum ir_type type)
{
    /* A temporary is a 32-bit index, and IR_NO_RESULT is the last one. */
    if (f->temp_count >= IR_NO_RESULT - 1) {
        alloc_out_of_memory();
    }
    f->temps = alloc_grow(f->temps, &f->temp_capacity, f->temp_count,
                          sizeof *f->temps);
    f->temps[f->temp_count] = type;
    return f->temp_count++;
}

uint32_t ir_param_add(struct ir_function *f, enum ir_type type, uint32_t agg)
{
    struct ir_param *p;

    f->params = alloc_grow(f->params, &f->param_capacity, f->param_count,
                     sizeof *f->params);
    p = &f->params[f->param_count++];
    p->type = type;
    p->ext = IR_EXT_NONE;
    p->agg = agg;
    p->nonnull = false;
    p->own = false;
    p->deref_size = IR_NO_INDEX;
    p->temp = f->is_extern ? IR_NO_RESULT
                           : ir_temp(f, type == IR_AGG ? IR_PTR : type);
    return p->temp;
}

struct ir_block *ir_block_add(struct ir_function *f)
{
    struct ir_block *b = alloc_zeroed(1, sizeof *b);

    f->blocks = alloc_grow(f->blocks, &f->block_capacity, f->block_count,
                     sizeof *f->blocks);
    b->index = ir_index(f->block_count);
    f->blocks[f->block_count++] = b;
    return b;
}

struct ir_global *ir_global_add(struct ir_module *m, const char *module,
                                const char *name, const uint8_t *bytes,
                                uint64_t size, uint64_t align)
{
    struct ir_global *g = arena_alloc(m->arena, sizeof *g);

    memset(g, 0, sizeof *g);
    m->globals = alloc_grow(m->globals, &m->global_capacity, m->global_count,
                      sizeof *m->globals);
    g->index = ir_index(m->global_count);
    g->module = keep(m->arena, module);
    g->name = keep(m->arena, name);
    g->bytes = arena_alloc(m->arena, size == 0 ? 1 : size);
    if (size > 0) {
        memcpy(g->bytes, bytes, size);
    }
    g->size = size;
    g->align = align;
    m->globals[m->global_count++] = g;
    return g;
}

struct ir_global *ir_global_add_value(struct ir_module *m, const char *module,
                                      const char *name,
                                      struct ir_const *value)
{
    struct ir_global *g = ir_global_add(m, module, name, NULL, 0, 0);

    g->bytes = NULL;
    g->value = value;
    return g;
}

struct ir_class *ir_class_add(struct ir_module *m, const char *module,
                              const char *name)
{
    struct ir_class *c = arena_alloc(m->arena, sizeof *c);

    memset(c, 0, sizeof *c);
    m->classes = alloc_grow(m->classes, &m->class_capacity, m->class_count,
                      sizeof *m->classes);
    c->module = keep(m->arena, module);
    c->name = keep(m->arena, name);
    c->descriptor = IR_NO_INDEX;
    c->base = IR_NO_INDEX;
    c->table = IR_NO_INDEX;
    c->init = IR_NO_INDEX;
    c->agg = IR_NO_AGG;
    m->classes[m->class_count++] = c;
    return c;
}

void ir_class_subtable(struct ir_class *c, uint32_t interface, uint32_t table,
                       uint32_t agg, uint32_t field)
{
    struct ir_subtable *grown =
        alloc_resize(c->subtables, c->subtable_count + 1, sizeof *grown);

    c->subtables = grown;
    c->subtables[c->subtable_count].interface = interface;
    c->subtables[c->subtable_count].table = table;
    c->subtables[c->subtable_count].agg = agg;
    c->subtables[c->subtable_count].field = field;
    c->subtable_count++;
}

void ir_class_inject(struct ir_module *m, struct ir_class *c,
                     const char *interface, const char *field,
                     uint32_t descriptor, bool final)
{
    struct ir_inject *grown =
        alloc_resize(c->injects, c->inject_count + 1, sizeof *grown);

    c->injects = grown;
    c->injects[c->inject_count].interface = keep(m->arena, interface);
    c->injects[c->inject_count].field = keep(m->arena, field);
    c->injects[c->inject_count].descriptor = descriptor;
    c->injects[c->inject_count].final = final;
    c->inject_count++;
}

void ir_class_provides(struct ir_module *m, struct ir_class *c,
                       const char *interface, uint32_t descriptor)
{
    struct ir_provides *grown =
        alloc_resize(c->provides, c->provides_count + 1, sizeof *grown);

    c->provides = grown;
    c->provides[c->provides_count].interface = keep(m->arena, interface);
    c->provides[c->provides_count].descriptor = descriptor;
    c->provides_count++;
}

void ir_class_mutable(struct ir_class *c, uint32_t field)
{
    uint32_t *grown =
        alloc_resize(c->mutable_fields, c->mutable_count + 1, sizeof *grown);

    c->mutable_fields = grown;
    c->mutable_fields[c->mutable_count++] = field;
}

struct ir_const *ir_const_agg(struct ir_module *m, struct ir_vtype type,
                              size_t count)
{
    struct ir_const *c = arena_alloc(m->arena, sizeof *c);

    c->kind = IR_CONST_AGG;
    c->type = type;
    c->item_count = count;
    if (count > SIZE_MAX / sizeof *c->items) {
        alloc_out_of_memory();
    }
    c->items = arena_alloc(m->arena, (count == 0 ? 1 : count) *
                                     sizeof *c->items);
    return c;
}

struct ir_const *ir_const_int(struct ir_module *m, enum ir_type type,
                              uint64_t integer)
{
    struct ir_const *c = arena_alloc(m->arena, sizeof *c);

    c->kind = IR_CONST_INT;
    c->scalar = type;
    c->integer = integer;
    return c;
}

bool ir_const_equal(const struct ir_const *a, const struct ir_const *b)
{
    size_t i;

    if (a == NULL || b == NULL) {
        return a == b;
    }
    if (a->kind != b->kind || a->scalar != b->scalar) {
        return false;
    }
    switch (a->kind) {
    case IR_CONST_NONE:
        return true;
    case IR_CONST_INT:
        return a->integer == b->integer;
    case IR_CONST_FLOAT:
        /* The bits, so that 0.0 and -0.0 are two constants. */
        return memcmp(&a->floating, &b->floating, sizeof a->floating) == 0;
    case IR_CONST_SYM:
        return a->sym == b->sym;
    case IR_CONST_ADDR:
    case IR_CONST_FUNC:
        return a->global == b->global;
    default: /* IR_CONST_AGG */
        if (a->type.agg != b->type.agg || a->item_count != b->item_count) {
            return false;
        }
        for (i = 0; i < a->item_count; i++) {
            if (!ir_const_equal(&a->items[i], &b->items[i])) {
                return false;
            }
        }
        return true;
    }
}

static void add_reloc(struct ir_module *m, struct ir_global *g,
                      uint64_t offset, uint32_t target, bool fn)
{
    struct ir_reloc *relocs = g->relocs;

    /* The arena keeps every list it gave out, so the list doubles and
       the copies it leaves behind add up to less than its length. */
    if (g->reloc_count == g->reloc_capacity) {
        size_t capacity =
            g->reloc_capacity == 0 ? 4 : alloc_product(g->reloc_capacity, 2);
        relocs =
            arena_alloc(m->arena, alloc_product(capacity, sizeof *relocs));
        if (g->reloc_count > 0) {
            memcpy(relocs, g->relocs, g->reloc_count * sizeof *relocs);
        }
        g->relocs = relocs;
        g->reloc_capacity = capacity;
    }
    relocs[g->reloc_count].offset = offset;
    relocs[g->reloc_count].global = target;
    relocs[g->reloc_count].fn = fn;
    g->reloc_count++;
}

void ir_global_reloc(struct ir_module *m, struct ir_global *g,
                     uint64_t offset, uint32_t target)
{
    add_reloc(m, g, offset, target, false);
}

/* The address of a function, which a class table holds one of per public
   function of its chain. */
void ir_global_reloc_fn(struct ir_module *m, struct ir_global *g,
                        uint64_t offset, uint32_t function)
{
    add_reloc(m, g, offset, function, true);
}

struct ir_operand ir_temp_op(const struct ir_function *f, uint32_t temp)
{
    struct ir_operand o = {IR_TEMP, IR_VOID, {0}};
    o.type = f->temps[temp];
    o.as.temp = temp;
    return o;
}

/* An integer constant keeps only the bits of its type, so that equal
   values have equal operands. */
struct ir_operand ir_int_op(enum ir_type type, uint64_t value)
{
    struct ir_operand o = {IR_INT, IR_VOID, {0}};
    o.type = type;
    switch (type) {
    case IR_I8:
        value &= 0xff;
        break;
    case IR_I16:
        value &= 0xffff;
        break;
    case IR_I32:
        value &= 0xffffffff;
        break;
    default:
        break;
    }
    o.as.integer = value;
    return o;
}

struct ir_operand ir_float_op(enum ir_type type, double value)
{
    struct ir_operand o = {IR_FLOAT, IR_VOID, {0}};
    o.type = type;
    o.as.floating = value;
    return o;
}

static struct ir_operand ir_block_op(const struct ir_block *b)
{
    struct ir_operand o = {IR_BLOCK, IR_VOID, {0}};
    o.as.index = b->index;
    return o;
}

struct ir_operand ir_func_op(const struct ir_function *f)
{
    struct ir_operand o = {IR_FUNC, IR_PTR, {0}};
    o.as.index = f->index;
    return o;
}

struct ir_operand ir_global_op(const struct ir_global *g)
{
    struct ir_operand o = {IR_GLOBAL, IR_PTR, {0}};
    o.as.index = g->index;
    return o;
}

/* Append an instruction of f to b and give it the line that f stands on.
   f is NULL for a copy of an instruction that carries its own line. */
static struct ir_inst *append(const struct ir_function *f,
                              struct ir_block *b, enum ir_op op,
                              enum ir_type type, uint32_t result)
{
    struct ir_inst *inst;

    b->insts = alloc_grow(b->insts, &b->capacity, b->count, sizeof *b->insts);
    inst = &b->insts[b->count++];
    memset(inst, 0, sizeof *inst);
    inst->of = ir_scalar(IR_VOID);
    inst->op = op;
    inst->type = type;
    inst->result = result;
    inst->line = f != NULL ? f->at_line : 0;
    return inst;
}

void ir_inst_add(struct ir_block *b, const struct ir_inst *inst)
{
    struct ir_inst *copy = append(NULL, b, inst->op, inst->type,
                                  inst->result);

    *copy = *inst;
    copy->args = NULL;
    if (inst->arg_count > 0) {
        copy->args = alloc_zeroed(inst->arg_count, sizeof *copy->args);
        memcpy(copy->args, inst->args, inst->arg_count * sizeof *copy->args);
    }
}

uint32_t ir_binary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                   enum ir_type type, struct ir_operand x,
                   struct ir_operand y)
{
    uint32_t result = ir_temp(f, type);
    struct ir_inst *inst = append(f, b, op, type, result);

    inst->a = x;
    inst->b = y;
    return result;
}

uint32_t ir_flag_op(struct ir_function *f, struct ir_block *b, enum ir_op op,
                    enum ir_type type, struct ir_operand x,
                    struct ir_operand y, struct ir_operand c)
{
    uint32_t result = ir_temp(f, type);
    struct ir_inst *inst = append(f, b, op, type, result);

    inst->a = x;
    inst->b = y;
    inst->c = c;
    return result;
}

uint32_t ir_flag(struct ir_function *f, struct ir_block *b, enum ir_flag flag,
                 struct ir_operand of)
{
    uint32_t result = ir_temp(f, IR_I8);
    struct ir_inst *inst = append(f, b, IR_FLAG, IR_I8, result);

    inst->a = of;
    inst->field = (uint32_t)flag;
    return result;
}

uint32_t ir_unary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                  enum ir_type type, struct ir_operand x)
{
    uint32_t result = ir_temp(f, type);
    append(f, b, op, type, result)->a = x;
    return result;
}

void ir_assign(struct ir_function *f, struct ir_block *b, uint32_t dst,
               struct ir_operand src)
{
    append(f, b, IR_COPY, f->temps[dst], dst)->a = src;
}

uint32_t ir_entry_slot(struct ir_function *f, struct ir_vtype of)
{
    struct ir_block *b = f->blocks[0];
    uint32_t result = ir_temp(f, IR_PTR);
    size_t at = 0;
    struct ir_inst *inst;

    while (at < b->count && b->insts[at].op == IR_SLOT) {
        at++;
    }
    b->insts = alloc_grow(b->insts, &b->capacity, b->count, sizeof *b->insts);
    memmove(&b->insts[at + 1], &b->insts[at],
            (b->count - at) * sizeof *b->insts);
    b->count++;
    inst = &b->insts[at];
    memset(inst, 0, sizeof *inst);
    inst->op = IR_SLOT;
    inst->type = IR_PTR;
    inst->result = result;
    inst->of = of;
    return result;
}

uint32_t ir_slot(struct ir_function *f, struct ir_block *b,
                 struct ir_vtype of)
{
    uint32_t result = ir_temp(f, IR_PTR);

    append(f, b, IR_SLOT, IR_PTR, result)->of = of;
    return result;
}

uint32_t ir_load(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand pointer)
{
    uint32_t result = ir_temp(f, type);
    append(f, b, IR_LOAD, type, result)->a = pointer;
    return result;
}

uint32_t ir_entry_zero_slot(struct ir_function *f)
{
    uint32_t slot = ir_entry_slot(f, ir_scalar(IR_PTR));
    struct ir_block *b = f->blocks[0];
    size_t at = 0;
    struct ir_inst *inst;

    while (at < b->count && b->insts[at].op == IR_SLOT) {
        at++;
    }
    b->insts = alloc_grow(b->insts, &b->capacity, b->count, sizeof *b->insts);
    memmove(&b->insts[at + 1], &b->insts[at],
            (b->count - at) * sizeof *b->insts);
    b->count++;
    inst = &b->insts[at];
    memset(inst, 0, sizeof *inst);
    inst->of = ir_scalar(IR_VOID);
    inst->op = IR_STORE;
    inst->type = IR_PTR;
    inst->result = IR_NO_RESULT;
    inst->a = ir_int_op(IR_PTR, 0);
    inst->b = ir_temp_op(f, slot);
    return slot;
}

void ir_store(struct ir_function *f, struct ir_block *b, enum ir_type type,
              struct ir_operand value, struct ir_operand pointer)
{
    struct ir_inst *inst = append(f, b, IR_STORE, type, IR_NO_RESULT);

    inst->a = value;
    inst->b = pointer;
}

uint32_t ir_load_access(struct ir_function *f, struct ir_block *b,
                        struct ir_operand pointer, enum ir_access access)
{
    uint32_t result = ir_load(f, b, IR_PTR, pointer);

    b->insts[b->count - 1].field = (uint32_t)access;
    return result;
}

void ir_store_access(struct ir_function *f, struct ir_block *b,
                     struct ir_operand value, struct ir_operand pointer,
                     enum ir_access access)
{
    ir_store(f, b, IR_PTR, value, pointer);
    b->insts[b->count - 1].field = (uint32_t)access;
}

uint32_t ir_ptradd(struct ir_function *f, struct ir_block *b,
                   struct ir_operand pointer, struct ir_operand offset)
{
    return ir_binary(f, b, IR_PTRADD, IR_PTR, pointer, offset);
}

void ir_memcopy(struct ir_function *f, struct ir_block *b,
                struct ir_operand dst, struct ir_operand src,
                struct ir_vtype of)
{
    struct ir_inst *inst = append(f, b, IR_MEMCOPY, IR_VOID, IR_NO_RESULT);

    inst->a = dst;
    inst->b = src;
    inst->of = of;
}

uint32_t ir_addr(struct ir_function *f, struct ir_block *b,
                 struct ir_operand target)
{
    return ir_unary(f, b, IR_ADDR, IR_PTR, target);
}

uint32_t ir_bitload(struct ir_function *f, struct ir_block *b,
                    enum ir_type type, struct ir_operand pointer, uint32_t agg,
                    uint32_t field)
{
    uint32_t result = ir_temp(f, type);
    struct ir_inst *inst = append(f, b, IR_BITLOAD, type, result);

    inst->a = pointer;
    inst->of = ir_aggregate(agg);
    inst->field = field;
    return result;
}

void ir_bitstore(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand value, struct ir_operand pointer,
                 uint32_t agg, uint32_t field)
{
    struct ir_inst *inst = append(f, b, IR_BITSTORE, type, IR_NO_RESULT);

    inst->a = value;
    inst->b = pointer;
    inst->of = ir_aggregate(agg);
    inst->field = field;
}

/* A simd operation on agg without a result, with lane operation op. */
static struct ir_inst *vector_op(struct ir_function *f, struct ir_block *b,
                                 enum ir_op kind, enum ir_op op,
                                 enum ir_type lane, uint32_t agg)
{
    struct ir_inst *inst = append(f, b, kind, lane, IR_NO_RESULT);

    inst->of = ir_aggregate(agg);
    inst->field = (uint32_t)op;
    return inst;
}

void ir_vbinary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                enum ir_type lane, struct ir_operand dst, struct ir_operand x,
                struct ir_operand y, uint32_t agg)
{
    struct ir_inst *inst = vector_op(f, b, IR_VBINARY, op, lane, agg);

    inst->a = dst;
    inst->b = x;
    inst->c = y;
}

void ir_vunary(struct ir_function *f, struct ir_block *b, enum ir_op op,
               enum ir_type lane, struct ir_operand dst, struct ir_operand x,
               uint32_t agg)
{
    struct ir_inst *inst = vector_op(f, b, IR_VUNARY, op, lane, agg);

    inst->a = dst;
    inst->b = x;
}

void ir_vsplat(struct ir_function *f, struct ir_block *b, enum ir_type lane,
               struct ir_operand dst, struct ir_operand value, uint32_t agg)
{
    struct ir_inst *inst = vector_op(f, b, IR_VSPLAT, IR_COPY, lane, agg);

    inst->a = dst;
    inst->b = value;
}

static struct ir_operand *operand_list(size_t count)
{
    return alloc_zeroed(count, sizeof(struct ir_operand));
}

void ir_vselect(struct ir_function *f, struct ir_block *b, enum ir_type lane,
                struct ir_operand dst, struct ir_operand mask,
                struct ir_operand x, struct ir_operand y, uint32_t agg)
{
    struct ir_inst *inst = vector_op(f, b, IR_VSELECT, IR_COPY, lane, agg);

    inst->a = dst;
    inst->b = mask;
    inst->c = x;
    inst->args = operand_list(1);
    inst->args[0] = y;
    inst->arg_count = 1;
}

void ir_vshuffle(struct ir_function *f, struct ir_block *b, enum ir_type lane,
                 struct ir_operand dst, struct ir_operand x,
                 const uint32_t *lanes, size_t count, uint32_t agg)
{
    struct ir_inst *inst = vector_op(f, b, IR_VSHUFFLE, IR_COPY, lane, agg);
    size_t i;

    inst->a = dst;
    inst->b = x;
    inst->args = operand_list(count);
    for (i = 0; i < count; i++) {
        inst->args[i] = ir_int_op(IR_I32, lanes[i]);
    }
    inst->arg_count = count;
}

uint32_t ir_vreduce(struct ir_function *f, struct ir_block *b, enum ir_op op,
                    enum ir_type type, struct ir_operand x, uint32_t agg)
{
    uint32_t result = ir_temp(f, type);
    struct ir_inst *inst = append(f, b, IR_VREDUCE, type, result);

    inst->a = x;
    inst->of = ir_aggregate(agg);
    inst->field = (uint32_t)op;
    return result;
}

uint32_t ir_call(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand callee, const struct ir_operand *args,
                 size_t arg_count)
{
    uint32_t result = type == IR_VOID ? IR_NO_RESULT
                                      : ir_temp(f, type == IR_AGG ? IR_PTR
                                                                  : type);
    struct ir_inst *inst = append(f, b, IR_CALL, type, result);

    inst->a = callee;
    inst->arg_count = arg_count;
    if (arg_count > 0) {
        inst->args = alloc_zeroed(arg_count, sizeof *inst->args);
        memcpy(inst->args, args, arg_count * sizeof *inst->args);
    }
    return result;
}

uint32_t ir_call_indirect(struct ir_function *f, struct ir_block *b,
                          enum ir_type type, struct ir_operand target,
                          const struct ir_function *signature,
                          const struct ir_operand *args, size_t arg_count)
{
    uint32_t result = ir_call(f, b, type, target, args, arg_count);

    b->insts[b->count - 1].b = ir_func_op(signature);
    return result;
}

void ir_jump(struct ir_function *f, struct ir_block *b,
             const struct ir_block *target)
{
    append(f, b, IR_JUMP, IR_VOID, IR_NO_RESULT)->a = ir_block_op(target);
}

void ir_branch(struct ir_function *f, struct ir_block *b,
               struct ir_operand cond, const struct ir_block *then_block,
               const struct ir_block *else_block)
{
    struct ir_inst *inst = append(f, b, IR_BRANCH, IR_VOID, IR_NO_RESULT);

    inst->a = cond;
    inst->b = ir_block_op(then_block);
    inst->c = ir_block_op(else_block);
}

void ir_branch_ov(struct ir_function *f, struct ir_block *b,
                  struct ir_operand value, const struct ir_block *then_block,
                  const struct ir_block *else_block)
{
    struct ir_inst *inst = append(f, b, IR_BRANCH_OV, IR_VOID, IR_NO_RESULT);

    inst->a = value;
    inst->b = ir_block_op(then_block);
    inst->c = ir_block_op(else_block);
}

void ir_ret(struct ir_function *f, struct ir_block *b, enum ir_type type,
            struct ir_operand value)
{
    append(f, b, IR_RET, type, IR_NO_RESULT)->a = value;
}

void ir_unreachable(struct ir_function *f, struct ir_block *b)
{
    append(f, b, IR_UNREACHABLE, IR_VOID, IR_NO_RESULT);
}
