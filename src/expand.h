#ifndef ANTIC_EXPAND_H
#define ANTIC_EXPAND_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"
#include "ir.h"
#include "layout.h"

/* What one target selects as instructions of their own at one level.
   flags_native says whether it selects a flag operation and reads its
   flags where the instruction leaves them. vector_native says whether it
   selects a simd operation on the simd struct agg of size bytes. */
struct expand_target {
    bool (*flags_native)(const struct ir_inst *inst);
    bool (*vector_native)(const struct ir_inst *inst,
                          const struct ir_aggtype *agg, uint64_t size,
                          enum cpu_level cpu);
    enum cpu_level cpu;
    const struct ir_module *m;
    struct layouts *layouts;
};

/* The back end expands the operations of the IR that no target selects
   as instructions of their own into plain operations. It runs after the
   layout resolved every type of f to a fixed width. The width and the
   signedness of each operation are then those of the target. So are the
   offsets of the lanes of a simd struct. A flag operation stays
   where flags_native says the target sets the flags that the reads
   take. A simd operation stays where vector_native says the target has
   instructions for it. Returns whether anything changed. */
bool expand_function(struct ir_function *f, const struct expand_target *t);

#endif
