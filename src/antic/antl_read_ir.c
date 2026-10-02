/* The reader of the IR of a library file: the aggregate and symbolic
   tables, the constants, the signatures and bodies of the functions, the
   globals with their addresses and the class records, each moved to the
   indices of the program. A copy of a generic the program has already is
   merged with it. */

#include "alloc.h"
#include "antl_io.h"

#include <stdlib.h>
#include <string.h>

enum { MAP_NONE, MAP_BUSY, MAP_DONE };

/* The larger of two heights. */
static uint32_t nest_max(uint32_t height, uint32_t h)
{
    return h > height ? h : height;
}

static bool valid_type(uint8_t type)
{
    return type <= IR_LOCK;
}

/* Where each function, global, aggregate and symbolic value of the file
   went in the program. */
struct ir_maps {
    uint32_t *files;                /* the program's index of each file */
    uint32_t file_count;
    uint32_t *functions;
    uint32_t function_count;
    uint32_t *globals;
    uint32_t global_count;
    struct ir_aggtype *aggs;        /* as read, with indices of the file */
    uint32_t *agg_map;
    uint8_t *agg_state;
    uint32_t *agg_height;           /* how deep each aggregate nests */
    uint32_t agg_count;
    struct ir_sym *syms;            /* as read, with indices of the file */
    uint32_t *sym_map;
    uint8_t *sym_state;
    uint32_t *sym_height;           /* how deep each value nests */
    uint32_t sym_count;
};

static uint32_t map_sym_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t sym, uint32_t depth);

/* The program's index of aggregate agg of the file. The types an
   aggregate is built from are added before it. depth counts the
   aggregates and values this call is mapped for. */
static uint32_t map_agg_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t agg, uint32_t depth)
{
    struct ir_aggtype *t;
    uint32_t height = 0;
    size_t i;

    if (agg >= maps->agg_count || maps->agg_state[agg] == MAP_BUSY ||
        depth >= TYPES_NEST_MAX) {
        antl_damaged(r);
        return 0;
    }
    if (maps->agg_state[agg] == MAP_DONE) {
        return maps->agg_map[agg];
    }
    maps->agg_state[agg] = MAP_BUSY;
    t = &maps->aggs[agg];
    for (i = 0; i < t->field_count && !r->failed; i++) {
        if (t->fields[i].type.type == IR_AGG) {
            uint32_t of = t->fields[i].type.agg;
            t->fields[i].type.agg = map_agg_at(r, program, maps, of,
                                               depth + 1);
            if (!r->failed) {
                height = nest_max(height, maps->agg_height[of]);
            }
        }
    }
    if (r->failed) {
        return 0;
    }
    if (t->kind == IR_AGG_ARRAY) {
        uint32_t length = map_sym_at(r, program, maps, t->length, depth + 1);
        if (!r->failed) {
            height = nest_max(height, maps->sym_height[t->length]);
        }
        maps->agg_map[agg] = r->failed ? 0
                                       : ir_array_add(program, t->name,
                                                      t->fields[0].type,
                                                      length, t->length_text);
    } else if (t->simd) {
        maps->agg_map[agg] = ir_simd_add(program, t->name, t->fields,
                                         t->field_count);
    } else {
        maps->agg_map[agg] = ir_struct_add(program, t->kind, t->name, t->fields,
                                           t->field_count, t->packed,
                                           t->align);
    }
    maps->agg_state[agg] = MAP_DONE;
    maps->agg_height[agg] = height + 1;
    if (height + 1 > TYPES_NEST_MAX) {
        antl_damaged(r);
    }
    return maps->agg_map[agg];
}

static uint32_t map_sym_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t sym, uint32_t depth)
{
    struct ir_sym s;
    uint32_t height = 0;

    if (sym >= maps->sym_count || maps->sym_state[sym] == MAP_BUSY ||
        depth >= TYPES_NEST_MAX) {
        antl_damaged(r);
        return 0;
    }
    if (maps->sym_state[sym] == MAP_DONE) {
        return maps->sym_map[sym];
    }
    maps->sym_state[sym] = MAP_BUSY;
    s = maps->syms[sym];
    if (s.kind == IR_SYM_SIZE_OF || s.kind == IR_SYM_OFFSET_OF) {
        if (s.of.type == IR_AGG) {
            s.of.agg = map_agg_at(r, program, maps, s.of.agg, depth + 1);
            if (!r->failed) {
                height = maps->agg_height[maps->syms[sym].of.agg];
            }
        }
        if (!r->failed && s.kind == IR_SYM_OFFSET_OF &&
            (s.of.type != IR_AGG ||
             s.field >= program->aggs[s.of.agg]->field_count)) {
            antl_damaged(r);
        }
    } else if (s.kind == IR_SYM_OP) {
        s.a = map_sym_at(r, program, maps, s.a, depth + 1);
        if (!r->failed) {
            height = maps->sym_height[maps->syms[sym].a];
        }
        if (s.b != IR_NO_AGG) {
            s.b = map_sym_at(r, program, maps, s.b, depth + 1);
            if (!r->failed) {
                height = nest_max(height, maps->sym_height[maps->syms[sym].b]);
            }
        }
    }
    if (r->failed) {
        return 0;
    }
    switch (s.kind) {
    case IR_SYM_INT:
        maps->sym_map[sym] = ir_sym_int(program, s.type, s.value);
        break;
    case IR_SYM_SIZE_OF:
        maps->sym_map[sym] = ir_sym_size_of(program, s.of);
        break;
    case IR_SYM_OFFSET_OF:
        maps->sym_map[sym] = ir_sym_offset_of(program, s.of.agg, s.field);
        break;
    case IR_SYM_OP:
        maps->sym_map[sym] = ir_sym_op(program, (enum ir_op)s.op, s.type, s.a,
                                       s.b);
        break;
    }
    maps->sym_state[sym] = MAP_DONE;
    maps->sym_height[sym] = height + 1;
    if (height + 1 > TYPES_NEST_MAX) {
        antl_damaged(r);
    }
    return maps->sym_map[sym];
}

