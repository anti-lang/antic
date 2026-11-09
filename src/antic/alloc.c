/* The checked allocation of antic, which ends the run on a failure or an
   overflow. The message names the tool that runs, antic or anti, which
   the platform layer holds. */

#include "alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "../rt/fault.h"

void alloc_out_of_memory(void)
{
    fprintf(stderr, "%s: out of memory\n", platform_tool());
    exit(70);
}

void alloc_out_of_memory_for(long long size)
{
    fprintf(stderr, "%s: out of memory for %lld bytes\n", platform_tool(),
            size);
    exit(70);
}

size_t alloc_product(size_t a, size_t b)
{
    if (b != 0 && a > SIZE_MAX / b) {
        alloc_out_of_memory();
    }
    return a * b;
}

size_t alloc_sum(size_t a, size_t b)
{
    if (a > SIZE_MAX - b) {
        alloc_out_of_memory();
    }
    return a + b;
}

void *alloc_zeroed(size_t count, size_t size)
{
    void *items;

    if (count == 0) {
        count = 1;
    }
    items = ANTI_FAULT(ALLOC, NULL,
                       calloc(1, alloc_product(count, size == 0 ? 1 : size)));
    if (items == NULL) {
        alloc_out_of_memory();
    }
    return items;
}

void *alloc_resize(void *items, size_t count, size_t size)
{
    size_t bytes = alloc_product(count, size);
    void *resized =
        ANTI_FAULT(ALLOC, NULL, realloc(items, bytes == 0 ? 1 : bytes));

    if (resized == NULL) {
        alloc_out_of_memory();
    }
    return resized;
}

void *alloc_grow(void *items, size_t *capacity, size_t count, size_t size)
{
    size_t next = *capacity == 0 ? 8 : *capacity;

    if (count < *capacity) {
        return items;
    }
    while (next <= count) {
        next = alloc_product(next, 2);
    }
    items = alloc_resize(items, next, size);
    *capacity = next;
    return items;
}
