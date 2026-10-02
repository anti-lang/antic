#ifndef ANTIC_SELECT_H
#define ANTIC_SELECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "ir.h"
#include "layout.h"
#include "mach.h"
#include "target.h"
#include "text.h"

/* Instruction selection turns the IR of one function into machine code on
   virtual registers. The shared part walks the blocks and gives each IR
   temporary a virtual register. It finds the pattern of each instruction
   in the table of the target. */

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

struct selector {
    const struct target_desc *target;
    const struct abi *abi;
    enum convention convention;
    enum cpu_level cpu;                 /* the level the code runs at */
    const struct ir_module *m;
    struct layouts *layouts;            /* of every aggregate of m */
    const struct ir_function *f;
    struct mach_function *out;
    struct mach_block *b;
    size_t block;                       /* the index of the IR block */
    uint32_t *uses;                     /* uses of each IR temporary */
    uint32_t line;                      /* the line of the instruction */
    const struct ir_inst *fused;        /* a comparison for the branch */
    const struct ir_inst *overflow;     /* What a branchov reads. */
    const struct ir_inst *flags;        /* What a flag read reads. */
    struct address address;             /* for the next load or store */
    bool has_address;
    struct mach_operand result_address; /* of an aggregate result */
    char *error;
    size_t error_size;
    bool failed;
    /* The module is a plugin on ELF or COFF, whose externs lie in the
       host. */
    bool imports;
};

const struct target_desc *target_desc_x86_64(void);
const struct target_desc *target_desc_arm64(void);
const struct target_desc *target_desc(enum target t);

/* Select the machine code of every function with a body in m into out,
   one entry per IR function, NULL for an extern function. Selection first
   lays out the aggregates of m for the target and replaces its symbolic
   values with constants. cpu is the level of the code, which picks an
   instruction or a call of the runtime where the levels differ. Returns
   false and writes a message to error for a layout error or an
   instruction without a pattern. Each function of out is the caller's,
   who frees it with mach_function_free and then free. */
bool select_module(enum target t, enum cpu_level cpu, struct ir_module *m,
                   struct mach_function **out, char *error,
                   size_t error_size);

/* The layout of aggregate agg on the target, NULL for IR_NO_AGG. */
const struct layout *select_layout(const struct selector *s, uint32_t agg);
uint64_t select_size(const struct selector *s, struct ir_vtype v);
uint64_t select_align(const struct selector *s, struct ir_vtype v);

/* Helpers for the pattern tables of the targets. */
struct mach_operand select_result(struct selector *s, const struct ir_inst *inst);
struct mach_operand select_new_vreg(struct selector *s, uint8_t width);
struct mach_operand select_new_fp_vreg(struct selector *s, uint8_t width);
/* The immediate v of width w in bits as a signed value. A width other
   than 8, 16 or 32 reads all 64 bits. */
int64_t select_signed(uint64_t v, uint8_t w);
bool select_is_float(enum ir_type type);
/* Store the register parts of an aggregate into the memory at address. */
void select_store_parts(struct selector *s, const struct arg_location *loc,
                        struct mach_operand address);
/* Load the register parts of an aggregate from address. Returns the
   registers it loads as a set. */
uint64_t select_load_parts(struct selector *s, const struct arg_location *loc,
                           struct mach_operand address);
/* The function whose parameters and result a call follows: the callee of
   a direct call, or the signature of a call through a pointer. */
const struct ir_function *select_callee(const struct selector *s,
                                        const struct ir_inst *inst);
/* The facts of a call that both targets read: its callee, the type and
   the location of each argument, the copy of each aggregate passed in
   memory, and the location of its result with the memory of an indirect
   one. select_call_begin fills them and select_call_finish frees them. */
struct select_call {
    const struct ir_function *callee;
    enum ir_type *types;
    struct arg_location *locations;
    struct mach_operand *copies;        /* filled by the target */
    struct arg_location result;
    struct mach_operand result_address;
};
void select_call_begin(struct selector *s, const struct ir_inst *inst,
                       struct select_call *c);
/* The end of the stack area of aggregate argument i, which is on the
   stack: its pointer, or its bytes rounded up to 8. */
uint64_t select_call_stack_end(const struct selector *s,
                               const struct select_call *c, size_t i);
/* Load aggregate argument i of c, which goes in registers, and return
   the registers it loads. Returns 0 and loads nothing for any other
   argument. *done tells the two apart. */
uint64_t select_call_aggregate(struct selector *s, const struct ir_inst *inst,
                               const struct select_call *c, size_t i,
                               bool *done);
/* The register of the memory of an indirect result, loaded, or 0. */
uint64_t select_call_result_address(struct selector *s,
                                    const struct select_call *c);
/* After the call instruction: the result into its temporary or its
   memory. Then free what select_call_begin made. */
void select_call_finish(struct selector *s, const struct ir_inst *inst,
                        struct select_call *c);
/* DESIGN: a PIE on Linux and macOS reaches a C function in a shared
   library through its GOT entry. Windows links the C runtime statically,
   so its C functions lie in the executable. */
bool select_uses_got(const struct selector *s, const struct ir_operand *o);
/* An aggregate result of a call: memory in a new slot for its value. */
struct mach_operand select_result_slot(struct selector *s,
                                       const struct ir_inst *inst,
                                       const struct layout *agg);
/* DESIGN: select_reg emits a load for a constant, as the lowering emits
   IR from many of its helpers. A call passes at most one argument that
   emits code, because C leaves the order of the arguments unspecified.
   clang for Windows takes them right to left, and antic built there
   emitted another program. */
struct mach_operand select_reg(struct selector *s,
                               const struct ir_operand *o);
struct mach_inst *select_emit(struct selector *s, uint16_t op, size_t count,
                              const struct mach_operand *operands);
enum mach_cond select_cond(enum ir_op op);
enum mach_cond select_negate(enum mach_cond cond);
bool select_is_next(const struct selector *s, const struct ir_operand *block);
/* The bits of a value of type in a register or in memory. */
uint8_t select_bits(enum ir_type type);
/* Whether o is a float register, virtual or physical. */
bool select_is_float_register(const struct selector *s,
                              struct mach_operand o);
/* A jump with the instruction op to the IR block target, unless control
   reaches it as the next block. */
void select_jump(struct selector *s, const struct ir_operand *target,
                 uint16_t op);
/* The pattern of IR_MEMCOPY, through the copy_memory of the target. */
void select_emit_memcopy(struct selector *s, const struct ir_inst *inst);
/* The memory of a value of type at the address in base. */
struct mach_operand select_memory(struct mach_operand base,
                                  enum ir_type type);
/* The memory operand of a load or a store of type: the folded address
   that selection found, or the pointer register itself. */
struct mach_operand select_address_of(struct selector *s,
                                      const struct ir_operand *pointer,
                                      enum ir_type type);

#endif
