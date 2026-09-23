#ifndef ANTI_FORMAT_H
#define ANTI_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

#include "diagnostic.h"

/* Check the layout of one Anti source against the formatter rules of
   "Formatter rules" in docs/tooling-addendum.md. One diagnostic per
   finding goes into out. The rules this reads are the ones a token stream
   settles. The indent is tabs, one tab per level, with one more for a
   wrapped line. Nothing is aligned past the indent. One statement stands
   per line. The parentheses around a whole condition are dropped.
   `} else {` and the `} while` of a `do` block are one line each. A file
   whose tokens do not lex reports nothing, because the front end reports
   it. Returns false when the file cannot be read. */
bool format_check(const char *path, struct diagnostics *out);

#endif
