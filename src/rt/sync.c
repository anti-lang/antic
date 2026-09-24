/* The channels of `chan T`. antic lowers each operation to one call here
   with the handle that the channel holds, the object that
   anti_rt_chan_new made. src/rt/lock.c holds the Mutex.

   DESIGN: a channel stands on the platform layer of the threading
   chapter. That is the one src/rt/threads.c uses, a lock and a condition
   variable of the system. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "atomic.h"
#include "std.h"

#if defined(_WIN32)

typedef SRWLOCK lock_t;
typedef CONDITION_VARIABLE cond_t;
#define LOCK_INIT SRWLOCK_INIT
#define COND_INIT CONDITION_VARIABLE_INIT

static int lock_init(lock_t *l)
{
    InitializeSRWLock(l);
    return 0;
}
static void lock_end(lock_t *l) { (void)l; }
static void hold(lock_t *l) { AcquireSRWLockExclusive(l); }
static void release(lock_t *l) { ReleaseSRWLockExclusive(l); }
static int cond_init(cond_t *c)
{
    InitializeConditionVariable(c);
    return 0;
}
static void cond_end(cond_t *c) { (void)c; }
static void wait_on(cond_t *c, lock_t *l)
{
    SleepConditionVariableSRW(c, l, INFINITE, 0);
}
static void wake_one(cond_t *c) { WakeConditionVariable(c); }
static void wake_all(cond_t *c) { WakeAllConditionVariable(c); }

#else

typedef pthread_mutex_t lock_t;
typedef pthread_cond_t cond_t;
#define LOCK_INIT PTHREAD_MUTEX_INITIALIZER
#define COND_INIT PTHREAD_COND_INITIALIZER

static int lock_init(lock_t *l) { return pthread_mutex_init(l, NULL); }
static void lock_end(lock_t *l) { pthread_mutex_destroy(l); }
static void hold(lock_t *l) { pthread_mutex_lock(l); }
static void release(lock_t *l) { pthread_mutex_unlock(l); }
static int cond_init(cond_t *c) { return pthread_cond_init(c, NULL); }
static void cond_end(cond_t *c) { pthread_cond_destroy(c); }
static void wait_on(cond_t *c, lock_t *l) { pthread_cond_wait(c, l); }
static void wake_one(cond_t *c) { pthread_cond_signal(c); }
static void wake_all(cond_t *c) { pthread_cond_broadcast(c); }

#endif

/* DESIGN: the language has no way to report these, and a program that
   reaches one cannot go on, as src/rt/threads.c decides for memory. */
static _Noreturn void fatal(const char *message)
{
    anti_rt_fail_abort("anti: %s", message);
}

/* Channels */

/* DESIGN: a channel is a ring of capacity values of size bytes each,
   behind one lock. A receiver waits on not_empty and a sender on
   not_full. `close` wakes both, so a receiver takes what is left and
   then gets `none`. The values follow the struct in the same block. */
struct channel {
    lock_t lock;
    cond_t not_empty;
    cond_t not_full;
    int64_t size;
    int64_t capacity;
    int64_t head;               /* the oldest value */
    int64_t count;
    int closed;
    unsigned char *values;
};

/* DESIGN: a `select` that finds no channel ready waits on one condition
   of the whole program, which `send` and `close` wake. waiting counts
   the selects that wait, so a `send` with no select to wake takes no
   lock beside its channel's. A select counts itself before it looks at
   its channels, and a send looks at the count after it put its value, so
   one of the two sees the other. */
static lock_t select_lock = LOCK_INIT;
static cond_t select_ready = COND_INIT;
static int64_t waiting;

/* The arm where the next select of this thread starts to look. It moves
   on each time, so a channel that is always ready, a closed one among
   them, does not hide the others. */
static _Thread_local uint32_t turn;

static void wake_selects(void)
{
    if (anti_rt_atomic_load(&waiting, 8) > 0) {
        hold(&select_lock);
        wake_all(&select_ready);
        release(&select_lock);
    }
}

static struct channel *channel_of(void *handle, const char *what)
{
    if (handle == NULL) {
        fatal(what);
    }
    return handle;
}

/* A new channel of capacity values of size bytes each, on the heap.
   anti_rt_chan_delete frees it, which `delete(c)` calls. */
