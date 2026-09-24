/* The lock of a Mutex, the hidden lock of a synchronized object and the
   record of the orders in which a dev build takes them.

   DESIGN: a Mutex is one word of the program's own memory, and the
   operating system keeps nothing for it until a thread has to wait: a
   futex word on Linux, os_unfair_lock on macOS and SRWLOCK on Windows.
   Zero is the unlocked state of all three, so memory that nothing wrote
   holds an unlocked Mutex and `Mutex.new()` gives zero. The word is four
   bytes on Linux and macOS and eight on Windows, which the layout of the
   compiler gives per target. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <os/lock.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "std.h"

#if defined(_WIN32)

typedef SRWLOCK word_t;

static void word_lock(word_t *w) { AcquireSRWLockExclusive(w); }
static void word_unlock(word_t *w) { ReleaseSRWLockExclusive(w); }

#elif defined(__APPLE__)

typedef os_unfair_lock word_t;

static void word_lock(word_t *w) { os_unfair_lock_lock(w); }
static void word_unlock(word_t *w) { os_unfair_lock_unlock(w); }

#else

typedef uint32_t word_t;

/* The private operations of futex(2), which the headers of musl do not
   name. */
#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

/* DESIGN: the word is 0 when free, 1 when held and 2 when held with a
   thread waiting, the mutex of Drepper's "Futexes Are Tricky". An
   unlock that finds 2 wakes one waiter, and a free lock costs one
   compare-and-swap to take and one exchange to give back. */
