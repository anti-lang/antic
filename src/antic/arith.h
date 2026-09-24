#ifndef ANTIC_ARITH_H
#define ANTIC_ARITH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Integer arithmetic of n bits, n being 8, 16, 32 or 64, which the
   checker, the optimizer and the back end compute on constants. Each
   function reads the low n bits of its operands. It returns the n bits of
   the result, sign-extended to 64 when is_signed is set and zero-extended
   otherwise. */

/* The low n bits of v as a signed value of n bits, sign-extended to 64.
   C11 6.3.1.3 leaves the conversion of an unsigned value above the
   signed maximum to the implementation, so this computes the two's
   complement value instead of casting. */
int64_t arith_signed(uint64_t v, int n);

/* The shift of the signed value a right by k bits, 0 <= k < 64, with the
   sign copied into the vacated bits. C leaves >> of a negative value to
   the implementation. */
int64_t arith_shift_right(int64_t a, unsigned k);

/* d rounded to the nearest float. A value past the largest float gives
   the largest float or an infinity, as IEEE 754 rounding does. C11
   6.3.1.5 leaves a cast of such a value undefined. */
float arith_to_f32(double d);

/* The upper half of the full product of a and b, which has 2n bits. */
uint64_t arith_mul_high(uint64_t a, uint64_t b, int n, bool is_signed);

/* a op b for op '+', '-' or '*', clamped at the minimum or the maximum
   of n bits. */
uint64_t arith_saturate(char op, uint64_t a, uint64_t b, int n,
                        bool is_signed);

/* The value of the float literal of length bytes at bytes, which need not
   end in a NUL. It is an f32 when single is set and an f64 otherwise. Every
   digit and the exponent count, however long the literal is, so the checker
   and the back end read one value. */
double arith_float_literal(const char *bytes, size_t length, bool single);

#endif
