#ifndef ANTIC_LLVM_TARGET_H
#define ANTIC_LLVM_TARGET_H

#include "cpu.h"
#include "target.h"
#include "text.h"

/* What the LLVM back end writes for a target and a processor level. The
   section "Targets, CPU levels and object formats" of
   docs/work-order-llvm-back-end.md settles each value. */

/* Append the triple of the text: the one of target_info, with the minimum
   macOS version appended on macOS. */
void llvm_triple(struct text *out, enum target t);

/* The `target datalayout` string the pinned clang writes for the triple. */
const char *llvm_data_layout(enum target t);

/* The -relocation-model= value of llc: pic on Linux and macOS, static on
   Windows. */
const char *llvm_relocation_model(enum target t);

/* The "target-cpu" and "target-features" function attributes of a level.
   The feature string is the one the pinned clang writes for the level. */
const char *llvm_target_cpu(enum cpu_level level);
const char *llvm_target_features(enum cpu_level level);

/* Append the -march= value clang takes for the feature string of a level:
   the value of cpu_clang_arch with the extensions the level adds. The test
   llvm_datalayout_pin passes it to the pinned clang. */
void llvm_clang_arch(struct text *out, enum cpu_level level);

#endif
