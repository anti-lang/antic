/* The facts for opt: the ranges of the loads, the !tbaa tree of the
   accesses, the facts of the tables, the weights of a cold branch and the
   functions whose frame a stack trace keeps. They read the module and add
   metadata, and translate nothing. */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "llvm_debug.h"
#include "llvm_emit_parts.h"

/* DESIGN: the table facts of "Tables and dispatch" in
   docs/work-order-llvm-optimization.md. A load or a store of a table
   pointer carries !invariant.group, so opt forwards the table an object
   was made with to every call through it, across any call between. A
   load of an entry carries !invariant.load, since no program writes a
   table. Both stand only in a module whose pass over the whole program
   found every table pointer written once, see whole_program. */
void llvm_emit_access_fact(struct emitter *e, const struct ir_inst *inst)
{
    if (!e->facts || inst->field == IR_ACCESS_PLAIN) {
        return;
    }
    text_appendf(e->out, ", %s !%" PRIu32,
                 inst->field == IR_ACCESS_TABLE ? "!invariant.group"
                                                : "!invariant.load",
                 e->facts_id);
}

/* The index of the range of inst among the ranges of the module, or
   range_count for a load without one. */
static size_t range_index(const struct load_range *ranges, size_t count,
                          const struct ir_inst *inst)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (ranges[i].type == inst->type &&
            ranges[i].low == inst->b.as.integer &&
            ranges[i].high == inst->c.as.integer) {
            return i;
        }
    }
    return count;
}

/* DESIGN: the range of a load as !range, in the metadata node of that
   range, which every load of the same range names. See lower_range_of. */
void llvm_emit_range_fact(struct emitter *e, const struct ir_inst *inst)
{
    if (inst->b.kind != IR_INT) {
        return;
    }
    text_appendf(e->out, ", !range !%" PRIu32,
                 e->range_id + (uint32_t)range_index(e->ranges,
                                                     e->range_count, inst));
}

/* Collect the ranges of the loads of m, each once, in the order of their
   first load. */
void llvm_emit_collect_ranges(struct emitter *e, const struct ir_module *m)
{
    size_t i;
    size_t j;
    size_t k;
    size_t capacity = 0;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        for (j = 0; j < f->block_count; j++) {
            const struct ir_block *b = f->blocks[j];
            for (k = 0; k < b->count; k++) {
                const struct ir_inst *inst = &b->insts[k];
                if (inst->op != IR_LOAD || inst->b.kind != IR_INT ||
                    range_index(e->ranges, e->range_count, inst) <
                        e->range_count) {
                    continue;
                }
                e->ranges = alloc_grow(e->ranges, &capacity, e->range_count,
                                       sizeof *e->ranges);
                e->ranges[e->range_count].type = inst->type;
                e->ranges[e->range_count].low = inst->b.as.integer;
                e->ranges[e->range_count].high = inst->c.as.integer;
                e->range_count++;
            }
        }
    }
}

/* The scalar types of the !tbaa tree, in the order of their ids. */
enum {
    TBAA_I16,
    TBAA_I32,
    TBAA_I64,
    TBAA_F32,
    TBAA_F64,
    TBAA_PTR,
    TBAA_SCALARS,
    TBAA_NONE = TBAA_SCALARS
};

/* DESIGN: the types of "Aliasing of views" in
   docs/anti-language-additions.md as !tbaa, in the scalar form of LLVM
   with struct paths, see ir_type_access. A root holds bytes, which every
   other type sits below, as an access of bytes may refer to any memory.
   Below bytes stands one node per scalar type, which merges the signs of
   an integer, an enum with its base, char with u32 and every pointer into
   one. A struct node lists the scalar fields of a struct at their offsets
   on the target, and an access of a field names the struct, the field's
   type and the offset. Two fields of two structs then never alias, a
   field and an access of its scalar type alone may, and bytes, a bool, a
   lock word and an aggregate field stay out of the struct node. f16 is
   storage of 16 bits and shares the node of i16, which keeps it beside
   u16, the one type a view of it reads. */
