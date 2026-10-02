/* The platform layer of antic and anti, src/antic/platform.c, on the host.
   A file opens in binary mode, so a CR LF comes back as it was written.
   Writing creates a file and empties one that is there, and reading a
   missing file gives NULL. An environment variable reads as its value,
   and an unset one as NULL. Paths follow the rules of the host that
   path_rules.h holds, and on Windows a path and a value outside ASCII
   reach the system as UTF-16. */
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
#include "alloc.h"
#include "path_rules.h"

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
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

/* A name outside ASCII and outside every ANSI code page: u with
   diaeresis and the CJK ideograph for middle, in UTF-16 and in UTF-8. */
#define WIDE_NAME L"tool_platform_\u00fc\u4e2d"
#define UTF8_NAME "tool_platform_\xc3\xbc\xe4\xb8\xad"
#define UTF8_VALUE "\xc3\xbc\xe4\xb8\xad"

/* The layer takes and gives UTF-8 and calls the UTF-16 entry points of
   Windows. A path of UTF-8 bytes passed to the ANSI ones names another
   file, and a value read through them loses every character outside
   the code page. */
static void utf8_on_windows(void)
{
    struct text full = {0};
    const char *value;
    FILE *f;

    CHECK(CreateDirectoryW(WIDE_NAME, NULL) ||
          GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(directory_exists(UTF8_NAME));
    f = platform_open(UTF8_NAME "/file.bin", true);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fputs("x", f) >= 0);
        CHECK(fclose(f) == 0);
    }
    CHECK(GetFileAttributesW(WIDE_NAME L"\\file.bin") !=
          INVALID_FILE_ATTRIBUTES);
    f = platform_open(UTF8_NAME "/file.bin", false);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fgetc(f) == 'x');
        CHECK(fclose(f) == 0);
    }
    CHECK(absolute_path(UTF8_NAME, &full));
    CHECK(full.length >= sizeof UTF8_NAME - 1 &&
          strcmp(text_cstr(&full) + full.length - (sizeof UTF8_NAME - 1),
                 UTF8_NAME) == 0);
    text_free(&full);
    CHECK(platform_remove(UTF8_NAME "/file.bin"));
    CHECK(GetFileAttributesW(WIDE_NAME L"\\file.bin") ==
          INVALID_FILE_ATTRIBUTES);
    CHECK(RemoveDirectoryW(WIDE_NAME));

    CHECK(_wputenv_s(L"ANTIC_TOOL_PLATFORM_WIDE", L"\u00fc\u4e2d") == 0);
    value = platform_getenv("ANTIC_TOOL_PLATFORM_WIDE");
    CHECK(value != NULL && strcmp(value, UTF8_VALUE) == 0);
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
    bytes = alloc_zeroed(64, 1);
    *length = fread(bytes, 1, 64, f);
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

/* Every case of path_rules.h, which the platform layer of the runtime
   answers the same way. */
static void path_rules_of_host(void)
{
    size_t i;

    for (i = 0; i < PATH_RULE_COUNT; i++) {
        const struct path_rule *r = &path_rules[i];
        const char *last = platform_last_separator(r->path);

        CHECK(path_is_absolute(r->path) == (r->absolute != 0));
        CHECK(r->separator < 0 ? last == NULL
                               : last == r->path + r->separator);
    }
}

/* A directory lists every entry but `.` and `..`, and a directory that
   does not exist cannot be listed. */
static void count_entry(void *context, const char *name)
{
    size_t *count = context;

    CHECK(strcmp(name, ".") != 0 && strcmp(name, "..") != 0);
    (*count)++;
}

static void list_directory(void)
{
    static const char dir[] = "tool_platform_dir";
    size_t count = 0;

#if defined(_WIN32)
    CHECK(_mkdir(dir) == 0);
#else
    CHECK(mkdir(dir, 0755) == 0);
#endif
    write_all("tool_platform_dir/a.bin", "a", 1);
    write_all("tool_platform_dir/b.bin", "b", 1);
    CHECK(directory_exists(dir));
    CHECK(platform_list_directory(dir, count_entry, &count));
    CHECK(count == 2);
    CHECK(platform_remove("tool_platform_dir/a.bin"));
    CHECK(platform_remove("tool_platform_dir/b.bin"));
    CHECK(!platform_remove("tool_platform_dir/b.bin"));
#if defined(_WIN32)
    CHECK(_rmdir(dir) == 0);
#else
    CHECK(rmdir(dir) == 0);
#endif
    CHECK(!platform_list_directory("tool_platform_none", count_entry, &count));
    CHECK(!directory_exists("tool_platform_none"));
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
    utf8_on_windows();
#endif
    remove(path);

    put("ANTIC_TOOL_PLATFORM", "value");
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM") != NULL &&
          strcmp(platform_getenv("ANTIC_TOOL_PLATFORM"), "value") == 0);
    CHECK(platform_getenv("ANTIC_TOOL_PLATFORM_UNSET") == NULL);

    path_rules_of_host();
    list_directory();
}
