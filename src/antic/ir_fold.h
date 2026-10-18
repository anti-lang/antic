#ifndef ANTIC_IR_FOLD_H
#define ANTIC_IR_FOLD_H

/* The interface of ir_fold.c: the folding of an integer operation on
   constants, and the widths it reads, which the optimizer and the layout
   share. */

#include <stdint.h>

#include "ir.h"

/* What folding an operation gives. */
enum ir_fold {
    IR_FOLDED,
    IR_FOLD_NOT_BINARY,          /* no integer operation of two operands */
    IR_FOLD_BY_ZERO,             /* a division or remainder by 0 */
    IR_FOLD_LEAST_BY_MINUS_ONE,  /* the least signed value by -1 */
    IR_FOLD_SHIFT_RANGE          /* a count at or above the width */
};

/* The width of the fixed-width integer type type: 8, 16, 32, or 64 for
   every other type. */
int ir_fold_bits(enum ir_type type);

/* The low bits of v that the integer type type holds. */
uint64_t ir_fold_trim(enum ir_type type, uint64_t v);

/* The low bits of v that the integer type type holds, as a signed value
   sign-extended to 64. */
int64_t ir_fold_signed(enum ir_type type, uint64_t v);

/* DESIGN: one rule folds an integer operation of the IR. The optimizer
   folds the operations of a function and the layout folds the symbolic
   values of a target, and both call this, so a constant has one value
   whichever of them computes it. The three outcomes that are not
   IR_FOLDED have no value: C leaves each undefined and the two CPUs give
   different results. The optimizer then leaves the operation to run, and
   the layout refuses it, as the checker refuses it in source.

   a and b are operands of the fixed-width integer type type. The shift
   count b is read whole, so a count of any width compares with the width
   of type, and a negative one compares as a large one. out receives the
   bits of type, or 0 or 1 for a comparison. */
enum ir_fold ir_fold_int(enum ir_op op, enum ir_type type, uint64_t a,
                         uint64_t b, uint64_t *out);

#endif
