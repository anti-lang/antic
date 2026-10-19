#ifndef ANTI_SYMS_H
#define ANTI_SYMS_H

/* The interface of syms.c: `anti symbols inventory`, `check` and
   `resolve` over the symbols archives of a deployment, and the two
   lookups of a function by its address. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "text.h"

/* `anti build --release` names the symbols archive beside a binary
   after its stem with this suffix, and `anti symbols` finds it so. */
#define SYMS_ARCHIVE_SUFFIX "-symbols.zip"

/* The ends of the names of the entries of a symbols archive: the twin of
   a binary with its debug information, the map of its functions and the
   PDB of a Windows program. `anti build --release` writes them and `anti
   symbols` reads them. */
#define SYMS_DEBUG_SUFFIX ".debug"
#define SYMS_MAP_SUFFIX ".map"
#define SYMS_PDB_SUFFIX ".pdb"

/* Read the runtime configuration at conf. Find the program beside it,
   the libraries of the `plugins` directories and those of
   `[injections]`. Fold the symbols archive of each into the archive
   out, keyed by build id, with an `index.toml`. from names the
   directory that holds the archives, or NULL for the directory of each
   binary. Returns the exit status of the command, 0 or 1. */
int syms_inventory(const char *conf, const char *from, const char *out);

/* Walk the binaries of the configuration at conf and report per module
   whether its symbols are present, stale or missing. The archives are
   the count archives at symbols, or the one beside each binary when
   count is 0. Returns the exit status of the command: 1 when any module
   lacks the symbols of its own build or the walk fails, and 0
   otherwise. */
int syms_check(const char *conf, const char *const *symbols, size_t count);

/* Print the raw trace at trace with the function, the file and the line
   of every frame whose module has an archive among symbols. Returns the
   exit status of the command, 0 or 1. */
int syms_resolve(const char *trace, const char *const *symbols,
                 size_t count);

/* Append to function the name of the function at vaddr in the text of a
   symbols map, as a person reads it. Append its `file:line` to where when
   the map gives one. Returns false when no line of the map holds
   vaddr. */
bool syms_map_lookup(const char *map, uint64_t vaddr, struct text *function,
                     struct text *where);

/* Append to function the name of the function at vaddr in binary, an
   ELF or a Mach-O file or a debug twin of one, and to where its
   `file:line` when the debug information gives one. Returns false for a
   file of neither kind. */
bool syms_resolve_binary(const struct text *binary, uint64_t vaddr,
                         struct text *function, struct text *where);

#endif
