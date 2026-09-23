/* A shared library that rt_plugin_tests loads: the table of a plugin with
   nothing in it, built for the runtime version the test gives. The test
   builds it twice, as two libraries with a table each. */
#include "../../src/rt/plugin.h"

#if defined(_WIN32)
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED const struct anti_provided anti_rt_provides = {
    0, NULL, (const unsigned char *)"0.0.0", 5, 0, NULL
};
