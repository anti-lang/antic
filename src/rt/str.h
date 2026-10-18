/* The layout of Anti's str, as a function of the runtime takes and
   gives it: the bytes and their count. The NUL after them lies outside
   the count. */

#ifndef ANTI_RT_STR_H
#define ANTI_RT_STR_H

#include <stdint.h>

struct anti_text {
    const unsigned char *ptr;
    int64_t len;
};

#endif
