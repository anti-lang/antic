#include "rt.h"

static int ready;

int anti_rt_option_backtrace = -1;

void anti_rt_init(void)
{
    ready = 1;
}

int anti_rt_ready(void)
{
    return ready;
}
