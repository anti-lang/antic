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
   point at. whole_build returns one that the caller frees with
   whole_free. */
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

/* The options of the passes. A NULL entry makes every function an
   entry. */
struct whole_options {
    const char *entry;              /* the main module */
    bool release;                   /* optimise the code as well */
    bool reflect;                   /* list the classes in the registry */
    bool bundled;                   /* the runtime joins the output */
    bool dev;                       /* one object per module. */
    bool library;                   /* a library for C */
    /* --lib shared --no-runtime: a plugin. It carries the table of what
       it provides, and the host holds the registry and the slots. */
    bool plugin;
    /* --closed: the program carries no exports for a plugin to bind
       against, and it takes no provider from a library. */
    bool closed;
    /* The `--inject Interface=Provider` arguments of the build, which
       name the provider of every injectable interface. */
    const char *const *inject;
    size_t inject_count;
};

/* Whether the program can host a plugin: it injects an interface, or it
   calls the loader. Such a program exports its symbols. */
bool whole_hosts_plugins(const struct ir_module *m);

/* Run the passes over program. Each error goes to errors as one line.
   Returns true when there is none. */
bool whole_program(struct ir_module *program,
                   const struct whole_options *options, struct text *errors);

#endif
