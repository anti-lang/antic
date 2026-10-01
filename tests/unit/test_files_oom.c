/* S30. The allocation helpers of anti returned the NULL of a failed
   calloc and realloc, and files_grow wrote through it. Each helper now
   exits through files_out_of_memory, as files.h says. The program asks
   one helper, named by its argument, for more memory than any host
   gives, and tests/run_files_oom.cmake checks the exit. A helper that
   comes back prints what it returned and ends with status 0. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "files.h"

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
    void *items = NULL;

    if (argc != 2) {
        fputs("usage: files_oom_tests array|resize|grow\n", stderr);
        return 2;
    }
    if (strcmp(argv[1], "array") == 0) {
        items = files_array(TOO_MANY, 1);
    } else if (strcmp(argv[1], "resize") == 0) {
        items = files_resize(NULL, TOO_MANY, 1);
    } else if (strcmp(argv[1], "grow") == 0) {
        /* The room doubles to TOO_MANY. */
        size_t room = TOO_MANY / 2;
        items = files_grow(NULL, &room, 1);
    } else {
        fprintf(stderr, "files_oom_tests: no helper %s\n", argv[1]);
        return 2;
    }
    printf("files_%s returned %s\n", argv[1], items == NULL ? "NULL" : "memory");
    free(items);
    return 0;
}
