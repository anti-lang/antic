/* map_work.c with its seed read at run time, from a volatile, so clang
   cannot fold mix(seed) as it does with the constant of the twin. The
   table, the keys and the line printed are those of map_work.c, which
   SlotTable<E> of anti.collection.map follows: one zeroed block of a mark
   per slot and the slots, linear probing, a key hashed by the finalizer of
   MurmurHash3 and mixed with a seed, and growth before the live and the
   removed entries reach half the slots. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIVE 0x8000000000000000u
#define REMOVED 1u
#define FIRST_SLOTS 16

struct entry {
    int64_t key;
    int64_t value;
};

struct table {
    unsigned char *block;
    uint64_t *marks;
    struct entry *slots;
    int64_t size;
    int64_t count;
    int64_t removed;
    uint64_t seed;
};

/* The seed of every table, which clang cannot read before the run. */
static volatile uint64_t seed_source = 0x9e3779b97f4a7c15u;

static uint64_t mix(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdu;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53u;
    x ^= x >> 33;
    return x;
}

static uint64_t marked(const struct table *t, int64_t key)
{
    return mix(mix((uint64_t)key) ^ mix(t->seed)) | LIVE;
}

static int64_t free_slot(const struct table *t, uint64_t h)
{
    int64_t mask = t->size - 1;
    int64_t i = (int64_t)(h & (uint64_t)mask);
    while (t->marks[i] > REMOVED) {
        i = (i + 1) & mask;
    }
    return i;
}

static int64_t slot_of(const struct table *t, uint64_t h, int64_t key)
{
    if (t->marks == NULL) {
        return -1;
    }
    int64_t mask = t->size - 1;
    int64_t i = (int64_t)(h & (uint64_t)mask);
    while (t->marks[i] != 0) {
        if (t->marks[i] == h && t->slots[i].key == key) {
            return i;
        }
        i = (i + 1) & mask;
    }
    return -1;
}

static void resize(struct table *t, int64_t size)
{
    unsigned char *old = t->block;
    uint64_t *old_marks = t->marks;
    struct entry *old_slots = t->slots;
    int64_t old_size = t->size;
    size_t at_slots = (size_t)size * sizeof(uint64_t);
    size_t total = at_slots + (size_t)size * sizeof(struct entry);
    unsigned char *made = malloc(total);
    if (made == NULL) {
        exit(1);
    }
    memset(made, 0, total);
    t->block = made;
    t->marks = (uint64_t *)(void *)made;
    t->slots = (struct entry *)(void *)(made + at_slots);
    t->size = size;
    t->removed = 0;
    for (int64_t i = 0; i < old_size; i++) {
        uint64_t h = old_marks[i];
        if (h > REMOVED) {
            int64_t to = free_slot(t, h);
            t->slots[to] = old_slots[i];
            t->marks[to] = h;
        }
    }
    free(old);
}

static void set(struct table *t, int64_t key, int64_t value)
{
    uint64_t h = marked(t, key);
    int64_t found = slot_of(t, h, key);
    if (found >= 0) {
        t->slots[found].value = value;
        return;
    }
    if ((t->count + t->removed + 1) * 2 > t->size) {
        int64_t size = t->size;
        if (size < FIRST_SLOTS) {
            size = FIRST_SLOTS;
        } else if ((t->count + 1) * 4 > size) {
            size *= 2;
        }
        resize(t, size);
    }
    int64_t i = free_slot(t, h);
    if (t->marks[i] == REMOVED) {
        t->removed -= 1;
    }
    t->marks[i] = h;
    t->slots[i].key = key;
    t->slots[i].value = value;
    t->count += 1;
}

/* The value under key, or `otherwise` when the table does not hold it. */
static int64_t get(const struct table *t, int64_t key, int64_t otherwise)
{
    int64_t found = slot_of(t, marked(t, key), key);
    return found < 0 ? otherwise : t->slots[found].value;
}

static void removekey(struct table *t, int64_t key)
{
    int64_t found = slot_of(t, marked(t, key), key);
    if (found >= 0) {
        t->marks[found] = REMOVED;
        t->count -= 1;
        t->removed += 1;
    }
}

static int64_t key_of(int64_t i, int64_t r)
{
    return (int64_t)((uint64_t)i * 2654435761u + (uint64_t)r);
}

int main(void)
{
    uint64_t total = 0;
    for (int64_t r = 0; r < 20; r++) {
        struct table m = {NULL, NULL, NULL, 0, 0, 0, seed_source};
        for (int64_t i = 0; i < 100000; i++) {
            set(&m, key_of(i, r), i);
        }
        for (int64_t i = 0; i < 400000; i++) {
            total += (uint64_t)get(&m, key_of(i, r), 1);
        }
        for (int64_t i = 0; i < 100000; i += 2) {
            removekey(&m, key_of(i, r));
        }
        for (int64_t i = 0; i < 100000; i++) {
            total += (uint64_t)get(&m, key_of(i, r), 3);
        }
        total += (uint64_t)m.count;
        free(m.block);
    }
    printf("%lld\n", (long long)total);
    return 0;
}
