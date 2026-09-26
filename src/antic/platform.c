/* The platform layer of antic and anti, which docs/c-guidelines.md names
   under rule 22. It holds the calls of the C library that the C runtime
   of Windows deprecates. No other file of the tools then needs a branch
   for them, and no build needs _CRT_SECURE_NO_WARNINGS. */

#include "platform.h"

#include <stdlib.h>

#if defined(_WIN32)
#include <share.h>

/* DESIGN: fopen of the Windows C runtime is _fsopen with _SH_DENYNO,
   which shares the file for reading and writing. The C runtime
   deprecates the first and not the second, so the call is the same one
   by the name that is not deprecated. fopen_s would lock the file
   against every other open. */
FILE *platform_open(const char *path, bool writing)
{
    return _fsopen(path, writing ? "wb" : "rb", _SH_DENYNO);
}

/* DESIGN: getenv gives storage that the program never frees. _dupenv_s,
   the call the C runtime names instead, gives a copy the caller frees,
   which is kept here until the program ends. antic and anti read a
   handful of variables, each once. */
const char *platform_getenv(const char *name)
{
    char *value = NULL;
    size_t size = 0;

    if (_dupenv_s(&value, &size, name) != 0) {
        return NULL;
    }
    return value;
}

#else

FILE *platform_open(const char *path, bool writing)
{
    return fopen(path, writing ? "wb" : "rb");
}

const char *platform_getenv(const char *name)
{
    return getenv(name);
}

#endif
