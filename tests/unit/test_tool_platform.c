/* The platform layer of antic and anti, src/antic/platform.c, on the host.
   A file opens in binary mode, so a CR LF comes back as it was written.
   Writing creates a file and empties one that is there, and reading a
   missing file gives NULL. An environment variable reads as its value,
   and an unset one as NULL. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/antic/platform.h"
#include "../binary_stdio.h"
#include "check.h"

/* Set the environment variable name to value, on the host's own call. */
static void put(const char *name, const char *value)
{
#if defined(_WIN32)
    CHECK(_putenv_s(name, value) == 0);
#else
    CHECK(setenv(name, value, 1) == 0);
#endif
}

/* The bytes of the file at path, which the caller frees, or NULL. */
static char *read_all(const char *path, size_t *length)
{
    FILE *f = platform_open(path, false);
    char *bytes;

    *length = 0;
    if (f == NULL) {
        return NULL;
    }
    bytes = malloc(64);
    if (bytes != NULL) {
        *length = fread(bytes, 1, 64, f);
    }
    fclose(f);
    return bytes;
}

static void write_all(const char *path, const char *bytes, size_t n)
{
    FILE *f = platform_open(path, true);

    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(bytes, 1, n, f) == n);
        CHECK(fclose(f) == 0);
    }
}

void test_tool_platform(void)
{
    static const char path[] = "tool_platform.bin";
    size_t length;
    char *bytes;

    remove(path);
    CHECK(platform_open(path, false) == NULL);
    write_all(path, "a\r\nb\n", 5);
    bytes = read_all(path, &length);
    CHECK(bytes != NULL && length == 5 && memcmp(bytes, "a\r\nb\n", 5) == 0);
    free(bytes);
    write_all(path, "c", 1);
    bytes = read_all(path, &length);
    CHECK(bytes != NULL && length == 1 && bytes[0] == 'c');
    free(bytes);
    remove(path);

    put("ANTIC_TOOL_PLATFORM", "value");
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM") != NULL &&
          strcmp(platform_getenv("ANTIC_TOOL_PLATFORM"), "value") == 0);
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM_UNSET") == NULL);
}
