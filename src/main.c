#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "cpu.h"
#include "driver.h"
#include "linker.h"
#include "selfpath.h"
#include "userdirs.h"
#include "target.h"

static int usage(FILE *out)
{
    fputs("usage: antic [options] <file.anti> [<library.antl>...]\n"
          "             [<object.o> <archive.a>...]\n"
          "\n"
          "options:\n"
          "  -o <file>            write the executable, or with -S the\n"
          "                       assembly or with -c the library, to <file>\n"
          "  -S                   stop after writing the assembly text\n"
          "  -c                   write the library file <file.antl>\n"
          "  -g                   write the source positions, and keep the\n"
          "                       debug sections of the link\n"
          "  --dev                compile the module alone into its object,\n"
          "                       and link it when it defines main. The\n"
          "                       input may be a library file instead\n"
          "  --tests              keep the `tests` and `fixtures` blocks,\n"
          "                       which every other build drops\n"
          "  --lib static|shared  write a library for C and its header\n"
          "  --bundle-runtime     put the runtime into the static library\n"
          "  --soname             give the shared library the major version\n"
          "  --no-runtime         with --lib shared: a plugin, which links\n"
          "                       no runtime and uses the host's\n"
          "  --closed             a program without the dynamic exports\n"
          "                       that a plugin resolves against\n"
          "  --llvm-ar <path>     the llvm-ar executable\n"
          "  --linker lld|platform  link with lld of the runtime archive, the\n"
          "                       default, or with the platform linker\n"
          "  --framework <name>   link a macOS program against a framework\n"
          "                       of Apple's SDK\n"
          "  -I <dir>             a search root: module a.b is a/b.anti\n"
          "                       or a/b.antl under it\n"
          "  --anti-internal      allow -c for a module under anti.\n"
          "  --strip-docs         leave the doc text out of the library\n"
          "  --doc-warnings       warn about documentation, for anti check\n"
          "  --front-end          run the lexer, the parser and the checker\n"
          "                       and write nothing, for anti check\n"
          "  --package-name <p>   the package header of the library: name,\n"
          "  --package-version <v>  version,\n"
          "  --inject <I=P>       the provider P of the injectable "
          "interface I\n"
          "  --dependency <n,c,u> a dependency with name, constraint, URL,\n"
          "  --license <spdx>     the licence identifier,\n"
          "  --license-text <f>   the file of the licence text\n"
          "  --attribution <line> and one attribution line\n"
          "  --target <name>      compile for <name>, for example macos-arm64\n"
          "  --cpu <level>        the processor level: v1, v2 or v3 on\n"
          "                       x86_64, armv8.0, armv8.2 or armv8.5 on\n"
          "                       ARM64. The target's default stands\n"
          "                       otherwise\n"
          "  --llvm-mc <path>     the llvm-mc executable\n"
          "  --runtime <dir>      the directory holding lib/<target>/\n"
          "  --dump-tokens        print the tokens of <file.anti> and stop\n"
          "  --dump-ast           print the syntax tree and stop\n"
          "  --dump-types         print the checked tree and stop\n"
          "  --dump-ir            print the IR after lowering and stop\n"
          "  --dump-opt           print the IR after optimization and stop\n"
          "  --dump-select        print the selected machine code and stop\n"
          "  --dump-alloc         print the machine code after register\n"
          "                       allocation and stop\n"
          "  --print-host-target  print the target antic runs on\n"
          "  --print-targets      print the six targets and their facts\n"
          "  --print-cpu-levels   print the processor levels and the\n"
          "                       default level of each target\n"
          "  --version            print the version\n",
          out);
    return out == stderr ? 2 : 0;
}

/* Take the value of an option such as -o, or report that it is missing. */
/* One line per target: object format, calling convention and triple. */
static int print_targets(void)
{
    int t;

    printf("%-15s %-7s %-15s %s\n", "target", "format", "convention",
           "triple");
    for (t = 0; t < TARGET_COUNT; t++) {
        const struct target_info *info = target_info((enum target)t);
        printf("%-15s %-7s %-15s %s\n", target_name((enum target)t),
               object_format_name(info->format),
               convention_name(info->convention), info->triple);
    }
    return 0;
}

