/* The C side of anti.mem: memory from the C library at an alignment the
   caller names, and its release.

   DESIGN: malloc guarantees the alignment of max_align_t alone, and
   Allocator.alloc takes any power of two. posix_memalign gives any of
   them, and free releases what it gave. The Universal C Runtime has no
   aligned_alloc, so Windows takes _aligned_malloc, whose memory only
   _aligned_free releases. A pointer therefore goes back through
   anti_rt_mem_free and never through the language's own free. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdlib.h>

#if defined(_WIN32)
#include <malloc.h>
#endif

#include "std.h"

void *anti_rt_mem_alloc(int64_t size, int64_t align)
{
    if (size < 0 || align <= 0 || (align & (align - 1)) != 0) {
        return NULL;
    }
    if ((uint64_t)size > SIZE_MAX || (uint64_t)align > SIZE_MAX) {
        return NULL;
    }
    /* A size of zero is one byte, so that every request that succeeds
       gives memory of its own. */
    if (size == 0) {
        size = 1;
    }
#if defined(_WIN32)
    return _aligned_malloc((size_t)size, (size_t)align);
#else
    /* posix_memalign takes a multiple of the size of a pointer. */
    if ((size_t)align < sizeof(void *)) {
        align = (int64_t)sizeof(void *);
    }
    void *p = NULL;
    if (posix_memalign(&p, (size_t)align, (size_t)size) != 0) {
        return NULL;
    }
    return p;
#endif
}

void anti_rt_mem_free(void *p)
{
#if defined(_WIN32)
    _aligned_free(p);
#else
    free(p);
#endif
}
