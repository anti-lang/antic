#ifndef ANTI_UNITS_H
#define ANTI_UNITS_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* One module of a run over the sources of a project. `anti check` and
   `anti doc` both compile a module after the modules it imports, so both
   read the same list. */
struct unit {
    const char *source;
    struct text path;           /* the module path */
    struct text library;        /* the interface file of the work directory */
    struct text *imports;       /* the module path of each import */
    size_t import_count;
    bool parsed;                /* the lexer and the parser took it */
    bool has_main;              /* the module declares `fn main` */
};

/* The path of the file at dir/<module as directories><suffix>, with every
   directory above it made. */
bool unit_file(const char *dir, const char *module, const char *suffix,
               struct text *out);

/* Read one module: its path, its imports and whether the front end's
   first two passes took it. work names the directory of the interface
   file, and NULL leaves that path empty for a caller that names its own. A file the parser refuses reports here, and
   no pass below sees it, so no message is printed twice. Returns false
   when the file cannot be read or names no module path. */
bool unit_read(const char *source, const char *const *roots,
               size_t root_count, const char *work, struct unit *out);

void unit_free(struct unit *u);

/* The order the modules are compiled in: a module after every module of
   the run that it imports. A cycle among the imports keeps the order the
   files came in, and the compiler reports it. order holds one index per
   unit. */
void unit_order(const struct unit *units, size_t count, size_t *order);

#endif
