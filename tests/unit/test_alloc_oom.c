/* M03. Each allocation helper of antic, and the memory pool and the text
   buffer built on them, exits through alloc_out_of_memory when the C
   library refuses the memory or a size overflows, with status 70 and the
   message, and never comes back with NULL. The program asks one helper,
   named by its argument, for more memory than any host gives, and
   tests/run_alloc_oom.cmake checks the exit. A helper that comes back
   prints what it returned and ends with status 0. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "alloc.h"
#include "arena.h"
#include "text.h"

/* AddressSanitizer stops the program at an allocation it cannot give,
   unless this option tells it to answer NULL as the C library does. The
   sanitizer reads the function at start. Every other build ignores it. */
const char *__asan_default_options(void);
const char *__asan_default_options(void)
{
    return "allocator_may_return_null=1";
}

/* More bytes than any host gives, and few enough that no product in the
   helpers overflows a size_t. */
#define TOO_MANY (SIZE_MAX / 4)

int main(int argc, char **argv)
{
    const char *helper;
    void *items = NULL;
    void *owned = NULL;
    size_t size = 0;
    struct arena arena = {0};
    struct text text = {0};

    if (argc != 2) {
        fputs("usage: alloc_oom_tests "
              "zeroed|resize|grow|product|sum|arena|text\n", stderr);
        return 2;
    }
    helper = argv[1];
    if (strcmp(helper, "zeroed") == 0) {
        owned = items = alloc_zeroed(TOO_MANY, 1);
    } else if (strcmp(helper, "resize") == 0) {
        owned = items = alloc_resize(NULL, TOO_MANY, 1);
    } else if (strcmp(helper, "grow") == 0) {
        /* The room doubles to TOO_MANY. */
        size_t room = TOO_MANY / 2;
        owned = items = alloc_grow(NULL, &room, room, 1);
    } else if (strcmp(helper, "product") == 0) {
        size = alloc_product(SIZE_MAX / 2, 3);
    } else if (strcmp(helper, "sum") == 0) {
        size = alloc_sum(SIZE_MAX, 1);
    } else if (strcmp(helper, "arena") == 0) {
        /* The rounding to the alignment and the header of the block
           overflow a size_t here. */
        items = arena_alloc(&arena, SIZE_MAX - 4);
    } else if (strcmp(helper, "text") == 0) {
        /* The length, the bytes and the NUL overflow a size_t. */
        text_append(&text, "x");
        text_append_bytes(&text, "x", SIZE_MAX - 1);
        items = text.data;
    } else {
        fprintf(stderr, "alloc_oom_tests: no helper %s\n", helper);
        return 2;
    }
    printf("%s returned %s, size %zu\n", helper,
           items == NULL ? "NULL" : "memory", size);
    free(owned);
    text_free(&text);
    arena_free(&arena);
    return 0;
}
