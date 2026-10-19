/* The writers of a message of antic and of anti about a file, a tool or
   an option, each with the name of its tool before the message. */

#include "say.h"

#include <stdarg.h>
#include <stdio.h>

void say_antic(const char *format, ...)
{
    va_list args;

    fputs("antic: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void say_anti(const char *format, ...)
{
    va_list args;

    fputs("anti: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
