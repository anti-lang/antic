#ifndef ANTIC_WHOLE_PARTS_H
#define ANTIC_WHOLE_PARTS_H

/* The inside of the pass over the whole program, which its two files
   share. whole.c holds the analysis: the class model of the program,
   release devirtualisation, the merge of identical copies, the singleton
   check, and whole_program, which runs the passes. whole_tables.c writes
   the tables the runtime reads: the registry of classes, the default of
   the backtraces, the trampolines of reflection, the used-slot bitmaps,
   the slots of injection and the table of what a plugin provides. No
   other file includes this one. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ir.h"
#include "reach.h"
#include "text.h"
#include "whole.h"

/* A walk over functions: each function seen, and the ones still to
   visit. The singleton check and the cycle check of the providers walk
   with it. */
struct whole_walk {
    bool *seen;
    uint32_t *work;
    size_t pending;
};

/* whole.c */

/* The class record whose descriptor is global g, or IR_NO_INDEX for the
   root and for any global that describes no class. */
uint32_t whole_class_of(const struct whole *w, uint32_t g);
/* The class record of a descriptor, the value that stands for the root
   when it is the root's, or IR_NO_INDEX. */
uint32_t whole_record_of(const struct whole *w, uint32_t descriptor);
/* Whether the class record lies at or below the class above, which may
   be the root's. */
bool whole_at_or_below(const struct whole *w, uint32_t record, uint32_t above);

/* whole_tables.c */

/* Whether the reach r of m holds a runtime function that reads the
   registry. */
bool whole_reads_registry(const struct ir_module *m, const struct ir_reach *r);
/* Write the registry of classes, which lists each class when reflect is
   set. */
void whole_write_registry(struct ir_module *m, bool reflect);
/* Whether the reach r of m holds the runtime function that every `fail`
   asks whether backtraces are on. */
bool whole_asks_backtrace(const struct ir_module *m, const struct ir_reach *r);
/* Write `anti_rt_backtrace_default`, off for a release build. */
void whole_write_backtrace_default(struct ir_module *m, bool release);
/* Whether the reach r of m holds the runtime function of reflect.call. */
bool whole_calls_through_reflection(const struct ir_module *m,
                                    const struct ir_reach *r);
/* Write `anti_rt_trampolines`, empty unless full is set. */
void whole_write_trampolines(struct ir_module *m, bool full);
/* Write the used-slot bitmaps of the calls of the reach r. every says
   that reflection may reach every other slot, and force writes the
   table where no call reaches a slot. */
void whole_write_slots(struct whole *w, struct ir_module *m,
                       const struct ir_reach *r, bool every, bool force);
/* The injection pass: the interfaces, their slots, their providers and
   the cycle check. Each error goes to errors as one line. */
void whole_write_injections(struct whole *w, struct ir_module *m,
                            const struct whole_options *o,
                            struct text *errors);
/* Write the table of what a plugin provides. Each error goes to errors
   as one line. */
void whole_write_provides(struct whole *w, struct ir_module *m,
                          struct text *errors);

#endif
