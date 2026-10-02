/* The growth of a block of the C library, which the runtime and antic
   share.

   DESIGN: one rule for every block that grows as it is appended to. The
   room doubles from a first size until it holds what is asked, and a size
   that no int64_t holds fails as memory that runs out does. What a
   failure means stays with each caller: the search of a pattern ends the
   program through the failure routine, the text builder keeps what it
   holds as "Standard library phase" in docs/decisions.md says, a listing
   of anti.fs fails with ENOMEM, the compile of a byte pattern fails, and
   the text of a trace is NULL, since the failure routine may be the one
   that builds it. */
#ifndef ANTI_RT_GROW_H
#define ANTI_RT_GROW_H

#include <stdint.h>
#include <stdlib.h>

/* The block at p, which malloc or realloc gave, or NULL, grown by realloc
   to hold used and then more bytes, with *room its size in bytes. The
   size doubles from *room, or from first when *room is 0, until it holds
   them. Gives p when *room holds them already. Gives NULL when no int64_t
   holds the sum or the memory runs out, and then the block at p and
   *room stay as they were. used, more and first are above or at 0, and
   first above it. */
static inline void *anti_rt_reserve(void *p, int64_t *room, int64_t used,
                                    int64_t more, int64_t first)
{
    int64_t need;
    int64_t size;
    void *grown;

    if (more > INT64_MAX - used) {
        return NULL;
    }
    need = used + more;
    if (need <= *room) {
        return p;
    }
    size = *room > 0 ? *room : first;
    while (size < need) {
        size = size > INT64_MAX / 2 ? need : size * 2;
    }
    grown = realloc(p, (size_t)size);
    if (grown == NULL) {
        return NULL;
    }
    *room = size;
    return grown;
}

#endif
