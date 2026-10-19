#ifndef ANTI_MANIFEST_H
#define ANTI_MANIFEST_H

/* The interface of manifest.c: the names of the manifest and the lock
   file, the grammar of a name, a version and a digest, a key of a TOML
   document, and the reader of `anti.toml` with its package, layout,
   targets, repositories, dependencies and `[inject]` table. */

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* The manifest of a project, in its root, which is where a build runs. */
#define MANIFEST_FILE "anti.toml"

/* The lock file, beside the manifest. `anti build` writes it. */
#define LOCK_FILE "anti.lock"

/* The build and the dist directory of the default layout of
   docs/tooling.md, which `[layout]` replaces for `anti build`. The work
   directories of `anti check`, `anti doc` and `anti test` stand in the
   first, as docs/decisions.md gives them. */
#define MANIFEST_BUILD_DIR "build"
#define MANIFEST_DIST_DIR "dist"

/* Whether name is a module path: lowercase ASCII identifiers joined by
   single dots. A package name and the module path of a library file both
   take this form, and a string that does not stands in no path. */
bool manifest_name_valid(const char *name);

/* The most decimal digits a part of a version holds. */
enum { MANIFEST_VERSION_DIGITS = 9 };

/* Whether version is one to three parts of decimal digits joined by dots,
   each part at most MANIFEST_VERSION_DIGITS digits long. A version of an
   index, a lock file, a manifest or a library header that is not stands
   nowhere in a path or in the comparison. */
bool manifest_version_valid(const char *version);

/* Whether digest is a SHA-256 digest in 64 lowercase hex digits. */
bool manifest_digest_valid(const char *digest);

struct anti_toml;

/* The value of the key path of a TOML document, as src/rt/toml.c gives
   it, or NULL when the document has no such key. An empty value is an
   empty string. The text belongs to the document. Every reader of
   `anti.toml`, `anti.lock` and a repository index looks a key up here. */
const char *manifest_value(const struct anti_toml *doc, const char *key);

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

/* Free every entry of table and its array, and leave table empty.
   manifest_inject_read leaves out empty before it reads, so the caller
   may free a table whose read failed. */
void manifest_inject_free(struct manifest_inject *table);

/* The source and test directories of a project, which `[layout]` of the
   manifest may name, and the package name of `[package]`. The defaults of
   docs/tooling.md stand where the file does not, and a missing file gives
   both defaults and no package name. The package name decides which
   modules share an `internal` item, so every compile of `anti check`,
   `anti build`, `anti test` and `anti doc` carries it. */
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
   docs/tooling.md allows. The message names the file and the entry. On
   failure out holds no allocation, so the caller frees nothing. The
   caller frees a manifest read with manifest_free. */
bool manifest_read(const char *path, bool tests, struct manifest *out);

/* Free every text, list and table of m, its `[inject]` table among them,
   and leave m zeroed. A failed manifest_read leaves out zeroed, so
   freeing it does nothing. */
void manifest_free(struct manifest *m);

/* The URL prefix that alias names, or NULL when the manifest has no such
   repository. */
const char *manifest_repository_url(const struct manifest *m,
                                    const char *alias);

#endif
