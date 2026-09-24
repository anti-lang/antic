/* anti_rt_fs_read of src/rt/fs.c, the one reader of a whole file that the
   configuration, discovery and the trace of the runtime share. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "std.h"

static void write_bytes(const char *name, const char *bytes, size_t n)
{
    FILE *f = fopen(name, "wb");

    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(bytes, 1, n, f) == n);
        CHECK(fclose(f) == 0);
    }
}

void test_fs(void)
{
    static const char name[] = "fs_read.bin";
    unsigned char *bytes;
    int64_t length = -1;

    /* The bytes, a NUL byte among them, with a NUL after the last. */
    write_bytes(name, "ab\0c", 4);
    bytes = anti_rt_fs_read(name, &length);
    CHECK(bytes != NULL);
    if (bytes != NULL) {
        CHECK(length == 4);
        CHECK(memcmp(bytes, "ab\0c", 5) == 0);
        free(bytes);
    }
    /* An empty file gives a buffer of one NUL. */
    write_bytes(name, "", 0);
    length = -1;
    bytes = anti_rt_fs_read(name, &length);
    CHECK(bytes != NULL);
    if (bytes != NULL) {
        CHECK(length == 0);
        CHECK(bytes[0] == '\0');
        free(bytes);
    }
    remove(name);
    /* A file that is not there, and a directory, give NULL. */
    CHECK(anti_rt_fs_read(name, &length) == NULL);
    CHECK(anti_rt_fs_read(".", &length) == NULL);
}
