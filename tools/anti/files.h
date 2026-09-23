#ifndef ANTI_FILES_H
#define ANTI_FILES_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* Make the directory path and every missing directory above it. */
bool make_dirs(const char *path);

/* Remove path and everything below it. A path that does not exist is
   removed already. A symbolic link is removed, never followed. */
bool remove_tree(const char *path);

/* Copy the bytes of the file from to the new file to. */
bool copy_file(const char *from, const char *to);

/* Whether path names a file or directory that exists. */
bool path_exists(const char *path);

/* A list of paths, one text each. */
struct file_list {
    struct text *items;
    size_t count;
    size_t capacity;
};

/* Append the path of every file under dir, and under every directory
   below it, whose name ends with suffix. The paths are sorted, so a run
   over a tree is the same run everywhere. A directory that does not exist
   adds nothing and is no error. Returns false when a directory cannot be
   read. */
bool list_tree(const char *dir, const char *suffix, struct file_list *out);

void file_list_free(struct file_list *list);

#endif
