/* flags.h, the C interface of com.example.flags, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef FLAGS_H
#define FLAGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/* The flags of one arithmetic operation. */
struct anti_Flags {
    bool overflow;
    bool carry;
    bool zero;
    bool negative;
};

/** A word of a sum and its flags. */
typedef struct FlagsStep {
    uint64_t sum;
    struct anti_Flags flags;
} FlagsStep;

/** The flags of a sum of two words and the carry of an earlier sum. */
struct anti_Flags flags_carry(uint64_t a, uint64_t b, struct anti_Flags f);
/** The sum of two words and the carry of an earlier sum, with its flags. */
FlagsStep flags_step(uint64_t a, uint64_t b, struct anti_Flags f);

#ifdef __cplusplus
}
#endif

#endif
