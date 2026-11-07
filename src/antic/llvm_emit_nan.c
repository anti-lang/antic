/* One NaN on every target in the LLVM text: the slots no view reaches,
   the floats that need no canonical NaN, the functions code outside Anti
   calls, and the select of the canonical NaN.

   DESIGN: Q1 of "Answers of 2026-11-06, fourth set" in docs/decisions.md,
   form B of "One NaN on every target" in docs/notes/math.md. A program
   sees the bits of a float only where they leave the registers: memory
   that a view or C can reach, a union, a float that C takes or that a
   function C calls gives back, the hash, the sign of copysign and the
   sixteen bits of an f16. The text writes the canonical NaN, 0x7fc00000
   and 0x7ff8000000000000, at each of them, as a select after `fcmp uno`.
   A NaN in a register keeps the bits of the processor, and no operation
   of the language can tell.

   A store into a slot that no view reaches needs none. Such a slot is one
   whose address serves loads and stores alone, directly or through an
   offset, and where no two accesses overlap but those of one type at one
   offset. A union, two types that overlap, an address that goes anywhere
   else and an offset the text does not know into a slot whose accesses
   differ in type make the slot one a view reaches. opt turns the slots of locals into registers, so a local
   of a float costs nothing.

   A float that a load gave from memory a view reaches is bits that stood
   in memory already, and a float of an integer is no NaN. Neither needs
   the select. Every other float does: the result of an operation, of a
   conversion, of a call and of a load from a slot no view reaches. */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "llvm_emit_parts.h"

/* The canonical NaN in the text of a float constant of LLVM, which writes
   both widths as the bits of a double. */
#define NAN_CANONICAL "0x7FF8000000000000"

/* The prefix of the functions of the runtime, which hand no float to C
   that a program sees. anti_rt_copysign takes the canonical NaN itself. */
#define NAN_RUNTIME_PREFIX "anti_rt_"

#define NO_SLOT UINT32_MAX

/* One load or store through an address inside a slot: the slot, the
   offset of the place in bytes, its size and the type. dynamic marks an
   offset the text does not know. */
struct access {
    uint32_t slot;
    uint64_t offset;
    uint64_t size;
    bool dynamic;
    enum ir_type type;
};

static bool is_float(enum ir_type type)
{
    return type == IR_F32 || type == IR_F64;
}

/* Whether the aggregate agg is a union or holds one, at any depth. */
static bool holds_union(const struct ir_module *m, uint32_t agg)
{
    const struct ir_aggtype *t = m->aggs[agg];
    size_t i;

    if (t->kind == IR_AGG_UNION) {
        return true;
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].type.type == IR_AGG &&
            holds_union(m, t->fields[i].type.agg)) {
            return true;
        }
    }
    return false;
}

/* The operand at position k of inst, a, b, c and then the arguments, or
   NULL past the last. */
static const struct ir_operand *operand_at(const struct ir_inst *inst,
                                           size_t k)
{
    if (k == 0) {
        return &inst->a;
    }
    if (k == 1) {
        return &inst->b;
    }
    if (k == 2) {
        return &inst->c;
    }
    return k - 3 < inst->arg_count ? &inst->args[k - 3] : NULL;
}

static bool is_temp(const struct ir_operand *o, uint32_t count)
{
    return o->kind == IR_TEMP && o->as.temp < count;
}

/* Whether the use of an address inside a slot at position k of inst lets
   no view reach the slot. A derivation whose result has more than one
   definition does not, since another definition may hold another
   address. */
static bool use_keeps(const struct ir_inst *inst, size_t k,
                      const uint32_t *defs)
{
    switch (inst->op) {
    case IR_LOAD:
    case IR_LIFE_START:
    case IR_LIFE_END:
        return k == 0;
    case IR_STORE:
        return k == 1;
    case IR_MEMCOPY:
        /* A copy into the slot takes bits from memory, and a copy out of
           it hands its bits on. */
        return k == 0;
    case IR_PTRADD:
    case IR_COPY:
        return k == 0 && inst->result != IR_NO_RESULT && defs[inst->result] == 1;
    default:
        return false;
    }
}

/* The slots of f that a view reaches, and the slot and the place each
   temporary addresses. */
struct slots {
    uint32_t count;
    uint32_t *defs;
    const struct ir_inst **def;
    uint32_t *slot;
    uint64_t *offset;
    bool *dynamic;
    bool *viewed;
    struct access *accesses;
    size_t access_count;
    size_t access_capacity;
};

static void record(struct emitter *e, struct slots *s, uint32_t temp,
                   enum ir_type type)
{
    struct access *a;

