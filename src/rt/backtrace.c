/* Whether `fail` captures the frames of the error it gives.

   DESIGN: the build of the program decides whether backtraces are on.
   A library file compiled once serves a dev build and a release build
   alike. So the pass over the whole program writes the default into the
   program as `anti_rt_backtrace_default`, as it writes the registry, and
   every `fail` asks the function below. It stands in a file of its own,
   so a program that captures a trace and never fails links no reference
   to the default. */
#include "trace.h"

#include "rt.h"

struct anti_backtrace_default {
    int64_t on;
};

extern const struct anti_backtrace_default anti_rt_backtrace_default;

bool anti_rt_backtrace_on(void)
{
    if (anti_rt_option_backtrace >= 0) {
        return anti_rt_option_backtrace != 0;
    }
    return anti_rt_backtrace_default.on != 0;
}
