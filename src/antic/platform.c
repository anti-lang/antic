/* The platform layer of antic and anti, which docs/c-guidelines.md names
   under rule 22. It holds the calls of the C library that the C runtime
   of Windows deprecates. No other file of the tools then needs a branch
   for them, and no build needs _CRT_SECURE_NO_WARNINGS. */

#include "platform.h"

#include <stdlib.h>

#if defined(_WIN32)
#include <errno.h>
#include <share.h>
#include <windows.h>

/* DESIGN: fopen of the Windows C runtime is _fsopen with _SH_DENYNO,
   which shares the file for reading and writing. The C runtime
   deprecates the first and not the second, so the call is the same one
   by the name that is not deprecated. fopen_s would lock the file
   against every other open. */
/* DESIGN: Windows lets a scanner of the machine hold a program that just
   ran, or was just written, without sharing it for writing, for a moment.
   A write then fails with EACCES. anti build writes the program into
   dist/ at every build, right after a user ran it, and failed three builds
   in ten on the Windows VM. A write waits for the file up to two seconds,
   in steps of 50 ms, as the file copies of CMake retry. A read never
   waits, and a file that stays locked still fails. */
#define OPEN_WRITE_TRIES 40
#define OPEN_WRITE_STEP_MS 50

FILE *platform_open(const char *path, bool writing)
{
    FILE *f = _fsopen(path, writing ? "wb" : "rb", _SH_DENYNO);
    int tries = 1;

    while (f == NULL && writing && errno == EACCES &&
           tries < OPEN_WRITE_TRIES) {
        Sleep(OPEN_WRITE_STEP_MS);
        f = _fsopen(path, "wb", _SH_DENYNO);
        tries++;
    }
    return f;
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
