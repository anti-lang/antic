/* The platform layer on macOS and Linux. See platform.h. */
#if !defined(_WIN32)

/* clock_gettime, nanosleep and setenv sit behind a feature macro, and
   the two systems spell it differently. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(__APPLE__)
#include <sys/random.h>
#endif

#include "platform.h"
#include "std.h"

/* DESIGN: anti.os builds the per-user directories, and the conventions
   differ between Windows and the two Unix platforms. The runtime is
   compiled once per target, so the answer is a constant of the build.
   Reading it from the environment instead would take the layout of
   Windows on any machine that happens to carry LOCALAPPDATA. */
int64_t anti_rt_is_windows(void)
{
    return 0;
}

static pthread_mutex_t locks[ANTI_RT_LOCK_COUNT] = {
    PTHREAD_MUTEX_INITIALIZER
};

void anti_rt_lock_hold(enum anti_rt_lock which)
{
    pthread_mutex_lock(&locks[which]);
}

void anti_rt_lock_release(enum anti_rt_lock which)
{
    pthread_mutex_unlock(&locks[which]);
}

int anti_rt_getenv(const char *name, char **value)
{
    const char *text = getenv(name);
    size_t length;

    *value = NULL;
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    length = strlen(text);
    *value = malloc(length + 1);
    if (*value == NULL) {
        return -1;
    }
    memcpy(*value, text, length + 1);
    return 0;
}

int anti_rt_path_is_absolute(const char *path)
{
    return path[0] == '/';
}

const char *anti_rt_path_last_separator(const char *path)
{
    return strrchr(path, '/');
}

void *anti_rt_library_open(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void anti_rt_library_close(void *handle)
{
    dlclose(handle);
}

void *anti_rt_library_symbol(void *handle, const char *name)
{
    return dlsym(handle, name);
}

const char *anti_rt_library_error(char *text, size_t size)
{
    const char *reason = dlerror();

    (void)text;
    (void)size;
    return reason != NULL ? reason : "cannot open the file";
}

int anti_rt_entropy(void *out, size_t count)
{
#if defined(__APPLE__)
    arc4random_buf(out, count);
    return 0;
#else
    unsigned char *at = out;

    while (count > 0) {
        ssize_t n = getrandom(at, count, 0);
        if (n <= 0) {
            return -1;
        }
        at += n;
        count -= (size_t)n;
    }
    return 0;
#endif
}

/* DESIGN: the monotonic clock never moves backwards and has no relation
   to the wall clock. A duration is therefore measured with the first and
   a date with the second. Both are given in nanoseconds, which holds 292
   years in an int64_t and is the resolution every target offers. */

int64_t anti_rt_monotonic(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + (int64_t)now.tv_nsec;
}

int64_t anti_rt_wall(void)
{
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    return (int64_t)now.tv_sec * 1000000000 + (int64_t)now.tv_nsec;
}

void anti_rt_sleep(int64_t nanoseconds)
{
    struct timespec wait;

    if (nanoseconds <= 0) {
        return;
    }
    wait.tv_sec = (time_t)(nanoseconds / 1000000000);
    wait.tv_nsec = (long)(nanoseconds % 1000000000);
    while (nanosleep(&wait, &wait) != 0) {
        /* A signal interrupted the wait, and wait holds what is left. */
    }
}

#else

/* ISO C wants a declaration in every file, and on Windows this one holds
   no other. */
typedef int anti_rt_platform_posix_unused;

#endif
