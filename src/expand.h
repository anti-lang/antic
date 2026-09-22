#ifndef ANTIC_EXPAND_H
#define ANTIC_EXPAND_H

#include <stdbool.h>

#include "ir.h"

/* The back end expands the operations of the IR that no target selects
   as instructions of their own into plain operations. It runs after the
   layout resolved every type of f to a fixed width. The width and the
   signedness of each operation are then those of the target. Returns
   whether anything changed. */
bool expand_function(struct ir_function *f);

#endif
