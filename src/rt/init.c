/* The start of the runtime: the flags that say it began and ran, the
   seed of the hashing collections, and the option of the backtrace that
   the configuration sets. */

#include "atomic.h"
#include "hash.h"
#include "rt.h"

/* 1 once anti_rt_init began, and 1 once it ran. main, or the constructor
   of a library for C, calls it before the program starts a thread. Two
   libraries for C in one program each bring the constructor, so the
   second call returns at once. A thread of the host may read ready while
   a library loads, so both are atomic. */
static int64_t started;
static int64_t ready;

int anti_rt_option_backtrace = -1;

void anti_rt_init(void)
{
    if (anti_rt_atomic_compare_swap(&started, (int64_t)sizeof started, 0,
                                    1) == 0) {
        return;
    }
    anti_rt_hash_seed_start();
    anti_rt_atomic_store(&ready, (int64_t)sizeof ready, 1);
}

int anti_rt_ready(void)
{
    return (int)anti_rt_atomic_load(&ready, (int64_t)sizeof ready);
}
