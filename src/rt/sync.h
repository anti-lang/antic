/* The calls of locking and channels that generated code makes: the
   Mutex of anti.lang, the hidden lock of a synchronized class, channels
   and `select`. src/rt/lock.c holds the locks and src/rt/sync.c the
   channels. RT_FUNCTIONS of src/antic/rt_abi.h lists the same functions,
   and the unit test runtime_functions compares the two. */
#ifndef ANTI_RT_SYNC_H
#define ANTI_RT_SYNC_H

#include <stdint.h>

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

/* `delete(c)`: free the channel and the values it still holds. */
void anti_rt_chan_delete(void *handle);

/* Wait until one of count channels holds a value or is closed, and give
   the index of its arm. got receives what `recv` of that channel would
   give: the slot of the arm, filled, or NULL. */
int64_t anti_rt_select(void *const *chans, void *const *slots,
                       int64_t count, void **got);

/* Take and give back the Mutex whose word is at word. The forms with
   `_at` are the ones of a dev build, which records the order the locks
   of a thread are taken in at the site, a text that names the place. */
void anti_rt_mutex_lock(void *word);
void anti_rt_mutex_unlock(void *word);
void anti_rt_mutex_lock_at(void *word, const char *site);
void anti_rt_mutex_unlock_at(void *word);

/* `m.destroy()`. The word holds nothing of the system, so only the
   orders of a dev build are forgotten. */
void anti_rt_mutex_destroy(void *word);

/* Take and give back the hidden lock of a synchronized class at lock,
   which the thread that holds it takes again without waiting. The `_at`
   forms are those of a dev build, as for a Mutex. */
void anti_rt_object_lock(void *lock);
void anti_rt_object_unlock(void *lock);
void anti_rt_object_lock_at(void *lock, const char *site);
void anti_rt_object_unlock_at(void *lock);

/* `sync a, b { }`: take the hidden locks of two objects in the order of
   their addresses, and give both back. The same object twice is one
   lock. */
void anti_rt_object_lock_pair(void *a, void *b);
void anti_rt_object_unlock_pair(void *a, void *b);
void anti_rt_object_lock_pair_at(void *a, void *b, const char *site);
void anti_rt_object_unlock_pair_at(void *a, void *b);

/* Whether a lies below b in memory. The standard library takes locks of
   its own in the order of their addresses with it, as `sync a, b` does,
   since Anti has no order of pointers. */
int anti_rt_address_below(const void *a, const void *b);

#endif