static void word_lock(word_t *w)
{
    uint32_t c = 0;

    if (__atomic_compare_exchange_n(w, &c, 1, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2) {
        c = __atomic_exchange_n(w, 2, __ATOMIC_ACQUIRE);
    }
    while (c != 0) {
        syscall(SYS_futex, w, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
        c = __atomic_exchange_n(w, 2, __ATOMIC_ACQUIRE);
    }
}

static void word_unlock(word_t *w)
{
    if (__atomic_exchange_n(w, 0, __ATOMIC_RELEASE) == 2) {
        syscall(SYS_futex, w, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    }
}

#endif

/* The hidden lock of a synchronized object, which the compiler lays out
   as a Mutex and two `int` fields. */
struct object_lock {
    word_t word;
    int64_t owner;
    int64_t depth;
};

/* A value that differs per thread and is never zero: the address of a
   variable each thread has its own copy of. */
static int64_t thread_tag(void)
{
    static _Thread_local char tag;

    return (int64_t)(intptr_t)&tag;
}

/* The order of the locks, taken by a dev build */

/* DESIGN: a thread keeps the locks it holds on a stack of its own. Each
   time it takes one while holding others, the program records an order,
   the lock held and the lock taken, with the site of each, once per
   pair. An order whose reverse is recorded already is a conflict, which
   is reported once with the four sites, before the thread waits. The
   records stand in a table that one internal lock guards, open
   addressed by the pair of addresses. A lock deeper than HELD_MAX is
   taken without a record. */
#define HELD_MAX 32

struct held {
    const void *lock;
    const char *site;
};

static _Thread_local struct held held[HELD_MAX];
static _Thread_local int64_t held_count;

struct order {
    const void *first;
    const void *second;
    const char *first_site;
    const char *second_site;
    int reported;
};

/* A record that `destroy` cleared. A search goes on past it. */
#define GONE ((const void *)1)

static word_t orders_lock;
static struct order *orders;
static size_t order_count;
static size_t order_capacity;

static size_t order_slot(const void *first, const void *second)
{
    uint64_t h = (uint64_t)(uintptr_t)first * 0x9e3779b97f4a7c15ULL ^
                 (uint64_t)(uintptr_t)second;

    return (size_t)(h ^ (h >> 29)) & (order_capacity - 1);
}

static struct order *order_find(const void *first, const void *second)
{
    size_t i;

    if (order_capacity == 0) {
        return NULL;
    }
    for (i = order_slot(first, second); orders[i].first != NULL;
         i = (i + 1) & (order_capacity - 1)) {
        if (orders[i].first == first && orders[i].second == second) {
            return &orders[i];
        }
    }
    return NULL;
}

static void order_put(const struct order *o);

/* Double the table, which is kept at most half full. */
static void orders_grow(void)
{
    struct order *old = orders;
    size_t old_capacity = order_capacity;
    size_t capacity = old_capacity == 0 ? 64 : old_capacity * 2;
    size_t i;

    orders = calloc(capacity, sizeof *orders);
    if (orders == NULL) {
        anti_rt_fail_abort("out of memory for the order of the locks");
    }
    __atomic_store_n(&order_capacity, capacity, __ATOMIC_RELAXED);
    order_count = 0;
    for (i = 0; i < old_capacity; i++) {
        if (old[i].first != NULL && old[i].first != GONE) {
            order_put(&old[i]);
        }
    }
    free(old);
}

static void order_put(const struct order *o)
{
    size_t i;

    if ((order_count + 1) * 2 > order_capacity) {
        orders_grow();
    }
    for (i = order_slot(o->first, o->second); orders[i].first != NULL;
         i = (i + 1) & (order_capacity - 1)) {
    }
    orders[i] = *o;
    order_count++;
}

/* Record that the thread takes lock at site while it holds what its
   stack holds, and report an order that conflicts with one recorded. */
static void order_take(const void *lock, const char *site)
{
    int64_t i;
    int64_t count = held_count < HELD_MAX ? held_count : HELD_MAX;

    if (count > 0) {
        word_lock(&orders_lock);
        for (i = 0; i < count; i++) {
            const struct held *h = &held[i];
            struct order *reverse;
            struct order one;
            if (h->lock == lock) {
                continue;
            }
            reverse = order_find(lock, h->lock);
            if (reverse != NULL && !reverse->reported) {
                reverse->reported = 1;
                anti_rt_note("anti: two locks are taken in opposite orders "
                             "and can deadlock: %s takes one while holding "
                             "the one of %s, and %s takes that one while "
                             "holding the one of %s",
                             site, h->site, reverse->second_site,
                             reverse->first_site);
            }
            if (order_find(h->lock, lock) == NULL) {
                one.first = h->lock;
                one.second = lock;
                one.first_site = h->site;
                one.second_site = site;
                one.reported = 0;
                order_put(&one);
            }
        }
        word_unlock(&orders_lock);
    }
    if (held_count < HELD_MAX) {
        held[held_count].lock = lock;
        held[held_count].site = site;
    }
    held_count++;
}

/* Take lock off the thread's stack, the topmost entry that holds it. */
static void order_give(const void *lock)
{
    int64_t i;

    if (held_count == 0) {
        return;
    }
    if (held_count > HELD_MAX) {
        held_count--;
        return;
    }
    for (i = held_count - 1; i >= 0; i--) {
        if (held[i].lock == lock) {
            memmove(&held[i], &held[i + 1],
                    (size_t)(held_count - 1 - i) * sizeof held[0]);
            break;
        }
    }
    held_count--;
}

/* Forget every order that names lock, whose memory may hold another
   lock later. */
static void order_forget(const void *lock)
{
    size_t i;

    word_lock(&orders_lock);
    for (i = 0; i < order_capacity; i++) {
        if (orders[i].first == lock || orders[i].second == lock) {
            orders[i].first = GONE;
            orders[i].second = GONE;
        }
    }
    word_unlock(&orders_lock);
}

/* Mutex */

void anti_rt_mutex_lock(void *word) { word_lock(word); }

void anti_rt_mutex_unlock(void *word) { word_unlock(word); }

void anti_rt_mutex_lock_at(void *word, const char *site)
{
    order_take(word, site);
    word_lock(word);
}

void anti_rt_mutex_unlock_at(void *word)
{
    order_give(word);
    word_unlock(word);
}

/* `m.destroy()`. The word holds nothing of the system, so only the
   orders of a dev build are forgotten. */
void anti_rt_mutex_destroy(void *word)
{
    if (__atomic_load_n(&order_capacity, __ATOMIC_RELAXED) != 0) {
        order_forget(word);
    }
}

/* The hidden lock of a synchronized object */

/* DESIGN: a thread that holds the lock of an object takes it again
   without waiting and counts how deep it is. A public function that
   calls another on the same object, a closure it runs and `sync obj`
   around calls of the object all take it that way. The owner is read
   by other threads, so it is read and written atomically. */
static int object_enter(struct object_lock *l, int64_t me)
{
    if (__atomic_load_n(&l->owner, __ATOMIC_RELAXED) == me) {
        l->depth++;
        return 1;
    }
    return 0;
}

static void object_hold(struct object_lock *l, int64_t me)
{
    word_lock(&l->word);
    __atomic_store_n(&l->owner, me, __ATOMIC_RELAXED);
    l->depth = 1;
}

static int object_leave(struct object_lock *l)
{
    if (--l->depth > 0) {
        return 0;
    }
    __atomic_store_n(&l->owner, 0, __ATOMIC_RELAXED);
    word_unlock(&l->word);
    return 1;
}

void anti_rt_object_lock(void *lock)
{
    int64_t me = thread_tag();

    if (!object_enter(lock, me)) {
        object_hold(lock, me);
    }
}

void anti_rt_object_unlock(void *lock) { object_leave(lock); }

void anti_rt_object_lock_at(void *lock, const char *site)
{
    int64_t me = thread_tag();

    if (!object_enter(lock, me)) {
        order_take(lock, site);
        object_hold(lock, me);
    }
}

void anti_rt_object_unlock_at(void *lock)
{
    if (object_leave(lock)) {
        order_give(lock);
    }
}