static unsigned tbaa_scalar(const struct emitter *e, enum ir_type type)
{
    switch (type) {
    case IR_I16: return TBAA_I16;
    case IR_I32: return TBAA_I32;
    case IR_I64: return TBAA_I64;
    case IR_F32: return TBAA_F32;
    case IR_F64: return TBAA_F64;
    case IR_PTR: return TBAA_PTR;
    case IR_CLONG:
    case IR_CWCHAR:
        switch (layout_size(e->l, ir_scalar(type))) {
        case 2: return TBAA_I16;
        case 4: return TBAA_I32;
        default: return TBAA_I64;
        }
    default:
        return TBAA_NONE;
    }
}

/* The field of the type of a typed access inst among the fields of the
   module, or tbaa_field_count. */
static size_t tbaa_field_index(const struct emitter *e,
                               const struct ir_inst *inst)
{
    size_t i;

    for (i = 0; i < e->tbaa_field_count; i++) {
        if (e->tbaa_fields[i].agg == inst->of.agg &&
            e->tbaa_fields[i].member == inst->member) {
            return i;
        }
    }
    return e->tbaa_field_count;
}

/* Whether the field that the typed access inst names stands in the node
   of its struct with the scalar type of the access. */
static bool tbaa_field_of(const struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_aggtype *agg;

    if (inst->of.type != IR_AGG || inst->of.agg >= e->m->agg_count) {
        return false;
    }
    agg = e->m->aggs[inst->of.agg];
    return agg->kind == IR_AGG_STRUCT && !agg->simd &&
           inst->member < agg->field_count &&
           agg->fields[inst->member].bits == 0 &&
           agg->fields[inst->member].type.type != IR_AGG &&
           tbaa_scalar(e, agg->fields[inst->member].type.type) ==
               tbaa_scalar(e, inst->type);
}

/* Collect the structs and the fields that the typed accesses of m name,
   each once, in the order of their first access. */
void llvm_emit_collect_tbaa(struct emitter *e, const struct ir_module *m)
{
    size_t agg_capacity = 0;
    size_t field_capacity = 0;
    size_t i;
    size_t j;
    size_t k;
    size_t a;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        for (j = 0; j < f->block_count; j++) {
            const struct ir_block *b = f->blocks[j];
            for (k = 0; k < b->count; k++) {
                const struct ir_inst *inst = &b->insts[k];
                if ((inst->op != IR_LOAD && inst->op != IR_STORE) ||
                    inst->of.type == IR_VOID ||
                    tbaa_scalar(e, inst->type) == TBAA_NONE) {
                    continue;
                }
                e->tbaa = true;
                if (!tbaa_field_of(e, inst) ||
                    tbaa_field_index(e, inst) < e->tbaa_field_count) {
                    continue;
                }
                for (a = 0; a < e->tbaa_agg_count; a++) {
                    if (e->tbaa_aggs[a] == inst->of.agg) {
                        break;
                    }
                }
                if (a == e->tbaa_agg_count) {
                    e->tbaa_aggs = alloc_grow(e->tbaa_aggs, &agg_capacity,
                                              e->tbaa_agg_count,
                                              sizeof *e->tbaa_aggs);
                    e->tbaa_aggs[e->tbaa_agg_count++] = inst->of.agg;
                }
                e->tbaa_fields = alloc_grow(e->tbaa_fields, &field_capacity,
                                            e->tbaa_field_count,
                                            sizeof *e->tbaa_fields);
                e->tbaa_fields[e->tbaa_field_count].agg = inst->of.agg;
                e->tbaa_fields[e->tbaa_field_count].member = inst->member;
                e->tbaa_fields[e->tbaa_field_count].node = (uint32_t)a;
                e->tbaa_field_count++;
            }
        }
    }
}

/* The number of ids the !tbaa tree of the module takes: the root, bytes,
   the scalar types and their tags, then the structs and their fields. */
uint32_t llvm_emit_tbaa_count(const struct emitter *e)
{
    if (!e->tbaa) {
        return 0;
    }
    return 2 + 2 * TBAA_SCALARS + (uint32_t)e->tbaa_agg_count +
           (uint32_t)e->tbaa_field_count;
}

/* The !tbaa of a typed access: the tag of its field, or of its scalar
   type. */
