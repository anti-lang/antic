#ifndef ANTIC_REACH_H
#define ANTIC_REACH_H

/* The interface of reach.c: what the entries of a program reach. */

#include <stdbool.h>

#include "ir.h"

/* What the entries of a program reach: one flag per function and one
   per global of the module, by index. */
struct ir_reach {
    bool *functions;
    bool *globals;
};

/* DESIGN: one rule decides what a program reaches. The optimizer removes
   what it does not reach, and the passes over the whole program write
   the registry, the slot bitmaps and the trampolines from it, so the two
   ask this one function and cannot disagree.

   The entry is main of the unit entry, or every function of that unit
   when it has no main or when all is set, and every export fn and export
   global. A copy of a generic belongs to the unit that made it. A NULL
   entry makes every function an entry. The table and the descriptor of
   an export class are entries of their own, because C code reads them
   and no Anti code has to. With all set, for the object of one module,
   every datum of the unit is an entry as well, because another module
   may name a descriptor that no function here does. A live declaration
   of a datum reaches the definition of the same module and name, which
   the program holds beside it once the library files are read. A host
   of plugins, m->hosts, makes every function and every datum it defines
   an entry, because a plugin binds against any name of the modules the
   host imports and of the runtime, whether the host uses it or not. */
void ir_reach(const struct ir_module *m, const char *entry, bool all,
              struct ir_reach *out);

/* Release the two lists of r. */
void ir_reach_free(struct ir_reach *r);

#endif
