/* The snapshots of `snapshot fn` that live on the heap, which an `own fn`
   field or a `keep own` parameter owns. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "std.h"

/* DESIGN: a snapshot on the heap is one block that starts with its size
   in bytes, so freeing it and copying it need nothing of the closure
   that reads it. The runtime counts the blocks alive, which a leak check
   reads with anti_rt_snapshots_alive. */
static int64_t snapshots_alive;

void *anti_rt_snapshot_new(int64_t size)
{
    void *made = malloc((size_t)size);

    if (made == NULL) {
        anti_rt_fail_abort("out of memory for a snapshot of %lld bytes",
                           (long long)size);
    }
    memcpy(made, &size, sizeof size);
    __atomic_fetch_add(&snapshots_alive, 1, __ATOMIC_RELAXED);
    return made;
}

void anti_rt_snapshot_text(void *snapshot, int64_t at,
                           const unsigned char *bytes, int64_t length)
{
    if (length > 0) {
        memcpy((char *)snapshot + at, bytes, (size_t)length);
    }
}

void anti_rt_snapshot_free(void *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    __atomic_fetch_sub(&snapshots_alive, 1, __ATOMIC_RELAXED);
    free(snapshot);
}

void *anti_rt_snapshot_dup(const void *snapshot)
{
    int64_t size;
    void *made;

    if (snapshot == NULL) {
        return NULL;
    }
    memcpy(&size, snapshot, sizeof size);
    made = anti_rt_snapshot_new(size);
    memcpy(made, snapshot, (size_t)size);
    return made;
}

int64_t anti_rt_snapshots_alive(void)
{
    return __atomic_load_n(&snapshots_alive, __ATOMIC_RELAXED);
}
