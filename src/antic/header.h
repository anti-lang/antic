#ifndef ANTIC_HEADER_H
#define ANTIC_HEADER_H

/* The interface of header.c: the C header of a library for C. */

#include <stdbool.h>
#include <stddef.h>

#include "sema.h"
#include "text.h"

/* The C header of a library for C: the export structs, unions, constants
   and functions of the given interfaces, in C types. name is the library
   name, which names the file name.h and its include guard. With bundled
   set, the top comment says that the archive holds the runtime. */
void header_write(struct text *out, const char *name,
                  const struct interface *const *ifaces, size_t count,
                  bool bundled);

/* DESIGN: a copy of a generic has one name in C, so two `export type`
   items of one copy are refused where both land in one header, by T281
   of docs/audit/tests-findings.md and E of "Answers of 2026-11-19" in
   docs/decisions.md. The checker refuses two of one module. A header of
   --lib holds the interfaces of every library the module imports, so
   two of two of them are refused there. Returns false and writes the
   message into out for the first such pair. */
bool header_copy_named_once(const struct interface *const *ifaces,
                            size_t count, struct text *out);

/* Whether name is a keyword of C11 or of C++17. */
bool header_is_keyword(const struct name *name);

#endif
