#ifndef ANTIC_PARSER_H
#define ANTIC_PARSER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"

/* DESIGN: the parser descends at most this many levels into nested
   expressions, types and statements, and refuses a deeper source with a
   diagnostic. The checker, the dump and the symbolic walks recurse over
   the same tree, so the one limit keeps each of them on the stack. */
#define PARSE_DEPTH_MAX 256

/* Build the syntax tree of one module from its tokens. Nodes go into the
   memory pool. Every syntax error goes to diags, one per mistake, and
   parsing continues after it. Returns true when no error occurred. The
   module is stored in *out in either case. */
bool parse(const char *source, const struct token_list *tokens,
           struct arena *arena, struct diagnostics *diags,
           struct module **out);

#endif