/* The level table and the defaults, in the form of tools/cpu-levels. The
   test cpu_levels_pin compares the two. */
static int print_cpu_levels(void)
{
    int i;

    for (i = 0; i < CPU_LEVEL_COUNT; i++) {
        enum cpu_level level = (enum cpu_level)i;
        const char *attributes = cpu_attributes(level);
        printf("level %s %s %d %s %s\n", cpu_name(level),
               cpu_arch(level) == ARCH_ARM64 ? "arm64" : "x86_64",
               (int)cpu_id(level), cpu_clang_arch(level),
               attributes[0] == '\0' ? "-" : attributes);
    }
    for (i = 0; i < TARGET_COUNT; i++) {
        printf("default %s %s\n", target_name((enum target)i),
               cpu_name(cpu_default((enum target)i)));
    }
    return 0;
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static const char *value_of(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "antic: %s needs a value\n", argv[*i]);
        return NULL;
    }
    *i += 1;
    return argv[*i];
}

static int run(int argc, char **argv, struct options *o)
{
    struct options options = *o;
    bool have_host = target_host(&options.target);
    const char *target = NULL;
    const char *cpu = NULL;
    struct text home = {0};
    int status;
    int i;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char **slot = NULL;

        if (strcmp(arg, "--version") == 0) {
            printf("antic %s\n", ANTIC_VERSION);
            return 0;
        } else if (strcmp(arg, "--help") == 0) {
            return usage(stdout);
        } else if (strcmp(arg, "--print-host-target") == 0) {
            if (!have_host) {
                fputs("antic: unknown host target\n", stderr);
                return 1;
            }
            printf("%s\n", target_name(options.target));
            return 0;
        } else if (strcmp(arg, "--print-targets") == 0) {
            return print_targets();
        } else if (strcmp(arg, "--print-cpu-levels") == 0) {
            return print_cpu_levels();
        } else if (strcmp(arg, "--dump-tokens") == 0) {
            options.dump_tokens = true;
            continue;
        } else if (strcmp(arg, "--dump-ast") == 0) {
            options.dump_ast = true;
            continue;
        } else if (strcmp(arg, "--dump-types") == 0) {
            options.dump_types = true;
            continue;
        } else if (strcmp(arg, "--dump-ir") == 0) {
            options.dump_ir = true;
            continue;
        } else if (strcmp(arg, "--dump-alloc") == 0) {
            options.dump_alloc = true;
            continue;
        } else if (strcmp(arg, "--dump-select") == 0) {
            options.dump_select = true;
            continue;
        } else if (strcmp(arg, "--dump-opt") == 0) {
            options.dump_opt = true;
            continue;
        } else if (strcmp(arg, "-c") == 0) {
            options.library = true;
            continue;
        } else if (strcmp(arg, "-S") == 0) {
            options.assembly_only = true;
            continue;
        } else if (strcmp(arg, "--anti-internal") == 0) {
            options.internal = true;
            continue;
        } else if (strcmp(arg, "--lib") == 0) {
            const char *kind = value_of(argc, argv, &i);
            if (kind == NULL) {
                return 2;
            }
            if (strcmp(kind, "static") == 0) {
                options.lib = LIB_STATIC;
            } else if (strcmp(kind, "shared") == 0) {
                options.lib = LIB_SHARED;
            } else {
                fprintf(stderr, "antic: --lib takes static or shared, not %s\n",
                        kind);
                return 2;
            }
            continue;
        } else if (strcmp(arg, "--bundle-runtime") == 0) {
            options.bundle_runtime = true;
            continue;
        } else if (strcmp(arg, "--soname") == 0) {
            options.soname = true;
            continue;
        } else if (strcmp(arg, "--no-runtime") == 0) {
            options.no_runtime = true;
            continue;
        } else if (strcmp(arg, "--closed") == 0) {
            options.closed = true;
            continue;
        } else if (strcmp(arg, "--llvm-ar") == 0) {
            slot = &options.llvm_ar;
        } else if (strcmp(arg, "-g") == 0) {
            options.debug = true;
            continue;
        } else if (strcmp(arg, "--dev") == 0) {
            options.dev = true;
            continue;
        } else if (strcmp(arg, "--tests") == 0) {
            options.tests = true;
            continue;
        } else if (strcmp(arg, "--no-reflect") == 0) {
            options.no_reflect = true;
            continue;
        } else if (strcmp(arg, "--asserts") == 0) {
            options.asserts = ASSERTS_ON;
            continue;
        } else if (strcmp(arg, "--no-asserts") == 0) {
            options.asserts = ASSERTS_OFF;
            continue;
        } else if (strcmp(arg, "--checks") == 0) {
            options.checks = CHECKS_ON;
            continue;
        } else if (strcmp(arg, "--no-checks") == 0) {
            options.checks = CHECKS_OFF;
            continue;
        } else if (strcmp(arg, "--no-hooks") == 0) {
            options.no_hooks = true;
            continue;
        } else if (strcmp(arg, "--no-trace") == 0) {
            options.trace = TRACE_OFF;
            continue;
        /* DESIGN: `--trace` takes a pattern or stands alone. The word
           after it is the pattern when it is no option and no last
           argument, because the last argument is the source file. The
           pattern `writes` names the writes and no package. */
        } else if (strcmp(arg, "--trace") == 0) {
            if (i + 2 < argc && argv[i + 1][0] != '-') {
                const char *pattern = argv[++i];
                if (strcmp(pattern, "writes") == 0) {
                    options.trace_writes = true;
                } else {
                    options.trace_patterns[options.trace_pattern_count++] =
                        pattern;
                }
            } else {
                options.trace = TRACE_ON;
            }
            continue;
        } else if (strcmp(arg, "--strip-docs") == 0) {
            options.strip_docs = true;
            continue;
        } else if (strcmp(arg, "--linker") == 0) {
            const char *value = value_of(argc, argv, &i);
            if (value == NULL) {
                return 2;
            }
            if (strcmp(value, "lld") == 0) {
                options.linker = LINKER_LLD;
            } else if (strcmp(value, "platform") == 0) {
                options.linker = LINKER_PLATFORM;
            } else {
                fprintf(stderr, "antic: --linker takes lld or platform\n");
                return 2;
            }
            continue;
        } else if (strcmp(arg, "--doc-warnings") == 0) {
            options.doc_warnings = true;
            continue;
        } else if (strcmp(arg, "--front-end") == 0) {
            options.front_end = true;
            continue;
        } else if (strcmp(arg, "--framework") == 0) {
            const char *value = value_of(argc, argv, &i);
            if (value == NULL) {
                return 2;
            }
            options.frameworks[options.framework_count++] = value;
            continue;
        } else if (strcmp(arg, "--inject") == 0) {
            const char *value = value_of(argc, argv, &i);
            if (value == NULL) {
                return 2;
            }
            if (strchr(value, '=') == NULL) {
                fprintf(stderr, "antic: --inject takes an interface and its "
                                "provider, as --inject Interface=Provider, "
                                "found %s\n", value);
                return 2;
            }
            options.inject[options.inject_count++] = value;
            continue;
        } else if (strcmp(arg, "--dependency") == 0 ||
                   strcmp(arg, "--attribution") == 0) {
            const char *value = value_of(argc, argv, &i);
            if (value == NULL) {
                return 2;
            }
            if (arg[2] == 'd') {
                options.dependencies[options.dependency_count++] = value;
            } else {
                options.attribution[options.attribution_count++] = value;
            }
            continue;
        } else if (strcmp(arg, "--package-name") == 0) {
            slot = &options.package_name;
        } else if (strcmp(arg, "--package-version") == 0) {
            slot = &options.package_version;
        } else if (strcmp(arg, "--license") == 0) {
            slot = &options.license;
        } else if (strcmp(arg, "--license-text") == 0) {
            slot = &options.license_text;
        } else if (strcmp(arg, "-I") == 0) {
            if ((options.roots[options.root_count] =
                     value_of(argc, argv, &i)) == NULL) {
                return 2;
            }
            options.root_count++;
            continue;
        } else if (strcmp(arg, "-o") == 0) {
            slot = &options.output;
        } else if (strcmp(arg, "--target") == 0) {
            slot = &target;
        } else if (strcmp(arg, "--cpu") == 0) {
            slot = &cpu;
        } else if (strcmp(arg, "--llvm-mc") == 0) {
            slot = &options.llvm_mc;
        } else if (strcmp(arg, "--runtime") == 0) {
            slot = &options.runtime;
        } else if (arg[0] == '-') {
            fprintf(stderr, "antic: unknown option %s\n", arg);
            return usage(stderr);
        } else if (ends_with(arg, ANTL_SUFFIX)) {
            options.libraries[options.library_count++] = arg;
            continue;
        } else if (link_is_input(arg)) {
            options.objects[options.object_count++] = arg;
            continue;
        } else if (options.input == NULL) {
            options.input = arg;
            continue;
        } else {
            fputs("antic: one input file only\n", stderr);
            return usage(stderr);
        }
        if ((*slot = value_of(argc, argv, &i)) == NULL) {
            return 2;
        }
    }

    /* DESIGN: in dev mode the first library file is the input when no
       source file is given. Each module of the graph gets its own object. */
    if (options.input == NULL && options.dev && options.library_count > 0) {
        options.input = options.libraries[0];
        options.libraries++;
        options.library_count--;
    }
    if (options.input == NULL) {
        return usage(stderr);
    }
    if (target != NULL) {
        if (!target_from_name(target, &options.target)) {
            fprintf(stderr, "antic: unknown target %s\n", target);
            return 2;
        }
    } else if (!have_host) {
        fputs("antic: unknown host target, pass --target\n", stderr);
        return 2;
    }
    /* The level follows the target, so --cpu is read after it and its
       name is checked against the target's architecture. */
    options.cpu = cpu_default(options.target);
    if (cpu != NULL && !cpu_from_name(cpu, options.target, &options.cpu)) {
        fprintf(stderr, "antic: %s is no processor level of %s\n", cpu,
                target_name(options.target));
        return 2;
    }
    /* DESIGN: antic looks for the runtime archive in two places, in this
       order. An antic that sits in the bin/ of an archive takes the
       directory above itself. That is the tree of a build, and of a
       package before it is installed. An installed antic sits on the
       PATH instead, in the bin directory of the user, and the archive
       stands in the user's data directory. runtime_archive holds the
       rule, and the anti tool asks the same function. */
    if (options.runtime == NULL && runtime_archive(&home)) {
        options.runtime = text_cstr(&home);
    }
    status = driver_run(&options);
    text_free(&home);
    return status;
}

