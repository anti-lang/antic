#ifndef ANTIC_NOTICE_H
#define ANTIC_NOTICE_H

#include <stddef.h>

/* The begin and end markers of the licence notice, by which a tool finds
   anti_licenses in any Anti binary, stand in src/rt/license.h, which the
   runtime and anti read as well. */
#include "../rt/license.h"
#include "antic.h"
#include "text.h"

/* Append the licence notice of the packages of a program between the
   markers. Each package name has a line with version and licence and its
   attribution lines, once. Then each distinct licence text follows once,
   after the names of the packages it covers. */
void notice_text(struct text *out, const struct package *const *packages,
                 size_t count);

#endif
