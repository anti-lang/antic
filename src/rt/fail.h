/* The failure routines of the runtime: the one form of every failure it
   reports in src/rt/assert.c, and the failed assertions, checks and
   casts of a program in assert.c, check.c and cast.c. */

#ifndef ANTI_RT_FAIL_H
#define ANTI_RT_FAIL_H

#include <stdint.h>

#include "platform.h"

/* DESIGN: the failure routine of the runtime, in src/rt/assert.c. Every
   failure the runtime reports goes through it, so each has one form. It
   flushes standard output, writes the formatted text and a newline to
   standard error, and ends the program. anti_rt_fail_exit ends it with
   the status, as a refusal at start does with 70. anti_rt_fail_abort
   aborts, for a state the program cannot go on from. */
ANTI_RT_PRINTF(2, 3) _Noreturn void anti_rt_fail_exit(int status,
                                                const char *format, ...);
ANTI_RT_PRINTF(1, 2) _Noreturn void anti_rt_fail_abort(const char *format,
                                                 ...);

/* The same form for a line that reports and goes on, as a library that
   discovery passes over. */
ANTI_RT_PRINTF(1, 2) void anti_rt_note(const char *format, ...);

/* Print the text of a failed assertion to standard error and abort. The
   compiler built the text, so this adds only a newline. */
_Noreturn void anti_rt_assert_failed(const unsigned char *text,
                                     int64_t length);

/* Name the test that is running, which a failed assertion reports before
   its position. The runner of `anti test` is the one caller. */
void anti_rt_test_running(const unsigned char *name, int64_t length);

/* Which values a failed dev-mode check prints after its text. The
   compiler names the file, the line and the operation. The kind names
   the labels of the values. */
enum anti_check {
    ANTI_CHECK_BOUNDS,      /* An index and a length. */
    ANTI_CHECK_OVERFLOW,    /* The two operands of + - or *. */
    ANTI_CHECK_VALUE,       /* One signed value. */
    ANTI_CHECK_VALUE_U,     /* One unsigned value. */
    ANTI_CHECK_LEFT,        /* The signed left operand of / or %. */
    ANTI_CHECK_LEFT_U,      /* The unsigned left operand of / or %. */
    ANTI_CHECK_SHIFT,       /* A shift count and the width of its type. */
    ANTI_CHECK_TEXT         /* No value: the text says it all. */
};

/* Print the text of a failed check, the values the kind names, and
   abort. */
_Noreturn void anti_rt_check_failed(const unsigned char *text,
                                    int64_t length, int32_t kind, int64_t a,
                                    int64_t b);

/* Print the text of a walk whose collection changed, which names the
   loop and the collection. Then print the file and the line of the last
   change when the collection recorded one, and abort. */
_Noreturn void anti_rt_walk_changed(const unsigned char *text,
                                    int64_t length,
                                    const unsigned char *file,
                                    int64_t file_length, int64_t line);

/* Print the name of the class a checked cast wanted and abort. The name
   is not a C string, so its length comes with it. */
_Noreturn void anti_rt_cast_failed(const unsigned char *name,
                                   int64_t length);

/* Print the name of the class an object with a zero table was taken for
   and abort. */
_Noreturn void anti_rt_table_unset(const unsigned char *name,
                                   int64_t length);

#endif
