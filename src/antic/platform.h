#ifndef ANTIC_PLATFORM_H
#define ANTIC_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "text.h"

/* The platform layer of antic and anti, which docs/c-guidelines.md names
   under rule 22. Every call that differs between the systems goes through
   it. A path, an argument and the value of a variable are UTF-8 on every
   host, and Windows sees UTF-16 at each of its calls. */

/* Open the file at path in binary mode, for reading, or for writing when
   writing is true. Writing creates the file or empties the one that is
   there. Returns NULL when it cannot. The caller closes the file with
   fclose. */
FILE *platform_open(const char *path, bool writing);

/* Remove the file at path. Returns false when it cannot. */
bool platform_remove(const char *path);

/* The value of the environment variable name, or NULL when it is unset.
   The value stays valid until the program ends, and the caller never
   frees it. A value that is no valid UTF-16 on Windows counts as unset. */
const char *platform_getenv(const char *name);

/* The arguments argv of main as UTF-8, as many as main was given and
   ended by NULL. The list stays valid until the program ends. Windows
   gives main the arguments in the code page of the machine, so they are
   read again as UTF-16. Every other host returns argv. */
char **platform_arguments(char **argv);

/* Whether path is absolute on the host: it starts with `/`, and on
   Windows also with `\` or a drive letter and a colon. A drive without a
   separator names a file on that drive, which no directory can be put
   before. src/rt/platform.h follows the same rule. */
bool path_is_absolute(const char *path);

/* The last separator of the directories of path, or NULL when it has
   none: `/` on every host, and `\` as well on Windows. */
const char *platform_last_separator(const char *path);

/* The separator of the paths the host's own programs read: `\` on
   Windows and `/` elsewhere. */
char platform_separator(void);

/* Whether path names a directory. */
bool directory_exists(const char *path);

/* Hand the name of each entry of the directory at path to each, without
   `.` and `..`, in the order the system gives. Returns false when the
   directory cannot be read, and when the system fails part of the way,
   after the names it gave. */
bool platform_list_directory(const char *path,
                             void (*each)(void *context, const char *name),
                             void *context);

/* Append the directory that holds the running executable to out, without
   a trailing separator. Returns false when the system does not say where
   the executable is. */
bool self_directory(struct text *out);

/* Append the absolute form of path to out, with each part separated by
   `/`. The directory that holds path must exist, and path itself need
   not. A POSIX host resolves symbolic links, and Windows resolves `.` and
   `..` by name as it does for every path. */
bool absolute_path(const char *path, struct text *out);

/* Run the program argv[0], found through PATH when it contains no '/',
   with the NULL-terminated argument list argv. Return its exit status, or
   -1 when it could not start or ended by a signal. The program inherits
   standard output and standard error. */
int process_run(const char *const argv[]);

/* Run a program as process_run does, with directory as its working
   directory, where every relative path of its arguments then starts. A
   relative argv[0] with a separator names the program from directory as
   well, and a bare name is found through PATH. */
int process_run_in(const char *directory, const char *const argv[]);

/* Run a program as process_run does and append its standard output to
   out. */
int process_capture(const char *const argv[], struct text *out);

#if defined(_WIN32)
/* The UTF-16 of text, or NULL when text is no valid UTF-8. The caller
   frees the result with free. The layer of anti converts with it. */
wchar_t *platform_widen(const char *text);
#endif

#if !defined(_WIN32)
/* Run a program as process_run does, with the environment variable name
   set to value, and hand each line of its standard error to each, with
   its newline. A last line without one comes last. Standard output stays
   the one of the tool. Windows has no caller: anti symbols leaves the
   symbolizing of a report to AddressSanitizer there. */
int process_run_lines(const char *const argv[], const char *name,
                      const char *value,
                      void (*each)(void *context, const char *line,
                                   size_t length),
                      void *context);
#endif

#endif
