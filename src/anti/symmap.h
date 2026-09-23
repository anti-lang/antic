#ifndef ANTI_SYMMAP_H
#define ANTI_SYMMAP_H

#include <stdbool.h>

#include "target.h"
#include "text.h"

/* The map of a symbols archive. It holds one line per function of a
   program, with the range of its addresses and its name. Where the debug
   information gives them, the file and the line follow. */

/* The build id of a program, the 64 digits of the `build` line of its
   licence notice. Returns false when the program carries none. */
bool symmap_build_id(const char *program, struct text *out);

/* Write the map of program at path. id is its build id, which the map
   names so that a trace of that program finds its own map. */
bool symmap_write(const char *program, enum target t, const char *id,
                  const char *path);

#endif
