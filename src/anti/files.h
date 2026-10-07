#ifndef ANTI_FILES_H
#define ANTI_FILES_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* Print that anti ran out of memory and exit with status 70. Every
   allocation of the tool that fails ends here. */
void files_out_of_memory(void);

/* An array of count elements of size bytes each, zeroed. A product that
   overflows a size_t or a failed calloc exits through
   files_out_of_memory. The caller frees the array with free. */
void *files_array(size_t count, size_t size);

/* items resized to count elements of size bytes each, as realloc does.
   The checks and the exit are those of files_array. The caller frees
   the array with free. */
void *files_resize(void *items, size_t count, size_t size);

/* items, an array of *room elements of size bytes each, grown to twice
   its room, or to 16 elements from none. The new elements are zeroed and
   *room holds the new room. The checks and the exit are those of
   files_array. The caller frees the array with free. */
void *files_grow(void *items, size_t *room, size_t size);

/* Make the directory path and every missing directory above it. */
bool files_make_dirs(const char *path);

/* Remove path and everything below it. A path that does not exist is
   removed already. A symbolic link is removed, never followed. */
bool files_remove_tree(const char *path);

/* Copy the bytes of the file from to the new file to. */
bool files_copy(const char *from, const char *to);

/* Copy the file and give the copy the execute bits of the original. A
   program that `anti build` copies into `dist/` is run from there, and
   files_copy writes a plain file. Windows decides by the suffix, so the
   call is a copy there. On Windows the copy replaces to whole or leaves
   it as it was, and may leave the old program for a moment under its
   name with PLATFORM_OLD_SUFFIX. */
bool files_copy_program(const char *from, const char *to);

/* Append the bytes of the file at path to out. Returns false when the
   file cannot be opened or a read fails part of the way, and out is then
   as it was. No caller takes the part of a file for the whole. Prints
   nothing, since for some callers a missing file is an answer. */
bool files_read(const char *path, struct text *out);

/* files_read of a regular file alone. A path that an input names reads
   through it, so that a device or a FIFO is refused rather than read
   without end or waited on. See platform_open_file. */
bool files_read_file(const char *path, struct text *out);

/* files_read, which also prints that path cannot be read when it fails. */
bool files_read_reported(const char *path, struct text *out);

/* Write bytes as the whole file at path. Returns false, and prints that
   path cannot be written, when fopen, fwrite or fclose fails. fclose is
   where a full disk reports the bytes of the last buffer. */
bool files_write(const char *path, const struct text *bytes);

/* The suffix of the file a write fills beside the one it replaces. */
#define FILES_NEW_SUFFIX ".new"

/* Write bytes as the whole file at path, which keeps its old bytes until
   every new one is written. Returns false, and prints why, when the
   write or the replacement fails, and path then stays as it was. */
bool files_replace(const char *path, const struct text *bytes);

/* The last part of path, after the last `/` or `\\`. Both separators
   count on every host, so a path written on Windows splits the same way
   on the machine that reads it. The result points into path. */
const char *files_base_name(const char *path);

/* Whether s ends with suffix. */
bool files_ends_with(const char *s, const char *suffix);

/* Cut suffix off the end of t when t ends with it. */
void files_cut_suffix(struct text *t, const char *suffix);

/* Whether path names a file or directory that exists. */
bool files_exists(const char *path);

/* A list of paths, one text each. */
struct files_list {
    struct text *items;
    size_t count;
    size_t capacity;
};

/* Append the path of every file under dir, and under every directory
   below it, whose name ends with suffix. The paths are sorted, so a run
   over a tree is the same run everywhere. A directory that does not exist
   adds nothing and is no error. A link to a directory is not followed,
   and a link to a file is listed. Returns false, and prints the path,
   when a directory or an entry of one cannot be read. */
bool files_list_tree(const char *dir, const char *suffix,
                     struct files_list *out);

/* Append the path of every file of dir itself, sorted, as files_list_tree
   does without going below it. */
bool files_list_dir(const char *dir, struct files_list *out);

/* Append the name of every entry of the directory dir to out, without
   `.` and `..`, in the order the system gives. Returns false, and prints
   nothing, when the directory cannot be read, after the names it gave. */
bool files_list_names(const char *dir, struct files_list *out);

void files_list_free(struct files_list *list);

#endif
