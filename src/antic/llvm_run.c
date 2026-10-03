#include "llvm_run.h"

#include <stdio.h>

#include "llvm_target.h"
#include "platform.h"
#include "text.h"

/* DESIGN: antic runs the opt and llc programs of anti-lang/llvm-tools on
   LLVM IR text, and links no LLVM library, so antic stays a C11 program
   that builds without LLVM headers. The CPU and its features stand as
   function attributes in the text, so the command lines carry neither,
   and the text alone decides the code. See "Integration route" in
   docs/work-order-llvm-back-end.md. */
bool llvm_run(const struct llvm_run *r, const char *text_path,
              const char *bitcode_path, const char *output)
{
    struct text model = {0};
    const char *opt[] = {r->opt, "-passes=default<O2>", "-o", bitcode_path,
                         text_path, NULL};
    const char *llc[] = {r->llc, NULL, NULL, NULL, "-o", output, NULL, NULL};
    int run;

    if (r->optimize && process_run(opt) != 0) {
        fprintf(stderr, "antic: opt failed\n");
        return false;
    }
    text_appendf(&model, "-relocation-model=%s",
                 llvm_relocation_model(r->target));
    llc[1] = r->optimize ? "-O2" : "-O1";
    llc[2] = r->assembly ? "-filetype=asm" : "-filetype=obj";
    llc[3] = text_cstr(&model);
    llc[6] = r->optimize ? bitcode_path : text_path;
    run = process_run(llc);
    text_free(&model);
    if (run != 0) {
        fprintf(stderr, "antic: llc failed\n");
        return false;
    }
    return true;
}