    s->accesses = alloc_grow(s->accesses, &s->access_capacity,
                             s->access_count, sizeof *s->accesses);
    a = &s->accesses[s->access_count++];
    a->slot = s->slot[temp];
    a->offset = s->offset[temp];
    a->size = layout_size(e->l, ir_scalar(type));
    a->dynamic = s->dynamic[temp];
    a->type = type;
}

/* Count the definitions of every temporary and find the slots. */
static void find_slots(struct slots *s, const struct ir_function *f)
{
    size_t b;
    size_t i;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b]->count; i++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[i];
            if (inst->result != IR_NO_RESULT && inst->result < s->count) {
                s->defs[inst->result]++;
                s->def[inst->result] = inst;
            }
        }
    }
    for (b = 0; b < s->count; b++) {
        const struct ir_inst *inst = s->def[b];
        s->slot[b] = NO_SLOT;
        if (inst != NULL && inst->op == IR_SLOT && s->defs[b] == 1) {
            s->slot[b] = (uint32_t)b;
            s->offset[b] = 0;
        }
    }
}

/* Follow every offset and copy of an address inside a slot to the end.
   A constant offset adds to the place of its base, and an offset the
   layout cannot fold names a place the text does not know. */
static void follow_addresses(struct emitter *e, struct slots *s)
{
    bool changed = true;
    uint32_t t;

    while (changed) {
        changed = false;
        for (t = 0; t < s->count; t++) {
            const struct ir_inst *inst = s->def[t];
            uint32_t base;
            if (inst == NULL || s->defs[t] != 1 || s->slot[t] != NO_SLOT ||
                (inst->op != IR_PTRADD && inst->op != IR_COPY) ||
                !is_temp(&inst->a, s->count) ||
                s->slot[inst->a.as.temp] == NO_SLOT) {
                continue;
            }
            base = inst->a.as.temp;
            s->slot[t] = s->slot[base];
            s->offset[t] = s->offset[base];
            s->dynamic[t] = s->dynamic[base];
            if (inst->op == IR_PTRADD) {
                uint64_t add = 0;
                if (inst->b.kind == IR_INT) {
                    add = inst->b.as.integer;
                } else if (inst->b.kind != IR_SYM ||
                           !layout_fold(e->l, inst->b.as.index, &add)) {
                    s->dynamic[t] = true;
                }
                s->offset[t] += add;
            }
            changed = true;
        }
    }
}

/* Mark every slot whose address reaches a use other than a load, a store
   or an offset, and record every access. */
static void find_uses(struct emitter *e, struct slots *s,
                      const struct ir_function *f)
{
    size_t b;
    size_t i;
    size_t k;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b]->count; i++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[i];
            const struct ir_operand *o;
            for (k = 0; (o = operand_at(inst, k)) != NULL; k++) {
                uint32_t t;
                if (!is_temp(o, s->count) || s->slot[o->as.temp] == NO_SLOT) {
                    continue;
                }
                t = o->as.temp;
                if (!use_keeps(inst, k, s->defs)) {
                    s->viewed[s->slot[t]] = true;
                } else if (inst->op == IR_LOAD || inst->op == IR_STORE) {
                    record(e, s, t, inst->type);
                }
            }
        }
    }
}

/* The order of the accesses: by slot, then the ones of a known place by
   offset and type, then those of a place the text does not know. */
static int compare_access(const void *x, const void *y)
{
    const struct access *a = x;
    const struct access *b = y;

    if (a->slot != b->slot) {
        return a->slot < b->slot ? -1 : 1;
    }
    if (a->dynamic != b->dynamic) {
        return a->dynamic ? 1 : -1;
    }
    if (a->offset != b->offset) {
        return a->offset < b->offset ? -1 : 1;
    }
    if (a->type != b->type) {
        return a->type < b->type ? -1 : 1;
    }
    return 0;
}

/* Mark every slot of a union and every slot with two types at one place,
   or with two types and a place the text does not know. */
