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

/* Whether `fail` captures the frames of the error it gives. The build
   decides, on in dev mode and off in release, unless the command line
   named --anti.backtrace. */
bool anti_rt_backtrace_on(void);

/* The default of anti_rt_backtrace_on, which the pass over the whole
   program writes as `anti_rt_backtrace_default`: on is 1 in a dev build
   and 0 in release. */
struct anti_backtrace_default {
    int64_t on;
};

extern const struct anti_backtrace_default anti_rt_backtrace_default;

/* anti_rt_trace_walk, the walk of the frames, is a call of the platform
   layer in platform.h. */

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
   file and line come from the line table when the build carries one.
   DESIGN: the image and its file come from the address, as the loader of
   this process knows it, never from the module and base the frame
   records. `deserialize` of a StackTrace writes those from text, so a
   record that named them would let the text choose the memory and the
   file symbolize reads. */
void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out);

#endif
