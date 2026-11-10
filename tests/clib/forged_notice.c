/* A C program that carries a notice `anti_licenses` of its own and calls
   a library for C that prints the build id of its caller. FORM picks the
   notice: 1 is one in the form antic writes, 2 lacks the begin marker and
   3 ends its build line without a line feed. The trace reads an id from
   the first alone. */
#include "../binary_stdio.h"

#include "witness.h"

/* The symbol the trace looks for in the symbol table of the program. */
extern const char anti_licenses[];

#if FORM == 1
const char anti_licenses[] = "ANTI_LICENSES_BEGIN\nbuild 1234\n";
#elif FORM == 2
const char anti_licenses[] = "not a notice\nbuild 1234\n";
#else
const char anti_licenses[] = "ANTI_LICENSES_BEGIN\nbuild 1234";
#endif

int main(void)
{
    caller_build_id();
    return 0;
}
