#ifndef ANTIC_LLVM_EMIT_H
#define ANTIC_LLVM_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "cpu.h"
#include "llvm_debug.h"
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
       objects. */
    bool one_module;
    /* The program loads plugins, which resolve its symbols against it. */
    bool exports;
    /* The function of the runtime that a shared library runs when it
       loads, or NULL. llvm.global_ctors names it beside the function of
       each module that compiles its patterns. */
    const char *constructor;
    /* -g, and release mode, which the compile unit records. */
    bool debug;
    bool optimized;
    /* The spans of the text that the debug information added, which the
       build id leaves out, or NULL. */
    struct debug_spans *spans;
    /* The text will end with the notice of llvm_emit_licenses, which
       llvm.used of the module names. */
    bool notice;
};

/* Append the LLVM IR text of m, laid out for the target by l, to out.
   An operation, an operand or a signature that no step built so far
   translates is refused: the function returns false and writes to error
   a message that names it and the step of the work order that adds it. */
bool llvm_emit_module(struct text *out, const struct llvm_emit_options *o,
                      const struct ir_module *m, struct layouts *l,
                      char *error, size_t error_size);

/* Append the text of the object that holds the copy of the package
   header of a static library for target t: the bytes in a private
   constant in the section of the format, which no symbol names. */
void llvm_emit_package(struct text *out, enum target t, const char *bytes,
                       size_t length);

/* Append the names a COFF host of plugins exports through its .def file,
   one per line: the symbol of each function m defines, and of each
   datum with DATA after it. */
void llvm_emit_names(struct text *out, enum target t,
                     const struct ir_module *m);

/* Append to the text of a program or a shared library the notice
   anti_licenses: the bytes and a NUL, in the read-only section. The
   module before it was emitted with notice set. */
void llvm_emit_licenses(struct text *out, enum target t, const char *bytes,
                        size_t length);

#endif
