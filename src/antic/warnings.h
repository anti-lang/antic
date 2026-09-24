#ifndef ANTIC_WARNINGS_H
#define ANTIC_WARNINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "diagnostic.h"

struct module;

/* A warning, which `allow` silences, or a safety check, which
   `unchecked` overrules. */
enum name_kind { KIND_WARNING, KIND_SAFETY_CHECK };

const char *warnings_name(enum diag_name name);
enum name_kind warnings_kind(enum diag_name name);

/* The name the text spells, or NAME_NONE. */
enum diag_name warnings_find(const char *text, size_t length);

/* The checks that ran in one compilation, one bit per name. An `allow`
   of a warning whose check did not run silences nothing and says
   nothing, because the build that runs the check decides. */
typedef unsigned long long warnings_ran;

#define WARNINGS_ALL_RAN (~0ULL)
#define WARNINGS_BIT(name) (1ULL << (unsigned)(name))

_Static_assert(NAME_COUNT <= 64, "a name needs a bit of warnings_ran");

/* Apply the `allow` and `unchecked` clauses of the module. A warning or
   a safety check that one of them covers is dropped. With complete set,
   the checker ran to its end, and a clause that silenced nothing is the
   warning `unused-allow` or `unused-unchecked`. */
void warnings_apply(const struct module *module, struct diagnostics *diags,
                    warnings_ran ran, bool complete);

/* Turn every warning that stands into an error, which a release build
   and --warnings-as-errors do. A doc warning stays one: it belongs to
   the doc class of `anti check`, which fails nothing. Returns whether
   one was turned. */
bool warnings_promote(struct diagnostics *diags);

#endif
