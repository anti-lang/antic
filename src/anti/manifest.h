#ifndef ANTI_MANIFEST_H
#define ANTI_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* The manifest of a project, in its root, which is where a build runs. */
#define MANIFEST_FILE "anti.toml"

/* The lock file, beside the manifest. `anti build` writes it. */
#define LOCK_FILE "anti.lock"

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

/* The source and test directories of a project, which `[layout]` of the
   manifest may name, and the package name of `[package]`. The defaults of
   docs/tooling.md stand where the file does not, and a missing file gives
   both defaults and no package name. The package name decides which
   modules share an `internal` item, so every call of a check carries
   it. */
bool manifest_layout_read(const char *path, struct text *src,
                          struct text *test, struct text *package);

/* One entry of `[repositories]`, an alias and the URL prefix it names. */
struct manifest_repository {
    struct text alias;
    struct text url;
};

/* One entry of `[dependencies]`. A dependency names a repository by alias
   in repo, or a directory in path, and never both. version holds the
   constraint, which may stand beside a path. */
struct manifest_dependency {
    struct text name;
    struct text version;
    struct text repo;
    struct text path;
};

/* Everything `anti build` reads from `anti.toml`. A field the file does
   not name is empty, and the four directories carry the defaults of
   docs/tooling.md. */
struct manifest {
    struct text name;           /* [package] name, the root module path */
    struct text version;
    struct text antic;          /* the minimum compiler version */
    struct text license;
    struct text license_text;
    struct text *attribution;
    size_t attribution_count;
    struct text src;
    struct text test;
    struct text build;
    struct text dist;
    struct text *default_targets;
    size_t default_target_count;
    struct text *all_targets;
    size_t all_target_count;
    struct manifest_repository *repositories;
    size_t repository_count;
    struct manifest_dependency *dependencies;
    size_t dependency_count;
    struct manifest_inject inject;
};

/* Read the whole manifest at path. Returns false when the file is
   missing, is no TOML the reader takes, or holds an entry that no rule of
   docs/tooling.md allows. The message names the file and the entry. */
bool manifest_read(const char *path, bool tests, struct manifest *out);

void manifest_free(struct manifest *m);

/* The URL prefix that alias names, or NULL when the manifest has no such
   repository. */
const char *manifest_repository_url(const struct manifest *m,
                                    const char *alias);

#endif
