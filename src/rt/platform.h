/* The platform layer of the runtime: what the rest of src/rt asks of the
   system it runs on.

   DESIGN: rule 22 of docs/c-guidelines.md. A `#if` on the host system
   stands only in this header, in platform_posix.c and in
   platform_windows.c. The build keeps one list of runtime sources, so
   both .c files are compiled for every target. Each holds its code
   inside one `#if` of its own system. The other file of a
   target then compiles to nothing. Every function below takes and gives
   text as UTF-8, whatever the system uses underneath. */
#ifndef ANTI_RT_PLATFORM_H
#define ANTI_RT_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* 1 in the runtime of a Windows target and 0 elsewhere. anti.os asks it. */
int64_t anti_rt_is_windows(void);

/* DESIGN: a monitor is a lock of the system with two condition
   variables, numbered 0 and 1. A thread waits on one while it holds the
   lock, and the wait gives the lock back until a wake. A monitor is the
   one form of a lock and a condition the runtime has: the pool of
   src/rt/threads.c, the channels and the selects of src/rt/sync.c and
   every lock at file scope. */
struct anti_rt_monitor;

/* The locks the runtime keeps at file scope. Each is a monitor with an
   initializer of the system, so it is ready before main, and the storage
   stands in the platform file. A new one is a new name here. */
enum anti_rt_lock {
    ANTI_RT_LOCK_CONF,          /* the keys of src/rt/conf.c */
    ANTI_RT_LOCK_PLUGINS,       /* the slots of src/rt/loaded.c */
    ANTI_RT_LOCK_SIGNALS,       /* the functions of src/rt/signal.c */
    ANTI_RT_LOCK_POOL,          /* the pool and its jobs, src/rt/threads.c */
    ANTI_RT_LOCK_SELECT,        /* the selects that wait, src/rt/sync.c */
    ANTI_RT_LOCK_ORDERS,        /* the orders of the locks, src/rt/lock.c */
    ANTI_RT_LOCK_COUNT
};

struct anti_rt_monitor *anti_rt_monitor_of(enum anti_rt_lock which);

/* A monitor on the heap, or NULL when the system or the memory gives
   none. anti_rt_monitor_free ends it, and no thread may hold it then. */
struct anti_rt_monitor *anti_rt_monitor_new(void);
void anti_rt_monitor_free(struct anti_rt_monitor *m);

void anti_rt_monitor_hold(struct anti_rt_monitor *m);
void anti_rt_monitor_release(struct anti_rt_monitor *m);

/* Wait on condition, 0 or 1, while the thread holds m. A wait may end
   without a wake, so the caller checks its own state in a loop. */
void anti_rt_monitor_wait(struct anti_rt_monitor *m, int condition);
void anti_rt_monitor_wake_one(struct anti_rt_monitor *m, int condition);
void anti_rt_monitor_wake_all(struct anti_rt_monitor *m, int condition);

static inline void anti_rt_lock_hold(enum anti_rt_lock which)
{
    anti_rt_monitor_hold(anti_rt_monitor_of(which));
}

static inline void anti_rt_lock_release(enum anti_rt_lock which)
{
    anti_rt_monitor_release(anti_rt_monitor_of(which));
}

/* DESIGN: the word of a Mutex and of the hidden lock of a synchronized
   object is one word of the program's own memory, and the system keeps
   nothing for it until a thread has to wait: a futex word on Linux,
   os_unfair_lock on macOS and SRWLOCK on Windows. Zero is the unlocked
   state of all three, so memory that nothing wrote holds an unlocked
   word. The word is four bytes on Linux and macOS and eight on Windows,
   which the layout of the compiler gives per target. The platform files
   check that the struct has the size and the alignment of the lock of
   the system. */
#if defined(_WIN32)
struct anti_rt_word {
    void *opaque;
};
#else
struct anti_rt_word {
    uint32_t opaque;
};
#endif

void anti_rt_word_lock(struct anti_rt_word *w);
void anti_rt_word_unlock(struct anti_rt_word *w);

