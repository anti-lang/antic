#include "regalloc.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "select.h"

/* DESIGN: linear scan in the form of Poletto and Sarkar. Each virtual
   register gets one live interval without holes, from its first to its
   last point of liveness in the order of the blocks. Instruction k reads
   at position 2k and writes at 2k + 1, so a register read by an
   instruction can be the one it writes. Physical registers that the
   machine code names, or that a call overwrites, form fixed ranges that
   an interval must not overlap. */

enum { NONE = -1, PREG_LIMIT = REGALLOC_REGISTERS };

/* The registers one instruction may read: each operand, and the base and
   the index of a memory operand. */
enum { BORROW_LIMIT = 2 * MACH_MAX_OPERANDS };

/* Sets of virtual registers */

struct set {
    uint64_t *bits;
    size_t words;
};

static bool set_has(const struct set *s, uint32_t v)
{
    return (s->bits[v / 64] >> (v % 64)) & 1;
}

static void set_add(struct set *s, uint32_t v)
{
    s->bits[v / 64] |= (uint64_t)1 << (v % 64);
}

/* Liveness */

/* DESIGN: a position is 2k for the reads of instruction k and 2k + 1 for
   its writes. Positions and slots are int64_t, so a function of any
   instruction count numbers them without overflow. */
struct range {
    int64_t from;
    int64_t to;
};

struct fixed {
    struct range *ranges;
    size_t count;
    size_t capacity;
};

struct interval {
    uint32_t vreg;
    int64_t start;
    int64_t end;
    int hint_preg;
    int hint_vreg;
    int preg;               /* NONE when spilled */
    int64_t slot;           /* the spill slot, NONE when in a register */
    bool fp;                /* in the float register class */
    /* DESIGN: the cost of spilling. Every use and definition counts one,
       multiplied by ten for each loop its block sits inside. The lowest
       cost gives up its register, so a value a loop reads stays in one. */
    uint64_t weight;
};

struct alloc {
    const struct target_desc *target;
    const struct abi *abi;
    struct mach_function *f;
    struct set *use;
    struct set *def;
    struct set *live_in;
    struct set *live_out;
    size_t words;
    struct interval *intervals;     /* indexed by virtual register */
    struct fixed fixed[PREG_LIMIT];
    /* The slot that saves each borrowed register, per class: integer,
       float. NONE until an instruction needs it. */
    int64_t borrow_slot[2][BORROW_LIMIT];
    bool refused;           /* an instruction found no register to borrow */
};

static bool is_reg(const struct mach_operand *o)
{
    return o->kind == MACH_VREG || o->kind == MACH_PREG;
}

/* Call visit for each register that inst reads, then for each register
   it writes. The base of a memory operand is read. */
static void each_register(const struct alloc *a, const struct mach_inst *inst,
                          void (*visit)(void *ctx, const struct mach_operand *o,
                                        bool write),
                          void *ctx)
{
    const struct mach_opcode *op = &a->target->opcodes[inst->op];
    struct mach_operand implicit;
    size_t i;
    int r;

    for (i = 0; i < inst->count; i++) {
        const struct mach_operand *o = &inst->operands[i];
        if (o->kind == MACH_MEM) {
            struct mach_operand base = *o;
            base.kind = o->base_vreg ? MACH_VREG : MACH_PREG;
            visit(ctx, &base, false);
            if (o->scale != 0) {
                base.kind = o->index_vreg ? MACH_VREG : MACH_PREG;
                base.reg = o->index_reg;
                visit(ctx, &base, false);
            }
        } else if (is_reg(o) && (op->roles[i] & ROLE_USE)) {
            visit(ctx, o, false);
        }
    }
    memset(&implicit, 0, sizeof implicit);
    implicit.kind = MACH_PREG;
    implicit.width = 64;
    for (r = 0; r < PREG_LIMIT; r++) {
        if ((inst->uses >> r) & 1) {
            implicit.reg = (uint32_t)r;
            visit(ctx, &implicit, false);
        }
    }
    for (i = 0; i < inst->count; i++) {
        const struct mach_operand *o = &inst->operands[i];
        if (is_reg(o) && (op->roles[i] & ROLE_DEF)) {
            visit(ctx, o, true);
        }
    }
    for (r = 0; r < PREG_LIMIT; r++) {
        if ((inst->defs >> r) & 1) {
            implicit.reg = (uint32_t)r;
            visit(ctx, &implicit, true);
        }
    }
}

struct block_ctx {
    struct alloc *a;
    size_t block;
};

