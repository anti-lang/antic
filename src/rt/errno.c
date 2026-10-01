#include <errno.h>

#include "std.h"

/* DESIGN: errno is a macro that expands to a thread-local lvalue, so an
   Anti `extern fn` cannot name it. The message of an errno value and the
   code and message of a Win32 error differ per system, and stand in the
   platform files. */

int32_t anti_rt_errno(void)
{
    return (int32_t)errno;
}