static uint32_t map_agg(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps, uint32_t agg)
{
    return map_agg_at(r, program, maps, agg, 0);
}

static uint32_t map_sym(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps, uint32_t sym)
{
    return map_sym_at(r, program, maps, sym, 0);
}

static struct ir_vtype read_vtype(struct reader *r, bool scalar_only)
{
    struct ir_vtype v;
    uint8_t type = antl_get_u8(r);

    v.agg = antl_get_u32(r);
    v.type = (enum ir_type)type;
    if (!r->failed && (!valid_type(type) || (scalar_only && type == IR_AGG) ||
                       ((type == IR_AGG) != (v.agg != IR_NO_AGG)))) {
        antl_damaged(r);
    }
    return v;
}

/* Whether the program holds everything that aggregate agg of the file
   is built from. */
static bool agg_ready(const struct ir_maps *maps, uint32_t agg)
{
    const struct ir_aggtype *t = &maps->aggs[agg];
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        uint32_t of = t->fields[i].type.agg;
        if (t->fields[i].type.type == IR_AGG &&
            (of >= maps->agg_count || maps->agg_state[of] != MAP_DONE)) {
            return false;
        }
    }
    return t->kind != IR_AGG_ARRAY ||
           (t->length < maps->sym_count &&
            maps->sym_state[t->length] == MAP_DONE);
}

/* Whether the program holds everything that symbolic value sym of the
   file is computed from. */
static bool sym_ready(const struct ir_maps *maps, uint32_t sym)
{
    const struct ir_sym *s = &maps->syms[sym];

    if ((s->kind == IR_SYM_SIZE_OF || s->kind == IR_SYM_OFFSET_OF) &&
        s->of.type == IR_AGG) {
        return s->of.agg < maps->agg_count &&
               maps->agg_state[s->of.agg] == MAP_DONE;
    }
    if (s->kind == IR_SYM_OP) {
        return s->a < maps->sym_count && maps->sym_state[s->a] == MAP_DONE &&
               (s->b == IR_NO_AGG ||
                (s->b < maps->sym_count && maps->sym_state[s->b] == MAP_DONE));
    }
    return true;
}

/* A bitfield of the IR has an integer type of a fixed width and no more
   bits than it. */
static bool ir_bitfield_fits(const struct ir_field *f)
{
    switch (f->type.type) {
    case IR_I8: return f->bits <= 8;
    case IR_I16: return f->bits <= 16;
    case IR_I32: return f->bits <= 32;
    case IR_I64: return f->bits <= 64;
    default: return false;
    }
}

/* The aggregate and symbolic tables of the file. They refer to each other
   by index, so both are read before either is added to the program. */
