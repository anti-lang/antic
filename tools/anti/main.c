#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "linker.h"
#include "manifest.h"
#include "sdk.h"
#include "selfpath.h"
#include "userdirs.h"
#include "test.h"
#include "text.h"

static int usage(FILE *out)
{
    fputs("usage: anti sdk export [--sdk <MacOSX.sdk>] [-o <dir>]\n"
          "       anti sdk import <bundle> [--sysroot <dir>]\n"
          "       anti test [--release] [--work <dir>] [-I <dir>]\n"
          "                 [--runtime <dir>] [--llvm-mc <path>]\n"
          "                 <file.anti>...\n"
          "       anti check [--warn-undocumented] [--targets all]\n"
          "                  [--work <dir>] [-I <dir>] [--runtime <dir>]\n"
          "                  [<file.anti>...]\n"
          "\n"
          "sdk export packs the .tbd stubs and the version of Apple's SDK into\n"
          "apple-sdk-<version>.tar.xz, on a Mac. sdk import unpacks that bundle\n"
          "into the sysroot of Anti on any host, for a program that names a\n"
          "framework. Apple's licence governs where the bundle may be used.\n"
          "\n"
          "test compiles each module with its `tests` and `fixtures` blocks,\n"
          "writes a runner that calls every test of the module, links it and\n"
          "runs it. Every other build drops both blocks. --release runs the\n"
          "same tests with the checks and the assertions off. It reads the\n"
          "`[inject]` and `[inject.test]` tables of anti.toml in the current\n"
          "directory and passes each provider to antic.\n"
          "\n"
          "check runs the front end on every source, compiles the `anti`\n"
          "blocks of the doc comments, reports the doc warnings and reads the\n"
          "layout of every file against the formatter rules. It writes no\n"
          "artifact of a program and reports one line per class. Without a\n"
          "file it takes every source under the directories that `[layout]`\n"
          "of anti.toml names. --targets all runs the front end once per\n"
          "target, so a program that type-checks on the host is proven to\n"
          "type-check on all six.\n",
          out);
    return out == stdout ? 0 : 2;
}

/* DESIGN: the sysroot of the runtime archive lies beside its lib/, so it
   is found wherever the archive is. --sysroot names another one. */
static bool default_sysroot(struct text *out)
{
    struct text archive = {0};

    if (!runtime_archive(&archive)) {
        text_free(&archive);
        return false;
    }
    text_appendf(out, "%s/%s", text_cstr(&archive), RUNTIME_SYSROOT_DIR);
    text_free(&archive);
    return true;
}

/* DESIGN: anti finds the runtime archive where antic does, by the one
   rule in runtime_archive. That is the directory above the running
   executable when it holds lib/, and the user's data directory
   otherwise. */
static bool default_runtime(struct text *out)
{
    return runtime_archive(out);
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
    if (argc >= 2 && strcmp(argv[1], "check") == 0) {
        const char **sources = malloc((size_t)argc * sizeof *sources);
        const char **roots = malloc((size_t)argc * sizeof *roots);
        const char *work = "build/check";
        const char *runtime = NULL;
        struct text home = {0};
        size_t count = 0;
        size_t root_count = 0;
        bool undocumented = false;
        bool all_targets = false;
        int status;
        if (sources == NULL || roots == NULL) {
            fputs("anti: out of memory\n", stderr);
            return 70;
        }
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--warn-undocumented") == 0) {
                undocumented = true;
            } else if (strcmp(argv[i], "--targets") == 0 && i + 1 < argc &&
                       strcmp(argv[i + 1], "all") == 0) {
                all_targets = true;
                i++;
            } else if (strcmp(argv[i], "--work") == 0 && i + 1 < argc) {
                work = argv[++i];
            } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
                runtime = argv[++i];
            } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
                roots[root_count++] = argv[++i];
            } else if (argv[i][0] == '-') {
                free((void *)sources);
                free((void *)roots);
                return usage(stderr);
            } else {
                sources[count++] = argv[i];
            }
        }
        if (runtime == NULL && default_runtime(&home)) {
            runtime = text_cstr(&home);
        }
        status = check_run(sources, count, roots, root_count, work, runtime,
                           undocumented, all_targets);
        free((void *)sources);
        free((void *)roots);
        text_free(&home);
        return status;
    }
    if (argc >= 2 && strcmp(argv[1], "test") == 0) {
        const char **sources = malloc((size_t)argc * sizeof *sources);
        const char **roots = malloc((size_t)argc * sizeof *roots);
        const char *work = "build/tests";
        const char *runtime = NULL;
        const char *llvm_mc = NULL;
        struct manifest_inject inject;
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
        /* DESIGN: the manifest is `anti.toml` of the project root, and
           `anti test` runs there. A project without one injects
           nothing, which is no error. */
        if (!manifest_inject_read(MANIFEST_FILE, true, &inject)) {
            free(sources);
            free(roots);
            text_free(&home);
            return 1;
        }
        status = test_run(sources, count, roots, root_count, work, runtime,
                          llvm_mc, release, inject.entries, inject.count);
        manifest_inject_free(&inject);
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
