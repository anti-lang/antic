#ifndef ANTIC_DEBUG_H
#define ANTIC_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ir.h"
#include "mach.h"
#include "target.h"
#include "text.h"

/* The debug information of `-g`: a `.file` directive per source file of
   the program and a `.loc` directive before the first instruction of
   every statement. llvm-mc turns them into a DWARF line table on ELF and
   Mach-O. On COFF it turns them into a CodeView line table.

   DESIGN: the emitter asks for a position before each instruction, and
   this writes a directive only where the line changes. An instruction of
   no statement carries the line 0 and writes nothing, a prologue or an
   epilogue among them. It keeps the position of the one before it. */

/* One range of an assembly file that `-g` added, as offsets into it. */
struct debug_span {
    size_t start;
    size_t end;
};

/* DESIGN: the build id is the digest of what a program executes, so the
   ranges below are left out of it. A `-g` link and a plain link of one
   program then carry one id. That id is what ties a trace of the plain
   binary to the symbols archive of the other. A range is recorded only
   where `-g` decides the bytes. The `S_LPROC32` records of COFF and the
   end label they read stand in every build, so neither is a range. */
struct debug_spans {
    struct debug_span *items;
    size_t count;
    size_t capacity;
};

void debug_spans_free(struct debug_spans *s);

/* The debug information of one assembly file. */
struct debug {
    enum target target;
    const struct ir_module *m;
    const char *module;         /* the program's own module */
    bool on;                    /* -g */
    uint32_t file;              /* the file being written */
    uint32_t line;              /* the line last written */
    size_t function;            /* the functions opened */
    struct debug_spans *spans;  /* NULL for a caller that reads none */
};

void debug_init(struct debug *d, enum target t, const struct ir_module *m,
                const char *module, bool on, struct debug_spans *spans);

/* Append a directive per source file and the label the code starts at. It
   goes after the text section and before the first function. */
void debug_files(struct debug *d, struct text *out);

/* Append what opens the function f, after its symbol: the position of its
   declaration, which its prologue belongs to. */
void debug_open(struct debug *d, struct text *out,
                const struct ir_function *f);

/* Append the position of the next instruction, when it has one and it
   differs from the position before it. */
void debug_at(struct debug *d, struct text *out, uint32_t line);

/* Append the label that ends the function last opened. */
void debug_close(struct debug *d, struct text *out);

/* Append the sections of the debug information. On ELF and Mach-O that is
   the compile unit which names the line table, and on COFF the line table
   of every function. functions holds what the emitter wrote, in its
   order. */
void debug_sections(struct debug *d, struct text *out,
                    struct mach_function *const *functions);

#endif