static void read_tables(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps)
{
    uint32_t i;
    uint32_t j;
    uint32_t a = 0;

    maps->file_count = antl_get_count(r, 4);
    maps->files = antl_allocate(r, maps->file_count, sizeof *maps->files);
    for (i = 0; i < maps->file_count && !r->failed; i++) {
        const char *path = antl_get_cstr(r);
        if (!r->failed) {
            maps->files[i] = ir_file_add(program, path);
        }
    }
    maps->sym_count = antl_get_count(r, 27);
    maps->syms = antl_allocate(r, maps->sym_count, sizeof *maps->syms);
    maps->sym_map = antl_allocate(r, maps->sym_count, sizeof *maps->sym_map);
    maps->sym_state =
        antl_allocate(r, maps->sym_count, sizeof *maps->sym_state);
    maps->sym_height =
        antl_allocate(r, maps->sym_count, sizeof *maps->sym_height);
    for (i = 0; i < maps->sym_count && !r->failed; i++) {
        struct ir_sym *s = &maps->syms[i];
        uint8_t kind = antl_get_u8(r);
        uint8_t type = antl_get_u8(r);
        s->kind = (enum ir_sym_kind)kind;
        s->type = (enum ir_type)type;
        s->value = antl_get_u64(r);
        s->of = read_vtype(r, false);
        s->field = antl_get_u32(r);
        s->op = antl_get_u8(r);
        s->a = antl_get_u32(r);
        s->b = antl_get_u32(r);
        if (kind > IR_SYM_OP || type < IR_I8 ||
            (type > IR_I64 && type != IR_CLONG && type != IR_CWCHAR &&
             type != IR_LOCK) ||
            s->op > IR_RET ||
            ((kind == IR_SYM_SIZE_OF || kind == IR_SYM_OFFSET_OF) &&
             s->of.type == IR_VOID)) {
            antl_damaged(r);
        }
    }
    maps->agg_count = antl_get_count(r, 26);
    maps->aggs = antl_allocate(r, maps->agg_count, sizeof *maps->aggs);
    maps->agg_map = antl_allocate(r, maps->agg_count, sizeof *maps->agg_map);
    maps->agg_state =
        antl_allocate(r, maps->agg_count, sizeof *maps->agg_state);
    maps->agg_height =
        antl_allocate(r, maps->agg_count, sizeof *maps->agg_height);
    for (i = 0; i < maps->agg_count && !r->failed; i++) {
        struct ir_aggtype *t = &maps->aggs[i];
        uint8_t kind = antl_get_u8(r);
        uint8_t flags;
        t->kind = (enum ir_agg_kind)kind;
        t->name = antl_get_cstr(r);
        flags = antl_get_u8(r);
        t->packed = (flags & 1) != 0;
        t->simd = (flags & 2) != 0;
        t->align = antl_get_u64(r);
        if (flags > 3 || (t->align & (t->align - 1)) != 0) {
            antl_damaged(r);
        }
        t->length = antl_get_u32(r);
        t->length_text = antl_get_cstr(r);
        t->field_count = antl_get_count(r, 10);
        t->fields = antl_allocate(r, t->field_count, sizeof *t->fields);
        for (j = 0; j < t->field_count && !r->failed; j++) {
            uint8_t ext;
            t->fields[j].name = antl_get_cstr(r);
            t->fields[j].type = read_vtype(r, false);
            t->fields[j].bits = antl_get_u8(r);
            ext = antl_get_u8(r);
            t->fields[j].ext = (enum ir_ext)ext;
            if (ext > IR_EXT_ZERO || t->fields[j].type.type == IR_VOID ||
                (t->fields[j].bits != 0 && !ir_bitfield_fits(&t->fields[j]))) {
                antl_damaged(r);
            }
        }
        if (kind > IR_AGG_ARRAY || t->name[0] == '\0' ||
            t->field_count == 0 ||
            (kind == IR_AGG_ARRAY && t->field_count != 1) ||
            (!r->failed && t->simd && !antl_verify_simd_agg(t))) {
            antl_damaged(r);
        }
    }
    /* DESIGN: the file keeps the aggregates and the symbolic values each
       in the order the program made them, and the two orders interleave.
       The reader keeps both orders. It makes the next aggregate when the
       program holds what it is built from, and the next value otherwise.
       The order they were made in is one such interleaving, so one of
       the two is always ready. A library read and written again then
       comes out byte for byte. A file that fits no interleaving is
       mapped on demand. */
    i = 0;
    while (!r->failed && (a < maps->agg_count || i < maps->sym_count)) {
        if (a < maps->agg_count &&
            (maps->agg_state[a] == MAP_DONE || agg_ready(maps, a))) {
            map_agg(r, program, maps, a++);
        } else if (i < maps->sym_count &&
                   (maps->sym_state[i] == MAP_DONE || sym_ready(maps, i))) {
            map_sym(r, program, maps, i++);
        } else if (a < maps->agg_count) {
            map_agg(r, program, maps, a++);
        } else {
            map_sym(r, program, maps, i++);
        }
    }
}

static uint32_t read_agg_ref(struct reader *r, struct ir_module *program,
                             struct ir_maps *maps, uint8_t type)
{
    uint32_t agg = antl_get_u32(r);

    if (r->failed || (type == IR_AGG) != (agg != IR_NO_AGG)) {
        antl_damaged(r);
        return IR_NO_AGG;
    }
    return type == IR_AGG ? map_agg(r, program, maps, agg) : IR_NO_AGG;
}

/* Read a constant tree, mapping its aggregate indices. A global index
   stays as written, because a constant may name a global that comes
   later. remap_const moves them. */
static struct ir_const *read_const(struct reader *r, struct ir_module *program,
                                   struct ir_maps *maps, int depth)
{
    struct ir_const *c;
    uint8_t kind;
    uint8_t scalar;
    uint64_t payload;
    uint64_t i;

