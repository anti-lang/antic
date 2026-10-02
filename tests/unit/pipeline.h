/* The compiler stages the unit tests run a source through: the front end
   alone, the front end and lowering, or the whole way to the machine code
   of every function. One copy, so a change of a stage reaches every test
   that runs it. */
#ifndef ANTIC_TEST_PIPELINE_H
#define ANTIC_TEST_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "ast.h"
#include "cpu.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "mach.h"
#include "target.h"
#include "text.h"
#include "types.h"

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

/* Check source into c. A source that does not parse counts a failure and
   prints its first message, since every test source is meant to parse.
   checked_release frees c whatever the result. */
void checked_run(struct checked *c, const char *source);
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
   needs. lowered_release frees l whatever the result. */
struct lowered {
    struct checked front;
    struct ir_module ir;
    bool ok;
};

void lowered_run(struct lowered *l, const char *source);
void lowered_release(struct lowered *l);

/* The machine functions of a module, after instruction selection at a
   level and, when allocate is set, register allocation. functions holds
   one entry per function of the module, NULL for one without a body, and
   error the reason when ok is false. machine_release frees m. */
struct machine {
    struct mach_function **functions;
    size_t count;
    char error[200];
    bool ok;
};

void machine_build(struct machine *m, struct ir_module *ir, enum target t,
                   enum cpu_level level, bool allocate);
void machine_release(struct machine *m);

/* Append the machine code of every function of ir to out, or the error
   of the stage that refused it. */
void machine_text(struct ir_module *ir, enum target t, enum cpu_level level,
                  bool allocate, struct text *out);

/* Run source through the front end, lowering and optimize_program, and
   append its machine code to out as machine_text does. */
void machine_of(const char *source, enum target t, enum cpu_level level,
                bool allocate, struct text *out);

#endif
