#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "std.h"

/* The one form of every line the runtime writes to standard error. */
static void write_line(const char *format, va_list rest)
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

void anti_rt_note(const char *format, ...)
{
    va_list rest;

    va_start(rest, format);
    write_line(format, rest);
    va_end(rest);
}

/* DESIGN: `anti test` names the test it is about to run, so a failed
   assertion reports which one failed before it prints the position the
   compiler built. Nothing else sets a name, and the report line is then
   the assertion alone. The name is the runner's own read-only text, so
   this keeps the pointer and copies nothing. */
static const unsigned char *running_name;
static int64_t running_length;

void anti_rt_test_running(const unsigned char *name, int64_t length)
{
    running_name = name;
    running_length = length;
}

/* DESIGN: the compiler builds the text of a failed assertion, and it
   lives in the read-only data of the module. This routine prints that
   one string and ends the program. A release build removes every call
   of it and the strings with them. */
void anti_rt_assert_failed(const unsigned char *text, int64_t length)
{
    if (running_name != NULL) {
        anti_rt_fail_abort("FAIL %.*s\n%.*s", (int)running_length,
                           (const char *)running_name, (int)length,
                           (const char *)text);
    }
    anti_rt_fail_abort("%.*s", (int)length, (const char *)text);
}