    if (depth > 32) {
        antl_damaged(r);
        return NULL;
    }
    kind = antl_get_u8(r);
    scalar = antl_get_u8(r);
    payload = antl_get_u64(r);
    if (r->failed || kind > IR_CONST_AGG || !valid_type(scalar)) {
        antl_damaged(r);
        return NULL;
    }
    c = arena_alloc(r->arena, sizeof *c);
    c->kind = (enum ir_const_kind)kind;
    c->scalar = (enum ir_type)scalar;
    switch (c->kind) {
    case IR_CONST_INT:
        c->integer = payload;
        break;
    case IR_CONST_FLOAT:
        memcpy(&c->floating, &payload, sizeof payload);
        break;
    case IR_CONST_SYM:
        if (payload >= maps->sym_count) {
            antl_damaged(r);
            return NULL;
        }
        c->sym = map_sym(r, program, maps, (uint32_t)payload);
        break;
    case IR_CONST_ADDR:
    case IR_CONST_FUNC:
        c->global = (uint32_t)payload;
        break;
    case IR_CONST_AGG:
        c->type = read_vtype(r, false);
        if (r->failed || c->type.type != IR_AGG) {
            antl_damaged(r);
            return NULL;
        }
        c->type.agg = map_agg(r, program, maps, c->type.agg);
        /* An item costs at least its kind, its type and its payload, so
           a count past that many bytes cannot be honest. */
        if (payload > (uint64_t)(r->size - r->pos) / 10) {
            antl_damaged(r);
            return NULL;
        }
        c->item_count = (size_t)payload;
        c->items = arena_alloc(r->arena,
                               (payload == 0 ? 1 : payload) * sizeof *c->items);
        for (i = 0; i < payload && !r->failed; i++) {
            struct ir_const *item = read_const(r, program, maps, depth + 1);
            if (item == NULL) {
                return NULL;
            }
            c->items[i] = *item;
        }
        break;
    default: /* IR_CONST_NONE */
        break;
    }
    if (!r->failed && !antl_verify_const(c)) {
        antl_damaged(r);
    }
    return r->failed ? NULL : c;
}

/* DESIGN: move the addresses inside a constant to the indices of the
   program. A table entry names a function, and the file lists the
   functions after the globals. The two kinds of address are therefore
   moved in two passes, and `functions` says which pass this is. */
static void remap_const(struct reader *r, struct ir_const *c,
                        const struct ir_maps *maps, bool functions)
{
    size_t i;

    if (c->kind == IR_CONST_ADDR && !functions) {
        if (c->global >= maps->global_count) {
            antl_damaged(r);
            return;
        }
        c->global = maps->globals[c->global];
    } else if (c->kind == IR_CONST_FUNC && functions) {
        if (c->global >= maps->function_count) {
            antl_damaged(r);
            return;
        }
        c->global = maps->functions[c->global];
    } else if (c->kind == IR_CONST_AGG) {
        for (i = 0; i < c->item_count; i++) {
            remap_const(r, &c->items[i], maps, functions);
        }
    }
}

/* The targets of a jump and of a branch are blocks. read_operand has
   checked the index of each block. */
static bool targets_blocks(const struct ir_inst *inst)
{
    if (inst->op == IR_JUMP) {
        return inst->a.kind == IR_BLOCK;
    }
    if (inst->op == IR_BRANCH || inst->op == IR_BRANCH_OV) {
        return inst->b.kind == IR_BLOCK && inst->c.kind == IR_BLOCK;
    }
    return true;
}

static struct ir_operand read_operand(struct reader *r,
                                      const struct ir_function *f,
                                      const struct ir_maps *maps)
{
    struct ir_operand o = {IR_NONE, IR_VOID, {0}};
    uint8_t kind = antl_get_u8(r);
    uint8_t type = antl_get_u8(r);
    uint64_t payload = antl_get_u64(r);

    if (r->failed) {
        return o;
    }
    if (kind > IR_SYM || !valid_type(type)) {
        antl_damaged(r);
        return o;
    }
    o.kind = (enum ir_operand_kind)kind;
    o.type = (enum ir_type)type;
    switch (o.kind) {
    case IR_TEMP:
        if (payload >= f->temp_count || f->temps[payload] != o.type) {
            antl_damaged(r);
        } else {
            o.as.temp = (uint32_t)payload;
        }
        break;
    case IR_INT:
        o.as.integer = payload;
        break;
    case IR_FLOAT:
        memcpy(&o.as.floating, &payload, sizeof payload);
        break;
    case IR_GLOBAL:
        if (payload >= maps->global_count) {
            antl_damaged(r);
        } else {
            o.as.index = maps->globals[payload];
        }
        break;
    case IR_FUNC:
        if (payload >= maps->function_count) {
            antl_damaged(r);
        } else {
            o.as.index = maps->functions[payload];
        }
        break;
    case IR_BLOCK:
        if (payload >= f->block_count) {
            antl_damaged(r);
        } else {
            o.as.index = (uint32_t)payload;
        }
        break;
    case IR_SYM:
        if (payload >= maps->sym_count ||
            maps->syms[payload].type != o.type) {
            antl_damaged(r);
        } else {
            o.as.index = maps->sym_map[payload];
        }
        break;
    case IR_NONE:
        break;
    }
    return o;
}

