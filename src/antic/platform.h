#ifndef ANTIC_PLATFORM_H
#define ANTIC_PLATFORM_H

#include <stdbool.h>
#include <stdio.h>

/* The calls of the C library that antic and anti make differently on
   Windows, whose C runtime deprecates them. Each behaves as the C11
   function it replaces does on every system. */

/* Open the file at path in binary mode, for reading, or for writing when
   writing is true. Writing creates the file or empties the one that is
   there. Returns NULL when it cannot. The caller closes the file with
   fclose. */
FILE *platform_open(const char *path, bool writing);

/* The value of the environment variable name, or NULL when it is unset.
   The value stays valid until the program ends, and the caller never
   frees it. */
const char *platform_getenv(const char *name);

#endif
