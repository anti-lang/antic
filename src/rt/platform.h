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

/* DESIGN: the locks the runtime keeps at file scope. Each has an
   initializer of the system, so it is ready before main, and the
   storage stands in the platform file. A new one is a new name here. */
enum anti_rt_lock {
    ANTI_RT_LOCK_CONF,          /* the keys of src/rt/conf.c */
    ANTI_RT_LOCK_COUNT
};

void anti_rt_lock_hold(enum anti_rt_lock which);
void anti_rt_lock_release(enum anti_rt_lock which);

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