static void gather_use_def(void *ctx, const struct mach_operand *o, bool write)
{
    struct block_ctx *c = ctx;
    struct set *use = &c->a->use[c->block];
    struct set *def = &c->a->def[c->block];

    if (o->kind != MACH_VREG) {
        return;
    }
    if (write) {
        set_add(def, o->reg);
    } else if (!set_has(def, o->reg)) {
        set_add(use, o->reg);
    }
}

/* The blocks that control can reach from block b. They are the targets
   of its jumps and branches, and the next block unless b ends with a jump
   or a return. */
static size_t successors(const struct alloc *a, size_t b, size_t out[3])
{
    const struct mach_block *block = &a->f->blocks[b];
    size_t n = 0;
    size_t i;
    size_t k;
    bool falls = true;

    for (i = 0; i < block->count; i++) {
        const struct mach_inst *inst = &block->insts[i];
        uint8_t flags = a->target->opcodes[inst->op].flags;
        for (k = 0; k < inst->count && (flags & (FLAG_JUMP | FLAG_BRANCH));
             k++) {
            if (inst->operands[k].kind == MACH_BLOCK && n < 2) {
                out[n++] = (size_t)inst->operands[k].value;
            }
        }
        if (i + 1 == block->count && (flags & (FLAG_JUMP | FLAG_RET))) {
            falls = false;
        }
    }
    if (falls && b + 1 < a->f->block_count) {
        out[n++] = b + 1;
    }
    return n;
}

static void compute_liveness(struct alloc *a)
{
    size_t n = a->f->block_count;
    size_t b;
    size_t i;
    size_t w;
    bool changed = true;

    a->words = a->f->vreg_count / 64 + 1;
    a->use = ir_alloc(n, sizeof *a->use);
    a->def = ir_alloc(n, sizeof *a->def);
    a->live_in = ir_alloc(n, sizeof *a->live_in);
    a->live_out = ir_alloc(n, sizeof *a->live_out);
    for (b = 0; b < n; b++) {
        struct block_ctx ctx;
        a->use[b].bits = ir_alloc(a->words, sizeof(uint64_t));
        a->def[b].bits = ir_alloc(a->words, sizeof(uint64_t));
        a->live_in[b].bits = ir_alloc(a->words, sizeof(uint64_t));
        a->live_out[b].bits = ir_alloc(a->words, sizeof(uint64_t));
        ctx.a = a;
        ctx.block = b;
        for (i = 0; i < a->f->blocks[b].count; i++) {
            each_register(a, &a->f->blocks[b].insts[i], gather_use_def, &ctx);
        }
    }
    /* live_out(b) is the union of live_in over the successors, and
       live_in(b) is use(b) plus live_out(b) without def(b). */
    while (changed) {
        changed = false;
        for (b = n; b-- > 0;) {
            size_t succ[3];
            size_t count = successors(a, b, succ);
            for (w = 0; w < a->words; w++) {
                uint64_t out = 0;
                uint64_t in;
                for (i = 0; i < count; i++) {
                    out |= a->live_in[succ[i]].bits[w];
                }
                in = a->use[b].bits[w] | (out & ~a->def[b].bits[w]);
                if (out != a->live_out[b].bits[w] ||
                    in != a->live_in[b].bits[w]) {
                    a->live_out[b].bits[w] = out;
                    a->live_in[b].bits[w] = in;
                    changed = true;
                }
            }
        }
    }
}

/* Intervals and fixed ranges */

static void extend(struct interval *iv, int64_t position)
{
    if (position < iv->start) {
        iv->start = position;
    }
    if (position > iv->end) {
        iv->end = position;
    }
}

struct scan_ctx {
    struct alloc *a;
    int64_t position;       /* 2k for instruction k */
    int64_t block_start;
    uint64_t weight;        /* what one use in this block costs */
    int64_t current[PREG_LIMIT];    /* the open fixed range of each one */
};

static void add_range(struct fixed *fx, int64_t from, int64_t to)
{
    fx->ranges = ir_grow(fx->ranges, &fx->capacity, fx->count,
                         sizeof *fx->ranges);
    fx->ranges[fx->count].from = from;
    fx->ranges[fx->count].to = to;
    fx->count++;
}

