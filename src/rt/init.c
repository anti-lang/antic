#include "atomic.h"
#include "rt.h"

/* 1 once anti_rt_init ran. main, or the constructor of a shared
   library, writes it before the program starts a thread. A static
   library for C sets it on its first use, where two threads of the
   host may ask at once, so it is atomic. */
static int64_t ready;

int anti_rt_option_backtrace = -1;

void anti_rt_init(void)
{
    anti_rt_hash_seed_start();
    anti_rt_atomic_store(&ready, (int64_t)sizeof ready, 1);
}

int anti_rt_ready(void)
{
    return (int)anti_rt_atomic_load(&ready, (int64_t)sizeof ready);
}

/* DESIGN: the runtime marks what it keeps until exit through a function
   that a program of --memory-checks replaces. The definition here is
   weak on ELF and Mach-O, so the one of the program wins and every other
   program pays one call that returns. Windows has no leak check in its
   runtime of AddressSanitizer, so no program replaces it there. */
#if defined(_WIN32)
void anti_rt_memory_kept(const void *p)
#else
__attribute__((weak)) void anti_rt_memory_kept(const void *p)
#endif
{
    (void)p;
}
