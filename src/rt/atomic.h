/* The atomic operations of the language. Every one is sequentially
   consistent, and the width of the value comes with its address. */

#ifndef ANTI_RT_ATOMIC_H
#define ANTI_RT_ATOMIC_H

#include <stdint.h>

/* A width of 1, 2 or 4 bytes reads and writes that many, and any other
   width reads and writes 8. A value is cut to the width before it is
   written, and a value read is sign-extended from it. */

/* The value at address. */
int64_t anti_rt_atomic_load(const void *address, int64_t width);

/* Writes value at address. */
void anti_rt_atomic_store(void *address, int64_t width, int64_t value);

/* Writes value at address and gives the value it replaced. */
int64_t anti_rt_atomic_swap(void *address, int64_t width, int64_t value);

/* Adds value to the value at address, wrapping at the width, and gives
   the value before the add. */
int64_t anti_rt_atomic_add(void *address, int64_t width, int64_t value);

/* Subtracts value from the value at address, wrapping at the width, and
   gives the value before the subtraction. */
int64_t anti_rt_atomic_sub(void *address, int64_t width, int64_t value);

/* The bitwise and of value into the value at address. Gives the value
   before it. */
int64_t anti_rt_atomic_and(void *address, int64_t width, int64_t value);

/* The bitwise or of value into the value at address. Gives the value
   before it. */
int64_t anti_rt_atomic_or(void *address, int64_t width, int64_t value);

/* True when the value at address was expected and is now desired. */
int8_t anti_rt_atomic_compare_swap(void *address, int64_t width,
                                   int64_t expected, int64_t desired);

#endif
