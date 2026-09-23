/* A shared library that rt_plugin_table_tests loads: the table of a
   plugin with one entry and one class. The table is writable, so the test
   damages one value of it before each load. */
#include "../binary_stdio.h"
#include "../../src/rt/plugin.h"
#include "../../src/rt/registry.h"

#if defined(_WIN32)
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

static void init(void *object)
{
    (void)object;
}

static const int64_t chain[1] = {1};

EXPORTED struct anti_descriptor anti_bad_class = {
    (const unsigned char *)"Impl", 4, NULL, 32, 0, NULL, 0, NULL, NULL, 0, 0,
    NULL, (const unsigned char *)"1.0.0", 5, NULL
};

EXPORTED struct anti_provides anti_bad_entry = {
    (const unsigned char *)"host.Service", 12, NULL, &anti_bad_class, init,
    16, 0, chain, 1, 0, 16, (const unsigned char *)"1.0.0", 5
};

EXPORTED struct anti_class anti_bad_classes[1] = {
    {&anti_bad_class, init, (const unsigned char *)"bad", 3, 0}
};

EXPORTED struct anti_provided anti_rt_provides = {
    1, &anti_bad_entry, (const unsigned char *)"0.0.0", 5, 1,
    anti_bad_classes
};
