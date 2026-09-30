#include "ptrset.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t ptr_slot(const void *p, size_t capacity)
{
    uint64_t h = (uint64_t)(uintptr_t)p;

    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (size_t)(h & (capacity - 1));
}

bool ptr_set_add(struct ptr_set *s, const void *p)
{
    size_t i;

    if ((s->count + 1) * 2 > s->capacity) {
        size_t capacity = s->capacity == 0 ? 64 : s->capacity * 2;
        const void **slots = calloc(capacity, sizeof *slots);
        if (slots == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        for (i = 0; i < s->capacity; i++) {
            if (s->slots[i] != NULL) {
                size_t at = ptr_slot(s->slots[i], capacity);
                while (slots[at] != NULL) {
                    at = (at + 1) & (capacity - 1);
                }
                slots[at] = s->slots[i];
            }
        }
        free(s->slots);
        s->slots = slots;
        s->capacity = capacity;
    }
    for (i = ptr_slot(p, s->capacity); s->slots[i] != NULL;
         i = (i + 1) & (s->capacity - 1)) {
        if (s->slots[i] == p) {
            return false;
        }
    }
    s->slots[i] = p;
    s->count++;
    return true;
}

bool ptr_set_has(const struct ptr_set *s, const void *p)
{
    size_t i;

    if (s->capacity == 0) {
        return false;
    }
    for (i = ptr_slot(p, s->capacity); s->slots[i] != NULL;
         i = (i + 1) & (s->capacity - 1)) {
        if (s->slots[i] == p) {
            return true;
        }
    }
    return false;
}

void *ptr_map_get(const struct ptr_map *m, const void *key)
{
    size_t i;

    if (m->capacity == 0) {
        return NULL;
    }
    for (i = ptr_slot(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
        if (m->keys[i] == key) {
            return m->values[i];
        }
    }
    return NULL;
}

void ptr_map_put(struct ptr_map *m, const void *key, void *value)
{
    size_t i;

    if ((m->count + 1) * 2 > m->capacity) {
        struct ptr_map grown;
        size_t k;
        grown.capacity = m->capacity == 0 ? 64 : m->capacity * 2;
        grown.count = 0;
        grown.keys = calloc(grown.capacity, sizeof *grown.keys);
        grown.values = calloc(grown.capacity, sizeof *grown.values);
        if (grown.keys == NULL || grown.values == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        for (k = 0; k < m->capacity; k++) {
            if (m->keys[k] != NULL) {
                ptr_map_put(&grown, m->keys[k], m->values[k]);
            }
        }
        free(m->keys);
        free(m->values);
        *m = grown;
    }
    for (i = ptr_slot(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
        if (m->keys[i] == key) {
            m->values[i] = value;
            return;
        }
    }
    m->keys[i] = key;
    m->values[i] = value;
    m->count++;
}

void ptr_map_free(struct ptr_map *m)
{
    free(m->keys);
    free(m->values);
    memset(m, 0, sizeof *m);
}
