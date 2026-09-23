#ifndef ANTI_ZIP_H
#define ANTI_ZIP_H

#include <stdbool.h>
#include <stddef.h>

/* The symbols archive of a release is a zip file, because every host
   opens one without a tool. The writer stores its entries and compresses
   nothing, so it needs no library and writes the same bytes from one
   input on every host. */

/* One entry of an archive: the name it carries and the file it holds. */
struct zip_entry {
    const char *name;
    const char *file;
    bool executable;
};

/* Write the archive at path with these entries, in order. Returns false
   and writes a message when a file cannot be read or the archive cannot
   be written. */
bool zip_write(const char *path, const struct zip_entry *entries,
               size_t count);

#endif
