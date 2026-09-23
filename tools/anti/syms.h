#ifndef ANTI_SYMS_H
#define ANTI_SYMS_H

#include <stddef.h>

/* `anti symbols`, the three commands over the symbols archives of a
   deployment. Each returns the exit status of the command. */

/* Read the runtime configuration at conf, find the program beside it,
   the libraries of the `plugins` directories and those of
   `[injections]`, and fold the symbols archive of each into the one
   archive out, keyed by build id, with an `index.toml`. from names the
   directory that holds the archives, or NULL for the directory of each
   binary. */
int syms_inventory(const char *conf, const char *from, const char *out);

/* Walk the binaries of the configuration at conf and report per module
   whether its symbols are present, stale or missing. The archives are
   the count archives at symbols, or the one beside each binary when
   count is 0. Returns 1 when any module lacks the symbols of its own
   build. */
int syms_check(const char *conf, const char *const *symbols, size_t count);

/* Print the raw trace at trace with the function, the file and the line
   of every frame whose module has an archive among symbols. */
int syms_resolve(const char *trace, const char *const *symbols,
                 size_t count);

#endif
