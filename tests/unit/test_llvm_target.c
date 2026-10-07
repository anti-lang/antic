/* What the LLVM back end writes per target and per processor level: the
   triple, the relocation model and the two function attributes. The data
   layout strings and the feature strings are compared with the pinned
   clang by the test llvm_datalayout_pin. */

#include "../binary_stdio.h"
#include "check.h"

#include "cpu.h"
#include "llvm_run.h"
#include "llvm_target.h"
#include "text.h"

static void triple(enum target t, const char *expected)
{
    struct text out = {0};
    llvm_triple(&out, t);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

static void clang_arch(enum cpu_level level, const char *expected)
{
    struct text out = {0};
    llvm_clang_arch(&out, level);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

/* Whether the comma-separated list holds item as one whole entry. */
static int has_feature(const char *list, const char *item)
{
    size_t n = strlen(item);
    const char *p = list;

    while (*p != '\0') {
        const char *end = strchr(p, ',');
        size_t length = end != NULL ? (size_t)(end - p) : strlen(p);
        if (length == n && strncmp(p, item, n) == 0) {
            return 1;
        }
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return 0;
}

/* A feature that cpu.c gives a level is in its LLVM feature string, and a
   feature it does not give is absent. */
static void agrees(enum cpu_level level, unsigned feature, const char *item)
{
    const char *features = llvm_target_features(level);
    int wanted = cpu_has(level, feature) ? 1 : 0;
    if (has_feature(features, item) != wanted) {
        check_failures++;
        fprintf(stderr, "%s: %s %s %s\n", cpu_name(level),
                wanted ? "lacks" : "has", item, features);
    }
}

void test_llvm_target(void)
{
    int i;

    /* The triples of target.c, with the minimum macOS version appended for
       the load command that .build_version wrote. */
    triple(TARGET_LINUX_X86_64, "x86_64-unknown-linux-gnu");
    triple(TARGET_LINUX_ARM64, "aarch64-unknown-linux-gnu");
    triple(TARGET_MACOS_X86_64, "x86_64-apple-macos11.0");
    triple(TARGET_MACOS_ARM64, "arm64-apple-macos11.0");
    triple(TARGET_WINDOWS_X86_64, "x86_64-pc-windows-msvc");
    triple(TARGET_WINDOWS_ARM64, "aarch64-pc-windows-msvc");

    /* pic on every target, as clang compiles. An x86_64 object of the
       static model holds the address of a datum in 32 bits, and a Windows
       image lies above 4 GiB. */
    CHECK_STR(llvm_relocation_model(TARGET_LINUX_X86_64), "pic");
    CHECK_STR(llvm_relocation_model(TARGET_LINUX_ARM64), "pic");
    CHECK_STR(llvm_relocation_model(TARGET_MACOS_X86_64), "pic");
    CHECK_STR(llvm_relocation_model(TARGET_MACOS_ARM64), "pic");
    CHECK_STR(llvm_relocation_model(TARGET_WINDOWS_X86_64), "pic");
    CHECK_STR(llvm_relocation_model(TARGET_WINDOWS_ARM64), "pic");

    /* The target-cpu column of the work order. */
    CHECK_STR(llvm_target_cpu(CPU_V1), "x86-64");
    CHECK_STR(llvm_target_cpu(CPU_V2), "x86-64-v2");
    CHECK_STR(llvm_target_cpu(CPU_V3), "x86-64-v3");
    CHECK_STR(llvm_target_cpu(CPU_ARMV8_0), "generic");
    CHECK_STR(llvm_target_cpu(CPU_ARMV8_2), "generic");
    CHECK_STR(llvm_target_cpu(CPU_ARMV8_5), "generic");

    /* D4 of docs/work-order-llvm-optimization.md: the five targets whose
       machines are unknown keep the scheduling model of target-cpu. The
       value of macos-arm64 is a measurement, which no test pins. */
    CHECK(llvm_tune_cpu(TARGET_LINUX_X86_64) == NULL);
    CHECK(llvm_tune_cpu(TARGET_LINUX_ARM64) == NULL);
    CHECK(llvm_tune_cpu(TARGET_MACOS_X86_64) == NULL);
    CHECK(llvm_tune_cpu(TARGET_WINDOWS_X86_64) == NULL);
    CHECK(llvm_tune_cpu(TARGET_WINDOWS_ARM64) == NULL);

    /* opt of release mode takes a pipeline and an inline threshold. The
       values are measurements, which no test pins. */
    CHECK(strncmp(llvm_opt_options[0], "-passes=", 8) == 0);
    CHECK(llvm_opt_options[0][8] != '\0');
    CHECK(strncmp(llvm_opt_options[1], "-inline-threshold=", 18) == 0);
    CHECK(llvm_opt_options[1][18] >= '1' && llvm_opt_options[1][18] <= '9');

    /* The -march= value clang takes for the feature string of a level: the
       one the runtime is built at, with the extensions the level adds. */
    clang_arch(CPU_V1, "x86-64");
    clang_arch(CPU_V3, "x86-64-v3");
    clang_arch(CPU_ARMV8_0, "armv8-a");
    clang_arch(CPU_ARMV8_2, "armv8.2-a+fp16+dotprod");
    clang_arch(CPU_ARMV8_5, "armv8.5-a+fp16+dotprod");

    for (i = 0; i < CPU_LEVEL_COUNT; i++) {
        enum cpu_level level = (enum cpu_level)i;
        CHECK(llvm_target_features(level)[0] != '\0');
        if (cpu_arch(level) == ARCH_X86_64) {
            agrees(level, CPU_AVX, "+avx");
            agrees(level, CPU_F16C, "+f16c");
            agrees(level, CPU_SSE4, "+sse4.2");
            agrees(level, CPU_SSE4, "+popcnt");
        } else {
            agrees(level, CPU_LSE, "+lse");
            agrees(level, CPU_FP16, "+fullfp16");
            agrees(level, CPU_DOTPROD, "+dotprod");
            CHECK(has_feature(llvm_target_features(level), "+neon"));
        }
    }

    /* Each target has a data layout, and the two architectures differ. */
    for (i = 0; i < TARGET_COUNT; i++) {
        CHECK(llvm_data_layout((enum target)i)[0] == 'e');
    }
    CHECK(strcmp(llvm_data_layout(TARGET_LINUX_X86_64),
                 llvm_data_layout(TARGET_LINUX_ARM64)) != 0);
}
