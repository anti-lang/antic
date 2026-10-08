#include "llvm_target.h"

/* DESIGN: every data layout string is the `target datalayout` line the
   pinned clang of build/deps/clang writes for the triple of the target,
   never one written by hand:

       clang -target <triple> -S -emit-llvm -x c /dev/null -o -

   The test llvm_datalayout_pin runs that command on every run and compares.
   layout.c and these strings agree on the size and the alignment of every
   type, because both follow the C rules of the target. One row per target,
   in the order of enum target. */
static const char *const layouts[TARGET_COUNT] = {
    [TARGET_LINUX_X86_64] = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-"
                            "i128:128-f80:128-n8:16:32:64-S128",
    [TARGET_LINUX_ARM64] = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                           "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32",
    [TARGET_MACOS_X86_64] = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-"
                            "i128:128-f80:128-n8:16:32:64-S128",
    [TARGET_MACOS_ARM64] = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-"
                           "i128:128-n32:64-S128-Fn32",
    [TARGET_WINDOWS_X86_64] = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-"
                              "i128:128-f80:128-n8:16:32:64-S128",
    [TARGET_WINDOWS_ARM64] = "e-m:w-p270:32:32-p271:32:32-p272:64:64-p:64:64-"
                             "i32:32-i64:64-i128:128-n32:64-S128-Fn32",
};

/* DESIGN: one row per level, in the order of enum cpu_level. target-cpu is
   the column of the work order. features is the "target-features" string
   the pinned clang writes for
   `-march=<clang_arch of cpu.c><extensions>`, with `-mcpu=generic
   -mno-fmv` on ARM64. Without -mcpu clang picks apple-m1 on macOS, whose
   features the lower levels lack, and -mno-fmv gives the three ARM64
   triples one string, so a row holds for every target of its architecture.
   extensions are the clang names of what the level adds to the
   architecture, the +fullfp16 and +dotprod of the llvm-mc attributes. The
   test llvm_datalayout_pin compares each row with clang for every target,
   and cpu_levels_pin compares target-cpu and features with
   tools/cpu-levels. */
static const struct {
    const char *target_cpu;
    const char *extensions;
    const char *features;
} levels[CPU_LEVEL_COUNT] = {
    [CPU_V1] = {"x86-64", "", "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87"},
    [CPU_V2] = {"x86-64-v2", "",
                "+cmov,+crc32,+cx16,+cx8,+fxsr,+mmx,+popcnt,+sahf,+sse,"
                "+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87"},
    [CPU_V3] = {"x86-64-v3", "",
                "+avx,+avx2,+bmi,+bmi2,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,"
                "+fxsr,+lzcnt,+mmx,+movbe,+popcnt,+sahf,+sse,+sse2,+sse3,"
                "+sse4.1,+sse4.2,+ssse3,+x87,+xsave"},
    [CPU_ARMV8_0] = {"generic", "", "+fp-armv8,+neon,+v8a,-fmv"},
    [CPU_ARMV8_2] = {"generic", "+fp16+dotprod",
                     "+crc,+dotprod,+fp-armv8,+fullfp16,+lse,+neon,+ras,"
                     "+rdm,+v8.1a,+v8.2a,+v8a,-fmv"},
    [CPU_ARMV8_5] = {"generic", "+fp16+dotprod",
                     "+bti,+ccidx,+complxnum,+crc,+dit,+dotprod,+flagm,"
                     "+fp-armv8,+fp16fml,+fullfp16,+jsconv,+lse,+neon,"
                     "+pauth,+predres,+ras,+rcpc,+rdm,+sb,+ssbs,+v8.1a,"
                     "+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,-fmv"},
};

void llvm_triple(struct text *out, enum target t)
{
    const struct target_info *info = target_info(t);

    text_append(out, info->triple);
    /* The version is the one the linker passes as the platform
       version. */
    if (info->os == OS_MACOS) {
        text_appendf(out, "%d.%d", MACOS_MIN_MAJOR, MACOS_MIN_MINOR);
    }
}

const char *llvm_data_layout(enum target t)
{
    return layouts[t];
}

/* DESIGN: the relocation model of every target is the one the pinned clang
   passes as -mrelocation-model, pic on all six, and llvm_datalayout_pin
   compares them on every run. The static model of x86_64 writes the
   address of a datum as 32 absolute bits. lld-link puts a 64-bit image at
   0x140000000 and keeps the low half of such an address without a word,
   so every windows-x86_64 program that wrote a string ended with
   0xC0000005 in the C library. One row per target, in the order of enum
   target. */
static const char *const models[TARGET_COUNT] = {
    [TARGET_LINUX_X86_64] = "pic",   [TARGET_LINUX_ARM64] = "pic",
    [TARGET_MACOS_X86_64] = "pic",   [TARGET_MACOS_ARM64] = "pic",
    [TARGET_WINDOWS_X86_64] = "pic", [TARGET_WINDOWS_ARM64] = "pic",
};

const char *llvm_relocation_model(enum target t)
{
    return models[t];
}

const char *llvm_target_cpu(enum cpu_level level)
{
    return levels[level].target_cpu;
}

const char *llvm_target_features(enum cpu_level level)
{
    return levels[level].features;
}

/* DESIGN: macos-arm64 tunes for apple-m1 and keeps the instructions of its
   level, choice D4 of docs/work-order-llvm-optimization.md under C1. Every
   Mac of the target has Apple silicon. The machines of the other five
   targets are unknown, so they keep the model of target-cpu and write no
   attribute. tests/bench/ablate/run.py measured it on 2026-10-05 under
   default<O3>, 15 runs each, the medians against generic:

       scalar_loop 0.98, objects 1.00, builder 0.99, simd_loop 0.97,
       map_work 1.00, mixed_work 1.00

   simd_loop lies beyond 2 percent and no object grows, so apple-m1
   wins. A later measurement may choose another, and no
   test pins the value. */
const char *llvm_tune_cpu(enum target t)
{
    return t == TARGET_MACOS_ARM64 ? "apple-m1" : NULL;
}

void llvm_clang_arch(struct text *out, enum cpu_level level)
{
    text_append(out, cpu_clang_arch(level));
    text_append(out, levels[level].extensions);
}
