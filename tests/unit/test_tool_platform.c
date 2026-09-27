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

#if defined(_WIN32)
#include <windows.h>

/* Close the handle of the file after 300 ms. */
static DWORD WINAPI release_later(LPVOID handle)
{
    Sleep(300);
    CloseHandle((HANDLE)handle);
    return 0;
}

/* A file that another handle holds without sharing it for writing, as a
   scanner of Windows holds a program that just ran, opens for writing
   once that handle is closed. anti build rewrote the program in dist/
   right after a run of it and failed. */
static void write_waits_for_lock(const char *path)
{
    HANDLE held;
    HANDLE thread;
    FILE *f;

    held = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(held != INVALID_HANDLE_VALUE);
    if (held == INVALID_HANDLE_VALUE) {
        return;
    }
    thread = CreateThread(NULL, 0, release_later, held, 0, NULL);
    CHECK(thread != NULL);
    f = platform_open(path, true);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fclose(f) == 0);
    }
    if (thread != NULL) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
}
#endif

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
#if defined(_WIN32)
    write_waits_for_lock(path);
#endif
    remove(path);

    put("ANTIC_TOOL_PLATFORM", "value");
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM") != NULL &&
          strcmp(platform_getenv("ANTIC_TOOL_PLATFORM"), "value") == 0);
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM_UNSET") == NULL);
}
