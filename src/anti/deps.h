#ifndef ANTI_DEPS_H
#define ANTI_DEPS_H

#include <stdbool.h>
#include <stddef.h>

#include "manifest.h"
#include "text.h"

/* One module of a resolved package: its module path, the digest of its
   library file and where that file stands on this machine. */
struct dep_module {
    struct text path;
    struct text digest;
    struct text file;
};

/* One resolved package. A package of a repository carries the URL prefix
   it came from, and a package of a path carries that directory. */
struct dep_package {
    struct text name;
    struct text version;
    struct text repo;
    struct text path;
    struct dep_module *modules;
    size_t module_count;
};

/* The closed dependency graph of a project. */
struct dep_graph {
    struct dep_package *packages;
    size_t count;
};

/* Where the library files of a path dependency stand. A directory that
   holds a manifest is a project. The build builds it first and answers
   with its `dist/` directory. A directory of library files is given as
   it is. */
typedef bool (*dep_path_fn)(void *context, const char *directory,
                            struct text *out);

/* Resolve the dependencies of the manifest, in the order of "Resolution"
   in docs/tooling.md. The lock file stands where it answers the
   manifest. Otherwise the index of each repository is read, the graph is
   walked and the lock file is written from the result. Every library
   file is then in the cache or on disk, and its digest is checked. root
   is the directory of the manifest, which a relative path of a
   dependency starts at. Returns false and writes a message on a
   constraint that no version satisfies, a digest that differs, or a file
   that cannot be had. out then holds no allocation. The caller frees a
   graph resolved with deps_free. */
bool deps_resolve(const struct manifest *m, const char *root, bool offline,
                  dep_path_fn path_of, void *context, struct dep_graph *out);

void deps_free(struct dep_graph *g);

/* Compare two versions of the form major.minor.patch. A missing part is
   zero, so `2.0` and `2.0.0` are one version. */
int deps_version_compare(const char *a, const char *b);

/* Whether version is one to three parts of decimal digits joined by dots,
   each part at most DEPS_VERSION_DIGITS digits long. A version of an
   index, a lock file or a library header that is not stands nowhere in a
   path or in the comparison. */
enum { DEPS_VERSION_DIGITS = 9 };
bool deps_version_valid(const char *version);

/* Whether version satisfies constraint. The three forms are `1.2.4`,
   which is `>= 1.2.4` and `< 2.0.0`, `=1.2.4`, which is that version
   alone, and `>=1.2.4`, which has no upper bound. The empty constraint
   takes every version. A constraint or a version that is not of these
   forms satisfies nothing. */
bool deps_satisfies(const char *constraint, const char *version);

/* Whether constraint is one of the three forms of deps_satisfies with a
   version deps_version_valid takes. The empty constraint of a path
   dependency is no constraint of these. */
bool deps_constraint_valid(const char *constraint);

#endif