static void read_body(struct reader *r, struct ir_module *program,
                      struct ir_function *f, struct ir_maps *maps)
{
    uint32_t temps = antl_get_count(r, 1);
    uint32_t blocks;
    uint32_t i;
    uint32_t j;
    uint32_t k;

    for (i = 0; i < temps && !r->failed; i++) {
        uint8_t type = antl_get_u8(r);
        if (i < f->param_count) {
            if (type != f->temps[i]) {
                antl_damaged(r);
            }
        } else if (!valid_type(type) || type == IR_VOID || type == IR_AGG) {
            antl_damaged(r);
        } else {
            ir_temp(f, (enum ir_type)type);
        }
    }
    if (temps < f->param_count) {
        antl_damaged(r);
    }
    blocks = antl_get_count(r, 4);
    /* A body starts at block 0, so it has one. */
    if (blocks == 0) {
        antl_damaged(r);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        ir_block_add(f);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        uint32_t count;
        uint8_t fail_kind;
        fail_kind = antl_get_u8(r);
        if (fail_kind > IR_FAIL_CHECK) {
            antl_damaged(r);
        }
        f->blocks[i]->fail = (enum ir_fail)fail_kind;
        count = antl_get_count(r, 50);
        for (j = 0; j < count && !r->failed; j++) {
            struct ir_inst inst;
            struct ir_operand *args;
            uint8_t op = antl_get_u8(r);
            uint8_t type = antl_get_u8(r);
            memset(&inst, 0, sizeof inst);
            inst.line = antl_get_u32(r);
            inst.result = antl_get_u32(r);
            inst.op = (enum ir_op)op;
            inst.type = (enum ir_type)type;
            if (op > IR_RET || !valid_type(type) ||
                (inst.result != IR_NO_RESULT && inst.result >= f->temp_count)) {
                antl_damaged(r);
                break;
            }
            inst.a = read_operand(r, f, maps);
            inst.b = read_operand(r, f, maps);
            inst.c = read_operand(r, f, maps);
            if (!targets_blocks(&inst)) {
                antl_damaged(r);
                break;
            }
            inst.of = read_vtype(r, false);
            inst.field = antl_get_u32(r);
            if (!r->failed && inst.of.type == IR_AGG) {
                inst.of.agg = map_agg(r, program, maps, inst.of.agg);
            }
            if (!r->failed &&
                ((op == IR_SLOT || op == IR_MEMCOPY || op == IR_BITLOAD ||
                  op == IR_BITSTORE ||
                  (op >= IR_VBINARY && op <= IR_VREDUCE)) ==
                     (inst.of.type == IR_VOID) ||
                 ((op == IR_BITLOAD || op == IR_BITSTORE) &&
                  (inst.of.type != IR_AGG ||
                   inst.field >= program->aggs[inst.of.agg]->field_count ||
                   program->aggs[inst.of.agg]->fields[inst.field].bits == 0)))) {
                antl_damaged(r);
            }
            inst.arg_count = antl_get_count(r, 10);
            args = antl_allocate(r, inst.arg_count, sizeof *args);
            for (k = 0; k < inst.arg_count && !r->failed; k++) {
                args[k] = read_operand(r, f, maps);
            }
            inst.args = args;
            if (!r->failed) {
                ir_inst_add(f->blocks[i], &inst);
            }
        }
    }
}

/* A function signature, mapped to a function of the program. A C function
   and a declaration share an existing entry of the same name. */
