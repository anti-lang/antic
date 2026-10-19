#ifndef ANTIC_SAY_H
#define ANTIC_SAY_H

/* The interface of say.c: the one writer per tool of a message about a
   file, a tool or an option. The name of each tool stands there alone.
   A problem in a program goes through the diagnostics of antic. */

#include "attributes.h"

/* Write `antic: ` and the message that format and its arguments give to
   standard error. The message carries its own line feed, so one that a
   second call goes on with ends without it. */
void say_antic(const char *format, ...) ATTRIBUTE_PRINTF(1, 2);

/* The same with `anti: `, for the anti tool. */
void say_anti(const char *format, ...) ATTRIBUTE_PRINTF(1, 2);

#endif
