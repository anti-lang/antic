#ifndef ANTIC_LLVM_DEBUG_H
#define ANTIC_LLVM_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "debug.h"
#include "ir.h"
#include "target.h"
#include "text.h"

/* The debug information of `-g` in the LLVM text, as the section "Debug
   information" of docs/work-order-llvm-back-end.md gives it: one
   !DICompileUnit, one !DIFile per source file, one !DISubprogram per
   function and a !DILocation on every instruction with a line. Lines
   only, so no variable, type or parameter is described.

   DESIGN: COFF writes the compile unit and a subprogram per function in
   every build, with -g and without it. CodeView then writes the symbol
   record of each function, which names a static function in the PDB, as
   the records of debug.c do for the native back end. Without -g no
   instruction carries a line, the subprogram names line 0, and a
   function takes the location of line 0 where it needs one.

   DESIGN: the build id digests the text without what the debug
   information added. Each addition is recorded as a span of the text:
   the ` !dbg !N` of a definition, the `, !dbg !N` of an instruction, the
   debug flags in the list of the module flags and the metadata nodes at
   the end. A `-g` build and a plain build of one program then share one
   build id, on COFF as well. */

struct llvm_debug {
    enum target target;
    const struct ir_module *m;
    const char *module;             /* the module that the text compiles */
    bool on;                        /* the text carries debug metadata */
    bool lines;                     /* -g */
    bool optimized;                 /* release mode */
    uint32_t next;                  /* the next metadata id */
    uint32_t flags;                 /* the id of the first debug flag */
    uint32_t flag_count;
    uint32_t unit;
    uint32_t files;                 /* the id of the DIFile of file 0 */
    uint32_t type;                  /* the !DISubroutineType */
    /* The function being written: its subprogram, whether one of its
       instructions has a location, and the last location it took. */
    const struct ir_function *f;
    uint32_t subprogram;
    bool located;
    uint32_t line;
    uint32_t location;
    struct text nodes;              /* the metadata nodes */
    struct debug_spans *spans;      /* NULL for a caller that reads none */
};

/* Whether a text for target t carries debug metadata, with -g or
   without it. */
bool llvm_debug_wanted(enum target t, bool lines);

/* Begin the debug information of the text of m. Its metadata ids start
   at first, after every id the rest of the text names. */
void llvm_debug_init(struct llvm_debug *d, enum target t,
                     const struct ir_module *m, const char *module,
                     bool lines, bool optimized, uint32_t first,
                     struct debug_spans *spans);

void llvm_debug_free(struct llvm_debug *d);

/* Append ` !dbg !N` to the definition of f, which out ends with, and
   write the subprogram of f. local says that f has internal linkage. */
void llvm_debug_open(struct llvm_debug *d, struct text *out,
                     const struct ir_function *f, bool local);

/* Give the instructions that body holds after from the location of
   line, the line of the IR instruction they translate. spans takes the
   additions as offsets into body. */
void llvm_debug_at(struct llvm_debug *d, struct text *body, size_t from,
                   uint32_t line, struct debug_spans *spans);

/* Add the spans of body, appended to out at base, to the spans of d. */
void llvm_debug_place(struct llvm_debug *d, const struct debug_spans *body,
                      size_t base);

/* Append the debug flags to the list of the module flags, which out ends
   with before its closing brace. */
void llvm_debug_flags(struct llvm_debug *d, struct text *out);

/* Append the debug flags, !llvm.dbg.cu and every metadata node. */
void llvm_debug_finish(struct llvm_debug *d, struct text *out);

/* Record the bytes from start to end of a text as a span. A span that
   follows the last one without a byte between them joins it. */
void llvm_debug_mark(struct debug_spans *s, size_t start, size_t end);

#endif