static uint32_t read_signature(struct reader *r, struct ir_module *program,
                               struct ir_maps *maps, bool *has_body,
                               bool *skip)
{
    uint8_t flags = antl_get_u8(r);
    const char *module = antl_get_cstr(r);
    const char *name = antl_get_cstr(r);
    uint8_t result = antl_get_u8(r);
    uint32_t result_agg = read_agg_ref(r, program, maps, result);
    uint32_t file = antl_get_u32(r);
    uint32_t decl_line = antl_get_u32(r);
    uint32_t param_count = antl_get_count(r, 6);
    struct ir_function *f = NULL;
    size_t i;

    *has_body = false;
    *skip = false;
    if (r->failed) {
        return 0;
    }
    if (!valid_type(result) || flags > 15 ||
        ((flags & 1) == 0 && module[0] == '\0') ||
        (module[0] != '\0' && (flags & 2) != 0) ||
        (module[0] == '\0' && (flags & 4) != 0)) {
        antl_damaged(r);
        return 0;
    }
    if (module[0] == '\0') {
        module = NULL;
    }
    for (i = 0; i < program->function_count; i++) {
        struct ir_function *g = program->functions[i];
        bool same_module = module == NULL
                               ? g->module == NULL
                               : g->module != NULL &&
                                     strcmp(g->module, module) == 0;
        if (same_module && strcmp(g->name, name) == 0) {
            f = g;
        }
    }
    /* DESIGN: each module that uses a copy of a generic defines it, so
       two library files may both define one. The program keeps the
       first, and the second stands for it. The copies are made from one
       tree with the same arguments, so they are the same code. */
    if (f != NULL && (flags & 1) == 0 && module != NULL &&
        ir_is_copy_name(name)) {
        if (f->is_extern) {
            f->is_extern = false;
            *has_body = true;
        } else {
            *skip = true;
        }
    } else if (f != NULL && (flags & 1) == 0) {
        antl_fail(r, "defines `%s.%s`, which another library defines",
                  module, name);
        return 0;
    }
    if (file != IR_NO_INDEX && file >= maps->file_count) {
        antl_damaged(r);
        return 0;
    }
    if (f == NULL) {
        if ((flags & 1) == 0) {
            f = ir_function_add(program, module, name, (enum ir_type)result,
                                result_agg);
            *has_body = true;
            /* A copy of a generic of another module belongs to the object
               of the module whose file defines it. */
            if (module != NULL && strcmp(module, r->iface->module) != 0) {
                f->unit = r->iface->module;
            }
        } else if (module == NULL) {
            f = ir_extern_add(program, name, (enum ir_type)result, flags & 2);
            f->result_agg = result_agg;
        } else {
            f = ir_declare_add(program, module, name, (enum ir_type)result,
                               result_agg);
        }
        f->exported = (flags & 4) != 0;
        f->worker = (flags & 8) != 0;
        f->file = file == IR_NO_INDEX ? IR_NO_INDEX : maps->files[file];
        f->decl_line = decl_line;
        for (i = 0; i < param_count && !r->failed; i++) {
            uint8_t type = antl_get_u8(r);
            uint8_t ext = antl_get_u8(r);
            uint32_t agg = read_agg_ref(r, program, maps, type);
            bool narrow = type == IR_I8 || type == IR_I16;
            if (!valid_type(type) || type == IR_VOID || ext > IR_EXT_ZERO ||
                (ext != IR_EXT_NONE) != narrow) {
                antl_damaged(r);
            } else {
                ir_param_add(f, (enum ir_type)type, agg);
                f->params[f->param_count - 1].ext = (enum ir_ext)ext;
            }
        }
    } else {
        for (i = 0; i < param_count && !r->failed; i++) {
            uint8_t type = antl_get_u8(r);
            antl_get_u8(r);
            read_agg_ref(r, program, maps, type);
        }
    }
    return f->index;
}

/* A global of the file as the program's index. none allows
   IR_NO_INDEX. */
static uint32_t map_global(struct reader *r, const struct ir_maps *maps,
                           uint32_t g, bool none)
{
    if (none && g == IR_NO_INDEX) {
        return g;
    }
    if (g >= maps->global_count) {
        antl_damaged(r);
        return 0;
    }
    return maps->globals[g];
}

/* Whether the program holds a record of the class of c before c. */
static bool has_class(const struct ir_module *program, const struct ir_class *c)
{
    size_t i;

    for (i = 0; i + 1 < program->class_count; i++) {
        if (strcmp(program->classes[i]->module, c->module) == 0 &&
            strcmp(program->classes[i]->name, c->name) == 0) {
            return true;
        }
    }
    return false;
}

/* The class records of the file. Each names globals, a function and an
   aggregate of the file, which move to the program's indices. */
static void read_classes(struct reader *r, struct ir_module *program,
                         struct ir_maps *maps)
{
    uint32_t count = antl_get_count(r, 37);
    uint32_t i;
    uint32_t j;

