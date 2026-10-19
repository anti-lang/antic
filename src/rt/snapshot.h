/* The snapshots of `snapshot fn` on the heap, which src/rt/snapshot.c
   makes, copies and frees. */

#ifndef ANTI_RT_SNAPSHOT_H
#define ANTI_RT_SNAPSHOT_H

#include <stdint.h>

/* A snapshot on the heap of size bytes, the size in its first eight.
   It aborts when the memory is not there. The owner of the snapshot, the
   `own fn` that holds it, frees it with anti_rt_snapshot_free. */
void *anti_rt_snapshot_new(int64_t size);

/* Copy length bytes of a captured str into the snapshot at offset at,
   and a NUL after them. */
void anti_rt_snapshot_text(void *snapshot, int64_t at,
                           const unsigned char *bytes, int64_t length);

/* Free a snapshot. NULL, the snapshot of a function that captures
   nothing, frees nothing. */
void anti_rt_snapshot_free(void *snapshot);

/* A copy of a snapshot, byte for byte, or NULL for NULL. The copy is the
   caller's, and anti_rt_snapshot_free frees it. */
void *anti_rt_snapshot_dup(const void *snapshot);

/* The count of the snapshots on the heap that are not freed. */
int64_t anti_rt_snapshots_alive(void);

#endif
