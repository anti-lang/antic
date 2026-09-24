#ifndef ANTI_FILES_H
#define ANTI_FILES_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

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
   call is a copy there. */
bool files_copy_program(const char *from, const char *to);

/* Append the bytes of the file at path to out. Returns false when the
   file cannot be opened or a read fails part of the way, and out is then
   as it was. No caller takes the part of a file for the whole. Prints
   nothing, since for some callers a missing file is an answer. */
bool files_read(const char *path, struct text *out);

/* files_read, which also prints that path cannot be read when it fails. */
bool files_read_reported(const char *path, struct text *out);

/* Write bytes as the whole file at path. Returns false, and prints that
   path cannot be written, when fopen, fwrite or fclose fails. fclose is
   where a full disk reports the bytes of the last buffer. */
bool files_write(const char *path, const struct text *bytes);

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

void files_list_free(struct files_list *list);

#endif
