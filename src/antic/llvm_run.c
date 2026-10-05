#include "llvm_run.h"

#include <stdio.h>

#include "llvm_target.h"
#include "platform.h"
#include "text.h"

/* The file name of path, after its last separator. Windows takes `\` as
   well as `/`. */
static const char *file_name(const char *path)
{
    const char *name = path;
    const char *p;

    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }
    return name;
}

/* DESIGN: llc runs in the directory of its output and writes the output
   by its file name alone. llc records the name of its output as the
   object name of the CodeView of a COFF object, S_OBJNAME, and lld-link
   carries it into the PDB, so a path there would ship a path of the
   build. The input and llc itself then go by absolute paths. It is the
   rule that nothing shipped holds a build path, which windows_debug in
   linker.c gives the link. */
static int run_llc(const char *llc, const char *const options[3],
                   const char *input, const char *output)
{
    const char *name = file_name(output);
    struct text directory = {0};
    struct text program = {0};
    struct text absolute = {0};
    const char *argv[] = {llc, options[0], options[1], options[2], input,
                          "-o", output, NULL};
    int run = -1;

    if (name == output) {
        return process_run(argv);
    }
    text_appendf(&directory, "%.*s", (int)(name - output), output);
    if (file_name(llc) != llc) {
        if (!absolute_path(llc, &program)) {
            goto done;
        }
        argv[0] = text_cstr(&program);
    }
    if (!absolute_path(input, &absolute)) {
        goto done;
    }
    argv[4] = text_cstr(&absolute);
    argv[6] = name;
    run = process_run_in(text_cstr(&directory), argv);
done:
    text_free(&absolute);
    text_free(&program);
    text_free(&directory);
    return run;
}

/* DESIGN: antic runs the opt and llc programs of anti-lang/llvm-tools on
   LLVM IR text, and links no LLVM library, so antic stays a C11 program
   that builds without LLVM headers. The CPU and its features stand as
   function attributes in the text, so the command lines carry neither,
   and the text alone decides the code. See "Integration route" in
   docs/work-order-llvm-back-end.md. */
/* DESIGN: under --lto opt runs the pipeline before the link that clang
   runs for -flto=full or -flto=thin at -O2, and ThinLTO writes the
   summary of the module beside the bitcode. The LTO of lld runs the rest
   over the program and the runtime together. */
static bool run_lto(const struct llvm_run *r, const char *text_path,
                    const char *output)
{
    const char *full[] = {r->opt, "-passes=lto-pre-link<O2>", "-o", output,
                          text_path, NULL};
    const char *thin[] = {r->opt, "--thinlto-bc",
                          "-passes=thinlto-pre-link<O2>", "-o", output,
                          text_path, NULL};

    if (process_run(r->lto == LTO_THIN ? thin : full) != 0) {
        fprintf(stderr, "antic: opt failed\n");
        return false;
    }
    return true;
}

bool llvm_run(const struct llvm_run *r, const char *text_path,
              const char *bitcode_path, const char *output)
{
    struct text model = {0};
    const char *opt[] = {r->opt, "-passes=default<O2>", "-o", bitcode_path,
                         text_path, NULL};
    const char *options[3];
    int run;

    if (r->lto != LTO_NONE) {
        return run_lto(r, text_path, output);
    }
    if (r->optimize && process_run(opt) != 0) {
        fprintf(stderr, "antic: opt failed\n");
        return false;
    }
    text_appendf(&model, "-relocation-model=%s",
                 llvm_relocation_model(r->target));
    options[0] = r->optimize ? "-O2" : "-O1";
    options[1] = r->assembly ? "-filetype=asm" : "-filetype=obj";
    options[2] = text_cstr(&model);
    run = run_llc(r->llc, options, r->optimize ? bitcode_path : text_path,
                  output);
    text_free(&model);
    if (run != 0) {
        fprintf(stderr, "antic: llc failed\n");
        return false;
    }
    return true;
}