void llvm_emit_tbaa_fact(struct emitter *e, const struct ir_inst *inst)
{
    unsigned scalar;
    uint32_t id;

    if (!e->tbaa || inst->of.type == IR_VOID) {
        return;
    }
    scalar = tbaa_scalar(e, inst->type);
    if (scalar == TBAA_NONE) {
        return;
    }
    id = e->tbaa_id + 2 + TBAA_SCALARS + scalar;
    if (tbaa_field_of(e, inst)) {
        id = e->tbaa_id + 2 + 2 * TBAA_SCALARS +
             (uint32_t)e->tbaa_agg_count +
             (uint32_t)tbaa_field_index(e, inst);
    }
    text_appendf(e->out, ", !tbaa !%" PRIu32, id);
}

/* The !tbaa tree of the module, in the ids that llvm_emit_tbaa_count
   gives. */
void llvm_emit_tbaa_nodes(struct emitter *e)
{
    static const char *const names[TBAA_SCALARS] = {"i16", "i32", "i64",
                                                    "f32", "f64", "ptr"};
    uint32_t first = e->tbaa_id + 2;
    uint32_t structs = first + 2 * TBAA_SCALARS;
    uint32_t i;
    size_t j;
    size_t k;

    if (!e->tbaa) {
        return;
    }
    text_appendf(e->out, "!%" PRIu32 " = !{!\"anti\"}\n", e->tbaa_id);
    text_appendf(e->out, "!%" PRIu32 " = !{!\"byte\", !%" PRIu32 ", i64 0}\n",
                 e->tbaa_id + 1, e->tbaa_id);
    for (i = 0; i < TBAA_SCALARS; i++) {
        text_appendf(e->out, "!%" PRIu32 " = !{!\"%s\", !%" PRIu32
                             ", i64 0}\n",
                     first + i, names[i], e->tbaa_id + 1);
    }
    for (i = 0; i < TBAA_SCALARS; i++) {
        text_appendf(e->out, "!%" PRIu32 " = !{!%" PRIu32 ", !%" PRIu32
                             ", i64 0}\n",
                     first + TBAA_SCALARS + i, first + i, first + i);
    }
    for (j = 0; j < e->tbaa_agg_count; j++) {
        const struct ir_aggtype *agg = e->m->aggs[e->tbaa_aggs[j]];
        const struct layout *layout = layout_agg(e->l, e->tbaa_aggs[j]);
        text_appendf(e->out, "!%" PRIu32 " = !{!", structs + (uint32_t)j);
        llvm_debug_string(e->out, agg->name);
        for (k = 0; k < agg->field_count; k++) {
            unsigned scalar = tbaa_scalar(e, agg->fields[k].type.type);
            if (agg->fields[k].bits != 0 || scalar == TBAA_NONE) {
                continue;
            }
            text_appendf(e->out, ", !%" PRIu32 ", i64 %" PRIu64,
                         first + scalar, layout->offsets[k]);
        }
        text_append(e->out, "}\n");
    }
    for (j = 0; j < e->tbaa_field_count; j++) {
        const struct tbaa_field *field = &e->tbaa_fields[j];
        const struct ir_aggtype *agg = e->m->aggs[field->agg];
        text_appendf(e->out, "!%" PRIu32 " = !{!%" PRIu32 ", !%" PRIu32
                             ", i64 %" PRIu64 "}\n",
                     structs + (uint32_t)e->tbaa_agg_count + (uint32_t)j,
                     structs + field->node,
                     first + tbaa_scalar(e, agg->fields[field->member].type.type),
                     layout_agg(e->l, field->agg)->offsets[field->member]);
    }
}

/* Whether a load or a store of a function of m reaches a table pointer or
   an entry of a table. */
