#ifndef ANTIC_LLVM_EMIT_H
#define ANTIC_LLVM_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "cpu.h"
#include "ir.h"
#include "layout.h"
#include "target.h"
#include "text.h"

/* The translation of the IR into LLVM IR text, the back end of
   docs/work-order-llvm-back-end.md. It runs after layout_data and
   layout_resolve, so it never sees a symbolic size. opt and llc of the
   pinned release read the text it writes. */

struct llvm_emit_options {
    enum target target;
    enum cpu_level cpu;
    const char *module;     /* the module that the object compiles */
    /* The object holds one module, a dev object or a plugin, and not the
       whole program. Its definitions are then global for the other
       objects, as emit_module of the native back end writes them. */
    bool one_module;
    /* The program loads plugins, which resolve its symbols against it. */
    bool exports;
};

/* Append the LLVM IR text of m, laid out for the target by l, to out.
   An operation, an operand or a signature that no step built so far
   translates is refused: the function returns false and writes to error
   a message that names it and the step of the work order that adds it. */
bool llvm_emit_module(struct text *out, const struct llvm_emit_options *o,
                      const struct ir_module *m, struct layouts *l,
                      char *error, size_t error_size);

#endif
