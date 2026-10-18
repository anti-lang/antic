#ifndef ANTIC_PTRSET_H
#define ANTIC_PTRSET_H

/* The interface of ptrset.c: a set of pointers, a map from a pointer to a
   pointer and one to an index. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A set of pointers, open addressed and at most half full. A zeroed
   struct is empty, and the owner frees slots with free(). */
struct ptr_set {
    const void **slots;
    size_t capacity;
    size_t count;
};

/* A map of pointers to pointers, open addressed and at most half full.
   A zeroed struct is empty, and the owner frees it with ptr_map_free. */
struct ptr_map {
    const void **keys;
    void **values;
    size_t capacity;
    size_t count;
};

/* A map of pointers to indices, as the set and the map are. The owner
   frees it with ptr_index_free. */
struct ptr_index {
    const void **keys;
    uint32_t *values;
    size_t capacity;
    size_t count;
};

/* Add p to s. True when p was not in s before. */
bool ptr_set_add(struct ptr_set *s, const void *p);

/* Whether p is in s. */
bool ptr_set_has(const struct ptr_set *s, const void *p);
/* The value of key in m, or NULL. */
void *ptr_map_get(const struct ptr_map *m, const void *key);

/* Set the value of key in m to value, adding key when it is not in m. */
void ptr_map_put(struct ptr_map *m, const void *key, void *value);

/* Release the table of m and leave it empty. */
void ptr_map_free(struct ptr_map *m);
/* Whether key is in m, with its value in *value when it is. */
bool ptr_index_get(const struct ptr_index *m, const void *key,
                   uint32_t *value);

/* Set the index of key in m to value, adding key when it is not in m. */
void ptr_index_put(struct ptr_index *m, const void *key, uint32_t value);

/* Release the table of m and leave it empty. */
void ptr_index_free(struct ptr_index *m);

#endif
