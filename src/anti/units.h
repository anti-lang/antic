#ifndef ANTI_UNITS_H
#define ANTI_UNITS_H

#include <stdbool.h>
#include <stddef.h>

#include "cpu.h"
#include "driver.h"
#include "target.h"
#include "text.h"

/* One module of a run over the sources of a project. `anti check`,
   `anti doc`, `anti build` and `anti test` read the same list. */
struct unit {
    const char *source;
    struct text path;           /* the module path */
    struct text library;        /* the interface file of the work directory */
    struct text *imports;       /* the module path of each import */
    size_t import_count;
    char **tests;               /* the names its `tests` block declares */
    size_t test_count;
    bool setup;                 /* its `fixtures` block declares setup */
    bool teardown;              /* and teardown */
    bool parsed;                /* the lexer and the parser took it */
    bool has_main;              /* the module declares `fn main` */
};

/* The path of the file at dir/<module as directories><suffix>, with every
   directory above it made. */
bool unit_file(const char *dir, const char *module, const char *suffix,
               struct text *out);

/* Read one module: its path, its imports, its tests and fixtures and
   whether the front end's first two passes took it. work names the
   directory of the interface file, and NULL leaves that path empty for a
   caller that names its own. A file the parser refuses reports here, and
   no pass below sees it, so no message is printed twice. Returns false
   when the file cannot be read or names no module path. The caller frees
   out with unit_free, whether the read succeeded or not. */
bool unit_read(const char *source, const char *const *roots,
               size_t root_count, const char *work, struct unit *out);

void unit_free(struct unit *u);

/* DESIGN: every compile of a run over a project starts from
   unit_options, so what each call carries stands once: the search roots,
   the runtime archive, the target with its processor level and the
   package name of `[package]` in the manifest. The package name decides
   which modules share an `internal` item, and a library file carries the
   name of the package that wrote it. A compile without it refused an
   item that a compile with it accepted, so `anti check` passed a project
   that `anti build`, `anti test` and `anti doc` refused. package is NULL
   outside a project. Every other field of o is zero. */
void unit_options(struct options *o, const char *package, const char *runtime,
                  const char *const *roots, size_t root_count, enum target target,
                  enum cpu_level cpu);

/* The host target at its default processor level, which a run of
   `anti check`, `anti test`, `anti doc` and `anti bind` compiles for.
   Prints the message and returns false on a host antic does not know. */
bool unit_host(enum target *target, enum cpu_level *cpu);

/* DESIGN: a binding names the frameworks of Apple's SDK it needs with
   `link framework`, and the libraries of the glibc sysroot with `link
   linux`. Its library file records both. A link reads them from every
   library file the program reaches and passes them to antic as
   --framework and --linux-lib, so a program never names one itself. */
struct unit_links {
    const char *const *frameworks;
    size_t framework_count;
    const char *const *linux_libraries;
    size_t linux_library_count;
};

/* The names of the `link framework` and `link linux` lines of the library
   files of search and of every library file they import, each once. The
   lists go into arena. Returns false when a library file cannot be read,
   and out then holds no names. */
bool unit_links_read(const struct options *search, struct arena *arena,
                     struct unit_links *out);

/* Pass the names of links to a compile with the options o. */
void unit_links_apply(const struct unit_links *links, struct options *o);

/* Append the module path with every dot turned into `_`, a name that
   stands in one file name. */
void unit_flat_path(const char *module, struct text *out);

/* The order the modules are compiled in: a module after every module of
   the run that it imports. A cycle among the imports keeps the order the
   files came in, and the compiler reports it. order holds one index per
   unit. */
void unit_order(const struct unit *units, size_t count, size_t *order);

#endif
