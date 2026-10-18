/* The calls of locking and channels that generated code makes: the
   Mutex of anti.lang, the hidden lock of a synchronized class, channels
   and `select`. src/rt/lock.c holds the locks and src/rt/sync.c the
   channels. RT_FUNCTIONS of src/antic/rt_abi.h lists the same functions,
   and the unit test runtime_functions compares the two. */

#ifndef ANTI_RT_SYNC_H
#define ANTI_RT_SYNC_H

#include <stdint.h>

#include "platform.h"

/* A new channel of capacity values of size bytes each, on the heap.
   anti_rt_chan_delete frees it, which `delete(c)` calls. */
void *anti_rt_chan_new(int64_t size, int64_t capacity);

/* Send the value at value, waiting while the channel is full. A send
   after `close` stops the program. */
void anti_rt_chan_send(void *handle, const void *value);

/* Wait for a value and write it to out, which the result then names.
   The result is NULL once the channel is closed and empty. */
void *anti_rt_chan_recv(void *handle, void *out);

/* `close(c)`. A second `close` changes nothing. */
void anti_rt_chan_close(void *handle);

/* Take the oldest value the channel holds into out, which the result
   then names, without waiting. The result is NULL when it holds none.
   `delete(c)` of a channel of owning values calls it until it does, and
   tears each value down. */
void *anti_rt_chan_held(void *handle, void *out);

/* `delete(c)`: free the channel and the memory of its queue. */
void anti_rt_chan_delete(void *handle);

/* Wait until one of count channels holds a value or is closed, and give
   the index of its arm. got receives what `recv` of that channel would
   give: the slot of the arm, filled, or NULL. */
int64_t anti_rt_select(void *const *chans, void *const *slots,
                       int64_t count, void **got);

/* The forms with `_at` below are the ones of a dev build. Each records
   the order in which the locks of a thread are taken at the site, a
   text that names the place, and writes one note to standard error when
   two locks are taken in opposite orders. */

/* Take the Mutex whose word is at word, and wait while another thread
   holds it. A thread that holds it already waits for itself. */
void anti_rt_mutex_lock(void *word);

/* Give back the Mutex whose word is at word, which the thread holds. */
void anti_rt_mutex_unlock(void *word);

/* anti_rt_mutex_lock of a dev build, which ends the program with the
   two sites when the thread holds the Mutex already. */
void anti_rt_mutex_lock_at(void *word, const char *site);

/* anti_rt_mutex_unlock of a dev build, which takes the Mutex off the
   record of the thread. */
void anti_rt_mutex_unlock_at(void *word);

/* `m.destroy()`. The word holds nothing of the system, so only the
   orders of a dev build are forgotten. */
void anti_rt_mutex_destroy(void *word);

/* The hidden lock of a synchronized object, which the compiler lays out
   as a Mutex and two `int` fields, the thread that holds it and how
   often. The unit test object_lock_layout compares the two. */
struct anti_object_lock {
    struct anti_rt_word word;
    int64_t owner;
    int64_t depth;
};

/* Take the hidden lock of a synchronized class at lock, and wait while
   another thread holds it. The thread that holds it takes it again
   without waiting and counts how deep. */
void anti_rt_object_lock(void *lock);

/* Give back one take of the hidden lock at lock, and free it for other
   threads after the last. */
void anti_rt_object_unlock(void *lock);

/* anti_rt_object_lock of a dev build, which records the order at the
   first take of the thread alone. */
void anti_rt_object_lock_at(void *lock, const char *site);

/* anti_rt_object_unlock of a dev build, which takes the lock off the
   record of the thread at the last give. */
void anti_rt_object_unlock_at(void *lock);

/* `sync a, b { }`: take the hidden locks of the objects a and b in the
   order of their addresses. The same object twice is one lock, taken
   once. */
void anti_rt_object_lock_pair(void *a, void *b);

/* Give back the two hidden locks that anti_rt_object_lock_pair took
   for a and b, or the one for the same object twice. */
void anti_rt_object_unlock_pair(void *a, void *b);

/* anti_rt_object_lock_pair of a dev build, through
   anti_rt_object_lock_at. */
void anti_rt_object_lock_pair_at(void *a, void *b, const char *site);

/* anti_rt_object_unlock_pair of a dev build, through
   anti_rt_object_unlock_at. */
void anti_rt_object_unlock_pair_at(void *a, void *b);

/* Whether a lies below b in memory. The standard library takes locks of
   its own in the order of their addresses with it, as `sync a, b` does,
   since Anti has no order of pointers. */
int anti_rt_address_below(const void *a, const void *b);

#endif