static void find_views(struct slots *s, const struct ir_module *m)
{
    size_t i;
    size_t j;
    uint32_t t;

    for (t = 0; t < s->count; t++) {
        const struct ir_inst *inst = s->def[t];
        if (s->slot[t] == t && inst->of.type == IR_AGG &&
            holds_union(m, inst->of.agg)) {
            s->viewed[t] = true;
        }
    }
    /* Sorted by slot and offset, an access that overlaps one of another
       type or another offset follows it. A slot ends its run before the
       next starts. */
    qsort(s->accesses, s->access_count, sizeof *s->accesses, compare_access);
    for (i = 0; i < s->access_count; i = j) {
        const struct access *place = &s->accesses[i];
        uint64_t end = place->offset + place->size;
        bool dynamic = false;
        bool mixed = false;
        for (j = i; j < s->access_count && s->accesses[j].slot == place->slot;
             j++) {
            const struct access *a = &s->accesses[j];
            dynamic = dynamic || a->dynamic;
            mixed = mixed || a->type != s->accesses[i].type;
            if (a->dynamic) {
                continue;
            }
            if (a->offset == place->offset && a->type == place->type) {
                continue;
            }
            if (a->offset < end) {
                s->viewed[a->slot] = true;
            }
            place = a;
            end = a->offset + a->size > end ? a->offset + a->size : end;
        }
        if (dynamic && mixed) {
            s->viewed[s->accesses[i].slot] = true;
        }
    }
}

/* Mark the temporaries whose float needs no canonical NaN. */
static void find_known(struct emitter *e, const struct slots *s)
{
    bool changed = true;
    uint32_t t;

    for (t = 0; t < s->count; t++) {
        const struct ir_inst *inst = s->def[t];
        if (inst == NULL || s->defs[t] != 1 || !is_float(inst->type)) {
            continue;
        }
        if (inst->op == IR_LOAD) {
            e->nan_known[t] = !is_temp(&inst->a, s->count) ||
                              !e->nan_private[inst->a.as.temp];
        } else if (inst->op == IR_SITOF || inst->op == IR_UITOF ||
                   inst->op == IR_HEXT) {
            /* An integer gives no NaN, and an f16 is bits from memory. */
            e->nan_known[t] = true;
        }
    }
    while (changed) {
        changed = false;
        for (t = 0; t < s->count; t++) {
            const struct ir_inst *inst = s->def[t];
            if (e->nan_known[t] || inst == NULL || s->defs[t] != 1 ||
                inst->op != IR_COPY || !is_float(inst->type)) {
                continue;
            }
            if ((is_temp(&inst->a, s->count) &&
                 e->nan_known[inst->a.as.temp]) ||
                inst->a.kind == IR_FLOAT || inst->a.kind == IR_INT) {
                e->nan_known[t] = true;
                changed = true;
            }
        }
    }
}

void llvm_emit_nan_function(struct emitter *e, const struct ir_function *f)
{
    struct slots s;
    uint32_t t;

    memset(&s, 0, sizeof s);
    s.count = f->temp_count;
    s.defs = alloc_zeroed(s.count + 1, sizeof *s.defs);
    s.def = alloc_zeroed(s.count + 1, sizeof *s.def);
    s.slot = alloc_zeroed(s.count + 1, sizeof *s.slot);
    s.offset = alloc_zeroed(s.count + 1, sizeof *s.offset);
    s.dynamic = alloc_zeroed(s.count + 1, sizeof *s.dynamic);
    s.viewed = alloc_zeroed(s.count + 1, sizeof *s.viewed);
    free(e->nan_private);
    free(e->nan_known);
    e->nan_private = alloc_zeroed(s.count + 1, sizeof *e->nan_private);
    e->nan_known = alloc_zeroed(s.count + 1, sizeof *e->nan_known);
    find_slots(&s, f);
    follow_addresses(e, &s);
    find_uses(e, &s, f);
    find_views(&s, e->m);
    for (t = 0; t < s.count; t++) {
        e->nan_private[t] = s.slot[t] != NO_SLOT && !s.viewed[s.slot[t]];
    }
    find_known(e, &s);
    free(s.defs);
    free(s.def);
    free(s.slot);
    free(s.offset);
    free(s.dynamic);
    free(s.viewed);
    free(s.accesses);
}

void llvm_emit_nan_module(struct emitter *e)
{
    const struct ir_module *m = e->m;
    size_t f;
    size_t b;
    size_t i;
    size_t k;

    e->nan_reached = alloc_zeroed(m->function_count + 1,
                                  sizeof *e->nan_reached);
    for (f = 0; f < m->function_count; f++) {
        const struct ir_function *fn = m->functions[f];
        if (fn->exported || fn->by_name) {
            e->nan_reached[f] = true;
        }
        for (b = 0; b < fn->block_count; b++) {
            for (i = 0; i < fn->blocks[b]->count; i++) {
                const struct ir_inst *inst = &fn->blocks[b]->insts[i];
                const struct ir_operand *o;
                for (k = 0; (o = operand_at(inst, k)) != NULL; k++) {
                    /* The callee and the signature of a call are no
                       address that leaves. */
                    if (inst->op == IR_CALL && k < 2) {
                        continue;
                    }
                    /* The thunk of a callback of C goes to Anti alone,
                       see lower_c_callback. */
                    if (o->kind == IR_FUNC &&
                        o->as.index < m->function_count &&
                        !m->functions[o->as.index]->result_of_c) {
                        e->nan_reached[o->as.index] = true;
                    }
                }
            }
        }
    }
}

