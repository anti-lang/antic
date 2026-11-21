#ifndef ANTIC_LAYOUT_H
#define ANTIC_LAYOUT_H

/* The back end lays out the aggregates of the IR for one target and folds
   its symbolic values. The rules of C on the target give each layout, and
   layout.c states them. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ir.h"
#include "target.h"

/* One scalar inside an aggregate, for the classification of its ABI. A
   simd struct of 16 bytes is one member, the vector type of C whose lanes
   have type type, since C passes a struct of such vectors by the rules of
   vectors, T373 of docs/audit/tests-findings.md. */
struct layout_member {
    uint64_t offset;
    enum ir_type type;
    bool vector;
};

/* Where a bitfield lies, and the integer that a load or store of it reads
   and writes. */
struct layout_bits {
    uint64_t pos;                   /* the first bit, from bit 0 of byte 0 */
    uint64_t unit_offset;           /* the byte the integer starts at */
    enum ir_type unit_type;
    uint8_t shift;                  /* the first bit inside the integer */
};

/* The layout of an aggregate on the target. */
struct layout {
    uint64_t size;
    uint64_t align;
    uint64_t *offsets;              /* the offset of each field */
    struct layout_bits *bits;       /* for each field, set for bitfields */
    struct layout_member *members;  /* recorded up to LAYOUT_MEMBER_LIMIT */
    size_t member_count;
    bool unaligned;                 /* a field lies off its alignment */
    bool shared;                    /* holds an atomic field or a Mutex */
    /* DESIGN: a simd struct of 16 bytes is the vector type of C. Every
       convention passes it in one vector register or as a vector. A
       simd struct of another size passes as the struct of its lanes. */
    bool vector;
};

/* DESIGN: the members of an aggregate are recorded up to 64 bytes, by 7
   of "Answers of 2026-11-21" in docs/decisions.md. AAPCS64 passes a
   homogeneous aggregate of up to four vectors of 16 bytes, 64 bytes, in
   vector registers, T375 of docs/audit/tests-findings.md, so members are
   kept up to that size. Every other aggregate above the size a
   convention takes in registers still passes in memory. */
enum { LAYOUT_MEMBER_LIMIT = 64 };

struct layouts {
    enum target target;
    const struct ir_module *m;
    struct layout *aggs;
    uint8_t *agg_state;
    uint64_t *values;
    uint8_t *sym_state;
    char *error;
    size_t error_size;
    bool failed;
};

/* Lay out every aggregate of m for target t. Returns false and writes a
   message to error for an array length below 1 or a layout that depends
   on itself. layout_free releases the tables in either case. The tables
   cover the aggregates and symbolic values that m holds at this call. */
bool layout_init(struct layouts *l, enum target t, const struct ir_module *m,
                 char *error, size_t error_size);

/* Release the tables of l. */
void layout_free(struct layouts *l);

/* The layout of aggregate agg, computed on its first use. */
const struct layout *layout_agg(struct layouts *l, uint32_t agg);

/* The size in bytes of a value of type v on the target. */
uint64_t layout_size(struct layouts *l, struct ir_vtype v);

/* The alignment in bytes of a value of type v on the target. */
uint64_t layout_align(struct layouts *l, struct ir_vtype v);

/* Fold symbolic value sym to its bits on the target. Returns false after
   a division by zero. */
bool layout_fold(struct layouts *l, uint32_t sym, uint64_t *out);

/* Replace every symbolic operand of function f of the module with a
   constant, and set resolved when there was one. c_long and c_wchar take
   the fixed type of the target, and an operation on c_wchar its signed
   form where wchar_t is signed. Returns false after a folding error. */
bool layout_resolve(struct layouts *l, struct ir_function *f, bool *resolved);

/* Write the bytes of every global that holds an aggregate constant, with
   its size and its alignment on the target. Returns false after a folding
   error. */
bool layout_data(struct layouts *l, struct ir_module *m);

#endif
