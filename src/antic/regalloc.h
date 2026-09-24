#ifndef ANTIC_REGALLOC_H
#define ANTIC_REGALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mach.h"
#include "target.h"

/* The physical registers of a target, one bit each of a 64-bit set. A
   frame has room to save all of them. The loop over the set then bounds
   the count. */
enum { REGALLOC_REGISTERS = 64 };

/* The stack frame of a function after register allocation. */
struct frame {
    bool needed;            /* the function calls or uses the stack */
    uint64_t size;          /* bytes below the frame record, a multiple of 16 */
    uint8_t saved[REGALLOC_REGISTERS];  /* the callee-saved ones in use */
    int64_t saved_offset[REGALLOC_REGISTERS];   /* offsets from sp */
    size_t saved_count;
    bool probe;             /* the prologue probes the pages of the frame */
    bool unwind;            /* the prologue and epilogues carry .seh_ lines */
    enum convention convention;
};

/* Replace every virtual register of f with a physical register of target
   t, or with a stack slot when none is free. Lay out the stack frame and
   add the prologue and the epilogues. Returns false and writes a message
   to error for a frame larger than the frame limit of the target. */
bool regalloc_function(enum target t, struct mach_function *f, char *error,
                       size_t error_size);

#endif