static void scan_register(void *ctx, const struct mach_operand *o, bool write)
{
    struct scan_ctx *c = ctx;
    struct alloc *a = c->a;
    int64_t position = c->position + (write ? 1 : 0);

    if (o->kind == MACH_VREG) {
        extend(&a->intervals[o->reg], position);
        a->intervals[o->reg].weight += c->weight;
        return;
    }
    if (o->reg >= PREG_LIMIT) {
        return;
    }
    if (write) {
        add_range(&a->fixed[o->reg], position, position);
        c->current[o->reg] = (int64_t)a->fixed[o->reg].count - 1;
    } else if (c->current[o->reg] == NONE) {
        add_range(&a->fixed[o->reg], c->block_start, position);
        c->current[o->reg] = (int64_t)a->fixed[o->reg].count - 1;
    } else {
        a->fixed[o->reg].ranges[c->current[o->reg]].to = position;
    }
}

/* A move between two registers suggests the same register for both, so
   that the move disappears. */
static void record_hint(struct alloc *a, const struct mach_inst *inst)
{
    const struct mach_operand *dst = &inst->operands[0];
    const struct mach_operand *src = &inst->operands[1];

    if (!(a->target->opcodes[inst->op].flags & FLAG_MOVE) ||
        inst->count != 2 || !is_reg(dst) || !is_reg(src)) {
        return;
    }
    if (dst->kind == MACH_VREG && src->kind == MACH_PREG &&
        a->intervals[dst->reg].hint_preg == NONE) {
        a->intervals[dst->reg].hint_preg = (int)src->reg;
    } else if (dst->kind == MACH_PREG && src->kind == MACH_VREG &&
               a->intervals[src->reg].hint_preg == NONE) {
        a->intervals[src->reg].hint_preg = (int)dst->reg;
    } else if (dst->kind == MACH_VREG && src->kind == MACH_VREG) {
        if (a->intervals[dst->reg].hint_vreg == NONE) {
            a->intervals[dst->reg].hint_vreg = (int)src->reg;
        }
        if (a->intervals[src->reg].hint_vreg == NONE) {
            a->intervals[src->reg].hint_vreg = (int)dst->reg;
        }
    }
}

/* DESIGN: a constant never spills. Its definition reads nothing, so it
   can be written again anywhere. Re-emitting it at each use costs one
   instruction, where a spill costs a store, a slot and a load. Each use
   gets a definition of its own, which leaves every live range one
   instruction long. */
static bool defines_constant(const struct target_desc *target,
                             const struct mach_inst *inst, uint32_t *vreg)
{
    const struct mach_opcode *op = &target->opcodes[inst->op];
    bool defines = false;
    size_t i;

    for (i = 0; i < inst->count; i++) {
        const struct mach_operand *o = &inst->operands[i];
        if ((op->roles[i] & ROLE_DEF) != 0) {
            if (defines || o->kind != MACH_VREG) {
                return false;
            }
            defines = true;
            *vreg = o->reg;
            continue;
        }
        if (o->kind != MACH_IMM) {
            return false;
        }
    }
    return defines;
}

static void rematerialise_constants(struct alloc *a)
{
    struct mach_function *f = a->f;
    struct mach_inst *definition =
        ir_alloc(f->vreg_count, sizeof *definition);
    bool *once = ir_alloc(f->vreg_count, sizeof *once);
    bool *many = ir_alloc(f->vreg_count, sizeof *many);
    size_t b;
    size_t i;
    size_t j;

    /* The one definition of each constant, and the vregs with more. */
    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b].count; i++) {
            struct mach_inst *inst = &f->blocks[b].insts[i];
            const struct mach_opcode *op = &a->target->opcodes[inst->op];
            uint32_t v = 0;
            for (j = 0; j < inst->count; j++) {
                if ((op->roles[j] & ROLE_DEF) != 0 &&
                    inst->operands[j].kind == MACH_VREG) {
                    many[inst->operands[j].reg] =
                        many[inst->operands[j].reg] || once[inst->operands[j].reg];
                    once[inst->operands[j].reg] = true;
                }
            }
            /* A vreg defined more than once is in many, so an instruction
               that defines no constant clears nothing here. */
            if (defines_constant(a->target, inst, &v)) {
                definition[v] = *inst;
            }
        }
    }
    /* Give every use a definition of its own, right before it. */
    for (b = 0; b < f->block_count; b++) {
        struct mach_block rebuilt;
        memset(&rebuilt, 0, sizeof rebuilt);
        rebuilt.loop_depth = f->blocks[b].loop_depth;
        for (i = 0; i < f->blocks[b].count; i++) {
            struct mach_inst *inst = &f->blocks[b].insts[i];
            const struct mach_opcode *op = &a->target->opcodes[inst->op];
            uint32_t made = 0;
            for (j = 0; j < inst->count; j++) {
                struct mach_operand *o = &inst->operands[j];
                struct mach_inst *copy;
                uint32_t v;
                size_t k;
                if (o->kind != MACH_VREG || (op->roles[j] & ROLE_USE) == 0) {
                    continue;
                }
                v = o->reg;
                if (many[v] || definition[v].count == 0) {
                    continue;
                }
                made = mach_vreg_add(f, f->fp[v]);
                copy = mach_append(&rebuilt);
                *copy = definition[v];
                for (k = 0; k < copy->count; k++) {
                    if ((a->target->opcodes[copy->op].roles[k] & ROLE_DEF) !=
                        0) {
                        copy->operands[k].reg = made;
                    }
                }
                o->reg = made;
            }
            /* The original definition goes, since every use has one. */
            if (defines_constant(a->target, inst, &made) && !many[made] &&
                definition[made].count != 0) {
                continue;
            }
            *mach_append(&rebuilt) = *inst;
        }
        free(f->blocks[b].insts);
        f->blocks[b] = rebuilt;
    }
    free(definition);
    free(once);
    free(many);
}

