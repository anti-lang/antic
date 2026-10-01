/* The C side of anti.mem: memory from the C library at an alignment the
   caller names, and its release. Also the memory of the objects that
   lowering makes, for which out of memory is fatal.

   DESIGN: malloc guarantees the alignment of max_align_t alone, and
   Allocator.alloc takes any power of two. The platform layer gives
   memory at any of them, and Windows keeps that memory apart from the
   one of malloc. A pointer therefore goes back through anti_rt_mem_free
   and never through the language's own free. */
#include <stdlib.h>

#include "platform.h"
#include "std.h"

void *anti_rt_mem_alloc(int64_t size, int64_t align)
{
    if (size < 0 || align <= 0 || (align & (align - 1)) != 0) {
        return NULL;
    }
    /* A size of zero is one byte, so that every request that succeeds
       gives memory of its own. */
    if (size == 0) {
        size = 1;
    }
    return anti_rt_aligned_alloc((size_t)size, (size_t)align);
}

void anti_rt_mem_free(void *p)
{
    anti_rt_aligned_free(p);
}

/* DESIGN: `alloc T { }` and `alloc T(args)` give `*T`, and out of memory
   is fatal for them. The compiler calls malloc where the object is made
   and tests the result there, so a report of AddressSanitizer names the
   Anti function right under malloc. Only the failure comes here. */
_Noreturn void anti_rt_out_of_memory(int64_t size)
{
    anti_rt_fail_abort("anti: out of memory for %lld bytes", (long long)size);
}

/* A size of zero is one byte, as for anti_rt_mem_alloc, since realloc may
   free the memory and give NULL for it. */
void *anti_rt_grow(void *p, int64_t size)
{
    void *grown = size >= 0 ? realloc(p, size > 0 ? (size_t)size : 1) : NULL;

    if (grown == NULL) {
        anti_rt_out_of_memory(size);
    }
    return grown;
}
