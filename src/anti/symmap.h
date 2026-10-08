#ifndef ANTI_SYMMAP_H
#define ANTI_SYMMAP_H

#include <stdbool.h>

#include "target.h"
#include "text.h"

/* The map of a symbols archive. It holds one line per function of a
   program, with the range of its addresses and its name. Where the debug
   information gives them, the file and the line follow. */

/* The head of the line of a map that names the build id of its
   program. It follows the first line of the map. */
#define SYMMAP_BUILD_LINE "# build "

/* The build id and the version of the licence notice in bytes. The id is
   the line after the begin marker of src/rt/license.h, and a line of the
   same form elsewhere in the bytes is passed over. The version is the
   one of the last `package` line, which names the package of the
   compiled module. Returns false when the bytes hold no notice. This and
   symmap_license are the readers of the notice in anti. */
bool symmap_notice(const struct text *bytes, struct text *id,
                   struct text *version);

/* Append the licence text of the notice in bytes to out: the lines
   between the build id and the end marker, as anti.license gives them in
   the program. Returns false when the bytes hold no notice. */
bool symmap_license(const struct text *bytes, struct text *out);

/* The licence text of the binary at the path, through symmap_license.
   Reports and returns false when the binary carries no notice. */
bool symmap_license_of(const char *binary, struct text *out);

/* The build id of the program at the path, through symmap_notice.
   Returns false when the program carries none. */
bool symmap_build_id(const char *program, struct text *out);

/* Write the map of program at path. id is its build id, which the map
   names so that a trace of that program finds its own map. */
bool symmap_write(const char *program, enum target t, const char *id,
                  const char *path);

#endif
