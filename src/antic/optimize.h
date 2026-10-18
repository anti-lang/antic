#ifndef ANTIC_OPTIMIZE_H
#define ANTIC_OPTIMIZE_H

/* The optimizer of the IR over a program, a module or one function, and
   the drop of the failure arms. */

#include "ir.h"

/* Run the passes on every function of program that has a body, then remove
   what the entries do not reach. entry names the unit of main, as ir_reach
   takes it. The class records go as well, since the removal renumbers the
   functions and the globals they name. */
void optimize_program(struct ir_module *program, const char *entry);

/* Optimize the functions of module alone, for an object of its own in dev
   mode. The functions and data of other modules become declarations,
   whose symbols the objects of those modules define. Every function of
   module stays, and so does every one of the runtime module, which the
   passes over the whole program write. */
void optimize_module(struct ir_module *program, const char *module);

/* Run the passes of optimize_program on one function, without removing
   unused functions. The back end uses it after it folds symbolic
   values. */
void optimize_function(struct ir_function *f);

/* DESIGN: an assertion and a dev-mode check both reach this pass as a
   branch to a block that lowering marked. A library file carries them,
   so the build that compiles the program decides. Cutting the branch
   makes the block unreachable, and the passes that follow remove the
   call and the text with it. */
void optimize_drop_failures(struct ir_module *program, enum ir_fail kind);

#endif
