#ifndef ANTI_FILES_H
#define ANTI_FILES_H

#include <stdbool.h>

/* Make the directory path and every missing directory above it. */
bool make_dirs(const char *path);

/* Remove path and everything below it. A path that does not exist is
   removed already. A symbolic link is removed, never followed. */
bool remove_tree(const char *path);

/* Copy the bytes of the file from to the new file to. */
bool copy_file(const char *from, const char *to);

/* Whether path names a file or directory that exists. */
bool path_exists(const char *path);

#endif
