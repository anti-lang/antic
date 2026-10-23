#ifndef ANTIC_ALLOC_H
#define ANTIC_ALLOC_H

/* The interface of alloc.c: the checked allocation of antic, which ends
   the run on a failure or an overflow. */

#include <stddef.h>

/* DESIGN: the one owner of the heap memory of antic. Every part of the
   compiler, the front end, the IR, the back ends and the driver, takes
   memory from these functions, and no other file calls malloc, calloc or
   realloc. The compiler cannot continue after an allocation fails, so
   these functions then write "antic: out of memory" and end the run with
   status 70, and never return NULL. A size whose sum or product overflows
   size_t fails the same way. The test one_allocator holds the rule. */

/* Write the message of a failed allocation and end the run with status
   70. */
_Noreturn void alloc_out_of_memory(void);

/* The same for a failed allocation of size bytes in a file of the
   runtime that antic builds in, whose message names them. */
_Noreturn void alloc_out_of_memory_for(long long size);

/* Return a * b, and end the run as an allocation failure does when the
   product overflows size_t. */
size_t alloc_product(size_t a, size_t b);

/* Return a + b, and end the run as an allocation failure does when the
   sum overflows size_t. */
size_t alloc_sum(size_t a, size_t b);

/* Return zeroed memory for count items of size bytes, or for one item when
   count is 0, so the result is never NULL. The caller frees it with
   free. */
void *alloc_zeroed(size_t count, size_t size);

/* Resize items to count items of size bytes, as realloc does. A count of
   0 keeps one byte, so the result is never NULL. The caller frees the
   result with free. */
void *alloc_resize(void *items, size_t count, size_t size);

/* Return items with room for at least count + 1 items. *capacity starts
   at 8 and doubles until it passes count. The caller frees the result
   with free. */
void *alloc_grow(void *items, size_t *capacity, size_t count, size_t size);

#endif
