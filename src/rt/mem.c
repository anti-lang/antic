/* The C side of anti.mem: memory from the C library at an alignment the
   caller names, and its release. Also the memory of the objects that
   lowering makes, for which out of memory is fatal.

   DESIGN: malloc guarantees the alignment of max_align_t alone, and
   Allocator.alloc takes any power of two. The platform layer gives
   memory at any of them, and Windows keeps that memory apart from the
   one of malloc. A pointer therefore goes back through anti_rt_mem_free
   and never through the language's own free. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* DESIGN: `alloc(T, n)` gives the memory of malloc, which aligns to 16.
   A type that `align(N)` raises above that takes its elements here, by
   R050 of "Rules of 2026-10-13". The program releases the memory with
   `free`, as it releases every other memory of `alloc(T, n)`, so it comes
   from the allocation of the platform that `free` takes back and not from
   anti_rt_aligned_alloc. A count below zero and a product that leaves the
   address space give NULL, as malloc gives for a size it cannot serve. A
   request of no bytes is one element of alignment, so it gives memory of
   its own. */
void *anti_rt_alloc_aligned(int64_t count, int64_t size, int64_t align,
                            int64_t zeroed)
{
    size_t bytes;
    void *p;

    if (count < 0 || size < 0 || align <= 0 || (align & (align - 1)) != 0 ||
        (size != 0 && (uint64_t)count > SIZE_MAX / (uint64_t)size)) {
        return NULL;
    }
    bytes = (size_t)count * (size_t)size;
    if (bytes == 0) {
        bytes = (size_t)align;
    }
    p = anti_rt_malloc_aligned(bytes, (size_t)align);
    if (p != NULL && zeroed != 0) {
        memset(p, 0, bytes);
    }
    return p;
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
