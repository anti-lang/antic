#include <stdio.h>
#include <stdlib.h>
#include "std.h"

/* DESIGN: the compiler builds the text that names the file, the line and
   the operation, and it lives in the read-only data of the module. The
   kind names the values that follow it, because the labels belong to the
   check and not to the site. A build without the checks removes every
   call of this routine and the strings with it. */
void anti_rt_check_failed(const unsigned char *text, int64_t length,
                          int32_t kind, int64_t a, int64_t b)
{
    fflush(stdout);
    fwrite(text, 1, (size_t)length, stderr);
    switch (kind) {
    case ANTI_CHECK_BOUNDS:
        fprintf(stderr, ": index %lld, length %lld", (long long)a,
                (long long)b);
        break;
    case ANTI_CHECK_OVERFLOW:
        fprintf(stderr, ": left %lld, right %lld", (long long)a,
                (long long)b);
        break;
    case ANTI_CHECK_VALUE:
        fprintf(stderr, ": value %lld", (long long)a);
        break;
    case ANTI_CHECK_VALUE_U:
        fprintf(stderr, ": value %llu", (unsigned long long)a);
        break;
    case ANTI_CHECK_LEFT:
        fprintf(stderr, ": left %lld", (long long)a);
        break;
    case ANTI_CHECK_LEFT_U:
        fprintf(stderr, ": left %llu", (unsigned long long)a);
        break;
    default:
        fprintf(stderr, ": count %lld, width %lld", (long long)a,
                (long long)b);
        break;
    }
    fputc('\n', stderr);
    abort();
}
