/* The platform layer on macOS and Linux. See platform.h. */
#if !defined(_WIN32)

/* clock_gettime, nanosleep, dladdr, sysconf and syscall sit behind a
   feature macro, and the two systems spell it differently. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <os/lock.h>
#else
#include <sys/random.h>
#include <sys/syscall.h>
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

/* The C library gives the message of strerror in storage of its own. */
const unsigned char *anti_rt_errno_text(int32_t code)
{
    return (const unsigned char *)strerror((int)code);
}

/* See the DESIGN comment of the declaration in std.h. */
int32_t anti_rt_last_error(void)
{
    return 0;
}

const unsigned char *anti_rt_last_error_text(int32_t code)
{
    (void)code;
    return (const unsigned char *)"";
}

struct anti_rt_monitor {
    pthread_mutex_t lock;
    pthread_cond_t conditions[2];
};

#define MONITOR_INIT                                                       \
    {PTHREAD_MUTEX_INITIALIZER,                                            \
     {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER}}

static struct anti_rt_monitor monitors[] = {
    MONITOR_INIT, MONITOR_INIT, MONITOR_INIT,
    MONITOR_INIT, MONITOR_INIT, MONITOR_INIT,
};

_Static_assert(sizeof monitors / sizeof monitors[0] == ANTI_RT_LOCK_COUNT,
               "one initializer per name of enum anti_rt_lock");

struct anti_rt_monitor *anti_rt_monitor_of(enum anti_rt_lock which)
{
    return &monitors[which];
}

struct anti_rt_monitor *anti_rt_monitor_new(void)
{
    struct anti_rt_monitor *m = malloc(sizeof *m);

    if (m == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&m->lock, NULL) != 0) {
        free(m);
        return NULL;
    }
    if (pthread_cond_init(&m->conditions[0], NULL) != 0) {
        pthread_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    if (pthread_cond_init(&m->conditions[1], NULL) != 0) {
        pthread_cond_destroy(&m->conditions[0]);
        pthread_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    return m;
}

void anti_rt_monitor_free(struct anti_rt_monitor *m)
{
    pthread_cond_destroy(&m->conditions[1]);
    pthread_cond_destroy(&m->conditions[0]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

void anti_rt_monitor_hold(struct anti_rt_monitor *m)
{
    pthread_mutex_lock(&m->lock);
}

void anti_rt_monitor_release(struct anti_rt_monitor *m)
{
    pthread_mutex_unlock(&m->lock);
}

void anti_rt_monitor_wait(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_wait(&m->conditions[condition], &m->lock);
}

void anti_rt_monitor_wake_one(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_signal(&m->conditions[condition]);
}

void anti_rt_monitor_wake_all(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_broadcast(&m->conditions[condition]);
}

#if defined(__APPLE__)

_Static_assert(sizeof(struct anti_rt_word) == sizeof(os_unfair_lock) &&
                   _Alignof(struct anti_rt_word) == _Alignof(os_unfair_lock),
               "the word of a Mutex is an os_unfair_lock");

void anti_rt_word_lock(struct anti_rt_word *w)
{
    os_unfair_lock_lock((os_unfair_lock_t)w);
}

void anti_rt_word_unlock(struct anti_rt_word *w)
{
    os_unfair_lock_unlock((os_unfair_lock_t)w);
}

#else

/* The private operations of futex(2), which the headers of musl do not
   name. */
#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

/* DESIGN: the word is 0 when free, 1 when held and 2 when held with a
   thread waiting, the mutex of Drepper's "Futexes Are Tricky". An
   unlock that finds 2 wakes one waiter, and a free lock costs one
   compare-and-swap to take and one exchange to give back. */
void anti_rt_word_lock(struct anti_rt_word *w)
{
    uint32_t c = 0;

    if (__atomic_compare_exchange_n(&w->opaque, &c, 1, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2) {
        c = __atomic_exchange_n(&w->opaque, 2, __ATOMIC_ACQUIRE);
    }
    while (c != 0) {
        syscall(SYS_futex, &w->opaque, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
        c = __atomic_exchange_n(&w->opaque, 2, __ATOMIC_ACQUIRE);
    }
}

void anti_rt_word_unlock(struct anti_rt_word *w)
{
    if (__atomic_exchange_n(&w->opaque, 0, __ATOMIC_RELEASE) == 2) {
        syscall(SYS_futex, &w->opaque, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    }
}

#endif

/* The body of a thread, on the heap until the thread takes it. */
struct start {
    void (*body)(void);
};

#if defined(__APPLE__)
/* DESIGN: on macOS the thread-local variables of a thread live in one
   heap block per image, which dyld allocates on the first access and
   frees when the thread ends. Taking the address of this variable makes
   the block of the runtime's image. */
static _Thread_local char thread_block;

const void *anti_rt_thread_block(void)
{
    return &thread_block;
}
#else
const void *anti_rt_thread_block(void)
{
    return NULL;
}
#endif

static void *thread_main(void *start)
{
    void (*body)(void) = ((struct start *)start)->body;

    free(start);
    body();
    return NULL;
}

int anti_rt_thread_start(void (*body)(void))
{
    struct start *start = malloc(sizeof *start);
    pthread_t thread;

    if (start == NULL) {
        return -1;
    }
    start->body = body;
    if (pthread_create(&thread, NULL, thread_main, start) != 0) {
        free(start);
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

int64_t anti_rt_processors(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);

    return n > 0 ? (int64_t)n : 1;
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

const void *anti_rt_library_image(const void *address)
{
    Dl_info info;

    if (dladdr(address, &info) == 0) {
        return NULL;
    }
    return info.dli_fbase;
}

/* The function the reader calls with each signal of the pipe. It is set
   once, under ANTI_RT_LOCK_SIGNALS and before the reader starts. A signal
   number of C fits the byte the handler writes. */
static int (*deliver_signal)(int64_t sig);

/* Written once, under ANTI_RT_LOCK_SIGNALS and before any handler that
   reads the write end is installed. */
static int pipe_ends[2] = {-1, -1};

/* The handler writes one byte and nothing else. Every function it could
   call beside write is undefined in a handler. */
static void on_raise(int sig)
{
    unsigned char byte = (unsigned char)sig;
    ssize_t written = write(pipe_ends[1], &byte, 1);

    /* A write that fails loses the signal, and a handler can do nothing
       else about it. */
    (void)written;
}

static void signal_reader(void)
{
    unsigned char byte;

    while (read(pipe_ends[0], &byte, 1) == 1) {
        deliver_signal((int64_t)byte);
    }
}

/* A pipe whose reader did not start is closed, so a later call starts
   afresh. */
int anti_rt_signal_route(int (*deliver)(int64_t sig))
{
    if (deliver_signal != NULL) {
        return 0;
    }
    if (pipe(pipe_ends) != 0) {
        return -1;
    }
    deliver_signal = deliver;
    if (anti_rt_thread_start(signal_reader) != 0) {
        deliver_signal = NULL;
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        pipe_ends[0] = -1;
        pipe_ends[1] = -1;
        return -1;
    }
    return 0;
}

void anti_rt_signal_catch(int64_t sig)
{
    signal((int)sig, on_raise);
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
