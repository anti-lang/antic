#ifndef ANTIC_TARGET_DESC_H
#define ANTIC_TARGET_DESC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "ir.h"
#include "mach.h"
#include "target.h"
#include "text.h"

/* The description of a target that selection, allocation and emission
   read: its pattern table, its calling conventions and the functions
   that write its instructions. Selection fills its callbacks in, so the
   machine code, the allocator and the emitter need the description
   without the selector. */

struct selector;

/* One row of a target's pattern table. The first row whose operation
   matches and whose match function accepts the instruction emits it. A
   NULL match function accepts every instruction of the operation. */
struct pattern {
    enum ir_op op;
    bool (*match)(const struct selector *s, const struct ir_inst *inst);
    void (*emit)(struct selector *s, const struct ir_inst *inst);
};

/* The registers of a calling convention, as physical register numbers of
   the target. The float registers have numbers above the integer ones. */
struct abi {
    const uint8_t *int_args;
    size_t int_arg_count;
    uint8_t int_result;
    uint64_t caller_saved;
    uint64_t callee_saved;
    const uint8_t *allocatable;         /* in the order of preference */
    size_t allocatable_count;
    uint8_t scratch[2];                 /* for spilled registers */
    uint8_t shadow_space;               /* bytes a caller reserves */
    bool probe_stack;                   /* frames of a page call a probe */
    const uint8_t *fp_args;
    size_t fp_arg_count;
    uint8_t fp_result;
    const uint8_t *fp_allocatable;
    size_t fp_allocatable_count;
    uint8_t fp_scratch[2];
    uint8_t fp_save_size;               /* bytes a callee saves of one */
};

/* One register part of an aggregate: bytes at offset go to reg. */
struct arg_part {
    uint8_t reg;
    uint8_t bytes;
    uint8_t offset;
};

/* Where an argument or a parameter goes: a register, or an offset in the
   argument area on the stack. A variadic float on Windows also goes to the
   integer register copy. An aggregate goes to register parts, or with its
   size to the stack, or when indirect as a pointer to a copy. */
struct arg_location {
    bool stack;
    uint8_t reg;
    int64_t offset;
    int copy;                           /* a second register, or -1 */
    bool indirect;
    size_t part_count;
    struct arg_part parts[4];
    uint64_t size;
};

/* A memory address that selection folds into a load or a store: base
   plus index shifted left by shift plus offset. */
struct address {
    const struct ir_operand *base;
    const struct ir_operand *index;     /* NULL without an index */
    uint8_t shift;
    int64_t offset;
};

struct frame;

struct target_desc {
    const char *name;                   /* x86_64 or arm64 */
    int chapter;                        /* the chapter with all patterns */
    const struct mach_opcode *opcodes;
    const struct pattern *patterns;
    size_t pattern_count;
    const struct abi *(*abi)(enum convention convention);
    uint8_t (*width)(enum ir_type type);
    void (*move)(struct selector *s, struct mach_operand dst,
                 struct mach_operand src);
    void (*load)(struct selector *s, struct mach_operand dst, uint64_t value);
    void (*print)(struct text *out, enum cpu_level cpu,
                  const struct ir_module *m, const struct mach_inst *inst,
                  const struct names *names);
    /* Addressing modes and stack parameters, chapters 14 and 15. */
    bool (*fits_address)(const struct selector *s, const struct address *a,
                         const struct ir_inst *use);
    void (*stack_param)(struct selector *s, int64_t offset,
                        struct mach_operand dst);
    /* The locations of count arguments of types for a call of callee, or
       of the parameters of callee. */
    void (*locate)(const struct selector *s, const struct ir_function *callee,
                   const enum ir_type *types, size_t count,
                   struct arg_location *out);
    void (*load_float)(struct selector *s, struct mach_operand dst,
                       enum ir_type type, double value);
    /* Aggregates, chapter 18. The result of f goes to a register or to the
       register parts of an aggregate. An indirect result goes to memory
       whose address arrives in reg and, with copy set, returns in copy. */
    void (*locate_result)(const struct selector *s,
                          const struct ir_function *f,
                          struct arg_location *out);
    void (*load_bytes)(struct selector *s, struct mach_operand dst,
                       struct mach_operand base, int64_t offset,
                       unsigned bytes);
    void (*store_bytes)(struct selector *s, struct mach_operand src,
                        struct mach_operand base, int64_t offset,
                        unsigned bytes);
    void (*slot_address)(struct selector *s, struct mach_operand dst,
                         uint32_t slot);
    void (*incoming_address)(struct selector *s, struct mach_operand dst,
                             int64_t offset);
    void (*copy_memory)(struct selector *s, struct mach_operand dst,
                        struct mach_operand src, uint64_t size);
    uint8_t fp_first;                   /* the first float register */
    uint64_t frame_limit;               /* the largest frame, below 2^32 */
    /* Whether the target selects the flag operation inst and reads its
       flags where the instruction leaves them. The back end expands every
       other one into plain operations. */
    bool (*flags_native)(const struct ir_inst *inst);
    /* Whether the target selects the simd operation inst on the simd
       struct agg of size bytes at level cpu. The back end expands every
       other one into an operation per lane. */
    bool (*vector_native)(const struct ir_inst *inst,
                          const struct ir_aggtype *agg, uint64_t size,
                          enum cpu_level cpu);
    /* Register allocation and frame layout, chapter 13. */
    void (*load_spill)(struct mach_block *b, uint8_t reg, int64_t offset);
    void (*store_spill)(struct mach_block *b, uint8_t reg, int64_t offset);
    void (*resolve_slot)(struct mach_operand *o, int64_t offset);
    /* Append inst after allocation, split where a frame offset exceeds an
       immediate of the target. NULL appends it unchanged. */
    void (*expand)(struct mach_block *b, const struct mach_inst *inst);
    void (*prologue)(struct mach_block *b, const struct frame *frame);
    void (*epilogue)(struct mach_block *b, const struct frame *frame);
};

const struct target_desc *target_desc_x86_64(void);
const struct target_desc *target_desc_arm64(void);
const struct target_desc *target_desc(enum target t);

#endif
