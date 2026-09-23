#include <stdio.h>
#include <stdlib.h>
#include "std.h"

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

/* DESIGN: the text of a failed assertion is built by the compiler and
   lives in the read-only data of the module, so this routine prints one
   string and ends the program. A release build removes every call of it
   and the strings with them. */
void anti_rt_assert_failed(const unsigned char *text, int64_t length)
{
    fflush(stdout);
    if (running_name != NULL) {
        fputs("FAIL ", stderr);
        fwrite(running_name, 1, (size_t)running_length, stderr);
        fputc('\n', stderr);
    }
    fwrite(text, 1, (size_t)length, stderr);
    fputc('\n', stderr);
    abort();
}
