/* The table of open libraries under threads that load and unload at once,
   and the reason of a failure, which each thread reads for itself. The
   loader of the runtime runs here with stand-ins for the rest of the
   runtime, so the sanitizer builds check it. */
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
#include "../../src/rt/plugin.h"
#include "check.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

int check_failures;

/* The stand-ins of what src/rt/plugin.c calls outside itself. */

const struct anti_slot_table anti_rt_slots = {0, NULL, 0};

struct anti_text anti_rt_runtime_version(void)
{
    struct anti_text text = {(const unsigned char *)"0.0.0", 5};

    return text;
}

void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing)
{
    (void)path;
    (void)len;
    (void)writing;
    return NULL;
}

int64_t anti_rt_fs_size(void *file)
{
    (void)file;
    return -1;
}

enum { ROUNDS = 200, PAIR = 2 };

struct one {
    const char *path;
    void *handle;
    int64_t *go;
    int matched;                /* the failure named this thread's path */
};

static int64_t go_load;
static int64_t go_fail;

static void wait_for(int64_t *flag)
{
    while (anti_rt_atomic_load(flag, (int64_t)sizeof *flag) == 0) {
    }
}

#if defined(_WIN32)
#define THREAD_FN(name) static DWORD WINAPI name(LPVOID data)
#define THREAD_END return 0
#else
#define THREAD_FN(name) static void *name(void *data)
#define THREAD_END return NULL
#endif

THREAD_FN(load_one)
{
    struct one *o = data;

    wait_for(o->go);
    o->handle = anti_rt_plugin_load((const unsigned char *)o->path,
                                    (int64_t)strlen(o->path));
    THREAD_END;
}

/* Fails a load again and again and checks, many times over, that the
   reason names the path this thread asked for. */
THREAD_FN(fail_one)
{
    struct one *o = data;
    size_t length = strlen(o->path);
    int i;

    wait_for(o->go);
    o->matched = 1;
    for (i = 0; i < ROUNDS; i++) {
        int k;
        if (anti_rt_plugin_load((const unsigned char *)o->path,
                                (int64_t)length) != NULL) {
            o->matched = 0;
            continue;
        }
        for (k = 0; k < 20000; k++) {
            struct anti_text why = anti_rt_plugin_message();
            if (why.len < (int64_t)length ||
                memcmp(why.ptr, o->path, length) != 0) {
                o->matched = 0;
            }
        }
    }
    THREAD_END;
}

static void run_pair(struct one *pair, int loading)
{
    int i;
#if defined(_WIN32)
    HANDLE threads[PAIR];

    for (i = 0; i < PAIR; i++) {
        threads[i] = CreateThread(NULL, 0, loading ? load_one : fail_one,
                                  &pair[i], 0, NULL);
        CHECK(threads[i] != NULL);
    }
    anti_rt_atomic_store(pair[0].go, (int64_t)sizeof *pair[0].go, 1);
    for (i = 0; i < PAIR; i++) {
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
    }
#else
    pthread_t threads[PAIR];

    for (i = 0; i < PAIR; i++) {
        CHECK(pthread_create(&threads[i], NULL, loading ? load_one : fail_one,
                             &pair[i]) == 0);
    }
    anti_rt_atomic_store(pair[0].go, (int64_t)sizeof *pair[0].go, 1);
    for (i = 0; i < PAIR; i++) {
        pthread_join(threads[i], NULL);
    }
#endif
}

/* Two threads load two libraries at once. Each gets a slot of its own,
   and each unloads. */
static void loads_at_once(void)
{
    int round;
    int apart = 1;

    for (round = 0; round < ROUNDS; round++) {
        struct one pair[PAIR] = {
            {PLUGIN_ONE, NULL, &go_load, 0},
            {PLUGIN_TWO, NULL, &go_load, 0},
        };
        anti_rt_atomic_store(&go_load, (int64_t)sizeof go_load, 0);
        run_pair(pair, 1);
        if (pair[0].handle == NULL || pair[1].handle == NULL ||
            pair[0].handle == pair[1].handle) {
            apart = 0;
        }
        if (pair[0].handle != NULL) {
            anti_rt_plugin_unload(pair[0].handle);
        }
        if (pair[1].handle != NULL && pair[1].handle != pair[0].handle) {
            anti_rt_plugin_unload(pair[1].handle);
        }
    }
    CHECK(apart);
}

/* Two threads fail at once, each with a path of its own. */
static void failures_at_once(void)
{
    struct one pair[PAIR] = {
        {"no-such-library-one", NULL, &go_fail, 0},
        {"no-such-library-two-and-longer", NULL, &go_fail, 0},
    };

    run_pair(pair, 0);
    CHECK(pair[0].matched);
    CHECK(pair[1].matched);
}

int main(void)
{
    loads_at_once();
    failures_at_once();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