void llvm_emit_nan_free(struct emitter *e)
{
    free(e->nan_private);
    free(e->nan_known);
    free(e->nan_reached);
    e->nan_private = NULL;
    e->nan_known = NULL;
    e->nan_reached = NULL;
}

bool llvm_emit_nan_value(const struct emitter *e, const struct ir_operand *o,
                         enum ir_type type)
{
    if (!is_float(type) || o->kind != IR_TEMP) {
        /* A float constant is written canonical, and an integer operand
           of a float type is bits the program wrote. */
        return false;
    }
    return o->as.temp >= e->f->temp_count || !e->nan_known[o->as.temp];
}

bool llvm_emit_nan_store(const struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_operand *b = &inst->b;

    if (b->kind == IR_TEMP && b->as.temp < e->f->temp_count &&
        e->nan_private[b->as.temp]) {
        return false;
    }
    return llvm_emit_nan_value(e, &inst->a, inst->type);
}

bool llvm_emit_nan_call(const struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_function *callee;

    if (inst->a.kind != IR_FUNC) {
        /* A call through an address may reach C, apart from a call
           through the table of a class. A call through a function with
           its context reaches C only where the context is null, which
           llvm_emit_nan_context gives. */
        return inst->c.kind == IR_NONE;
    }
    callee = e->m->functions[inst->a.as.index];
    return callee->module == NULL &&
           strncmp(callee->name, NAN_RUNTIME_PREFIX,
                   strlen(NAN_RUNTIME_PREFIX)) != 0;
}

/* DESIGN: Q1 of "Answers of 2026-11-07, second set" in docs/decisions.md.
   A call through a function with its context tests the context word. A
   context that is not null comes from a closure, which C never makes, so
   that path hands its floats on as they are. A null context may come with
   a function of C, such as `apply(fabsf, x)`, and that path takes the
   canonical NaN. llvm_emit_call writes the one comparison and the selects
   in the branch of the null context. */
bool llvm_emit_nan_context(const struct ir_inst *inst)
{
    return inst->a.kind != IR_FUNC && inst->c.kind == IR_NONE &&
           inst->field == IR_CALL_CONTEXT && inst->arg_count > 0;
}

bool llvm_emit_nan_return(const struct emitter *e, const struct ir_inst *inst)
{
    return e->nan_reached[e->f->index] &&
           llvm_emit_nan_value(e, &inst->a, inst->type);
}

void llvm_emit_canonical(struct emitter *e, enum ir_type type,
                         struct text *value)
{
    const char *name = llvm_emit_value_type(e, type);
    uint32_t unordered = llvm_emit_fresh(e);
    uint32_t chosen = llvm_emit_fresh(e);

    text_appendf(e->out, "  %%v%" PRIu32 " = fcmp uno %s %s, 0.0\n",
                 unordered, name, text_cstr(value));
    text_appendf(e->out, "  %%v%" PRIu32 " = select i1 %%v%" PRIu32 ", %s "
                         NAN_CANONICAL ", %s %s\n",
                 chosen, unordered, name, name, text_cstr(value));
    value->length = 0;
    text_appendf(value, "%%v%" PRIu32, chosen);
}

uint32_t llvm_emit_canonical_lanes(struct emitter *e, const char *vector,
                                   const char *lane, size_t lanes,
                                   uint32_t value)
{
    uint32_t unordered = llvm_emit_fresh(e);
    uint32_t chosen = llvm_emit_fresh(e);
    size_t k;

    text_appendf(e->out, "  %%v%" PRIu32 " = fcmp uno %s %%v%" PRIu32
                         ", zeroinitializer\n",
                 unordered, vector, value);
    text_appendf(e->out, "  %%v%" PRIu32 " = select <%zu x i1> %%v%" PRIu32
                         ", %s <",
                 chosen, lanes, unordered, vector);
    for (k = 0; k < lanes; k++) {
        text_appendf(e->out, "%s%s " NAN_CANONICAL, k > 0 ? ", " : "", lane);
    }
    text_appendf(e->out, ">, %s %%v%" PRIu32 "\n", vector, value);
    return chosen;
}

bool llvm_emit_nan_lanes(enum ir_type lane)
{
    return is_float(lane);
}

void llvm_emit_nan_constant(struct text *out)
{
    text_append(out, NAN_CANONICAL);
}
