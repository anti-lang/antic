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

/* The most options of llc before its input: the level, the file type,
   the relocation model, the two that split sections and the table of
   significant addresses. */
#define LLC_OPTIONS 6

/* DESIGN: llc runs in the directory of its output and writes the output
   by its file name alone. llc records the name of its output as the
   object name of the CodeView of a COFF object, S_OBJNAME, and lld-link
   carries it into the PDB, so a path there would ship a path of the
   build. The input and llc itself then go by absolute paths. It is the
   rule that nothing shipped holds a build path, which windows_debug in
   linker.c gives the link. */
static int run_llc(const char *llc, const char *const options[LLC_OPTIONS],
                   const char *input, const char *output)
{
    const char *name = file_name(output);
    struct text directory = {0};
    struct text program = {0};
    struct text absolute = {0};
    const char *argv[LLC_OPTIONS + 5];
    size_t n = 0;
    size_t i;
    size_t at_input;
    size_t at_output;
    int run = -1;

    argv[n++] = llc;
    for (i = 0; i < LLC_OPTIONS && options[i] != NULL; i++) {
        argv[n++] = options[i];
    }
    at_input = n;
    argv[n++] = input;
    argv[n++] = "-o";
    at_output = n;
    argv[n++] = output;
    argv[n] = NULL;

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
    argv[at_input] = text_cstr(&absolute);
    argv[at_output] = name;
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

/* DESIGN: release mode runs default<O3> at an inline threshold of 225,
   choices D8 and D3 of docs/work-order-llvm-optimization.md under C1.
   tests/bench/ablate/run.py measured them on macos-arm64 on 2026-10-05,
   15 runs each. Medians against default<O2>, the pipelines first:

       program      O2        O3        O2,licm,unswitch,irce
       scalar_loop  299.9 ms  1.00      1.00
       objects      141.1 ms  1.01      1.01
       builder      264.9 ms  0.94      1.00
       simd_loop    130.3 ms  1.00      1.00
       map_work     182.5 ms  0.82      0.96
       mixed_work   193.3 ms  0.89      0.99

   The thresholds under default<O3>, against default<O3> alone, which
   inlines at 250. Every threshold lies within 2 percent of every other on
   every program, so the release compile and the object decide:

       program      225   500   1000  2000
       scalar_loop  1.00  1.00  1.00  1.00
       objects      1.00  1.00  1.01  1.00
       builder      1.00  1.00  1.00  1.00
       simd_loop    1.00  1.00  1.00  1.00
       map_work     1.00  1.01  1.01  1.02
       mixed_work   1.01  1.03  1.02  1.01
       mixed_work   2388 ms, 695,312 B at 225, against 2546 ms,
                    713,528 B at 250 and 3920 ms, 933,600 B at 2000

   default<O2> followed by loop-mssa(licm,simple-loop-unswitch) and irce
   lost to default<O3>. opt -verify-each accepted the three pipelines on
   the text of every program. A later measurement may choose another, and
   no test pins either value. docs/reports/2026-10-05-llvm-opt-config.md
   holds the tables. */
const char *const llvm_opt_options[LLVM_OPT_OPTION_COUNT] = {
    "-passes=default<O3>", "-inline-threshold=225"};

/* DESIGN: --profile-generate and --profile-use run the pipeline of
   release mode with the instrumentation of a profile or with its use, as
   clang does for -fprofile-generate and -fprofile-use. Both place it at
   the same point of the pipeline, so the profile of an instrumented
   build fits the build that uses it. The instrumented program writes
   LLVM_PROFILE_DEFAULT in its working directory, the name clang gives,
   and LLVM_PROFILE_FILE names another file. %m stands for the signature of
   the program, so the runs of one program merge into one file and those
   of two programs stay apart. */
#define LLVM_PROFILE_DEFAULT "default_%m.profraw"

static bool run_opt(const struct llvm_run *r, const char *text_path,
                    const char *bitcode_path)
{
    struct text profile = {0};
    const char *opt[9];
    size_t n = 0;
    bool ok;

    opt[n++] = r->opt;
    opt[n++] = llvm_opt_options[0];
    opt[n++] = llvm_opt_options[1];
    if (r->profile_generate) {
        opt[n++] = "-pgo-kind=pgo-instr-gen-pipeline";
        opt[n++] = "-profile-file=" LLVM_PROFILE_DEFAULT;
    } else if (r->profile_use != NULL) {
        text_appendf(&profile, "-profile-file=%s", r->profile_use);
        opt[n++] = "-pgo-kind=pgo-instr-use-pipeline";
        opt[n++] = text_cstr(&profile);
    }
    opt[n++] = "-o";
    opt[n++] = bitcode_path;
    opt[n++] = text_path;
    opt[n] = NULL;
    ok = process_run(opt) == 0;
    text_free(&profile);
    if (!ok) {
        fprintf(stderr, "antic: opt failed\n");
    }
    return ok;
}

bool llvm_run(const struct llvm_run *r, const char *text_path,
              const char *bitcode_path, const char *output)
{
    struct text model = {0};
    const char *options[LLC_OPTIONS] = {NULL};
    size_t n = 0;
    int run;

    if (r->lto != LTO_NONE) {
        return run_lto(r, text_path, output);
    }
    if (r->optimize && !run_opt(r, text_path, bitcode_path)) {
        return false;
    }
    text_appendf(&model, "-relocation-model=%s",
                 llvm_relocation_model(r->target));
    options[n++] = r->optimize ? "-O2" : "-O1";
    options[n++] = r->assembly ? "-filetype=asm" : "-filetype=obj";
    options[n++] = text_cstr(&model);
    /* DESIGN: an ELF or COFF object puts every function and every datum
       into a section of its own, in release and dev mode, so that the
       link drops the ones the program never reaches. Eddie decided on
       2026-10-06 that an executable carries no code it never uses. llc
       splits a Mach-O object per symbol already, and -dead_strip alone
       gave 32.1 percent off hello world and 4.3 off mixed_work on
       macos-arm64 that day. See the entry on dropped code under "Scope
       and toolchain" in docs/decisions.md, and drop_unused in
       linker.c. */
    if (target_info(r->target)->format != FORMAT_MACHO) {
        options[n++] = "-function-sections";
        options[n++] = "-data-sections";
    }
    /* The table names every symbol whose address the program uses, so
       the safe folding of lld leaves those apart. See drop_unused in
       linker.c. */
    if (llvm_safe_folding(r->target)) {
        options[n++] = "-addrsig";
    }
    run = run_llc(r->llc, options, r->optimize ? bitcode_path : text_path,
                  output);
    text_free(&model);
    if (run != 0) {
        fprintf(stderr, "antic: llc failed\n");
        return false;
    }
    return true;
}
