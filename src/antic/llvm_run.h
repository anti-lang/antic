#ifndef ANTIC_LLVM_RUN_H
#define ANTIC_LLVM_RUN_H

#include <stdbool.h>

#include "antic.h"
#include "target.h"

/* The run of opt and llc of the pinned release on the text that
   llvm_emit_module writes, "Integration route" of
   docs/work-order-llvm-back-end.md. */

/* The suffixes of the intermediate files beside the output: the text and
   the bitcode opt writes from it. */
#define LLVM_TEXT_SUFFIX ".ll"
#define LLVM_BITCODE_SUFFIX ".bc"

struct llvm_run {
    const char *opt;            /* the opt program */
    const char *llc;            /* the llc program */
    enum target target;         /* gives the relocation model of llc */
    /* Release mode: opt runs llvm_opt_options into the bitcode, and llc
       reads the bitcode at -O2. Dev mode: llc reads the text at -O1
       alone. */
    bool optimize;
    bool assembly;              /* -S: llc writes assembly, not an object */
    /* --lto in release mode: opt writes the bitcode of the mode as the
       output, which lld links, and llc does not run. */
    enum lto lto;
    /* Release mode: opt instruments the program, or optimizes it with the
       indexed profile at profile_use. */
    bool profile_generate;
    const char *profile_use;
};

/* The options of opt in release mode without --lto, before the output
   and the text: the pass pipeline and the inline threshold. */
#define LLVM_OPT_OPTION_COUNT 2
extern const char *const llvm_opt_options[LLVM_OPT_OPTION_COUNT];

/* Run the tools of r on the text at text_path and write the object, or
   the assembly, to output. bitcode_path receives the output of opt in
   release mode. A tool that fails has printed its own message on
   standard error, and the function then prints `antic: opt failed` or
   `antic: llc failed` and returns false. */
bool llvm_run(const struct llvm_run *r, const char *text_path,
              const char *bitcode_path, const char *output);

#endif