/* Start a thread that runs body and ends with it. Nothing joins the
   thread. Returns 0, or -1 when the system starts none. */
int anti_rt_thread_start(void (*body)(void));

/* The block of the heap that holds the thread-local variables of the
   calling thread in the image of the runtime, made by the call, or NULL
   where the system keeps them elsewhere. Only macOS has one. */
const void *anti_rt_thread_block(void);

/* The processors the machine that runs the program has online, at
   least 1. */
int64_t anti_rt_processors(void);

/* The value of the environment variable name in UTF-8, in memory the
   caller frees, into *value. *value is NULL when the variable is unset
   or empty. Returns 0, or -1 when memory runs out. Windows reads the
   variable as UTF-16, so a value of any length and any character comes
   through. A value that is no valid UTF-16 counts as unset. */
int anti_rt_getenv(const char *name, char **value);

/* 1 when path names a file without a directory to start from: `/` on
   every system, and `\` and a drive letter with a colon on Windows. */
int anti_rt_path_is_absolute(const char *path);

/* The last separator of the directories of path, or NULL when it has
   none. `/` on every system, and `\` as well on Windows. */
const char *anti_rt_path_last_separator(const char *path);

/* The system's loader of shared libraries. open takes a path in UTF-8
   and gives NULL when it fails, and error then gives the reason. A reason
   the system writes itself goes to text, of size bytes. The reason
   dlerror gives is kept per thread by the C library. */
void *anti_rt_library_open(const char *path);
void anti_rt_library_close(void *handle);
void *anti_rt_library_symbol(void *handle, const char *name);
const char *anti_rt_library_error(char *text, size_t size);

/* The base of the image of the program or of a library that address
   lies in, or NULL when it lies in none. The loader of the system holds
   a lock of its own while it answers. */
const void *anti_rt_library_image(const void *address);

/* DESIGN: a signal handler runs on a stack the program knows nothing
   about, so it does the least it can. On macOS and Linux it writes one
   byte into a pipe of the runtime's own, and a thread of the runtime
   reads the pipe and calls deliver. That self-pipe is what makes the
   function of the program an ordinary function. Anything else would run
   Anti code inside a handler, where a call of malloc or of the runtime
   is undefined. Windows has no signals of that kind. Its console control
   handler runs on a thread of its own, and the C runtime calls a handler
   of raise on the thread that raised. Both call deliver directly.
   deliver returns 1 when the program has a function for the signal. */

/* Start the route of signals to deliver, once. The caller holds
   ANTI_RT_LOCK_SIGNALS. Returns 0, or -1 when the system gave no pipe or
   no thread, and a later call then tries again. */
int anti_rt_signal_route(int (*deliver)(int64_t sig));

/* Send the signal sig through the route from now on. */
void anti_rt_signal_catch(int64_t sig);

/* Fill the count bytes at out from the random source of the system:
   arc4random on macOS, getrandom on Linux and rand_s on Windows. Returns
   0, or -1 when the system gave fewer bytes. */
int anti_rt_entropy(void *out, size_t count);

/* DESIGN: a Sleep of Windows takes 32 bits of milliseconds, and the
   largest of them means to wait for ever. A long wait is therefore a
   loop of steps of at most a day. A step rounds up to the whole
   millisecond, so that a wait is never shorter than asked, as nanosleep
   guarantees on the other systems. */
#define ANTI_RT_SLEEP_STEP_MAX 86400000

/* The milliseconds of the next Sleep of a wait with nanoseconds left,
   which is above 0. */
static inline uint32_t anti_rt_sleep_step(int64_t nanoseconds)
{
    int64_t milliseconds = nanoseconds / 1000000 +
                           (nanoseconds % 1000000 != 0 ? 1 : 0);

    return milliseconds > ANTI_RT_SLEEP_STEP_MAX
               ? (uint32_t)ANTI_RT_SLEEP_STEP_MAX
               : (uint32_t)milliseconds;
}

#endif
