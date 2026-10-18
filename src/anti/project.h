#ifndef ANTI_PROJECT_H
#define ANTI_PROJECT_H

/* The interface of project.c: `anti new`, and the sources of a
   project. */

#include <stdbool.h>

#include "files.h"

/* `anti new <name>`: a project of the default layout, with a starter
   manifest and one module that prints and returns. name is the package
   name, a module path of at least two segments, and the directory takes
   its last segment. Returns the exit status of anti. */
int project_new(const char *name);

/* The sources of a project: every `.anti` file under the source
   directory src and the test directory test of `[layout]`, sorted by
   path, appended to out. test is NULL for a command that reads the
   source directory alone. With no file found it says so and returns
   false, and the caller ends with 2, the status of a wrong use by R160
   of "Rules of 2026-10-13". `anti check`, `anti doc` and `anti fmt`
   take their files from here, so the three read the same ones. */
bool project_sources(const char *src, const char *test,
                     struct files_list *out);

#endif
