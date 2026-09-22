#include "../binary_stdio.h"
#include "check.h"
#include <string.h>
#include "symbols.h"

/* The name that anti_coff_demangle gives for symbol, or "" when it gives
   none. */
static void demangles(const char *symbol, size_t room, const char *expected)
{
    char out[128];
    size_t length;

    memset(out, 'x', sizeof out);
    length = anti_coff_demangle(symbol, strlen(symbol), out, room);
    CHECK(length < sizeof out);
    out[length < sizeof out ? length : 0] = 0;
    CHECK_STR(out, expected);
}

void test_symbols(void)
{
    /* The forms of "Symbols" in docs/decisions.md. */
    demangles("_A3com7example8geometry3vec_push", 128,
              "com.example.geometry.vec.push");
    demangles("_A8geometry_length", 128, "geometry.length");
    /* A segment may hold `_`, and a method keeps its `.`. */
    demangles("_A11stack_trace_inner", 128, "stack_trace.inner");
    demangles("_A4anti4lang_StackTrace.capture", 128,
              "anti.lang.StackTrace.capture");
    demangles("_A4anti2rt_main", 128, "anti.rt.main");
    /* A name after the segments may start with a digit. */
    demangles("_A11stack_trace_0", 128, "stack_trace.0");
    /* Anything else is not a mangled name. */
    demangles("main", 128, "");
    demangles("_A", 128, "");
    demangles("_Ax_main", 128, "");
    demangles("_A4anti", 128, "");
    demangles("_A9anti_main", 128, "");
    demangles("_A4anti2rt", 128, "");
    demangles("_A4anti2rt_", 128, "");
    demangles("_A0_main", 128, "");
    /* A name that does not fit is none. */
    demangles("_A8geometry_length", 14, "");
    demangles("_A8geometry_length", 15, "geometry.length");
    demangles("_A8geometry_length", 8, "");
}