static void build_intervals(struct alloc *a)
{
    struct mach_function *f = a->f;
    struct scan_ctx ctx;
    size_t b;
    size_t i;
    uint32_t v;
    int64_t k = 0;

    a->intervals = ir_alloc(f->vreg_count, sizeof *a->intervals);
    for (v = 0; v < f->vreg_count; v++) {
        a->intervals[v].vreg = v;
        a->intervals[v].start = INT64_MAX;
        a->intervals[v].end = NONE;
        a->intervals[v].hint_preg = NONE;
        a->intervals[v].hint_vreg = NONE;
        a->intervals[v].preg = NONE;
        a->intervals[v].slot = NONE;
        a->intervals[v].fp = f->fp[v];
        a->intervals[v].weight = 0;
    }
    ctx.a = a;
    for (b = 0; b < f->block_count; b++) {
        const struct mach_block *block = &f->blocks[b];
        int64_t start = 2 * k;
        int64_t end = 2 * (k + (int64_t)block->count) - 1;
        if (block->count == 0) {
            end = start;
        }
        for (v = 0; v < f->vreg_count; v++) {
            if (set_has(&a->live_in[b], v)) {
                extend(&a->intervals[v], start);
            }
            if (set_has(&a->live_out[b], v)) {
                extend(&a->intervals[v], end);
            }
        }
        ctx.block_start = start;
        for (i = 0; i < PREG_LIMIT; i++) {
            ctx.current[i] = NONE;
        }
        ctx.weight = 1;
        for (i = 0; i < block->loop_depth && ctx.weight < 100000000; i++) {
            ctx.weight *= 10;
        }
        for (i = 0; i < block->count; i++) {
            ctx.position = 2 * k++;
            each_register(a, &block->insts[i], scan_register, &ctx);
            record_hint(a, &block->insts[i]);
        }
    }
}

/* Linear scan */

static bool overlaps_fixed(const struct alloc *a, int preg,
                           const struct interval *iv)
{
    const struct fixed *fx = &a->fixed[preg];
    size_t i;

    for (i = 0; i < fx->count; i++) {
        if (fx->ranges[i].from <= iv->end && iv->start <= fx->ranges[i].to) {
            return true;
        }
    }
    return false;
}

static bool is_free(const struct alloc *a, struct interval **active,
                    size_t active_count, int preg, const struct interval *iv)
{
    size_t i;

    for (i = 0; i < active_count; i++) {
        if (active[i]->preg == preg) {
            return false;
        }
    }
    return !overlaps_fixed(a, preg, iv);
}

/* The registers that the allocator hands out for the class of iv, in the
   order of preference. */
static const uint8_t *class_registers(const struct alloc *a,
                                      const struct interval *iv, size_t *count)
{
    *count = iv->fp ? a->abi->fp_allocatable_count
                    : a->abi->allocatable_count;
    return iv->fp ? a->abi->fp_allocatable : a->abi->allocatable;
}

static bool is_allocatable(const struct alloc *a, const struct interval *iv,
                           int preg)
{
    size_t count;
    const uint8_t *regs = class_registers(a, iv, &count);
    size_t i;

    for (i = 0; i < count; i++) {
        if (regs[i] == preg) {
            return true;
        }
    }
    return false;
}

static int by_start(const void *x, const void *y)
{
    const struct interval *const *a = x;
    const struct interval *const *b = y;

    if ((*a)->start != (*b)->start) {
        return (*a)->start < (*b)->start ? -1 : 1;
    }
    return (*a)->vreg < (*b)->vreg ? -1 : (*a)->vreg > (*b)->vreg;
}

