#ifndef ANTIC_LOWER_H
#define ANTIC_LOWER_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "diagnostic.h"
#include "ir.h"

/* Translate a module that passed semantic analysis into IR. The ir field
   of a local or parameter symbol names the temporary of its value, or of
   its slot address when the address is taken. For a function it holds the
   IR function index. Returns false after an error in diags. Semantic
   analysis rejects every module that lowering cannot translate, so no
   construct of the language reports one. */
enum lower_option {
    LOWER_NO_REFLECT = 1u << 0,     /* --no-reflect: no field list */
    LOWER_DEV = 1u << 1,            /* --dev: every dispatch checks its table */
    /* DESIGN: `--no-hooks` drops every hook site the compiler writes,
       the five always-on ones among them. `--trace` decides the code
       that asked for the call hooks with the contextual `trace`, and it
       follows the mode where neither `--trace` nor `--no-trace` stands.
       `--trace writes` adds the `changed` hook after a write. */
    LOWER_NO_HOOKS = 1u << 2,       /* --no-hooks: no hook site at all */
    LOWER_TRACE = 1u << 3,          /* trace-marked code is instrumented */
    LOWER_TRACE_WRITES = 1u << 4    /* --trace writes: the changed hook */
};

/* patterns holds the `--trace <pattern>` arguments, which instrument a
   package or a class by name whether it asked or not. */
bool lower_module(struct module *module, const char *module_name,
                  struct ir_module *out, struct diagnostics *diags,
                  unsigned options, const char *const *patterns,
                  size_t pattern_count);

#endif
