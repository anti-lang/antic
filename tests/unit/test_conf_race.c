/* rt.configure replaces the value of a key while another thread reads
   it. tests/conf/replace.toml includes one file 64 times, and each include
   sets `threads` again. A thread reads the key and its bytes the whole
   time. The sanitizer builds report a value freed under that reader. The
   runtime of an Anti program carries no sanitizer, so the check stands
   here, with the loader and the files of anti.fs replaced by stand-ins. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/atomic.h"
#include "../../src/rt/conf.h"
#include "../../src/rt/plugin.h"
#include "../../src/rt/rt.h"
#include "check.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

int check_failures;

/* The stand-ins of what src/rt/conf.c calls outside itself. */

int anti_rt_option_backtrace = -1;
const struct anti_injectables anti_rt_injectable = {0, NULL};

struct anti_text anti_rt_runtime_version(void)
{
    struct anti_text text = {(const unsigned char *)"0.0.0", 5};

    return text;
}

void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing)
{
    char name[1024];

    (void)writing;
    if (len < 0 || (size_t)len >= sizeof name) {
        return NULL;
    }
    memcpy(name, path, (size_t)len);
    name[len] = '\0';
    return fopen(name, "rb");
}

int64_t anti_rt_fs_size(void *file)
{
    long size;

    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    size = ftell(file);
    return fseek(file, 0, SEEK_SET) == 0 ? (int64_t)size : -1;
}

void *anti_rt_plugin_provider(const unsigned char *path, int64_t path_length,
                              const unsigned char *library,
                              int64_t library_length, const char *dirs)
{
    (void)path;
    (void)path_length;
    (void)library;
    (void)library_length;
    (void)dirs;
    return NULL;
}

struct anti_text anti_rt_plugin_message(void)
{
    struct anti_text text = {(const unsigned char *)"", 0};

    return text;
}

const struct anti_slots *anti_rt_plugin_slots(const struct anti_descriptor *d)
{
    (void)d;
    return NULL;
}

static int64_t started;
static int64_t done;
static int64_t digits;

/* Read the key and every byte of its value until the main thread is
   done. */
#if defined(_WIN32)
static DWORD WINAPI read_key(LPVOID unused)
#else
static void *read_key(void *unused)
#endif
{
    int64_t sum = 0;

    (void)unused;
    while (anti_rt_atomic_load(&done, (int64_t)sizeof done) == 0) {
        struct anti_text t =
            anti_rt_conf_get((const unsigned char *)"threads", 7);
        int64_t i;
        for (i = 0; i < t.len; i++) {
            sum += t.ptr[i] - '0';
        }
        anti_rt_atomic_store(&started, (int64_t)sizeof started, 1);
    }
    anti_rt_atomic_store(&digits, (int64_t)sizeof digits, sum > 0);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

int main(void)
{
    const char *path = ANTIC_SOURCE_DIR "/tests/conf/replace.toml";
    struct anti_text t;
#if defined(_WIN32)
    HANDLE thread = CreateThread(NULL, 0, read_key, NULL, 0, NULL);

    CHECK(thread != NULL);
#else
    pthread_t thread;

    CHECK(pthread_create(&thread, NULL, read_key, NULL) == 0);
#endif
    while (anti_rt_atomic_load(&started, (int64_t)sizeof started) == 0) {
    }
    anti_rt_conf_configure((const unsigned char *)path,
                           (int64_t)strlen(path));
    anti_rt_atomic_store(&done, (int64_t)sizeof done, 1);
#if defined(_WIN32)
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    t = anti_rt_conf_get((const unsigned char *)"threads", 7);
    CHECK(t.len == 1 && t.ptr[0] == '2');
    CHECK(anti_rt_atomic_load(&digits, (int64_t)sizeof digits) == 1);
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
