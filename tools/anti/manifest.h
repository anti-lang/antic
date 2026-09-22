#ifndef ANTI_MANIFEST_H
#define ANTI_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>

/* The manifest of a project, in its root, which is where a build runs. */
#define MANIFEST_FILE "anti.toml"

/* The `[inject]` table of `anti.toml`, one `Interface=Provider` entry
   per interface, in the form `--inject` takes. `anti build` passes the
   table and `anti test` passes `[inject.test]` over it, per key. */
struct manifest_inject {
    const char **entries;
    size_t count;
};

/* Read `[inject]` of the manifest at path, with `[inject.test]` laid
   over it when tests is true. A missing file gives an empty table.
   Returns false when the file is no TOML the reader takes. */
bool manifest_inject_read(const char *path, bool tests,
                          struct manifest_inject *out);

void manifest_inject_free(struct manifest_inject *table);

#endif
