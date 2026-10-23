/* The end of a run whose memory ran out in a file of the runtime that
   antic and anti build in, src/rt/toml.c and src/rt/regex.c.

   DESIGN: those files stop through anti_rt_out_of_memory, which the
   runtime defines in src/rt/mem.c. The tools take the definition below
   instead, which ends the run through alloc.c, the one owner of the
   memory of antic. It stands in a file of its own, so a unit test that
   links mem.c and the library of antic takes the runtime's and no linker
   sees two. */

#include <stdint.h>

#include "alloc.h"
#include "../rt/std.h"

void anti_rt_out_of_memory(int64_t size)
{
    alloc_out_of_memory_for((long long)size);
}
