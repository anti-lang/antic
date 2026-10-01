/* The lock of a Mutex, the hidden lock of a synchronized object and the
   record of the orders in which a dev build takes them.

   DESIGN: a Mutex is one word of the program's own memory, the word of
   the platform layer, and `Mutex.new()` gives zero, its unlocked state.
   See struct anti_rt_word in platform.h. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atomic.h"
#include "platform.h"
#include "std.h"
#include "sync.h"

/* The hidden lock of a synchronized object, which the compiler lays out
   as a Mutex and two `int` fields. */
struct object_lock {
    struct anti_rt_word word;
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
   records stand in a table that ANTI_RT_LOCK_ORDERS guards, open
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
    anti_rt_atomic_store(&order_capacity, (int64_t)sizeof order_capacity,
                         (int64_t)capacity);
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
        anti_rt_lock_hold(ANTI_RT_LOCK_ORDERS);
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
        anti_rt_lock_release(ANTI_RT_LOCK_ORDERS);
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

    anti_rt_lock_hold(ANTI_RT_LOCK_ORDERS);
    for (i = 0; i < order_capacity; i++) {
        if (orders[i].first == lock || orders[i].second == lock) {
            orders[i].first = GONE;
            orders[i].second = GONE;
        }
    }
    anti_rt_lock_release(ANTI_RT_LOCK_ORDERS);
}

/* Mutex */

void anti_rt_mutex_lock(void *word) { anti_rt_word_lock(word); }

void anti_rt_mutex_unlock(void *word) { anti_rt_word_unlock(word); }

void anti_rt_mutex_lock_at(void *word, const char *site)
{
    order_take(word, site);
    anti_rt_word_lock(word);
}

void anti_rt_mutex_unlock_at(void *word)
{
    order_give(word);
    anti_rt_word_unlock(word);
}

/* `m.destroy()`. The word holds nothing of the system, so only the
   orders of a dev build are forgotten. */
void anti_rt_mutex_destroy(void *word)
{
    if (anti_rt_atomic_load(&order_capacity,
                            (int64_t)sizeof order_capacity) != 0) {
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
    if (anti_rt_atomic_load(&l->owner, (int64_t)sizeof l->owner) == me) {
        l->depth++;
        return 1;
    }
    return 0;
}

static void object_hold(struct object_lock *l, int64_t me)
{
    anti_rt_word_lock(&l->word);
    anti_rt_atomic_store(&l->owner, (int64_t)sizeof l->owner, me);
    l->depth = 1;
}

static int object_leave(struct object_lock *l)
{
    if (--l->depth > 0) {
        return 0;
    }
    anti_rt_atomic_store(&l->owner, (int64_t)sizeof l->owner, 0);
    anti_rt_word_unlock(&l->word);
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

/* DESIGN: `sync a, b { }` takes the hidden locks of two objects in the
   order of their addresses, so two threads that name the same two objects
   in opposite orders take them in one order and never wait for each other
   in a cycle. The same object twice is one lock, taken once. A dev build
   records the order it takes them in, which is the same for every pair of
   the two, so the check of the orders accepts it. */
void anti_rt_object_lock_pair(void *a, void *b)
{
    void *first = (uintptr_t)a <= (uintptr_t)b ? a : b;
    void *second = first == a ? b : a;

    anti_rt_object_lock(first);
    if (second != first) {
        anti_rt_object_lock(second);
    }
}

void anti_rt_object_unlock_pair(void *a, void *b)
{
    if (a != b) {
        anti_rt_object_unlock(b);
    }
    anti_rt_object_unlock(a);
}

void anti_rt_object_lock_pair_at(void *a, void *b, const char *site)
{
    void *first = (uintptr_t)a <= (uintptr_t)b ? a : b;
    void *second = first == a ? b : a;

    anti_rt_object_lock_at(first, site);
    if (second != first) {
        anti_rt_object_lock_at(second, site);
    }
}

void anti_rt_object_unlock_pair_at(void *a, void *b)
{
    if (a != b) {
        anti_rt_object_unlock_at(b);
    }
    anti_rt_object_unlock_at(a);
}

/* Whether a lies below b in memory. The standard library takes locks of
   its own in the order of their addresses with it, as `sync a, b` does,
   since Anti has no order of pointers. */
int anti_rt_address_below(const void *a, const void *b)
{
    return (uintptr_t)a < (uintptr_t)b;
}
