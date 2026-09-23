#ifndef ANTIC_SELFPATH_H
#define ANTIC_SELFPATH_H

#include <stdbool.h>

#include "text.h"

/* Append the directory that holds the running antic to out, without a
   trailing separator. Returns false when the system does not say where
   the executable is. */
bool self_directory(struct text *out);

/* Whether path names a directory. */
bool directory_exists(const char *path);

/* Whether path is absolute on the host: it starts with `/`, and on
   Windows also with a drive or `\`. */
bool path_is_absolute(const char *path);

/* Append the absolute form of path to out, with each part separated by
   `/`. The directory that holds path must exist, and path itself need
   not. A POSIX host resolves symbolic links, and Windows resolves `.` and
   `..` by name as it does for every path. */
bool absolute_path(const char *path, struct text *out);

#endif
