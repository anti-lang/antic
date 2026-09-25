/* The platform layer on Windows. See platform.h. */
#if defined(_WIN32)

/* rand_s stands behind this macro, which comes before the first header
   that includes stdlib.h. */
#define _CRT_RAND_S

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include "platform.h"
#include "std.h"

/* See the DESIGN comment of the same function in platform_posix.c. */
int64_t anti_rt_is_windows(void)
{
    return 1;
}

static SRWLOCK locks[ANTI_RT_LOCK_COUNT] = {SRWLOCK_INIT};

void anti_rt_lock_hold(enum anti_rt_lock which)
{
    AcquireSRWLockExclusive(&locks[which]);
}

void anti_rt_lock_release(enum anti_rt_lock which)
{
    ReleaseSRWLockExclusive(&locks[which]);
}

/* The UTF-16 form of the UTF-8 text, in memory the caller frees. NULL
   when it is no valid UTF-8, is too long or memory runs out. */
static wchar_t *wide_of(const char *text)
{
    size_t length = strlen(text);
    int units;
    wchar_t *wide;

    if (length >= INT_MAX) {
        return NULL;
    }
    units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
                                (int)length + 1, NULL, 0);
    if (units <= 0) {
        return NULL;
    }
    wide = malloc((size_t)units * sizeof *wide);
    if (wide == NULL) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, (int)length + 1,
                        wide, units);
    return wide;
}

/* DESIGN: GetEnvironmentVariableW reports the length the value needs, and
   another thread may change the value between that call and the read. So
   the read repeats until the value fits the buffer it was sized for. The
   UTF-16 then becomes the UTF-8 the rest of the runtime reads, as
   anti_rt_fs_open reads a path. */
int anti_rt_getenv(const char *name, char **value)
{
    wchar_t *wide_name = wide_of(name);
    wchar_t *wide = NULL;
    DWORD size = 0;
    DWORD length;
    int bytes;

    *value = NULL;
    if (wide_name == NULL) {
        return -1;
    }
    for (;;) {
        length = GetEnvironmentVariableW(wide_name, wide, size);
        if (length == 0) {
            free(wide);
            free(wide_name);
            return 0;
        }
        if (length < size) {
            break;
        }
        free(wide);
        size = length;
        wide = malloc((size_t)size * sizeof *wide);
        if (wide == NULL) {
            free(wide_name);
            return -1;
        }
    }
    free(wide_name);
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide,
                                (int)length + 1, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        free(wide);
        return 0;
    }
    *value = malloc((size_t)bytes);
    if (*value == NULL) {
        free(wide);
        return -1;
    }
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)length + 1,
                        *value, bytes, NULL, NULL);
    free(wide);
    return 0;
}

int anti_rt_path_is_absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' ||
           ((path[0] | 32) >= 'a' && (path[0] | 32) <= 'z' && path[1] == ':');
}

const char *anti_rt_path_last_separator(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    return back != NULL && (slash == NULL || back > slash) ? back : slash;
}

/* The table anti_rt_imports of a plugin, which antic writes. */
struct imports {
    int64_t count;
    volatile LONG64 state;      /* IMPORTS_NONE until the places are done */
    void **places[];
};

enum { IMPORTS_NONE, IMPORTS_BUSY, IMPORTS_DONE };

/* DESIGN: a plugin reaches a name of its host through the __imp_ entry of
   the import library, which Windows fills when it loads the library. An
   address in the data of the plugin cannot be such a load. Each one holds
   the address of its __imp_ entry instead, and the loader replaces it
   with the address the entry holds. The first open of a library does it. A
   second open of the same library returns the same image, and the state
   word keeps it from replacing an address twice. A place whose page
   cannot be written fails the open, which then closes the library. */
static bool fill_imports(HMODULE handle)
{
    union {
        FARPROC from;
        struct imports *to;
    } cast;
    struct imports *t;
    bool ok = true;
    int64_t i;

    cast.from = GetProcAddress(handle, "anti_rt_imports");
    t = cast.to;
    if (t == NULL) {
        return true;
    }
    if (InterlockedCompareExchange64(&t->state, IMPORTS_BUSY, IMPORTS_NONE) !=
        IMPORTS_NONE) {
        while (InterlockedCompareExchange64(&t->state, IMPORTS_DONE,
                                            IMPORTS_DONE) != IMPORTS_DONE) {
            SwitchToThread();
        }
        return true;
    }
    for (i = 0; ok && i < t->count; i++) {
        void **place = t->places[i];
        DWORD was;
        ok = VirtualProtect(place, sizeof *place, PAGE_READWRITE, &was) != 0;
        if (ok) {
            *place = *(void **)*place;
            ok = VirtualProtect(place, sizeof *place, was, &was) != 0;
        }
    }
    InterlockedExchange64(&t->state, IMPORTS_DONE);
    return ok;
}

void *anti_rt_library_open(const char *path)
{
    wchar_t *wide = wide_of(path);
    HMODULE handle;

    if (wide == NULL) {
        SetLastError(ERROR_INVALID_NAME);
        return NULL;
    }
    handle = LoadLibraryW(wide);
    free(wide);
    if (handle != NULL && !fill_imports(handle)) {
        DWORD error = GetLastError();
        FreeLibrary(handle);
        SetLastError(error);
        return NULL;
    }
    return handle;
}

void anti_rt_library_close(void *handle)
{
    FreeLibrary((HMODULE)handle);
}

void *anti_rt_library_symbol(void *handle, const char *name)
{
    union {
        FARPROC from;
        void *to;
    } cast;

    cast.from = GetProcAddress((HMODULE)handle, name);
    return cast.to;
}

const char *anti_rt_library_error(char *text, size_t size)
{
    int written =
        snprintf(text, size, "error %lu", (unsigned long)GetLastError());

    /* A text cut short still names the error, and a failure leaves an
       empty one. */
    if (written < 0 && size > 0) {
        text[0] = '\0';
    }
    return text;
}

int anti_rt_entropy(void *out, size_t count)
{
    unsigned char *at = out;

    while (count > 0) {
        unsigned int word;
        size_t n = count < sizeof word ? count : sizeof word;
        if (rand_s(&word) != 0) {
            return -1;
        }
        memcpy(at, &word, n);
        at += n;
        count -= n;
    }
    return 0;
}

/* See the DESIGN comment on the clocks in platform_posix.c. */

int64_t anti_rt_monotonic(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart / frequency.QuadPart) * 1000000000 +
           (int64_t)(now.QuadPart % frequency.QuadPart) * 1000000000 /
               (int64_t)frequency.QuadPart;
}

int64_t anti_rt_wall(void)
{
    FILETIME file;
    ULARGE_INTEGER value;

    GetSystemTimeAsFileTime(&file);
    value.LowPart = file.dwLowDateTime;
    value.HighPart = file.dwHighDateTime;
    /* FILETIME counts 100-nanosecond units from 1601, and the epoch of
       the language is 1970. */
    return (int64_t)(value.QuadPart - 116444736000000000ULL) * 100;
}

/* A wait of any length is a loop of Sleep steps, which platform.h sizes
   below the INFINITE of Sleep. */
void anti_rt_sleep(int64_t nanoseconds)
{
    while (nanoseconds > 0) {
        uint32_t step = anti_rt_sleep_step(nanoseconds);

        Sleep((DWORD)step);
        nanoseconds -= (int64_t)step * 1000000;
    }
}

#else

/* ISO C wants a declaration in every file, and elsewhere this one holds
   no other. */
typedef int anti_rt_platform_windows_unused;

#endif
