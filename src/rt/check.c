#include "std.h"

/* DESIGN: the compiler builds the text that names the file, the line and
   the operation, and it lives in the read-only data of the module. The
   kind names the values that follow it, because the labels belong to the
   check and not to the site. A build without the checks removes every
   call of this routine and the strings with it. */
void anti_rt_check_failed(const unsigned char *text, int64_t length,
                          int32_t kind, int64_t a, int64_t b)
{
    int n = (int)length;
    const char *at = (const char *)text;

    switch (kind) {
    case ANTI_CHECK_BOUNDS:
        anti_rt_fail_abort("%.*s: index %lld, length %lld", n, at,
                           (long long)a, (long long)b);
    case ANTI_CHECK_OVERFLOW:
        anti_rt_fail_abort("%.*s: left %lld, right %lld", n, at,
                           (long long)a, (long long)b);
    case ANTI_CHECK_VALUE:
        anti_rt_fail_abort("%.*s: value %lld", n, at, (long long)a);
    case ANTI_CHECK_VALUE_U:
        anti_rt_fail_abort("%.*s: value %llu", n, at,
                           (unsigned long long)a);
    case ANTI_CHECK_LEFT:
        anti_rt_fail_abort("%.*s: left %lld", n, at, (long long)a);
    case ANTI_CHECK_LEFT_U:
        anti_rt_fail_abort("%.*s: left %llu", n, at, (unsigned long long)a);
    default:
        anti_rt_fail_abort("%.*s: count %lld, width %lld", n, at,
                           (long long)a, (long long)b);
    }
}

/* DESIGN: a collection records the place of each change with `here`, and
   the loop that finds the counts apart passes the last one on. A place of
   line 0 was never written, and the message then names the loop alone. */
void anti_rt_walk_changed(const unsigned char *text, int64_t length,
                          const unsigned char *file, int64_t file_length,
                          int64_t line)
{
    if (line == 0) {
        anti_rt_fail_abort("%.*s", (int)length, (const char *)text);
    }
    anti_rt_fail_abort("%.*s: changed at %.*s:%lld", (int)length,
                       (const char *)text, (int)file_length,
                       (const char *)file, (long long)line);
}
