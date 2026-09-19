#include <stdio.h>
#include <stdlib.h>
#include "std.h"

/* DESIGN: `p as *T` on a class pointer traps when the object is of no
   class below T. The compiler passes the name of T, which lives in the
   read-only data of the module. This routine prints one message and ends
   the program. `p as? *T` gives null instead and never calls it. */
void anti_rt_cast_failed(const unsigned char *name, int64_t length)
{
    fflush(stdout);
    fputs("cast failed: the object is no ", stderr);
    fwrite(name, 1, (size_t)length, stderr);
    fputc('\n', stderr);
    abort();
}

/* DESIGN: an object whose table is zero was never made, as an element of
   `alloc(T, n)` that the program has not filled. Every read of its table
   would jump through address zero, so the check stops the program first
   with the class the object was taken for. The runtime calls check in
   every mode, and lowering adds the check to a dispatch in dev mode. */
void anti_rt_table_unset(const unsigned char *name, int64_t length)
{
    fflush(stdout);
    fputs("table not set: the object is no ", stderr);
    fwrite(name, 1, (size_t)length, stderr);
    fputc('\n', stderr);
    abort();
}
