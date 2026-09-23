#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bind.h"
#include "build.h"
#include "check.h"
#include "doc.h"
#include "files.h"
#include "fmt.h"
#include "linker.h"
#include "manifest.h"
#include "modpath.h"
#include "sdk.h"
#include "selfpath.h"
#include "syms.h"
#include "userdirs.h"
#include "test.h"
#include "text.h"

/* The text stands in several literals, since C99 guarantees a length of
   4095 bytes for one. */
static int usage(FILE *out)
{
    fputs("usage: anti new <name>\n"
          "       anti build [--release] [--target <t>|all] [--offline]\n"
          "                  [--strip-docs] [--cpu <level>]\n"
          "                  [--lib static|shared] [--bundle-runtime]\n"
          "                  [--soname] [--runtime <dir>] [--llvm-mc <path>]\n"
          "                  [--llvm-ar <path>]\n"
          "       anti run [--release] [--cpu <level>] [--offline]\n"
          "                [--runtime <dir>] [--llvm-mc <path>]\n"
          "       anti sdk export [--sdk <MacOSX.sdk>] [-o <dir>]\n"
          "       anti sdk import <bundle> [--sysroot <dir>]\n"
          "       anti test [--release] [--work <dir>] [-I <dir>]\n"
          "                 [--runtime <dir>] [--llvm-mc <path>]\n"
          "                 <file.anti>...\n"
          "       anti check [--warn-undocumented] [--targets all]\n"
          "                  [--work <dir>] [-I <dir>] [--runtime <dir>]\n"
          "                  [<file.anti>...]\n"
          "       anti doc [--dev] [--private] [--markdown] [-o <dir>]\n"
          "                [--work <dir>] [-I <dir>] [--runtime <dir>]\n"
          "                [<file.anti>|<file.antl>...]\n"
          "       anti fmt [--check] [<file.anti>...]\n"
          "       anti bind <api.json>|--clang <header> [-o <dir>]\n"
          "                 [--module <path>] [--probe] [--target <t>]\n"
          "                 [-I <dir>] [-D <name>[=<value>]] [--runtime <dir>]\n"
          "       anti bind --header <file.antl> [-o <dir>] [-I <dir>]\n"
          "                 [--runtime <dir>]\n"
          "       anti symbols inventory --conf <config.toml> [--from <dir>]\n"
          "                              [--out <symbols.zip>]\n"
          "       anti symbols check --conf <config.toml>\n"
          "                          [--symbols <symbols.zip>]...\n"
          "       anti symbols resolve <trace.txt> --symbols <symbols.zip>...\n",
          out);
    fputs("\n"
          "new writes a project of the default layout: anti.toml, src/ with\n"
          "one module whose path is the package name, and test/. The name is\n"
          "a module path of at least two segments, and the directory takes\n"
          "its last segment.\n"
          "\n"
          "build reads anti.toml of the current directory, resolves the\n"
          "dependencies into anti.lock, fetches every library file it needs\n"
          "and compiles the project. Dev mode is the default: one object per\n"
          "module, cached by the digest of its input, the compiler version\n"
          "and the target, with -g and the dev-mode checks on. --release\n"
          "compiles the whole program in one call, without -g and with the\n"
          "checks off, and writes the symbols archive beside the program.\n"
          "Both write build/<target>/<mode>/ and dist/<target>/<mode>/. A\n"
          "project without `main` is a library project, whose build writes\n"
          "the library file of each module. --lib static and --lib shared\n"
          "write a library for C instead, with its header beside it.\n"
          "\n"
          "run builds for the host and runs what it wrote.\n"
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
          "fmt writes every source in the canonical form of the formatter\n"
          "rules: one tab per level, an item body whose brace opens on its own\n"
          "line, a statement block whose brace opens on the line of the\n"
          "statement, no parentheses around a whole condition, one statement\n"
          "per line and a doc comment re-wrapped at 80 columns. --check writes\n"
          "nothing and lists the files that differ. Without a file it takes\n"
          "every source under the directories that `[layout]` of anti.toml\n"
          "names.\n"
          "\n"
          "doc writes one page per module and an index of them. User docs\n"
          "come from the public interface alone, so a library file is\n"
          "enough and a page built from one reads as the page built from\n"
          "the source. --dev writes the developer's docs, which need the\n"
          "source and carry the private items and the `//#` notes.\n"
          "--private keeps the private items in the user docs and needs\n"
          "the source as well. The pages are plain semantic HTML with a\n"
          "fixed set of class names and no styling, or Markdown with\n"
          "--markdown, which passes the doc text through unchanged.\n"
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
    fputs("\n"
          "bind writes a binding module from raylib_api.json, or from a C\n"
          "header with --clang, which runs clang on it. The module is\n"
          "anti.<name of the file> unless --module names another, and it\n"
          "goes to <dir>/<last segment>.anti with shim_<name>.c beside it\n"
          "when the header has inline functions. --probe writes the ABI\n"
          "probe in C and in Anti as well. -I and -D reach clang and the\n"
          "shim. The layout of every struct is antic's, never clang's.\n"
          "bind --header writes <name>.h from the public interface of the\n"
          "library file <name>.antl, the header that antic --lib writes for\n"
          "the same module.\n"
          "\n"
          "symbols inventory reads the runtime configuration, finds the\n"
          "program beside it, the libraries of its `plugins` directories and\n"
          "those of `[injections]`, and folds the symbols archive of each into\n"
          "one archive keyed by build id, with an index.toml of module, id,\n"
          "version and source. --from names the directory of the archives,\n"
          "which otherwise stand beside each binary. symbols check reports per\n"
          "module whether its symbols are present, stale or missing, and\n"
          "exits with 1 unless every one is present. symbols resolve prints a\n"
          "raw trace with the function and the line of every frame whose\n"
          "build id an archive holds, and leaves every other frame raw.\n",
          out);
    return out == stdout ? 0 : 2;
}

/* `anti symbols` and its three commands. */
static int symbols_command(int argc, char **argv)
{
    const char **symbols = calloc((size_t)argc, sizeof *symbols);
    const char *conf = NULL;
    const char *from = NULL;
    const char *out = "symbols.zip";
    const char *trace = NULL;
    size_t count = 0;
    int status;
    int i;

    if (symbols == NULL) {
        fputs("anti: out of memory\n", stderr);
        return 70;
    }
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--conf") == 0 && i + 1 < argc) {
            conf = argv[++i];
        } else if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            from = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (strcmp(argv[i], "--symbols") == 0 && i + 1 < argc) {
            symbols[count++] = argv[++i];
        } else if (argv[i][0] != '-' && trace == NULL) {
            trace = argv[i];
        } else {
            free((void *)symbols);
            return usage(stderr);
        }
    }
    if (strcmp(argv[2], "inventory") == 0 && conf != NULL && trace == NULL &&
        count == 0) {
        status = syms_inventory(conf, from, out);
    } else if (strcmp(argv[2], "check") == 0 && conf != NULL &&
               trace == NULL && from == NULL) {
        status = syms_check(conf, symbols, count);
    } else if (strcmp(argv[2], "resolve") == 0 && trace != NULL &&
               count > 0 && conf == NULL && from == NULL) {
        status = syms_resolve(trace, symbols, count);
    } else {
        status = usage(stderr);
    }
    free((void *)symbols);
    return status;
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
    if (argc >= 3 && strcmp(argv[1], "new") == 0) {
        if (argc != 3 || argv[2][0] == '-') {
            return usage(stderr);
        }
        return build_new(argv[2]);
    }
    if (argc >= 2 && (strcmp(argv[1], "build") == 0 ||
                      strcmp(argv[1], "run") == 0)) {
        struct build_request request;
        struct text home = {0};
        int status;
        memset(&request, 0, sizeof request);
        request.root = ".";
        request.run = strcmp(argv[1], "run") == 0;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--release") == 0) {
                request.release = true;
            } else if (strcmp(argv[i], "--offline") == 0) {
                request.offline = true;
            } else if (strcmp(argv[i], "--strip-docs") == 0) {
                request.strip_docs = true;
            } else if (strcmp(argv[i], "--bundle-runtime") == 0) {
                request.bundle_runtime = true;
            } else if (strcmp(argv[i], "--soname") == 0) {
                request.soname = true;
            } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
                request.target = argv[++i];
            } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
                request.cpu = argv[++i];
            } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
                request.runtime = argv[++i];
            } else if (strcmp(argv[i], "--llvm-mc") == 0 && i + 1 < argc) {
                request.llvm_mc = argv[++i];
            } else if (strcmp(argv[i], "--llvm-ar") == 0 && i + 1 < argc) {
                request.llvm_ar = argv[++i];
            } else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
                i++;
                if (strcmp(argv[i], "static") == 0) {
                    request.lib = BUILD_LIB_STATIC;
                } else if (strcmp(argv[i], "shared") == 0) {
                    request.lib = BUILD_LIB_SHARED;
                } else {
                    return usage(stderr);
                }
            } else {
                return usage(stderr);
            }
        }
        if (request.runtime == NULL && default_runtime(&home)) {
            request.runtime = text_cstr(&home);
        }
        status = build_run(&request);
        text_free(&home);
        return status;
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
    if (argc >= 2 && strcmp(argv[1], "doc") == 0) {
        const char **sources = malloc((size_t)argc * sizeof *sources);
        const char **roots = malloc((size_t)argc * sizeof *roots);
        struct file_list found = {0};
        struct text src = {0};
        struct text test = {0};
        struct text package = {0};
        struct text home = {0};
        const char *out = "build/doc";
        const char *work = "build/doc-work";
        const char *runtime = NULL;
        size_t count = 0;
        size_t root_count = 0;
        enum doc_form form = DOC_HTML;
        bool dev = false;
        bool private_items = false;
        int status;
        if (sources == NULL || roots == NULL) {
            fputs("anti: out of memory\n", stderr);
            return 70;
        }
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--dev") == 0) {
                dev = true;
            } else if (strcmp(argv[i], "--private") == 0) {
                private_items = true;
            } else if (strcmp(argv[i], "--markdown") == 0) {
                form = DOC_MARKDOWN;
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out = argv[++i];
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
        /* DESIGN: without a file the command takes the source directory
           of `[layout]`, as `anti check` and `anti fmt` do, so the three
           read the same files. The test directory holds no module a
           reader of the library documents. */
        if (count == 0) {
            size_t j;
            if (!manifest_layout_read(MANIFEST_FILE, &src, &test, &package) ||
                !list_tree(text_cstr(&src), SOURCE_SUFFIX, &found)) {
                free((void *)sources);
                file_list_free(&found);
                text_free(&src);
                text_free(&test);
                text_free(&package);
                text_free(&home);
                free((void *)roots);
                return 1;
            }
            free((void *)sources);
            sources = malloc((found.count + 1) * sizeof *sources);
            if (sources == NULL) {
                fputs("anti: out of memory\n", stderr);
                return 70;
            }
            for (j = 0; j < found.count; j++) {
                sources[j] = text_cstr(&found.items[j]);
            }
            count = found.count;
            if (root_count == 0) {
                roots[root_count++] = text_cstr(&src);
            }
        }
        status = doc_run(sources, count, roots, root_count, out, work,
                         runtime, form, dev, private_items);
        free((void *)sources);
        free((void *)roots);
        file_list_free(&found);
        text_free(&src);
        text_free(&test);
        text_free(&package);
        text_free(&home);
        return status;
    }
    if (argc >= 2 && strcmp(argv[1], "fmt") == 0) {
        const char **sources = malloc((size_t)argc * sizeof *sources);
        struct file_list found = {0};
        struct text src = {0};
        struct text test = {0};
        struct text package = {0};
        size_t count = 0;
        bool check = false;
        int status;
        if (sources == NULL) {
            fputs("anti: out of memory\n", stderr);
            return 70;
        }
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--check") == 0) {
                check = true;
            } else if (argv[i][0] == '-') {
                free((void *)sources);
                return usage(stderr);
            } else {
                sources[count++] = argv[i];
            }
        }
        /* DESIGN: without a file the command takes the source and the
           test directory of `[layout]`, as `anti check` does, so that the
           two read the same files. */
        if (count == 0) {
            if (!manifest_layout_read(MANIFEST_FILE, &src, &test, &package) ||
                !list_tree(text_cstr(&src), SOURCE_SUFFIX, &found) ||
                !list_tree(text_cstr(&test), SOURCE_SUFFIX, &found)) {
                free((void *)sources);
                file_list_free(&found);
                text_free(&src);
                text_free(&test);
                text_free(&package);
                return 1;
            }
            free((void *)sources);
            sources = malloc((found.count + 1) * sizeof *sources);
            if (sources == NULL) {
                fputs("anti: out of memory\n", stderr);
                return 70;
            }
            for (count = 0; count < found.count; count++) {
                sources[count] = text_cstr(&found.items[count]);
            }
        }
        status = fmt_run(sources, count, check);
        free((void *)sources);
        file_list_free(&found);
        text_free(&src);
        text_free(&test);
        text_free(&package);
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
    if (argc >= 2 && strcmp(argv[1], "bind") == 0) {
        const char **roots = malloc((size_t)argc * sizeof *roots);
        const char **defines = malloc((size_t)argc * sizeof *defines);
        const char *header = NULL;
        struct bind_request request;
        struct text home = {0};
        size_t root_count = 0;
        int status;
        if (roots == NULL || defines == NULL) {
            fputs("anti: out of memory\n", stderr);
            return 70;
        }
        memset(&request, 0, sizeof request);
        request.out_dir = ".";
        request.includes = roots;
        request.defines = defines;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--header") == 0 && i + 1 < argc) {
                header = argv[++i];
            } else if (strcmp(argv[i], "--clang") == 0 && i + 1 < argc) {
                request.clang = true;
                request.input = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                request.out_dir = argv[++i];
            } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
                request.runtime = argv[++i];
            } else if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) {
                request.module = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
                request.target = argv[++i];
            } else if (strcmp(argv[i], "--probe") == 0) {
                request.probe = true;
            } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
                roots[root_count++] = argv[++i];
            } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
                defines[request.define_count++] = argv[++i];
            } else if (argv[i][0] != '-' && request.input == NULL) {
                request.input = argv[i];
            } else {
                free((void *)roots);
                free((void *)defines);
                return usage(stderr);
            }
        }
        if ((header == NULL) == (request.input == NULL)) {
            free((void *)roots);
            free((void *)defines);
            return usage(stderr);
        }
        if (request.runtime == NULL && default_runtime(&home)) {
            request.runtime = text_cstr(&home);
        }
        if (header != NULL) {
            status = bind_header(header, request.out_dir, request.runtime,
                                 roots, root_count);
        } else {
            request.include_count = root_count;
            status = bind_run(&request);
        }
        free((void *)roots);
        free((void *)defines);
        text_free(&home);
        return status;
    }
    if (argc >= 3 && strcmp(argv[1], "symbols") == 0) {
        return symbols_command(argc, argv);
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
