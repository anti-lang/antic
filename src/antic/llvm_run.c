/* The runs of opt and llc on the LLVM text, with the options of each mode. */

#include "llvm_run.h"

#include <stdio.h>
#include <stdlib.h>

#include "llvm_target.h"
#include "platform.h"
#include "text.h"

/* The most arguments of a run of opt or llc. The longest is llc with
   seven options, the level, the file type, the relocation model, the two
   that split sections, the table of significant addresses and the
   alignment of functions, then the input, -o and the output. opt takes
   nine at most under --lto. */
#define RUN_ARGS 11

/* The arguments of one run, with a NULL after the last. */
struct args {
    const char *v[RUN_ARGS + 1];
    size_t n;
};

static void push(struct args *a, const char *arg)
{
    if (a->n >= RUN_ARGS) {
        /* Unreachable while RUN_ARGS covers every command. */
        fputs("antic: a run of opt or llc needs more than RUN_ARGS "
              "arguments\n", stderr);
        abort();
    }
    a->v[a->n++] = arg;
    a->v[a->n] = NULL;
}

/* DESIGN: llc runs in the directory of its output and writes the output
   by its file name alone. llc records the name of its output as the
   object name of the CodeView of a COFF object, S_OBJNAME, and lld-link
   carries it into the PDB, so a path there would ship a path of the
   build. The input and llc itself then go by absolute paths. It is the
   rule that nothing shipped holds a build path, which windows_debug in
   linker.c gives the link. */
