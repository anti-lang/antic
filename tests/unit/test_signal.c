/* The signal table of the runtime under threads that register at once.
   Windows starts no reader thread and opens no pipe, so the checks are of
   the other two systems. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>

#include "../../src/rt/atomic.h"
#include "../../src/rt/signal.h"
#include "check.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

enum { REGISTERING = 8 };

static int64_t go;
static int64_t calls;

static void count(int64_t sig)
{
    (void)sig;
    anti_rt_atomic_add(&calls, (int64_t)sizeof calls, 1);
}

/* Waits for the start, so every thread makes the first registration at
   once. */
static void *registers(void *unused)
{
    (void)unused;
    while (anti_rt_atomic_load(&go, (int64_t)sizeof go) == 0) {
    }
    anti_rt_on_signal(SIGUSR2, count);
    return NULL;
}

static int open_files(void)
{
    int n = 0;
    int fd;

    for (fd = 0; fd < 1024; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            n++;
        }
    }
    return n;
}

/* The first registrations of eight threads at once start one reader, with
   one pipe, and the signal reaches the function once. */
static void first_registrations(void)
{
    pthread_t threads[REGISTERING];
    struct timespec pause = {0, 1000000};
    int before = open_files();
    int started = 0;
    int i;

    for (i = 0; i < REGISTERING; i++) {
        if (pthread_create(&threads[i], NULL, registers, NULL) == 0) {
            started++;
        }
    }
    CHECK(started == REGISTERING);
    anti_rt_atomic_store(&go, (int64_t)sizeof go, 1);
    for (i = 0; i < started; i++) {
        pthread_join(threads[i], NULL);
    }
    CHECK(open_files() == before + 2);

    raise(SIGUSR2);
    for (i = 0; i < 2000 &&
                anti_rt_atomic_load(&calls, (int64_t)sizeof calls) == 0;
         i++) {
        nanosleep(&pause, NULL);
    }
    CHECK(anti_rt_atomic_load(&calls, (int64_t)sizeof calls) == 1);
}
#endif

int check_failures;

int main(void)
{
#if !defined(_WIN32)
    first_registrations();
#endif
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
