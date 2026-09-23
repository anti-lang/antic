/* Operating system signals for anti.os. A signal handler runs on a stack
   the program knows nothing about, so it does the least it can. It writes
   one byte into a pipe of the runtime's own. A thread reads that pipe and
   calls the function the program registered.

   DESIGN: the self-pipe is what makes the callback an ordinary function.
   Anything else would run Anti code inside a handler, where a call of
   malloc or of the runtime is undefined. Windows has no signals of that
   kind. Its console control handler runs on a thread of its own. The C
   runtime calls a handler of raise on the thread that raised. Both call
   the function directly. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <string.h>

#include <signal.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "atomic.h"
#include "signal.h"

/* The signals a program may wait for. The numbers are the C ones, which
   every platform the compiler targets spells the same way. */
#define ANTI_SIGNAL_MAX 32

/* DESIGN: the registered functions, whether the reader or the console
   handler runs, and the pipe are state of the process, because a signal
   is. Any thread may register, while the reader thread or the console
   thread of Windows reads the table. The lock guards all three. Each
   reader copies the function under it and calls the copy after, so a
   function that registers another does not wait for itself. pending is
   atomic instead, because a handler of raise writes it. */
static void (*handlers[ANTI_SIGNAL_MAX])(int64_t sig);
static int64_t pending;
static int started;

#if defined(_WIN32)
static SRWLOCK lock = SRWLOCK_INIT;
static void hold(void) { AcquireSRWLockExclusive(&lock); }
static void release(void) { ReleaseSRWLockExclusive(&lock); }
#else
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void hold(void) { pthread_mutex_lock(&lock); }
static void release(void) { pthread_mutex_unlock(&lock); }
#endif

/* The function registered for sig, read under the lock. */
static void (*handler_of(int64_t sig))(int64_t)
{
    void (*f)(int64_t);

    hold();
    f = handlers[sig];
    release();
    return f;
}

#if !defined(_WIN32)
/* Written once, under the lock and before any handler that reads the
   write end is installed. */
static int pipe_ends[2] = {-1, -1};

/* The handler writes one byte and nothing else. Every function it could
   call beside write is undefined in a handler. */
static void on_raise(int sig)
{
    unsigned char byte = (unsigned char)sig;
    ssize_t written = write(pipe_ends[1], &byte, 1);

    (void)written;
}

static void *reader(void *unused)
{
    unsigned char byte;

    (void)unused;
    while (read(pipe_ends[0], &byte, 1) == 1) {
        int64_t sig = (int64_t)byte;
        void (*f)(int64_t);
        anti_rt_atomic_store(&pending, (int64_t)sizeof pending, sig);
        if (sig <= 0 || sig >= ANTI_SIGNAL_MAX) {
            continue;
        }
        f = handler_of(sig);
        if (f != NULL) {
            f(sig);
        }
    }
    return NULL;
}

/* Start the reader once. The caller holds the lock. A pipe whose reader
   did not start is closed, so a later call starts afresh. */
static int start_reader(void)
{
    pthread_t thread;

    if (started) {
        return 1;
    }
    if (pipe(pipe_ends) != 0) {
        return 0;
    }
    if (pthread_create(&thread, NULL, reader, NULL) != 0) {
        goto failed;
    }
    pthread_detach(thread);
    started = 1;
    return 1;

failed:
    close(pipe_ends[0]);
    close(pipe_ends[1]);
    pipe_ends[0] = -1;
    pipe_ends[1] = -1;
    return 0;
}
#else
/* The console control handler runs on a thread Windows makes, so it
   calls the function the program registered without a pipe. */
static BOOL WINAPI on_console(DWORD event)
{
    int64_t sig = event == CTRL_BREAK_EVENT ? SIGBREAK_SIGNAL : SIGINT_SIGNAL;
    void (*f)(int64_t) = handler_of(sig);

    anti_rt_atomic_store(&pending, (int64_t)sizeof pending, sig);
    if (f == NULL) {
        return FALSE;
    }
    f(sig);
    return TRUE;
}

/* The C runtime of Windows answers raise from a table of its own. Its
   default ends the program with code 3, and the console handler above
   never sees a raised signal. The C runtime resets the handler before it
   calls it, so the handler installs itself again. */
static void __cdecl on_raise(int sig)
{
    void (*f)(int64_t) = handler_of(sig);

    signal(sig, on_raise);
    anti_rt_atomic_store(&pending, (int64_t)sizeof pending, sig);
    if (f != NULL) {
        f(sig);
    }
}
#endif

void anti_rt_on_signal(int64_t sig, void (*f)(int64_t))
{
    if (sig <= 0 || sig >= ANTI_SIGNAL_MAX) {
        return;
    }
    hold();
    handlers[sig] = f;
#if defined(_WIN32)
    if (!started) {
        SetConsoleCtrlHandler(on_console, TRUE);
        started = 1;
    }
#else
    if (!start_reader()) {
        release();
        return;
    }
#endif
    release();
    signal((int)sig, on_raise);
}

int64_t anti_rt_signal_pending(void)
{
    return anti_rt_atomic_swap(&pending, (int64_t)sizeof pending, 0);
}
