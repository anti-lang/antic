#ifndef ANTIC_LLVM_EMIT_PARTS_H
#define ANTIC_LLVM_EMIT_PARTS_H

/* The inside of the translation into LLVM IR text, which its files
   share: struct emitter and the functions of more than one file.
   llvm_emit.c writes the module, its globals, declarations and metadata,
   and the small texts beside it. llvm_emit_function.c writes the text of
   a function and each instruction. llvm_emit_call.c holds the C
   convention: the parameters, the prologue, the calls and the signature
   of a function. llvm_emit_facts.c collects the facts for opt, the ranges,
   the !tbaa tree, the cold branches and the frames a trace keeps. No other
   file includes this one. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "abi.h"
#include "ir.h"
#include "layout.h"
#include "llvm_debug.h"
#include "llvm_emit.h"
#include "rt_abi.h"
#include "text.h"

/* The classification of the parameters and the result of a function. */
struct classified {
    struct abi_param *params;       /* one per parameter */
    struct abi_param result;
};

/* The range of a load, as the operands of its IR_LOAD give it. */
struct load_range {
    enum ir_type type;
    uint64_t low;
    uint64_t high;
};

/* A field of a struct that an access names as its type. */
struct tbaa_field {
    uint32_t agg;
    uint32_t member;
    uint32_t node;                  /* the index of agg in tbaa_aggs */
};

/* The metadata of a branch into a cold block, one id for each side. */
enum { COLD_THEN, COLD_ELSE, COLD_COUNT };

struct emitter {
    const struct llvm_emit_options *o;
    struct layouts *l;
    struct text *out;
    char *error;
    size_t error_size;
    bool failed;
    const struct ir_function *f;
    uint32_t value;                 /* the next %v<n> of the function */
    uint32_t cold_id;               /* the metadata id of COLD_THEN */
    bool cold_used[COLD_COUNT];
    /* The id of the empty node that !invariant.group and !invariant.load
       name, after the ids of the cold branches. It is there where the
       module carries the table facts and an access names one. */
    uint32_t facts_id;
    bool facts;
    struct text intrinsics;         /* the declarations of the intrinsics */
    struct text entry;              /* the type of main, for its alias */
    const struct ir_module *m;
    struct text *allocas;           /* the allocas of the entry block */
    uint32_t slots;                 /* the next %a<n> of the function */
    struct classified signature;    /* of the function */
    /* The four i1 values of the last flag operation, in the order of
       enum ir_flag, each a value or a constant, and its result
       temporary. */
    char flags[4][24];
    uint32_t flag_result;
    /* The i1 of the last addov, subov or mulov, and its result
       temporary, which the branchov right after it reads. */
    uint32_t overflow;
    uint32_t overflow_result;
    struct llvm_debug debug;        /* the metadata of -g */
    /* The places of a COFF plugin that hold an __imp_ entry, as the
       entries of anti_rt_imports, and the __imp_ entries they name, one
       per line. */
    struct text places;
    size_t place_count;
    struct text imports;
    /* Per function, whether its frame stays, see llvm_emit_frames_kept. */
    bool *keeps_frame;
    /* The ranges the loads of the module read, each once, with the
       metadata id of the first. */
    struct load_range *ranges;
    size_t range_count;
    uint32_t range_id;
    /* The types of "Aliasing of views" the accesses of the module name:
       whether any does, the id of the root, and each struct and each field
       of one in the order of its first access. */
    bool tbaa;
    uint32_t tbaa_id;
    uint32_t *tbaa_aggs;
    size_t tbaa_agg_count;
    struct tbaa_field *tbaa_fields;
    size_t tbaa_field_count;
};

/* llvm_emit.c */
void llvm_emit_function_name(struct text *out, enum target t,
                             const struct ir_function *f);
void llvm_emit_global_symbol(struct text *out, enum target t,
                             const struct ir_global *g);
bool llvm_emit_imports(const struct emitter *e);

/* llvm_emit_function.c */
const char *llvm_emit_value_type(struct emitter *e, enum ir_type type);
uint64_t llvm_emit_align_of(struct emitter *e, enum ir_type type);
void llvm_emit_int_constant(struct text *out, enum ir_type type,
                            uint64_t value);
void llvm_emit_operand(struct emitter *e, const struct ir_operand *o,
                       enum ir_type type, struct text *value);
void llvm_emit_store_result(struct emitter *e, const struct ir_inst *inst,
                            const char *value);
uint32_t llvm_emit_fresh(struct emitter *e);
void llvm_emit_effects(struct text *out, enum ir_effects kind,
                       uint8_t guarantees);
void llvm_emit_runtime_function(struct emitter *e, const char *result,
                                enum rt_function f, const char *parameters);
bool llvm_emit_f16_calls(const struct emitter *e);
void llvm_emit_copy_bytes(struct emitter *e, const char *dst, const char *src,
                          uint64_t size);
bool llvm_emit_is_main(const struct emitter *e, const struct ir_function *f);
void llvm_emit_definition(struct emitter *e, const struct ir_function *f);

/* llvm_emit_call.c */
void llvm_emit_classify(struct emitter *e, const struct ir_function *f,
                        struct classified *c);
void llvm_emit_prologue(struct emitter *e, const struct ir_function *f,
                        const struct classified *c);
void llvm_emit_return_aggregate(struct emitter *e, const struct ir_operand *a);
void llvm_emit_call(struct emitter *e, const struct ir_inst *inst);
bool llvm_emit_signature(struct emitter *e, const struct ir_function *f,
                         bool names, struct text *type);
bool llvm_emit_link_once(const struct emitter *e, const struct ir_function *f);
bool llvm_emit_local_export(const struct emitter *e,
                            const struct ir_function *f);
void llvm_emit_linkage(struct emitter *e, const struct ir_function *f);

/* llvm_emit_facts.c */
void llvm_emit_access_fact(struct emitter *e, const struct ir_inst *inst);
void llvm_emit_range_fact(struct emitter *e, const struct ir_inst *inst);
void llvm_emit_collect_ranges(struct emitter *e, const struct ir_module *m);
void llvm_emit_collect_tbaa(struct emitter *e, const struct ir_module *m);
uint32_t llvm_emit_tbaa_count(const struct emitter *e);
void llvm_emit_tbaa_fact(struct emitter *e, const struct ir_inst *inst);
void llvm_emit_tbaa_nodes(struct emitter *e);
bool llvm_emit_names_table(const struct ir_module *m);
void llvm_emit_branch_weights(struct emitter *e, uint32_t then_block,
                              uint32_t else_block);
void llvm_emit_frames_kept(const struct ir_module *m, bool *kept);

#endif
