/* The compiler stages the unit tests run a source through: the front end
   alone, or the front end and lowering, for one module or for a
   compilation of several. One copy, so a change of a stage reaches every
   test that runs it. */

#ifndef ANTIC_TEST_PIPELINE_H
#define ANTIC_TEST_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"
#include "cpu.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "target.h"
#include "text.h"
#include "types.h"

/* A source lexed and parsed. ok is false when the parser refused it, with
   its messages in diags. */
struct parsed {
    struct arena arena;
    struct diagnostics diags;
    struct token_list tokens;
    struct module *module;
    bool ok;
};

/* Lex and parse source into p, which needs no setup. A source the lexer
   refuses counts a failure. */
void parsed_run(struct parsed *p, const char *source);

/* Free the tokens, the messages and the memory pool of p, whatever
   parsed_run gave. */
void parsed_release(struct parsed *p);

/* Check that source parses without a message and that its tree, as
   ast_dump writes it, reads expected. */
void parses_to(const char *source, const char *expected);

/* A source lexed, parsed and checked as the module main. parsed tells a
   refused parse from a refused check. */
struct checked {
    struct arena arena;
    struct diagnostics diags;
    struct token_list tokens;
    struct module *module;
    struct types types;
    bool parsed;
    bool ok;
};

/* Check source into c as the module main, as checked_run_as does. */
void checked_run(struct checked *c, const char *source);

/* Check source into c, which needs no setup, as the module name. A source
   that does not lex or parse counts a failure and prints its first
   message, since every test source is meant to parse, and leaves ok
   false. */
void checked_run_as(struct checked *c, const char *name, const char *source);

/* Free the tokens, the messages and the memory pool of c, which holds
   its tree and its types, whatever checked_run gave. */
void checked_release(struct checked *c);

/* Print every message of d and the source they came from. */
void print_diagnostics(const struct diagnostics *d, const char *source);

/* Count a failure when ok is false, and print every message of d with
   the source. */
void expect_accepted(bool ok, const struct diagnostics *d, const char *source);

/* Count a failure unless ok is false and the first message of d stands at
   line and column and reads message. */
void expect_refused(bool ok, const struct diagnostics *d, const char *source,
                    int line, int column, const char *message);

/* Check that source is accepted, and print every message when it is
   not. */
void accepts(const char *source);

/* Check that source is refused, and that the first message stands at line
   and column and reads message. */
void rejects(const char *source, int line, int column, const char *message);

/* Check that source is refused with message first, wherever it stands. */
void rejects_with(const char *source, const char *message);

/* A source checked and lowered into ir. ok is false when it does not
   check, which counts a failure. The caller optimises ir as its test
   needs. */
struct lowered {
    struct checked front;
    struct ir_module ir;
    bool ok;
};

/* Check source as the module main and lower it into l with no options
   of lower_module, as lowered_run_with does. */
void lowered_run(struct lowered *l, const char *source);

/* lowered_run with the options of lower_module, LOWER_DEV among them. A
   source that does not check is not lowered, and its ir stays empty. */
void lowered_run_with(struct lowered *l, const char *source,
                      unsigned options);

/* Free the IR and the checked source of l, whatever lowered_run gave. */
void lowered_release(struct lowered *l);

/* Lower the checked module as name into out, with the options of
   lower_module, no `--trace` pattern and the default package version. */
void lower_as(struct module *module, const char *name, struct ir_module *out,
              unsigned options);

/* The modules one compilation holds. */
#define COMPILATION_MODULES 16

/* One compilation of several modules, as antic runs it. The modules share
   the memory pool, the types and the messages, so a type keeps one
   identity, and each module checks against the libraries added before
   it. */
struct compilation {
    struct arena arena;
    struct types types;
    struct diagnostics diags;
    struct token_list tokens[COMPILATION_MODULES];
    size_t token_lists;
    const struct interface *libraries[COMPILATION_MODULES];
    size_t library_count;
};

/* Set up c, which needs no setup before. */
void compilation_open(struct compilation *c);

/* Free the tokens, the messages and the memory pool of c, which holds
   every tree and type of its modules. */
void compilation_close(struct compilation *c);

/* Lex, parse and check source as the module name against the libraries
   of c. program is that of sema_check. *ok tells whether all three
   passed, with the messages in c->diags. Gives the tree the parser built,
   or NULL. A module past COMPILATION_MODULES counts a failure, gives NULL
   and leaves *ok false. */
struct module *compilation_check(struct compilation *c, const char *name,
                                 const char *source, bool program, bool *ok);

/* Add iface to the libraries of c. A library past COMPILATION_MODULES
   counts a failure and is dropped. */
void compilation_add(struct compilation *c, const struct interface *iface);

/* Read the library file of the size bytes at data with antl_read against
   no library, into the types and the memory pool of c. Its functions and
   globals are appended to program. Gives the interface, or NULL with the
   message in error. */
struct interface *compilation_read(struct compilation *c,
                                   const uint8_t *data, size_t size,
                                   struct ir_module *program, char *error,
                                   size_t error_size);

/* Append the IR of m to out as ir_print does, without the lines of the
   types the runtime declares, which start with "type anti.rt.", and
   without the globals that hold the text of a failed check, which only
   a call of anti_rt_check_failed prints. Their forms are the
   subjects of runtime_types and check_texts in test_lower.c alone, and
   every other expected text leaves them out, so a change of the
   descriptor or of the words of a check rewrites one text. */
void ir_print_own(struct text *out, const struct ir_module *m);

#endif