bool llvm_emit_names_table(const struct ir_module *m)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        for (j = 0; j < f->block_count; j++) {
            const struct ir_block *b = f->blocks[j];
            for (k = 0; k < b->count; k++) {
                if ((b->insts[k].op == IR_LOAD || b->insts[k].op == IR_STORE) &&
                    b->insts[k].field != IR_ACCESS_PLAIN) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* The metadata of a branch whose one side is the failure arm of an
   assertion or a check, or the arm of a none guard, which marks that side
   cold. */
void llvm_emit_branch_weights(struct emitter *e, uint32_t then_block,
                              uint32_t else_block)
{
    bool then_cold = e->f->blocks[then_block]->fail != IR_FAIL_NONE;
    bool else_cold = e->f->blocks[else_block]->fail != IR_FAIL_NONE;

    if (then_cold == else_cold) {
        return;
    }
    text_appendf(e->out, ", !prof !%" PRIu32,
                 e->cold_id + (then_cold ? COLD_THEN : COLD_ELSE));
    e->cold_used[then_cold ? COLD_THEN : COLD_ELSE] = true;
}

/* The function of the runtime that walks the stack from the frame of its
   caller, which anti.lang.StackTrace.capture declares and calls. */
#define TRACE_WALK "anti_rt_trace_walk"

/* The function that the call inst calls, or IR_NO_INDEX. The IR calls
   a static function of a class through a temporary that `addr` of the
   function fills, which callees maps to the function. */
static uint32_t callee_of(const struct ir_inst *inst, const uint32_t *callees)
{
    if (inst->a.kind == IR_FUNC) {
        return inst->a.as.index;
    }
    return inst->a.kind == IR_TEMP ? callees[inst->a.as.temp] : IR_NO_INDEX;
}

/* Whether the call inst passes another skip than the constant 0 to a
   function that keeps its frame. The skip is the first argument of
   anti_rt_trace_walk, of StackTrace.capture and of debug.backtrace. */
static bool skips_own_frame(const struct ir_module *m, const bool *kept,
                            const struct ir_inst *inst, const uint32_t *callees)
{
    uint32_t callee;

    if (inst->op != IR_CALL) {
        return false;
    }
    callee = callee_of(inst, callees);
    if (callee == IR_NO_INDEX || !kept[callee]) {
        return false;
    }
    if (m->functions[callee]->is_extern) {
        return true;
    }
    return inst->arg_count > 0 && (inst->args[0].kind != IR_INT ||
                                   inst->args[0].as.integer != 0);
}

/* Fill callees with the function each temporary of f holds the address
   of, from an `addr` or a copy of the function, or IR_NO_INDEX. */
static void callees_of(const struct ir_function *f, uint32_t *callees)
{
    size_t b;
    size_t k;

    for (k = 0; k < f->temp_count; k++) {
        callees[k] = IR_NO_INDEX;
    }
    for (b = 0; b < f->block_count; b++) {
        const struct ir_block *block = f->blocks[b];
        for (k = 0; k < block->count; k++) {
            const struct ir_inst *inst = &block->insts[k];
            if ((inst->op == IR_ADDR || inst->op == IR_COPY) &&
                inst->a.kind == IR_FUNC &&
                inst->result != IR_NO_RESULT) {
                callees[inst->result] = inst->a.as.index;
            }
        }
    }
}

/* DESIGN: StackTrace.capture skips its own frame by count, `skip + 1`,
   and debug.backtrace and a caller of capture(1) count their own the same
   way. Inlined into its caller, or left by a tail call, such a function
   would skip the frame of its caller as well, and the trace would not
   begin where the specification says. So a function keeps its frame,
   `noinline` and without tail calls, when it calls anti_rt_trace_walk,
   or when it passes another skip than 0 to a function that keeps its
   frame. A call without arguments passes no skip. A function that captures with skip 0
   may be inlined, and its frames are then the ones of its caller, with
   the lines of the callee under -g, as "Debug information" of
   docs/work-order-llvm-back-end.md says. The rule runs to a fixed point
   over the functions of the text. */
void llvm_emit_frames_kept(const struct ir_module *m, bool *kept)
{
    bool changed = true;
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        kept[i] = f->is_extern && f->module == NULL &&
                  strcmp(f->name, TRACE_WALK) == 0;
    }
    while (changed) {
        changed = false;
        for (i = 0; i < m->function_count; i++) {
            const struct ir_function *f = m->functions[i];
            uint32_t *callees;
            size_t b;
            if (kept[i] || f->is_extern) {
                continue;
            }
            callees = alloc_zeroed(f->temp_count + 1, sizeof *callees);
            callees_of(f, callees);
            for (b = 0; !kept[i] && b < f->block_count; b++) {
                const struct ir_block *block = f->blocks[b];
                size_t k;
                for (k = 0; !kept[i] && k < block->count; k++) {
                    if (skips_own_frame(m, kept, &block->insts[k], callees)) {
                        kept[i] = true;
                        changed = true;
                    }
                }
            }
            free(callees);
        }
    }
}