    for (i = 0; i < count && !r->failed; i++) {
        const char *module = antl_get_cstr(r);
        const char *name = antl_get_cstr(r);
        uint8_t flags = antl_get_u8(r);
        uint32_t descriptor = antl_get_u32(r);
        uint32_t base = antl_get_u32(r);
        uint32_t table = antl_get_u32(r);
        uint32_t init = antl_get_u32(r);
        uint32_t agg = antl_get_u32(r);
        uint32_t subtables;
        uint32_t mutables;
        uint32_t injects;
        uint32_t provides;
        struct ir_class *c;

        if (r->failed ||
            flags > (IR_CLASS_ABSTRACT | IR_CLASS_FINAL | IR_CLASS_SINGLETON |
                     IR_CLASS_ARGS | IR_CLASS_REQUIRED) ||
            module[0] == '\0' ||
            (init != IR_NO_INDEX && init >= maps->function_count)) {
            antl_damaged(r);
            return;
        }
        c = ir_class_add(program, module, name);
        c->flags = flags;
        c->descriptor = map_global(r, maps, descriptor, false);
        c->base = map_global(r, maps, base, false);
        c->table = map_global(r, maps, table, true);
        c->init = init == IR_NO_INDEX ? init : maps->functions[init];
        c->agg = map_agg(r, program, maps, agg);
        subtables = antl_get_count(r, 16);
        for (j = 0; j < subtables && !r->failed; j++) {
            uint32_t interface = antl_get_u32(r);
            uint32_t at = antl_get_u32(r);
            uint32_t sub_agg = antl_get_u32(r);
            uint32_t sub_field = antl_get_u32(r);
            uint32_t mapped = map_agg(r, program, maps, sub_agg);
            uint32_t mapped_interface;
            uint32_t mapped_at;
            if (r->failed || sub_field >= program->aggs[mapped]->field_count) {
                antl_damaged(r);
                return;
            }
            /* Each call may mark the file damaged, so each has a line
               of its own. */
            mapped_interface = map_global(r, maps, interface, false);
            mapped_at = map_global(r, maps, at, false);
            ir_class_subtable(c, mapped_interface, mapped_at, mapped,
                              sub_field);
        }
        mutables = antl_get_count(r, 4);
        for (j = 0; j < mutables && !r->failed; j++) {
            uint32_t field = antl_get_u32(r);
            if (r->failed || field >= program->aggs[c->agg]->field_count) {
                antl_damaged(r);
            }
            ir_class_mutable(c, field);
        }
        injects = antl_get_count(r, 13);
        for (j = 0; j < injects && !r->failed; j++) {
            const char *path = antl_get_cstr(r);
            const char *named = antl_get_cstr(r);
            uint32_t of = antl_get_u32(r);
            uint8_t last = antl_get_u8(r);
            if (r->failed || path[0] == '\0' || last > 1) {
                antl_damaged(r);
                return;
            }
            ir_class_inject(program, c, path, named,
                            map_global(r, maps, of, false), last != 0);
        }
        provides = antl_get_count(r, 8);
        for (j = 0; j < provides && !r->failed; j++) {
            const char *path = antl_get_cstr(r);
            uint32_t of = antl_get_u32(r);
            if (r->failed || path[0] == '\0') {
                antl_damaged(r);
                return;
            }
            ir_class_provides(program, c, path, map_global(r, maps, of,
                                                           false));
        }
        /* The record of a copy of a generic that the program has
           already stays behind. */
        if (ir_is_copy_name(name) && has_class(program, c)) {
            ir_class_free(c);
            program->class_count--;
        }
    }
}

/* The body of a copy the program has already: read to move past it,
   and dropped. */
static void skip_body(struct reader *r, struct ir_module *program,
                      const struct ir_function *f, struct ir_maps *maps)
{
    struct ir_function scratch;

    memset(&scratch, 0, sizeof scratch);
    scratch.params = f->params;
    scratch.param_count = f->param_count;
    scratch.temps = alloc_zeroed(f->param_count + 1, sizeof *scratch.temps);
    memcpy(scratch.temps, f->temps, f->param_count * sizeof *scratch.temps);
    scratch.temp_count = (uint32_t)f->param_count;
    scratch.temp_capacity = f->param_count + 1;
    read_body(r, program, &scratch, maps);
    ir_function_free_body(&scratch);
}

/* A datum of a copy of a generic that the program has already, as g,
   the global read last. The program keeps the first, and *index
   receives it. Returns whether g is such a twin, which the program then
   drops. */
static bool merge_copy_global(struct ir_module *program, struct ir_global *g,
                              uint32_t *index)
{
    size_t i;

    for (i = 0; i + 1 < program->global_count; i++) {
        struct ir_global *old = program->globals[i];
        if (old->module == NULL || strcmp(old->module, g->module) != 0 ||
            strcmp(old->name, g->name) != 0) {
            continue;
        }
        if (old->is_extern) {
            /* A declaration takes the definition. */
            old->bytes = g->bytes;
            old->size = g->size;
            old->align = g->align;
            old->value = g->value;
            old->exported = g->exported;
            old->mutable = g->mutable;
            old->is_extern = false;
            program->global_count--;
            *index = old->index;
            return false;
        }
        program->global_count--;
        *index = old->index;
        return true;
    }
    return false;
}

struct relocs {
    uint32_t count;
    uint64_t *offsets;
    uint32_t *targets;
    uint8_t *functions;             /* the target is a function */
};

static int compare_offsets(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;

    return x < y ? -1 : x > y;
}

/* Whether the count addresses at offsets lie at least eight bytes apart,
   so that no two share a byte of the data. Each takes the eight bytes at
   its offset. */
static bool addresses_apart(const uint64_t *offsets, uint32_t count)
{
    uint64_t *sorted;
    uint32_t i;
    bool apart = true;

    if (count < 2) {
        return true;
    }
    sorted = alloc_zeroed(count, sizeof *sorted);
    memcpy(sorted, offsets, count * sizeof *sorted);
    qsort(sorted, count, sizeof *sorted, compare_offsets);
    for (i = 1; i < count && apart; i++) {
        apart = sorted[i] - sorted[i - 1] >= 8;
    }
    free(sorted);
    return apart;
}

