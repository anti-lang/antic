#ifndef ANTIC_WHOLE_H
#define ANTIC_WHOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ir.h"
#include "text.h"

/* The passes over the IR of the whole program. They run after the last
   module compiles and before the link, in every build mode, and read the
   class records that lowering writes.

   DESIGN: the program holds the IR of every module. A library file
   carries the IR of its module, and antic loads every library file the
   program imports. The compilation of the main module therefore sees the
   whole program in dev mode as well as in release mode. */

/* The classes of a program and the tables that each class pointer may
   point at. */
struct whole;

struct whole *whole_build(const struct ir_module *program);
void whole_free(struct whole *w);

/* The distinct functions that a call through the table of a class may
   reach at slot. The class is the one whose descriptor is the global
   descriptor. The entries come from every concrete table that a pointer
   to the class may point at. An empty entry is IR_NO_INDEX. Returns the
   count, and the list lives until the next call or whole_free. */
size_t whole_entries(struct whole *w, uint32_t descriptor, uint32_t slot,
                     const uint32_t **out);

#endif
