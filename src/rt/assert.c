/* The failure routines of the runtime: one line to standard error, then
   exit, abort or return. Also the failed assertion of a test. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "fail.h"

/* The one form of every line the runtime writes to standard error. */
ANTI_RT_PRINTF(1, 0) static void write_line(const char *format,
                                           va_list rest)
{
    fflush(stdout);
    vfprintf(stderr, format, rest);
    fputc('\n', stderr);
    fflush(stderr);
}

void anti_rt_fail_exit(int status, const char *format, ...)
{
    va_list rest;

    va_start(rest, format);
    write_line(format, rest);
    va_end(rest);
    exit(status);
}

void anti_rt_fail_abort(const char *format, ...)
{
    va_list rest;

    va_start(rest, format);
    write_line(format, rest);
    va_end(rest);
    abort();
}

void anti_rt_fail_memory(const char *format, ...)
{
    va_list rest;

    fflush(stdout);
    fputs("anti: out of memory ", stderr);
    va_start(rest, format);
    write_line(format, rest);
    va_end(rest);
    abort();
}

void anti_rt_note(const char *format, ...)
{
    va_list rest;

    va_start(rest, format);
    write_line(format, rest);
    va_end(rest);
}

_Noreturn void anti_rt_start_out_of_memory(void)
{
    anti_rt_fail_exit(70, "anti: out of memory at program start");
}

void *anti_rt_start_alloc(size_t size)
{
    void *p = malloc(size == 0 ? 1 : size);

    if (p == NULL) {
        anti_rt_start_out_of_memory();
    }
    return p;
}

/* DESIGN: `anti test` names the test it is about to run, so a failed
   assertion reports which one failed before it prints the position the
   compiler built. Nothing else sets a name, and the report line is then
   the assertion alone. The name is the runner's own read-only text, so
   this keeps the pointer and copies nothing. A worker of the test may
   fail while the runner names the next, so ANTI_RT_LOCK_TEST guards the
   pointer and the length as one pair. */
static const unsigned char *running_name;
static int64_t running_length;

void anti_rt_test_running(const unsigned char *name, int64_t length)
{
    anti_rt_lock_hold(ANTI_RT_LOCK_TEST);
    running_name = name;
    running_length = length;
    anti_rt_lock_release(ANTI_RT_LOCK_TEST);
}

/* DESIGN: the compiler builds the text of a failed assertion, and it
   lives in the read-only data of the module. This routine prints that
   one string and ends the program. A release build removes every call
   of it and the strings with them. */
_Noreturn void anti_rt_assert_failed(const unsigned char *text,
                                     int64_t length)
{
    const unsigned char *name;
    int64_t name_length;

    anti_rt_lock_hold(ANTI_RT_LOCK_TEST);
    name = running_name;
    name_length = running_length;
    anti_rt_lock_release(ANTI_RT_LOCK_TEST);
    if (name != NULL) {
        anti_rt_fail_abort("FAIL %.*s\n%.*s", anti_rt_print_width(name_length),
                           (const char *)name, anti_rt_print_width(length),
                           (const char *)text);
    }
    anti_rt_fail_abort("%.*s", anti_rt_print_width(length),
                       (const char *)text);
}
