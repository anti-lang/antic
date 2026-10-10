#ifndef ANTI_LICENSING_H
#define ANTI_LICENSING_H

#include <stdbool.h>
#include <stddef.h>

#include "antic.h"
#include "text.h"

/* `anti license` over the runtime archive, over a project and over a
   static archive. Every form prints the lines of a notice: one `package
   <name> <version> <identifier>` line per package with its `attribution`
   lines, then each distinct text once after `text for <names>`, and the
   `source` line of the record of upstream sources after the text of each
   component the record names. `--from` reads the notice of a binary
   through symmap.h and prints the same lines. */

/* The plain form: the licence of antic and anti, then every component of
   licenses/ of the runtime archive at runtime with its name, its version,
   its identifier and its text. Returns the exit status of anti. */
int licensing_plain(const char *runtime);

/* `--from-archive`: the licence fields of the copy of the package header
   that a static library for C carries. Returns the exit status of
   anti. */
int licensing_from_archive(const char *archive);

/* What the notice of a project is made from. */
struct licensing_project {
    const char *runtime;        /* the runtime archive */
    /* Whether the link of the program took the C library of musl, and
       mimalloc with it, as the driver reports it in links_musl. */
    bool musl;
    /* The library files of every package of anti.lock, then those of
       the bundled modules the project imports. */
    const char *const *libraries;
    size_t library_count;
    const struct package *own;  /* the package of the project itself */
};

/* Append the notice of a project to out: the runtime, musl and mimalloc
   where the program links them, the package of every library file once,
   and the package of the project last, as the notice of a binary has it.
   Returns false, after a message, when a library file or the record of
   upstream sources cannot be read. */
bool licensing_project(const struct licensing_project *p, struct text *out);

#endif
