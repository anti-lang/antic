#ifndef ANTI_SYMMAP_H
#define ANTI_SYMMAP_H

/* The interface of symmap.c: the text map of a symbols archive and the
   build id of a program. The map holds one line per function of a
   program, with the range of its addresses and its name. Where the debug
   information gives them, the file and the line follow. */

#include <stdbool.h>

#include "target.h"
#include "text.h"

/* The head of the line of a map that names the build id of its
   program. It follows the first line of the map. */
#define SYMMAP_BUILD_LINE "# build "

/* The head of the line of the map of a Mach-O program that names the
   address of its text, from which the offset of a frame counts. The map
   alone then answers a frame, by R183 of "Rules of 2026-10-13". A map
   without the line counts from address 0, as ELF does. */
#define SYMMAP_TEXT_LINE "# text "

/* The build id of the program at the path, through licensing_notice.
   Returns false when the program carries none. */
bool symmap_build_id(const char *program, struct text *out);

/* Write the map of program at path. id is its build id, which the map
   names so that a trace of that program finds its own map. */
bool symmap_write(const char *program, enum target t, const char *id,
                  const char *path);

#endif