static int choose(const struct alloc *a, struct interval **active,
                  size_t active_count, const struct interval *iv)
{
    int hint = iv->hint_preg;
    size_t count;
    const uint8_t *regs = class_registers(a, iv, &count);
    size_t i;

    if (hint == NONE && iv->hint_vreg != NONE) {
        hint = a->intervals[iv->hint_vreg].preg;
    }
    if (hint != NONE && is_allocatable(a, iv, hint) &&
        is_free(a, active, active_count, hint, iv)) {
        return hint;
    }
    for (i = 0; i < count; i++) {
        if (is_free(a, active, active_count, regs[i], iv)) {
            return regs[i];
        }
    }
    return NONE;
}

/* DESIGN: when no register is free, the interval of the lowest spill
   cost gives up its register. That is the new interval itself, or an
   active one whose register the new interval can take. The cost counts
   every use and definition, ten times over for each enclosing loop. So a
   counter of a loop keeps its register, and a value used once outside one
   gives it up. */
static void linear_scan(struct alloc *a)
{
    struct mach_function *f = a->f;
    struct interval **order = ir_alloc(f->vreg_count, sizeof *order);
    struct interval **active = ir_alloc(f->vreg_count, sizeof *active);
    size_t count = 0;
    size_t active_count = 0;
    size_t i;
    size_t j;
    uint32_t v;

    for (v = 0; v < f->vreg_count; v++) {
        if (a->intervals[v].end != NONE) {
            order[count++] = &a->intervals[v];
        }
    }
    qsort(order, count, sizeof *order, by_start);
    for (i = 0; i < count; i++) {
        struct interval *cur = order[i];
        int preg;
        for (j = 0; j < active_count;) {
            if (active[j]->end < cur->start) {
                active[j] = active[--active_count];
            } else {
                j++;
            }
        }
        preg = choose(a, active, active_count, cur);
        if (preg == NONE) {
            struct interval *victim = NULL;
            /* A candidate outlives the current interval, so the value
               that gives up its register is one the scan still has to
               place. Among those the lowest cost gives it up. */
            for (j = 0; j < active_count; j++) {
                if (active[j]->end > cur->end && active[j]->fp == cur->fp &&
                    !overlaps_fixed(a, active[j]->preg, cur) &&
                    (victim == NULL ||
                     active[j]->weight < victim->weight)) {
                    victim = active[j];
                }
            }
            /* The current interval is itself a candidate, and it spills
               when it is the cheapest of them. */
            if (victim != NULL && victim->weight >= cur->weight) {
                victim = NULL;
            }
            if (victim == NULL) {
                cur->slot = mach_slot_add(f, 8, 8);
                continue;
            }
            preg = victim->preg;
            victim->preg = NONE;
            victim->slot = mach_slot_add(f, 8, 8);
            for (j = 0; j < active_count; j++) {
                if (active[j] == victim) {
                    active[j] = active[--active_count];
                    break;
                }
            }
        }
        cur->preg = preg;
        active[active_count++] = cur;
    }
    free(order);
    free(active);
}

/* Rewriting */

struct rewrite {
    struct alloc *a;
    struct mach_block out;
    const struct frame *frame;
};

static void emit(struct mach_block *b, const struct mach_inst *inst)
{
    *mach_append(b) = *inst;
}

/* The scratch register of a spilled virtual register within one
   instruction. */
struct spill_map {
    uint32_t vreg[MACH_MAX_OPERANDS];
    uint8_t scratch[MACH_MAX_OPERANDS];
    size_t count;
    size_t loads[2];        /* per class: integer, float */
    uint8_t borrowed[2][BORROW_LIMIT];  /* per class, after the scratch */
    size_t borrow_count[2];
};

static int find_spill(const struct spill_map *map, uint32_t vreg)
{
    size_t i;

    for (i = 0; i < map->count; i++) {
        if (map->vreg[i] == vreg) {
            return (int)i;
        }
    }
    return NONE;
}

/* The physical register of virtual register vreg within one instruction.
   A spilled register that the instruction reads is loaded into a scratch
   register first. A spilled register that it only writes takes the first
   scratch register, which the instruction writes after it has read its
   operands. */
