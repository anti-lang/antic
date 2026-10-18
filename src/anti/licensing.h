#ifndef ANTI_LICENSING_H
#define ANTI_LICENSING_H

/* The interface of licensing.c: `anti license` over the runtime archive,
   over a project and over a static archive, and the readers of the
   licence notice of a binary and of the record of upstream sources.
   Every form prints the lines of a notice: one `package
   <name> <version> <identifier>` line per package with its `attribution`
   lines, then each distinct text once after `text for <names>`, and the
   `source` line of the record of upstream sources after the text of each
   component the record names. `--from` reads the notice of a binary
   through licensing_text_of and prints the same lines. */

#include <stdbool.h>
#include <stddef.h>

#include "antic.h"
#include "text.h"

/* The build id and the version of the licence notice in bytes. The id is
   the line after the begin marker of src/rt/license.h, and a line of the
   same form elsewhere in the bytes is passed over. The version is the
   one of the last `package` line, which names the package of the
   compiled module. Returns false when the bytes hold no notice. This and
   licensing_text are the readers of the notice in anti. */
bool licensing_notice(const struct text *bytes, struct text *id,
                      struct text *version);

/* Append the licence text of the notice in bytes to out: the lines
   between the build id and the end marker, as anti.license gives them in
   the program. Returns false when the bytes hold no notice. */
bool licensing_text(const struct text *bytes, struct text *out);

/* The licence text of the binary at the path, through licensing_text.
   Reports and returns false when the binary carries no notice. */
bool licensing_text_of(const char *binary, struct text *out);

/* Follow the text of each component of the notice with its line of
   RUNTIME_SOURCES_FILE of the runtime archive at runtime, as
   `source <name> <version> <url>`. A component the record does not name
   gets no line. Reports and returns false when the record cannot be
   read. */
bool licensing_add_sources(struct text *notice, const char *runtime);

/* Append to out the version that the record of upstream sources in
   sources gives the component of name, the second word of its line.
   Returns false when the record names no such component. */
bool licensing_source_version(const struct text *sources, const char *name,
                              struct text *out);

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