static int run_llc(const char *llc, const struct args *options,
                   const char *input, const char *output)
{
    const char *name = platform_file_name(output);
    struct text directory = {0};
    struct text program = {0};
    struct text absolute = {0};
    struct args argv = {0};
    size_t i;
    size_t at_input;
    size_t at_output;
    int run = -1;

    push(&argv, llc);
    for (i = 0; i < options->n; i++) {
        push(&argv, options->v[i]);
    }
    at_input = argv.n;
    push(&argv, input);
    push(&argv, "-o");
    at_output = argv.n;
    push(&argv, output);

    if (name == output) {
        return platform_run(argv.v);
    }
    text_appendf(&directory, "%.*s", (int)(name - output), output);
    if (platform_file_name(llc) != llc) {
        if (!platform_absolute_path(llc, &program)) {
            goto done;
        }
        argv.v[0] = text_cstr(&program);
    }
    if (!platform_absolute_path(input, &absolute)) {
        goto done;
    }
    argv.v[at_input] = text_cstr(&absolute);
    argv.v[at_output] = name;
    run = platform_run_in(text_cstr(&directory), argv.v);
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
   runs for -flto=full or -flto=thin, and ThinLTO writes the summary of
   the module beside the bitcode. The LTO of lld runs the rest over the
   program and the runtime together, which link_command sets to the same
   level and threshold. Both take O3 and the threshold of
   llvm_opt_options, which release mode chose under C1. At O2 the LTO
   that a release build takes by default ran builder in 1.28 and map_work
   in 1.20 of the time of the object link on macos-arm64 on 2026-10-07,
   and at O3 with 225 every program of tests/bench and mixed_work lay
   within 1 percent of it or below, objects at 0.73. */
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

/* Append the options of the profile of r to the n options of opt. profile
   holds the text of the option of --profile-use, which the caller frees.
   DESIGN: under --lto the profile acts on the pipeline before the link,
   as clang gives -fprofile-generate and -fprofile-use with -flto, and the
   bitcode carries its counts and its summary into the LTO of lld. That
   LTO takes no profile of the instrumentation of IR: ld.lld, ld64.lld and
   lld-link of the pinned release read a context-sensitive profile and a
   sample profile alone. */
static void profile_options(const struct llvm_run *r, struct text *profile,
                            struct args *opt)
{
    if (r->profile_generate) {
        push(opt, "-pgo-kind=pgo-instr-gen-pipeline");
        push(opt, "-profile-file=" LLVM_PROFILE_DEFAULT);
    } else if (r->profile_use != NULL) {
        text_appendf(profile, "-profile-file=%s", r->profile_use);
        push(opt, "-pgo-kind=pgo-instr-use-pipeline");
        push(opt, text_cstr(profile));
    }
}

static bool run_lto(const struct llvm_run *r, const char *text_path,
                    const char *output)
{
    struct text profile = {0};
    struct args opt = {0};
    bool ok;

    push(&opt, r->opt);
    if (r->lto == LTO_THIN) {
        push(&opt, "--thinlto-bc");
        push(&opt, "-passes=thinlto-pre-link<O3>");
    } else {
        push(&opt, "-passes=lto-pre-link<O3>");
    }
    push(&opt, LLVM_INLINE_THRESHOLD);
    profile_options(r, &profile, &opt);
    push(&opt, "-o");
    push(&opt, output);
    push(&opt, text_path);
    ok = platform_run(opt.v) == 0;
    text_free(&profile);
    if (!ok) {
        fprintf(stderr, "antic: opt failed\n");
    }
    return ok;
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
    "-passes=default<O3>", LLVM_INLINE_THRESHOLD};

static bool run_opt(const struct llvm_run *r, const char *text_path,
                    const char *bitcode_path)
{
    struct text profile = {0};
    struct args opt = {0};
    bool ok;

    push(&opt, r->opt);
    push(&opt, llvm_opt_options[0]);
    push(&opt, llvm_opt_options[1]);
    profile_options(r, &profile, &opt);
    push(&opt, "-o");
    push(&opt, bitcode_path);
    push(&opt, text_path);
    ok = platform_run(opt.v) == 0;
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
    struct args options = {0};
    int run;

    if (r->lto != LTO_NONE) {
        return run_lto(r, text_path, output);
    }
    if (r->optimize && !run_opt(r, text_path, bitcode_path)) {
        return false;
    }
    text_appendf(&model, "-relocation-model=%s",
                 llvm_relocation_model(r->target));
    push(&options, r->optimize ? "-O2" : "-O1");
    push(&options, r->assembly ? "-filetype=asm" : "-filetype=obj");
    push(&options, text_cstr(&model));
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
        push(&options, "-function-sections");
        push(&options, "-data-sections");
    }
    /* The address-significance table names every symbol whose address
       the program uses, so the safe folding of lld leaves those apart.
       Every target writes it. See drop_unused in linker.c. */
    push(&options, "-addrsig");
    /* DESIGN: every function of an ARM64 object starts on a 16-byte
       boundary, and the object runtime of each ARM64 target is compiled
       with -falign-functions=16 to match, see CMakeLists.txt. Eddie
       decided it on 2026-10-08 for the object link, which --lto none and
       dev mode take, and every other link of objects. The LTO link of a
       release build keeps the alignment LLVM chooses. llc aligns a small
       function at 4 bytes, so in the object link the four functions of
       the shapes of tests/bench/objects.anti stood in 0x54 bytes and the
       calls through the table alternated between them. Measured with
       tests/bench/ablate/run.py on 2026-10-08, 15 runs each, objects
       under --lto none ran 105.7 ms against 138.4 on macos-arm64. Every
       other program of tests/bench and mixed_work stayed within 1
       percent on macos-arm64 and on anti-linux, which ran objects in
       104 ms either way. mixed_work grew by 1.6 percent on macos-arm64
       and 0.4 on linux-arm64. In the LTO link the alignment changed no
       program by more than 2 percent on 2026-10-07 and grew mixed_work
       by 1.6 percent, so that link stays as it is.
       docs/reports/2026-10-08-allocator-and-alignment.md holds the
       tables. */
    if (target_info(r->target)->arch == ARCH_ARM64) {
        push(&options, "-align-all-functions=4");
    }
    run = run_llc(r->llc, &options, r->optimize ? bitcode_path : text_path,
                  output);
    text_free(&model);
    if (run != 0) {
        fprintf(stderr, "antic: llc failed\n");
        return false;
    }
    return true;
}