static uint32_t place(struct rewrite *rw, struct spill_map *map, uint32_t vreg,
                      bool reads)
{
    struct alloc *a = rw->a;
    const struct interval *iv = &a->intervals[vreg];
    int k;

    if (iv->slot == NONE) {
        return (uint32_t)iv->preg;
    }
    k = find_spill(map, vreg);
    if (k == NONE) {
        const uint8_t *scratch = iv->fp ? a->abi->fp_scratch : a->abi->scratch;
        size_t n = reads ? map->loads[iv->fp] : 0;
        k = (int)map->count++;
        map->vreg[k] = vreg;
        map->scratch[k] = scratch[0];
        if (n < sizeof a->abi->scratch) {
            map->scratch[k] = scratch[n];
        } else if (n - sizeof a->abi->scratch < map->borrow_count[iv->fp]) {
            map->scratch[k] = map->borrowed[iv->fp][n - sizeof a->abi->scratch];
        }
        if (reads) {
            a->target->load_spill(&rw->out, map->scratch[k],
                                  a->f->slots[iv->slot].offset);
            map->loads[iv->fp]++;
        }
    }
    return map->scratch[k];
}

/* Both classes have as many scratch registers, which place counts with
   the size of the integer array. */
_Static_assert(sizeof ((struct abi *)0)->scratch ==
                   sizeof ((struct abi *)0)->fp_scratch,
               "one count of scratch registers per class");

/* What one instruction needs of the registers. named holds the physical
   registers it names, its own and those of its virtual registers in a
   register. spilled holds the distinct spilled ones it reads, per class. */
struct needs {
    const struct alloc *a;
    uint64_t named;
    uint32_t spilled[2][BORROW_LIMIT];
    size_t count[2];
};

static void note_register(void *ctx, const struct mach_operand *o, bool write)
{
    struct needs *n = ctx;
    const struct interval *iv;
    size_t i;

    if (o->kind == MACH_PREG) {
        if (o->reg < PREG_LIMIT) {
            n->named |= (uint64_t)1 << o->reg;
        }
        return;
    }
    iv = &n->a->intervals[o->reg];
    if (iv->slot == NONE) {
        if (iv->preg != NONE) {
            n->named |= (uint64_t)1 << iv->preg;
        }
        return;
    }
    if (write) {
        return;
    }
    for (i = 0; i < n->count[iv->fp]; i++) {
        if (n->spilled[iv->fp][i] == o->reg) {
            return;
        }
    }
    if (n->count[iv->fp] < BORROW_LIMIT) {
        n->spilled[iv->fp][n->count[iv->fp]++] = o->reg;
    }
}

static struct needs needs_of(const struct alloc *a,
                             const struct mach_inst *inst)
{
    struct needs n;

    memset(&n, 0, sizeof n);
    n.a = a;
    each_register(a, inst, note_register, &n);
    return n;
}

/* A slot for each register that an instruction borrows, taken before the
   frame is laid out. */
static void reserve_borrow_slots(struct alloc *a)
{
    struct mach_function *f = a->f;
    size_t b;
    size_t i;
    size_t k;
    int c;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b].count; i++) {
            struct needs n = needs_of(a, &f->blocks[b].insts[i]);
            for (c = 0; c < 2; c++) {
                for (k = sizeof a->abi->scratch; k < n.count[c]; k++) {
                    int64_t *slot =
                        &a->borrow_slot[c][k - sizeof a->abi->scratch];
                    if (*slot == NONE) {
                        *slot = mach_slot_add(f, 8, 8);
                    }
                }
            }
        }
    }
}

/* DESIGN: an instruction may read more spilled registers of one class
   than the target has scratch registers. ARM64's msub of a remainder
   reads three. Each read past the scratch registers borrows an
   allocatable register that the instruction names nowhere and that the
   caller does not expect preserved. Its value goes to a slot of its own
   before the instruction. It comes back after the instruction and after
   the store of a spilled result, so every scratch register is free at
   both points. An
   instruction that leaves the block cannot restore it and is refused. */
static void borrow(struct rewrite *rw, const struct mach_inst *inst,
                   struct spill_map *map)
{
    struct alloc *a = rw->a;
    const struct mach_opcode *op = &a->target->opcodes[inst->op];
    struct needs n = needs_of(a, inst);
    uint64_t taken = n.named | a->abi->callee_saved;
    size_t k;
    int c;

    for (c = 0; c < 2; c++) {
        const uint8_t *regs = c ? a->abi->fp_allocatable : a->abi->allocatable;
        size_t regs_count =
            c ? a->abi->fp_allocatable_count : a->abi->allocatable_count;
        size_t r = 0;
        for (k = sizeof a->abi->scratch; k < n.count[c]; k++) {
            int64_t slot = a->borrow_slot[c][k - sizeof a->abi->scratch];
            while (r < regs_count && ((taken >> regs[r]) & 1) != 0) {
                r++;
            }
            if (r == regs_count || slot == NONE ||
                (op->flags & (FLAG_JUMP | FLAG_BRANCH | FLAG_RET |
                              FLAG_CALL)) != 0) {
                a->refused = true;
                return;
            }
            map->borrowed[c][map->borrow_count[c]++] = regs[r];
            a->target->store_spill(&rw->out, regs[r],
                                   a->f->slots[slot].offset);
            r++;
        }
    }
}