void *anti_rt_chan_new(int64_t size, int64_t capacity)
{
    struct channel *ch = NULL;

    if (capacity < 1) {
        anti_rt_fail_abort("anti: a channel holds one value or more, and "
                           "this one was made for %lld",
                           (long long)capacity);
    }
    if (size >= 0 && (size == 0 || capacity <= (INT64_MAX / 2) / size)) {
        ch = malloc(sizeof *ch + (size_t)(size * capacity));
    }
    if (ch == NULL) {
        anti_rt_fail_abort("anti: no memory for a channel of %lld values",
                           (long long)capacity);
    }
    if (lock_init(&ch->lock) != 0 || cond_init(&ch->not_empty) != 0 ||
        cond_init(&ch->not_full) != 0) {
        fatal("no mutex for a channel");
    }
    ch->size = size;
    ch->capacity = capacity;
    ch->head = 0;
    ch->count = 0;
    ch->closed = 0;
    ch->values = (unsigned char *)(ch + 1);
    return ch;
}

/* Take the oldest value into out. The caller holds the lock and has
   seen a value. */
static void take(struct channel *ch, void *out)
{
    memcpy(out, ch->values + ch->head * ch->size, (size_t)ch->size);
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    wake_one(&ch->not_full);
}

/* DESIGN: a value sent after `close` would never be received. The send
   stops the program there, where a send on a full channel would
   otherwise wait for ever. */
void anti_rt_chan_send(void *handle, const void *value)
{
    struct channel *ch = channel_of(handle, "`send` on a channel that "
                                            "holds no queue");
    int64_t at;

    hold(&ch->lock);
    while (ch->count == ch->capacity && !ch->closed) {
        wait_on(&ch->not_full, &ch->lock);
    }
    if (ch->closed) {
        release(&ch->lock);
        fatal("`send` on a closed channel");
    }
    at = (ch->head + ch->count) % ch->capacity;
    memcpy(ch->values + at * ch->size, value, (size_t)ch->size);
    ch->count++;
    wake_one(&ch->not_empty);
    release(&ch->lock);
    wake_selects();
}

/* Wait for a value and write it to out, which the result then names.
   The result is NULL once the channel is closed and empty. */
void *anti_rt_chan_recv(void *handle, void *out)
{
    struct channel *ch = channel_of(handle, "`recv` on a channel that "
                                            "holds no queue");
    void *got = NULL;

    hold(&ch->lock);
    while (ch->count == 0 && !ch->closed) {
        wait_on(&ch->not_empty, &ch->lock);
    }
    if (ch->count > 0) {
        take(ch, out);
        got = out;
    }
    release(&ch->lock);
    return got;
}

/* A second `close` changes nothing. */
void anti_rt_chan_close(void *handle)
{
    struct channel *ch = channel_of(handle, "`close` on a channel that "
                                            "holds no queue");

    hold(&ch->lock);
    ch->closed = 1;
    wake_all(&ch->not_empty);
    wake_all(&ch->not_full);
    release(&ch->lock);
    wake_selects();
}

void anti_rt_chan_delete(void *handle)
{
    struct channel *ch = handle;

    if (ch == NULL) {
        return;
    }
    cond_end(&ch->not_full);
    cond_end(&ch->not_empty);
    lock_end(&ch->lock);
    free(ch);
}

/* Look at channel index of chans once. A value goes into its slot and
   got names the slot, a closed and empty channel gives NULL in got, and
   either one returns 1. An empty open channel returns 0. */
static int try_arm(void *const *chans, void *const *slots, int64_t index,
                   void **got)
{
    struct channel *ch = channel_of(chans[index], "`select` on a channel "
                                                  "that holds no queue");
    int ready = 1;

    hold(&ch->lock);
    if (ch->count > 0) {
        take(ch, slots[index]);
        *got = slots[index];
    } else if (ch->closed) {
        *got = NULL;
    } else {
        ready = 0;
    }
    release(&ch->lock);
    return ready;
}

/* Whether any channel of chans holds a value or is closed. */
static int any_ready(void *const *chans, int64_t count)
{
    int64_t i;
    int ready = 0;

    for (i = 0; i < count && !ready; i++) {
        struct channel *ch = chans[i];
        hold(&ch->lock);
        ready = ch->count > 0 || ch->closed;
        release(&ch->lock);
    }
    return ready;
}

/* Wait until one of count channels holds a value or is closed, and give
   the index of its arm. got receives what `recv` of that channel would
   give: the slot of the arm, filled, or NULL. */
int64_t anti_rt_select(void *const *chans, void *const *slots,
                       int64_t count, void **got)
{
    for (;;) {
        int64_t start = count > 0 ? (int64_t)(turn++ % (uint64_t)count) : 0;
        int64_t k;

        for (k = 0; k < count; k++) {
            int64_t index = (start + k) % count;
            if (try_arm(chans, slots, index, got)) {
                return index;
            }
        }
        anti_rt_atomic_add(&waiting, 8, 1);
        hold(&select_lock);
        if (!any_ready(chans, count)) {
            wait_on(&select_ready, &select_lock);
        }
        release(&select_lock);
        anti_rt_atomic_sub(&waiting, 8, 1);
    }
}
