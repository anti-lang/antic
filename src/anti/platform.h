#ifndef ANTI_PLATFORM_H
#define ANTI_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* The platform layer of antic, which anti calls as well. */
#include "../antic/platform.h"

/* The platform layer of anti, which docs/c-guidelines.md names under rule
   22: the calls that differ between the systems and that antic does not
   make. A path is UTF-8 on every host, and Windows sees UTF-16 at each of
   its calls, as in the layer of antic. */

/* What a path names. A link is a symbolic link, and on Windows any
   reparse point, which a junction is as well. */
enum platform_kind {
    PLATFORM_MISSING,
    PLATFORM_FILE,
    PLATFORM_DIRECTORY,
    PLATFORM_LINK,
    PLATFORM_UNREADABLE
};

/* What path names. With follow set a link counts as what it leads to,
   and as PLATFORM_MISSING when that is nothing, so PLATFORM_LINK comes
   only without follow. PLATFORM_MISSING means nothing is there and
   PLATFORM_UNREADABLE that the system did not say. */
enum platform_kind platform_kind(const char *path, bool follow);

/* Open path for reading when it names a regular file, through every
   link, and return NULL for anything else: a directory, a device, a FIFO
   or a socket. The open itself does not wait, so a FIFO with no writer
   is refused at once. A path that an input names, such as the object
   file of a debug map, is read through it, because /dev/zero would give
   bytes without end and a pipe would wait for a writer. The caller
   closes the stream with fclose. */
FILE *platform_open_file(const char *path);

/* Make the directory path, whose parent exists. Returns true when it was
   made or something of that name is there already. */
bool platform_make_dir(const char *path);

/* Remove what path names without following it: a file, a link or an
   empty directory. Windows removes a file marked read-only as well.
   Returns false when it cannot. */
bool platform_remove_entry(const char *path);

/* Give the file or directory from the name to. Returns false when it
   cannot. A to that exists is replaced on POSIX and refused on Windows,
   as rename of the C library does on each. */
bool platform_rename(const char *from, const char *to);

/* Give the file from the name to, replacing a file that has it, on every
   host. Returns false when it cannot. */
bool platform_replace(const char *from, const char *to);

/* True where files_copy_program writes a program beside its place and
   then replaces it with platform_replace_program, false where it writes
   the program in place. The DESIGN of files_copy_program gives the
   reason. */
bool platform_program_replaced(void);

/* platform_replace for a program that anti writes into dist/. On Windows
   it also replaces a program that another process holds mapped, and the
   holder keeps the old file until it lets go. A program mapped as an
   image is first moved aside, to its name with PLATFORM_OLD_SUFFIX, and
   then deleted. One that cannot be deleted yet stays there until the
   next call for the same name removes it. Returns false when it cannot,
   and to then stays as it was. */
bool platform_replace_program(const char *from, const char *to);

/* The suffix of the name a program that platform_replace_program
   replaces on Windows takes for a moment. */
#define PLATFORM_OLD_SUFFIX ".old"

/* Give the file to the permissions of the file from. Windows keeps no
   permission bits, so there it does nothing. */
bool platform_copy_permissions(const char *from, const char *to);

/* Run a program as process_run does and hand each line of its standard
   error to each, with the environment variable name set to value, as
   process_run_lines does.

   DESIGN: Windows runs the program as process_run does, sets no
   variable and hands no line, since Anti's symbolizer reads no PDB and
   would leave every frame of a report raw. There the report keeps the
   frames that AddressSanitizer symbolizes from the PDB of the link. */
int platform_run_symbolized(const char *const argv[], const char *name,
                            const char *value,
                            void (*each)(void *context, const char *line,
                                         size_t length),
                            void *context);

#endif