/* Replace the virtual registers of inst, drop a move of a register into
   itself and add the epilogue before a return. Every instruction this
   writes belongs to the statement of inst, the loads of its spilled
   operands among them. Each one that carries no line takes the line of
   inst. */
static void rewrite_inst(struct rewrite *rw, const struct mach_inst *inst)
{
    struct alloc *a = rw->a;
    const struct mach_opcode *op = &a->target->opcodes[inst->op];
    struct mach_inst copy = *inst;
    size_t first = rw->out.count;
    struct spill_map map;
    size_t i;
    int k;
    int c;

    memset(&map, 0, sizeof map);
    borrow(rw, inst, &map);
    for (i = 0; i < copy.count; i++) {
        struct mach_operand *o = &copy.operands[i];
        if (o->kind == MACH_MEM && o->base_vreg) {
            o->reg = place(rw, &map, o->reg, true);
            o->base_vreg = false;
        }
        if (o->kind == MACH_MEM && o->scale != 0 && o->index_vreg) {
            o->index_reg = place(rw, &map, o->index_reg, true);
            o->index_vreg = false;
        }
        if (o->kind == MACH_VREG && (op->roles[i] & ROLE_USE)) {
            o->reg = place(rw, &map, o->reg, true);
            o->kind = MACH_PREG;
        }
    }
    for (i = 0; i < copy.count; i++) {
        struct mach_operand *o = &copy.operands[i];
        if (o->kind == MACH_VREG) {
            o->reg = place(rw, &map, o->reg, false);
            o->kind = MACH_PREG;
        }
    }
    /* DESIGN: a move of a register into itself does nothing, so it goes.
       The store that follows a spilled result does not go with it. Two
       spilled values may share one scratch register. The move between
       them is then a move into itself, while the store is still the only
       thing that writes the slot. */
    if (!((op->flags & FLAG_MOVE) && copy.count == 2 &&
          copy.operands[0].kind == MACH_PREG &&
          copy.operands[1].kind == MACH_PREG &&
          copy.operands[0].reg == copy.operands[1].reg)) {
        if (op->flags & FLAG_RET) {
            a->target->epilogue(&rw->out, rw->frame);
        }
        if (a->target->expand != NULL) {
            a->target->expand(&rw->out, &copy);
        } else {
            emit(&rw->out, &copy);
        }
    }
    for (i = 0; i < inst->count; i++) {
        const struct mach_operand *o = &inst->operands[i];
        if (o->kind == MACH_VREG && (op->roles[i] & ROLE_DEF) &&
            a->intervals[o->reg].slot != NONE) {
            k = find_spill(&map, o->reg);
            a->target->store_spill(
                &rw->out, map.scratch[k],
                a->f->slots[a->intervals[o->reg].slot].offset);
        }
    }
    for (c = 0; c < 2; c++) {
        for (i = 0; i < map.borrow_count[c]; i++) {
            a->target->load_spill(
                &rw->out, map.borrowed[c][i],
                a->f->slots[a->borrow_slot[c][i]].offset);
        }
    }
    for (i = first; i < rw->out.count; i++) {
        if (rw->out.insts[i].line == 0) {
            rw->out.insts[i].line = inst->line;
        }
    }
}

static uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) / align * align;
}

/* Slots from the bottom of the frame up, above the space a call needs,
   and the saved callee-saved registers at the top. */
static bool is_fp_register(const struct alloc *a, uint8_t preg)
{
    size_t i;

    for (i = 0; i < a->abi->fp_allocatable_count; i++) {
        if (a->abi->fp_allocatable[i] == preg) {
            return true;
        }
    }
    return false;
}

/* The bytes from the top of the frame to the end of the save of register
   preg. The saves above it take the first used bytes. A 16-byte save
   starts at a multiple of 16, which UWOP_SAVE_XMM128 of the Windows x64
   unwind data requires. */
static uint64_t save_end(const struct alloc *a, uint8_t preg, uint64_t used)
{
    uint64_t size = is_fp_register(a, preg) ? a->abi->fp_save_size : 8;

    return align_up(used, size) + size;
}

