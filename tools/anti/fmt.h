#ifndef ANTI_FMT_H
#define ANTI_FMT_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* Write the canonical form of one Anti source into out. The rules are
   "Formatter rules" in docs/tooling-addendum.md. They are one tab per
   level, with one more for a wrapped line, and an item body whose brace
   opens on its own line. A statement block opens its brace on the line of
   the statement. The parentheses around a whole condition go, a statement
   takes a line of its own, and a doc comment is filled to 80 columns.
   Returns false when the source does not lex, and out is then
   untouched. */
bool fmt_source(const char *source, size_t length, struct text *out);

/* Write the canonical form of each path in place. With check nothing is
   written and the files that differ are listed instead. Both forms print
   one path per file that is not in the canonical form. Returns the exit
   status of the command. */
int fmt_run(const char *const *paths, size_t count, bool check);

#endif
