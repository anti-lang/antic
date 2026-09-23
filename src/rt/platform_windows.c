/* The platform layer on Windows. See platform.h. */
#if defined(_WIN32)

#include <limits.h>
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
    snprintf(text, size, "error %lu", (unsigned long)GetLastError());
    return text;
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