static void layout_frame(struct alloc *a, struct frame *frame)
{
    struct mach_function *f = a->f;
    uint64_t saved_bytes = 0;
    uint64_t offset = 0;
    uint64_t used = 0;
    bool calls = false;
    size_t b;
    size_t i;
    uint32_t v;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b].count; i++) {
            calls = calls ||
                    (a->target->opcodes[f->blocks[b].insts[i].op].flags &
                     FLAG_CALL);
        }
    }
    for (v = 0; v < f->vreg_count; v++) {
        if (a->intervals[v].preg != NONE) {
            used |= (uint64_t)1 << a->intervals[v].preg;
        }
    }
    memset(frame, 0, sizeof *frame);
    for (i = 0; i < PREG_LIMIT; i++) {
        if (((used & a->abi->callee_saved) >> i) & 1) {
            frame->saved[frame->saved_count++] = (uint8_t)i;
        }
    }
    offset = f->outgoing;
    for (i = 0; i < f->slot_count; i++) {
        offset = align_up(offset, f->slots[i].align);
        f->slots[i].offset = (int64_t)offset;
        offset += f->slots[i].size;
    }
    for (i = 0; i < frame->saved_count; i++) {
        saved_bytes = save_end(a, frame->saved[i], saved_bytes);
    }
    frame->size = align_up(offset + saved_bytes, 16);
    saved_bytes = 0;
    for (i = 0; i < frame->saved_count; i++) {
        saved_bytes = save_end(a, frame->saved[i], saved_bytes);
        frame->saved_offset[i] = (int64_t)(frame->size - saved_bytes);
    }
    frame->needed = calls || frame->size > 0 || f->stack_params;
    frame->probe = a->abi->probe_stack && frame->size >= 4096;
}

bool regalloc_function(enum target t, struct mach_function *f, char *error,
                       size_t error_size)
{
    struct alloc a;
    struct frame frame;
    struct rewrite rw;
    size_t b;
    size_t i;
    size_t k;
    bool ok;

    memset(&a, 0, sizeof a);
    a.target = target_desc(t);
    a.abi = a.target->abi(target_info(t)->convention);
    a.f = f;
    for (b = 0; b < 2; b++) {
        for (k = 0; k < BORROW_LIMIT; k++) {
            a.borrow_slot[b][k] = NONE;
        }
    }
    /* Constants are re-emitted before liveness, so their ranges are one
       instruction long and the scan never has to spill one. */
    rematerialise_constants(&a);
    compute_liveness(&a);
    build_intervals(&a);
    linear_scan(&a);
    reserve_borrow_slots(&a);
    layout_frame(&a, &frame);
    frame.convention = target_info(t)->convention;
    frame.unwind = target_info(t)->format == FORMAT_COFF;
    f->unwind = frame.unwind && frame.needed;
    ok = a.target->frame_limit == 0 || frame.size <= a.target->frame_limit;
    if (!ok) {
        snprintf(error, error_size, "the stack frame of `%s.%s` needs %llu "
                 "bytes, and an %s frame holds at most %llu", f->ir->module,
                 f->ir->name, (unsigned long long)frame.size, a.target->name,
                 (unsigned long long)a.target->frame_limit);
    }
    for (b = 0; ok && b < f->block_count; b++) {
        memset(&rw, 0, sizeof rw);
        rw.a = &a;
        rw.frame = &frame;
        if (b == 0) {
            a.target->prologue(&rw.out, &frame);
        }
        for (i = 0; i < f->blocks[b].count; i++) {
            struct mach_inst *inst = &f->blocks[b].insts[i];
            for (k = 0; k < inst->count; k++) {
                if (inst->operands[k].kind == MACH_SLOT) {
                    a.target->resolve_slot(
                        &inst->operands[k],
                        f->slots[inst->operands[k].value].offset);
                }
            }
            rewrite_inst(&rw, inst);
        }
        free(f->blocks[b].insts);
        f->blocks[b] = rw.out;
    }
    if (ok && a.refused) {
        snprintf(error, error_size, "an instruction of `%s.%s` reads more "
                 "spilled registers than %s can load for it", f->ir->module,
                 f->ir->name, a.target->name);
        ok = false;
    }
    for (b = 0; b < f->block_count; b++) {
        free(a.use[b].bits);
        free(a.def[b].bits);
        free(a.live_in[b].bits);
        free(a.live_out[b].bits);
    }
    for (i = 0; i < PREG_LIMIT; i++) {
        free(a.fixed[i].ranges);
    }
    free(a.use);
    free(a.def);
    free(a.live_in);
    free(a.live_out);
    free(a.intervals);
    return ok;
}