int main(int argc, char **argv)
{
    struct options options = {0};
    int status;

    /* At most argc - 1 arguments are library files or link inputs. */
    options.libraries = malloc((size_t)argc * sizeof *options.libraries);
    options.objects = malloc((size_t)argc * sizeof *options.objects);
    options.roots = malloc((size_t)argc * sizeof *options.roots);
    options.dependencies = malloc((size_t)argc * sizeof *options.dependencies);
    options.attribution = malloc((size_t)argc * sizeof *options.attribution);
    options.frameworks = malloc((size_t)argc * sizeof *options.frameworks);
    options.trace_patterns =
        malloc((size_t)argc * sizeof *options.trace_patterns);
    options.inject = malloc((size_t)argc * sizeof *options.inject);
    if (options.libraries == NULL || options.objects == NULL ||
        options.roots == NULL || options.dependencies == NULL ||
        options.attribution == NULL || options.frameworks == NULL ||
        options.trace_patterns == NULL || options.inject == NULL) {
        fputs("antic: out of memory\n", stderr);
        return 70;
    }
    status = run(argc, argv, &options);
    free((void *)options.libraries);
    free((void *)options.objects);
    free((void *)options.roots);
    free((void *)options.dependencies);
    free((void *)options.attribution);
    free((void *)options.frameworks);
    free((void *)options.trace_patterns);
    free((void *)options.inject);
    return status;
}