void antl_read_ir(struct reader *r, struct ir_module *program)
{
    struct ir_maps maps;
    struct relocs *relocs;
    struct ir_function **bodies;
    bool *skips;
    bool *twins;
    uint32_t i;
    uint32_t j;

    memset(&maps, 0, sizeof maps);
    read_tables(r, program, &maps);
    maps.global_count = antl_get_count(r, 28);
    maps.globals = antl_allocate(r, maps.global_count, sizeof *maps.globals);
    relocs = antl_allocate(r, maps.global_count, sizeof *relocs);
    twins = antl_allocate(r, maps.global_count, sizeof *twins);
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        const char *module = antl_get_cstr(r);
        const char *name = antl_get_cstr(r);
        uint64_t size = antl_get_u64(r);
        uint64_t align = antl_get_u64(r);
        struct ir_global *g;
        /* A global of the runtime has no module, and an empty name is
           how the file spells that. */
        if (module != NULL && module[0] == '\0') {
            module = NULL;
        }
        /* The back end writes the alignment as a power of two. */
        if ((align & (align - 1)) != 0) {
            antl_damaged(r);
        }
        if (!antl_take(r, size)) {
            break;
        }
        g = ir_global_add(program, module, name, r->data + r->pos, size, align);
        r->pos += size;
        maps.globals[i] = g->index;
        relocs[i].count = antl_get_count(r, 13);
        relocs[i].offsets = antl_allocate(r, relocs[i].count, sizeof(uint64_t));
        relocs[i].targets = antl_allocate(r, relocs[i].count, sizeof(uint32_t));
        relocs[i].functions =
            antl_allocate(r, relocs[i].count, sizeof(uint8_t));
        for (j = 0; j < relocs[i].count && !r->failed; j++) {
            relocs[i].offsets[j] = antl_get_u64(r);
            relocs[i].targets[j] = antl_get_u32(r);
            relocs[i].functions[j] = antl_get_u8(r) != 0 ? 1 : 0;
            if (relocs[i].offsets[j] > size ||
                size - relocs[i].offsets[j] < 8) {
                antl_damaged(r);
            }
        }
        if (!r->failed &&
            !addresses_apart(relocs[i].offsets, relocs[i].count)) {
            antl_damaged(r);
        }
        {
            uint8_t marks = antl_get_u8(r);
            g->exported = (marks & 2) != 0;
            g->is_extern = (marks & 4) != 0;
            g->mutable = (marks & 8) != 0;
            if ((marks & 1) != 0 && !r->failed) {
                g->value = read_const(r, program, &maps, 0);
            }
        }
        if (!g->is_extern && module != NULL && !r->failed) {
            if (ir_is_copy_name(name)) {
                twins[i] = merge_copy_global(program, g, &maps.globals[i]);
                g = program->globals[maps.globals[i]];
            }
            if (!twins[i] && strcmp(module, r->iface->module) != 0) {
                g->unit = r->iface->module;
            }
        }
    }
    /* A pointer may name a global that the file lists later. */
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        struct ir_global *g = program->globals[maps.globals[i]];
        if (twins[i]) {
            continue;
        }
        if (g->value != NULL) {
            remap_const(r, g->value, &maps, false);
        }
        for (j = 0; j < relocs[i].count; j++) {
            if (relocs[i].functions[j]) {
                continue;
            }
            if (relocs[i].targets[j] >= maps.global_count) {
                antl_damaged(r);
                break;
            }
            ir_global_reloc(program, g, relocs[i].offsets[j],
                            maps.globals[relocs[i].targets[j]]);
        }
    }
    maps.function_count = antl_get_count(r, 15);
    maps.functions =
        antl_allocate(r, maps.function_count, sizeof *maps.functions);
    bodies = antl_allocate(r, maps.function_count, sizeof *bodies);
    skips = antl_allocate(r, maps.function_count, sizeof *skips);
    for (i = 0; i < maps.function_count && !r->failed; i++) {
        bool has_body;
        maps.functions[i] = read_signature(r, program, &maps, &has_body,
                                           &skips[i]);
        bodies[i] = has_body ? program->functions[maps.functions[i]] : NULL;
    }
    for (i = 0; i < maps.function_count && !r->failed; i++) {
        if (bodies[i] != NULL) {
            read_body(r, program, bodies[i], &maps);
        } else if (skips[i]) {
            skip_body(r, program, program->functions[maps.functions[i]],
                      &maps);
        }
    }
    /* A table entry names a function, and the file lists the functions
       after the globals, so those addresses wait until here. */
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        struct ir_global *g = program->globals[maps.globals[i]];
        if (twins[i]) {
            continue;
        }
        if (g->value != NULL) {
            remap_const(r, g->value, &maps, true);
        }
        for (j = 0; j < relocs[i].count; j++) {
            if (!relocs[i].functions[j]) {
                continue;
            }
            if (relocs[i].targets[j] >= maps.function_count) {
                antl_damaged(r);
                break;
            }
            ir_global_reloc_fn(program, g, relocs[i].offsets[j],
                               maps.functions[relocs[i].targets[j]]);
        }
    }
    if (!r->failed) {
        read_classes(r, program, &maps);
    }
}
