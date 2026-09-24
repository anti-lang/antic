#ifndef ANTI_ZIP_H
#define ANTI_ZIP_H

#include <stdbool.h>
#include <stddef.h>

/* The symbols archive of a release is a zip file, because every host
   opens one without a tool. The writer stores its entries and compresses
   nothing, so it needs no library and writes the same bytes from one
   input on every host. */

#include "text.h"

/* One entry of an archive: the name it carries and the file it holds.
   An entry whose file is NULL holds the size bytes at bytes. */
struct zip_entry {
    const char *name;
    const char *file;
    const char *bytes;
    size_t size;
    bool executable;
};

/* Write the archive at path with these entries, in order. Returns false
   and writes a message when a file cannot be read or the archive cannot
   be written. */
bool zip_write(const char *path, const struct zip_entry *entries,
               size_t count);

/* An archive read whole, with the entries of its central directory. */
struct zip_archive {
    struct text bytes;
    struct zip_item *items;
    size_t count;
};

/* One entry of the central directory. The data of the entry stands at
   offset in bytes, packed as method says. */
struct zip_item {
    struct text name;
    size_t offset;
    size_t packed;
    size_t size;
    unsigned long crc;
    unsigned method;
};

/* Read the archive at path. The reader takes the stored entries of a
   symbols archive of Anti. It takes the ones another tool compressed
   with deflate as well, so an archive a user packed again opens. Returns
   false and writes a message when the file is no such archive, and out
   then holds no allocation. The caller frees an archive read with
   zip_archive_free. */
bool zip_read(const char *path, struct zip_archive *out);

/* The bytes of the entry at index, unpacked into out. Returns false and
   writes a message when its data is broken. */
bool zip_unpack(const struct zip_archive *a, size_t index, struct text *out);

void zip_archive_free(struct zip_archive *a);

#endif
