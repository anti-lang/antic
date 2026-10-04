#ifndef ANTIC_ABI_H
#define ANTIC_ABI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ir.h"
#include "layout.h"
#include "target.h"

/* The classification of every parameter and result for the C convention
   of a target, in the terms of an LLVM signature. The section "Calling
   convention" of docs/work-order-llvm-back-end.md gives the rules. LLVM
   places what the classification leaves to it: the
   registers, the stack slots and the variadic rules of the triple. */

enum abi_kind {
    ABI_DIRECT,      /* a scalar in its own type */
    ABI_COERCE,      /* an aggregate as one or two integer or float words */
    ABI_BYVAL,       /* an aggregate copied to the stack by the caller */
    ABI_INDIRECT,    /* a pointer to a copy the caller makes */
    ABI_SRET,        /* a result written through a hidden first pointer */
    ABI_VECTOR       /* a simd struct of 16 bytes as its vector type */
};

/* DESIGN: types holds the LLVM type of each word the signature names: the
   scalar of ABI_DIRECT, the words of ABI_COERCE, the vector of ABI_VECTOR
   and ptr for the other three. A void result is ABI_DIRECT with no word.
   A coerced word may reach past the end of the aggregate, as the second
   i64 of a struct of 12 bytes does, so the translation copies through a
   slot of the words' size. size and align are those of the value on the
   target. */
struct abi_param {
    enum abi_kind kind;
    char types[2][16];      /* LLVM types of the coerced words */
    size_t word_count;
    uint64_t size;
    uint64_t align;
    bool sign_extend;       /* i8 and i16 parameters, signext or zeroext */
};

/* Classify the parameters of f into params, one per parameter, and its
   result into result, for the convention of target t. The layouts of the
   module of f on t give the size, the alignment and the members of each
   aggregate. A variadic f classifies its named parameters. */
void abi_classify(enum target t, struct layouts *l, const struct ir_function *f,
                  struct abi_param *params, struct abi_param *result);

#endif
