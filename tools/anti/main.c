#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linker.h"
#include "sdk.h"
#include "selfpath.h"
#include "test.h"
#include "text.h"

static int usage(FILE *out)
{
    fputs("usage: anti sdk export [--sdk <MacOSX.sdk>] [-o <dir>]\n"
          "       anti sdk import <bundle> [--sysroot <dir>]\n"
          "       anti test [--release] [--work <dir>] [-I <dir>]\n"
          "                 [--runtime <dir>] [--llvm-mc <path>]\n"
          "                 <file.anti>...\n"
          "\n"
          "sdk export packs the .tbd stubs and the version of Apple's SDK into\n"
          "apple-sdk-<version>.tar.xz, on a Mac. sdk import unpacks that bundle\n"
          "into the sysroot of Anti on any host, for a program that names a\n"
          "framework. Apple's licence governs where the bundle may be used.\n"
          "\n"
          "test compiles each module with its `tests` and `fixtures` blocks,\n"
          "writes a runner that calls every test of the module, links it and\n"
          "runs it. Every other build drops both blocks. --release runs the\n"
          "same tests with the checks and the assertions off.\n",
          out);
    return out == stdout ? 0 : 2;
}

/* DESIGN: an installed anti sits in bin/ beside antic, and the sysroot of
   the runtime archive lies beside bin/. --sysroot names another one. */
static bool default_sysroot(struct text *out)
{
    struct text bin = {0};
    size_t cut;

    if (!self_directory(&bin)) {
        text_free(&bin);
        return false;
    }
    cut = bin.length;
    while (cut > 0 && bin.data[cut - 1] != '/' && bin.data[cut - 1] != '\\') {
        cut--;
    }
    text_append_bytes(out, bin.data, cut);
    text_append(out, RUNTIME_SYSROOT_DIR);
    text_free(&bin);
    return true;
}

/* DESIGN: an installed anti sits in bin/ of the runtime archive, so
   without --runtime it takes the directory above itself, as antic does.
   A tree with no lib/ is a build tree, and the driver reports the missing
   runtime then. */
static bool default_runtime(struct text *out)
{
    struct text bin = {0};
    struct text lib = {0};
    bool ok = false;
    size_t cut;

    if (!self_directory(&bin)) {
        text_free(&bin);
        return false;
    }
    cut = bin.length;
    while (cut > 0 && bin.data[cut - 1] != '/' && bin.data[cut - 1] != '\\') {
        cut--;
    }
    if (cut > 1) {
        text_append_bytes(out, bin.data, cut - 1);
        text_appendf(&lib, "%s/%s", text_cstr(out), RUNTIME_LIB_DIR);
        ok = directory_exists(text_cstr(&lib));
    }
    text_free(&bin);
    text_free(&lib);
    return ok;
}

int main(int argc, char **argv)
{
    int i;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        return usage(stdout);
    }
    /* The version of the tool is the version of Anti, which the build
       takes from tools/version. Step 10 of a release reads it from a
       fresh install, beside the one antic prints. */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("anti %s\n", ANTIC_VERSION);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "test") == 0) {
        const char **sources = malloc((size_t)argc * sizeof *sources);
        const char **roots = malloc((size_t)argc * sizeof *roots);
        const char *work = "build/tests";
        const char *runtime = NULL;
        const char *llvm_mc = NULL;
        struct text home = {0};
        size_t count = 0;
        size_t root_count = 0;
        bool release = false;
        int status;
        if (sources == NULL || roots == NULL) {
            fputs("anti: out of memory\n", stderr);
            return 70;
        }
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--release") == 0) {
                release = true;
            } else if (strcmp(argv[i], "--work") == 0 && i + 1 < argc) {
                work = argv[++i];
            } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
                runtime = argv[++i];
            } else if (strcmp(argv[i], "--llvm-mc") == 0 && i + 1 < argc) {
                llvm_mc = argv[++i];
            } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
                roots[root_count++] = argv[++i];
            } else if (argv[i][0] == '-') {
                free(sources);
                free(roots);
                return usage(stderr);
            } else {
                sources[count++] = argv[i];
            }
        }
        /* The runtime archive lies beside bin/, as sdk import finds it. */
        if (runtime == NULL && default_runtime(&home)) {
            runtime = text_cstr(&home);
        }
        status = test_run(sources, count, roots, root_count, work, runtime,
                          llvm_mc, release);
        free(sources);
        free(roots);
        text_free(&home);
        return status;
    }
    if (argc < 3 || strcmp(argv[1], "sdk") != 0) {
        return usage(stderr);
    }
    if (strcmp(argv[2], "export") == 0) {
        const char *sdk = NULL;
        const char *out = ".";
        for (i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--sdk") == 0 && i + 1 < argc) {
                sdk = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out = argv[++i];
            } else {
                return usage(stderr);
            }
        }
        return sdk_export(sdk, out);
    }
    if (strcmp(argv[2], "import") == 0 && argc >= 4) {
        const char *bundle = argv[3];
        struct text sysroot = {0};
        int status;
        for (i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--sysroot") == 0 && i + 1 < argc) {
                text_append(&sysroot, argv[++i]);
            } else {
                text_free(&sysroot);
                return usage(stderr);
            }
        }
        if (sysroot.length == 0 && !default_sysroot(&sysroot)) {
            fputs("anti: the system does not say where anti is, so pass "
                  "--sysroot\n", stderr);
            return 1;
        }
        status = sdk_import(bundle, text_cstr(&sysroot));
        text_free(&sysroot);
        return status;
    }
    return usage(stderr);
}
