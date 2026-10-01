#include "ptrset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DESIGN: the set, the map and the index are one table with values of
   three kinds: none, a pointer and a uint32_t. All three hash a key and
   probe for it in probe, and grow in make_room, so the three cannot
   drift apart. */

#define FIRST_CAPACITY 64

static size_t ptr_slot(const void *p, size_t capacity)
{
    uint64_t h = (uint64_t)(uintptr_t)p;

    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (size_t)(h & (capacity - 1));
}

/* The slot of p among keys, or the empty slot where it would stand.
   capacity is a power of two above 0, and keys has an empty slot. */
static size_t probe(const void *const *keys, size_t capacity, const void *p)
{
    size_t i;

    for (i = ptr_slot(p, capacity); keys[i] != NULL && keys[i] != p;
         i = (i + 1) & (capacity - 1)) {
    }
    return i;
}

static void *zeroed(size_t count, size_t size)
{
    void *p = calloc(count, size);

    if (p == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    return p;
}

/* Make room among *keys for one key more than count, by twice the room
   once the table is half full. *values holds a value of size bytes per
   key, or nothing when size is 0. */
static void make_room(const void ***keys, void **values, size_t size,
                      size_t count, size_t *capacity)
{
    size_t room = *capacity == 0 ? FIRST_CAPACITY : *capacity * 2;
    const void **old_keys = *keys;
    const unsigned char *old_values = *values;
    const void **new_keys;
    unsigned char *new_values = NULL;
    size_t i;

    if ((count + 1) * 2 <= *capacity) {
        return;
    }
    new_keys = zeroed(room, sizeof *new_keys);
    if (size > 0) {
        new_values = zeroed(room, size);
    }
    for (i = 0; i < *capacity; i++) {
        if (old_keys[i] != NULL) {
            size_t at = probe(new_keys, room, old_keys[i]);
            new_keys[at] = old_keys[i];
            if (size > 0) {
                memcpy(new_values + at * size, old_values + i * size, size);
            }
        }
    }
    free(old_keys);
    free(*values);
    *keys = new_keys;
    *values = new_values;
    *capacity = room;
}

bool ptr_set_add(struct ptr_set *s, const void *p)
{
    void *none = NULL;
    size_t i;

    make_room(&s->slots, &none, 0, s->count, &s->capacity);
    i = probe(s->slots, s->capacity, p);
    if (s->slots[i] != NULL) {
        return false;
    }
    s->slots[i] = p;
    s->count++;
    return true;
}

bool ptr_set_has(const struct ptr_set *s, const void *p)
{
    return s->capacity > 0 && s->slots[probe(s->slots, s->capacity, p)] != NULL;
}

void *ptr_map_get(const struct ptr_map *m, const void *key)
{
    size_t i;

    if (m->capacity == 0) {
        return NULL;
    }
    i = probe(m->keys, m->capacity, key);
    return m->keys[i] != NULL ? m->values[i] : NULL;
}

void ptr_map_put(struct ptr_map *m, const void *key, void *value)
{
    void *values = m->values;
    size_t i;

    make_room(&m->keys, &values, sizeof *m->values, m->count, &m->capacity);
    m->values = values;
    i = probe(m->keys, m->capacity, key);
    if (m->keys[i] == NULL) {
        m->keys[i] = key;
        m->count++;
    }
    m->values[i] = value;
}

void ptr_map_free(struct ptr_map *m)
{
    free(m->keys);
    free(m->values);
    memset(m, 0, sizeof *m);
}

bool ptr_index_get(const struct ptr_index *m, const void *key,
                   uint32_t *value)
{
    size_t i;

    if (m->capacity == 0) {
        return false;
    }
    i = probe(m->keys, m->capacity, key);
    if (m->keys[i] == NULL) {
        return false;
    }
    *value = m->values[i];
    return true;
}

void ptr_index_put(struct ptr_index *m, const void *key, uint32_t value)
{
    void *values = m->values;
    size_t i;

    make_room(&m->keys, &values, sizeof *m->values, m->count, &m->capacity);
    m->values = values;
    i = probe(m->keys, m->capacity, key);
    if (m->keys[i] == NULL) {
        m->keys[i] = key;
        m->count++;
    }
    m->values[i] = value;
}

void ptr_index_free(struct ptr_index *m)
{
    free(m->keys);
    free(m->values);
    memset(m, 0, sizeof *m);
}
