/* The runtime of anti.lang.StackTrace and of the frames that `fail`
   captures. */
#ifndef ANTI_TRACE_H
#define ANTI_TRACE_H

#include <stdbool.h>
#include <stdint.h>

#include "std.h"

/* The layout of anti.lang.RawFrame. */
struct anti_raw_frame {
    uint64_t address;
    struct anti_text module;
    struct anti_text build_id;
    uint64_t base;
};

/* The layout of anti.lang.Frame. */
struct anti_frame {
    uint64_t address;
    struct anti_text function;
    struct anti_text file;
    int64_t line;
};

/* The return addresses of the caller and the calls above it, at most
   room of them into into, less the skip innermost. Gives the count. */
int64_t anti_rt_trace_walk(uint64_t *into, int64_t room, int64_t skip);

/* Fill out with the module that address lies in, the build id of the
   module and its load base. Both texts are empty and the base is 0 for
   an address outside every module. */
void anti_rt_trace_frame(uint64_t address, struct anti_raw_frame *out);

/* The text of count frames: one line per module, then one per frame,
   ended by a NUL and without a last newline. The caller frees it, and
   NULL means the memory ran out. */
unsigned char *anti_rt_trace_text(const struct anti_raw_frame *frames,
                                  int64_t count);

/* Fill out with the function of the frame from the symbol table. Its
   file and line come from the line table when the build carries one. */
void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out);

#endif
