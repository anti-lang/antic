#ifndef ANTIC_HEADER_H
#define ANTIC_HEADER_H

#include <stdbool.h>
#include <stddef.h>

#include "sema.h"
#include "text.h"

/* The suffix of the C header that a library for C carries. antic writes
   the file and `anti build` copies it into `dist/`, so the name stands
   here once. */
#define HEADER_SUFFIX ".h"

/* The C header of a library for C: the export structs, unions, constants
   and functions of the given interfaces, in C types. name is the library
   name, which names the file name.h and its include guard. With bundled
   set, the top comment says that the archive holds the runtime. */
void header_write(struct text *out, const char *name,
                  const struct interface *const *ifaces, size_t count,
                  bool bundled);

#endif
