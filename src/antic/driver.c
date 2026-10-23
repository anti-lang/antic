/* The driver of antic: it reads the files of a compilation and runs it,
   the front end, lowering, the passes over the IR, the back end, opt and
   llc, and writes the library file, the C header and the assembly.
   driver_parts.h names the files that hold the rest. */

#include "driver.h"
#include "driver_parts.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "antl.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "linker.h"
#include "layout.h"
#include "llvm_emit.h"
#include "llvm_run.h"
#include "lower.h"
#include "memcheck.h"
#include "header.h"
#include "modpath.h"
#include "platform.h"
#include "notice.h"
#include "optimize.h"
#include "say.h"
#include "whole.h"
#include "rt_abi.h"
#include "parser.h"
#include "sema.h"
#include "types.h"
#include "text.h"
#include "warnings.h"

/* The hash of the runtime, which digests the inputs of a module. */
#include "../rt/digest.h"
#include "../rt/fault.h"

static void extras_free(struct extras *e)
{
    text_free(&e->name);
    text_free(&e->header);
    text_free(&e->package);
    text_free(&e->marker);
    text_free(&e->notice);
    text_free(&e->exports);
    text_free(&e->provides);
    text_free(&e->host_names);
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

/* Read the whole file into out. A source stops at LEX_SOURCE_MAX, and a
   larger one is refused before more of it is read. */
static bool read_file(const char *path, struct text *out, bool source)
{
    size_t limit = source ? LEX_SOURCE_MAX : SIZE_MAX;
    FILE *f = platform_open(path, false);
    char buffer[4096];
    size_t n;
    size_t total = 0;
    bool ok = true;

    if (f == NULL) {
        say_antic("cannot open %s\n", path);
        return false;
    }
    while ((n = ANTI_FREAD(buffer, 1, sizeof buffer, f)) > 0) {
        if (n > limit - total) {
            say_antic("%s is larger than " LEX_SOURCE_MAX_TEXT "\n",
                      path);
            ok = false;
            break;
        }
        total += n;
        text_append_bytes(out, buffer, n);
    }
    if (ANTI_FERROR(f)) {
        say_antic("cannot read %s\n", path);
        ok = false;
    }
    ANTI_FCLOSE(f);
    return ok;
}

bool driver_read_bytes(const char *path, struct text *out)
{
    return read_file(path, out, false);
}

/* Read a source file. The lexer holds the rules on its bytes, NUL among
   them, and reading stops at the size the lexer refuses. */
static bool read_source(const char *path, struct text *out)
{
    return read_file(path, out, true);
}

/* Read a text that antic keeps as a C string, such as a licence text. A
   NUL byte would end it early, so a file that holds one is refused. */
static bool read_text(const char *path, struct text *out)
{
    if (!driver_read_bytes(path, out)) {
        return false;
    }
    if (memchr(text_cstr(out), '\0', out->length) != NULL) {
        say_antic("%s contains a NUL byte\n", path);
        return false;
    }
    return true;
}

/* Report that path cannot be created. By R149 of "Rules of 2026-10-13"
   the message names the directory of path when that is not there, as for
   -o into a directory that does not exist. */
static void cannot_create(const char *path)
{
    const char *slash = platform_last_separator(path);
    struct text directory = {0};

    if (slash != NULL && slash != path) {
        text_appendf(&directory, "%.*s", (int)(slash - path), path);
    }
    if (directory.length > 0 &&
        !platform_directory_exists(text_cstr(&directory))) {
        say_antic("cannot create %s, the directory %s does not exist\n", path,
                  text_cstr(&directory));
    } else {
        say_antic("cannot create %s\n", path);
    }
    text_free(&directory);
}

bool driver_write_file(const char *path, const struct text *content)
{
    FILE *f = platform_open(path, true);
    bool ok;

    if (f == NULL) {
        cannot_create(path);
        return false;
    }
    ok = ANTI_FWRITE(text_cstr(content), 1, content->length, f) ==
         content->length;
    ok = ANTI_FCLOSE(f) == 0 && ok;
    if (!ok) {
        say_antic("cannot write %s\n", path);
    }
    return ok;
}

/* DESIGN: the two blocks are compiled only by `anti test`. Their functions
   are ordinary module functions by now, each carrying its own block, so
   dropping them here is one pass over the item list. Every pass
   after this one reads a module that never held them. No object, no
   `.antl` and no header can carry one. */
static void drop_test_blocks(struct module *tree, bool keep)
{
    size_t kept = 0;
    size_t i;
    size_t j;

    /* DESIGN: under `--tests` the two blocks stay and their functions are
       public, so the runner module that `anti test` writes calls them by
       name. Nothing else compiles them, so the name reaches no other
       build. */
    if (keep) {
        for (i = 0; i < tree->item_count; i++) {
            struct item *it = tree->items[i];
            if (it->block != BLOCK_NONE) {
                it->pub = true;
                it->vis = VIS_PUB;
            }
        }
        return;
    }
    for (i = 0; i < tree->item_count; i++) {
        if (tree->items[i]->block == BLOCK_NONE) {
            tree->items[kept++] = tree->items[i];
        }
    }
    tree->item_count = kept;
    /* A clause of a dropped block goes with it, so it does not read as
       one that silences nothing. */
    for (i = j = 0; i < tree->clause_count; i++) {
        if (tree->clauses[i].block == BLOCK_NONE) {
            tree->clauses[j++] = tree->clauses[i];
        }
    }
    tree->clause_count = j;
}

/* The module path of the input, from its path under the search roots.
   DESIGN: a library whose module path is reserved for the language's own
   libraries is refused without --anti-internal. A library with a path of
   one segment gets a warning, since such a path is for a program's own
   files. No compilation may define the runtime's module. */
static bool module_name(const struct options *o, struct text *out,
                        struct diagnostics *diags)
{
    char error[200];

    if (!modpath_of_source(o->input, o->roots, o->root_count, out, error,
                               sizeof error)) {
        say_antic("%s\n", error);
        return false;
    }
    if (strcmp(text_cstr(out), RUNTIME_MODULE) == 0) {
        say_antic("%s: the module path `%s` is the runtime's\n",
                  o->input, RUNTIME_MODULE);
        return false;
    }
    if (o->library && !o->front_end && !o->internal &&
        modpath_reserved(text_cstr(out))) {
        say_antic("%s: the module path `%s` is reserved for the "
                  "language's own libraries\n",
                  o->input, text_cstr(out));
        return false;
    }
    /* The warning stands at the top of the file, where a clause of the
       whole file covers it. */
    if (o->library && !o->front_end && diags != NULL &&
        modpath_segments(text_cstr(out)) == 1) {
        diagnostics_warn(diags, NAME_SINGLE_SEGMENT_PATH, 1, 1,
                         "the module path `%s` has one segment, which is "
                         "for a program's own files", text_cstr(out));
    }
    return true;
}

/* Whether the build writes a plugin: a shared library that links no
   runtime and is bound against the host that loads it. */
bool driver_is_plugin(const struct options *o)
{
    return o->lib == LIB_SHARED && o->no_runtime;
}

/* Whether the command ends with a link, rather than a dump, a library
   file or an assembly file. */
static bool links(const struct options *o)
{
    return !o->assembly_only && !o->dump_tokens && !o->dump_ast &&
           !o->dump_types && !o->dump_ir && !o->dump_opt && !o->dump_llvm &&
           !o->library && !o->front_end && o->lib == LIB_NONE;
}

/* DESIGN: lld links for every target from any host, with the sysroot of
   the runtime archive. The platform linker links only for the operating
   system it runs on, where the C library of the target is installed. */
static bool can_link(const struct options *o)
{
    enum target host;

    if (o->linker == LINKER_LLD) {
        return true;
    }
    if (!target_host(&host) ||
        target_info(host)->os != target_info(o->target)->os) {
        say_antic("linking for %s with the platform linker needs "
                  "a host with the same operating system. -S writes "
                  "the assembly without linking.\n",
                  target_name(o->target));
        return false;
    }
    return true;
}

/* DESIGN: Eddie decided on 2026-10-07 that a release build links the
   program and the runtime as bitcode through full LTO, since bitcode
   gives the finer control over what code goes into a program. The mode
   holds where --lto is not given and the build links a program with
   lld. Dev mode writes an object per module, -S, -c and --lib need real
   objects, and the platform linker has no LTO of the pinned release, so
   each of those keeps LTO_NONE. So does a build of --memory-checks,
   whose options hook the AddressSanitizer runtime of macOS does not find
   under LTO, and a Windows program that hosts plugins, which driver_run
   learns after the front end. Each of them refuses --lto full or thin
   when it is given, and --memory-checks refuses them on macOS. A build
   with a profile takes the default as well, see profile_usable. */
static enum lto lto_mode(const struct options *o)
{
    if (o->lto_given || o->dev || o->front_end || o->assembly_only ||
        o->library || o->lib != LIB_NONE || o->linker != LINKER_LLD ||
        o->memory_checks) {
        return o->lto;
    }
    return LTO_FULL;
}

/* DESIGN: --lto links a program in release mode. Dev mode writes an
   object per module, and the other refusals write no program or link
   with a linker of the host. A runtime archive without the runtime as
   bitcode of the mode is refused before anything compiles, and where
   the mode is the default the message names --lto none. */
static bool lto_usable(const struct options *o)
{
    struct text library = {0};
    bool ok = true;

    if (o->dev || o->assembly_only || o->library || o->lib != LIB_NONE ||
        o->linker != LINKER_LLD) {
        say_antic("--lto links a program in release mode, without --dev, "
                  "-S, -c, --lib or --linker platform\n");
        return false;
    }
    /* DESIGN: the AddressSanitizer runtime of macOS is a dylib that
       takes its options from the hook of the program, which overrides a
       weak definition of the dylib. ld64.lld marks no definition that
       its LTO wrote as such an override, so the dylib kept its own
       options and a report ended the program with SIGABRT. */
    if (o->memory_checks && target_info(o->target)->os == OS_MACOS) {
        say_antic("--lto links no macOS program of --memory-checks\n");
        return false;
    }
    if (o->runtime == NULL) {
        return true;
    }
    link_runtime_bitcode(&library, o->runtime, o->target, o->cpu, o->lto);
    if (!driver_file_exists(text_cstr(&library)) && o->lto_given) {
        say_antic("--lto %s needs %s, the runtime as bitcode\n",
                  link_lto_name(o->lto), text_cstr(&library));
        ok = false;
    } else if (!driver_file_exists(text_cstr(&library))) {
        say_antic("a release build links the runtime as "
                  "bitcode, %s, which is missing. --lto none links "
                  "the runtime as objects\n",
                  text_cstr(&library));
        ok = false;
    }
    text_free(&library);
    return ok;
}

/* The first bytes of an indexed profile, which llvm-profdata merge
   writes, and of a raw profile of 64 bits, which a run writes. Both are
   little-endian on every target of Anti. */
static const unsigned char indexed_profile[] = {0xff, 'l', 'p', 'r',
                                                'o',  'f', 'i', 0x81};
static const unsigned char raw_profile[] = {0x81, 'r', 'f', 'o',
                                            'r',  'p', 'l', 0xff};

/* Whether the file at path is an indexed profile. A message names the
   problem otherwise, and for a raw profile the command that merges it. */
static bool indexed_profile_file(const char *path)
{
    unsigned char head[sizeof indexed_profile] = {0};
    FILE *f = platform_open(path, false);
    size_t got;

    if (f == NULL) {
        say_antic("cannot read %s\n", path);
        return false;
    }
    got = ANTI_FREAD(head, 1, sizeof head, f);
    ANTI_FCLOSE(f);
    if (got == sizeof head &&
        memcmp(head, indexed_profile, sizeof head) == 0) {
        return true;
    }
    if (got == sizeof head && memcmp(head, raw_profile, sizeof head) == 0) {
        say_antic("%s is a raw profile. llvm-profdata merge "
                  "-o <file> %s writes the profile that "
                  "--profile-use reads\n",
                  path, path);
        return false;
    }
    say_antic("%s is no profile of llvm-profdata merge\n", path);
    return false;
}

/* DESIGN: --profile-generate and --profile-use act on the run of opt in
   release mode, choice D7 of docs/work-order-llvm-optimization.md, and
   neither is a default, condition C3. Dev mode runs no opt. Eddie decided
   on 2026-10-08 that a build with a profile links through the LTO of a
   release build as well, full by default, so a program gets both the
   profile and the smaller link. The profile acts on the pipeline opt runs
   before the link, under --lto full and thin as without them, and --lto
   none keeps the link of objects. The instrumented program links the
   profile runtime, so --profile-generate builds a program that lld
   links. A profile is the indexed file of llvm-profdata merge, since opt
   reads no raw profile. */
static bool profile_usable(const struct options *o)
{
    const char *option =
        o->profile_generate ? "--profile-generate" : "--profile-use";

    if (o->dev) {
        say_antic("%s builds in release mode, without --dev\n",
                  option);
        return false;
    }
    if (o->profile_generate && o->profile_use != NULL) {
        say_antic("--profile-generate and --profile-use exclude each "
                  "other\n");
        return false;
    }
    if (o->profile_generate &&
        (o->assembly_only || o->library || o->lib != LIB_NONE ||
         o->linker != LINKER_LLD)) {
        say_antic("--profile-generate links a program, without -S, -c, "
                  "--lib or --linker platform\n");
        return false;
    }
    return o->profile_use == NULL || indexed_profile_file(o->profile_use);
}

bool driver_file_exists(const char *path)
{
    FILE *f = platform_open(path, false);

    if (f != NULL) {
        ANTI_FCLOSE(f);
    }
    return f != NULL;
}

/* Print the diagnostics of the compilation and count them for the caller
   that asked, which is `anti check`. */
static void report_diagnostics(const struct options *o,
                               const struct diagnostics *diags)
{
    size_t i;

    diagnostics_print(o->input, diags);
    if (o->counts == NULL) {
        return;
    }
    for (i = 0; i < diags->count; i++) {
        if (diags->items[i].promoted) {
            o->counts->warnings++;
        } else if (!diags->items[i].warning) {
            o->counts->errors++;
        } else if (diags->items[i].doc) {
            o->counts->doc_warnings++;
        } else {
            o->counts->warnings++;
        }
    }
}

/* Whether the module exports an item, or a function of a class body. */
static bool exports_something(const struct module *tree)
{
    size_t i;
    size_t j;

    for (i = 0; i < tree->item_count; i++) {
        const struct item *it = tree->items[i];
        if (it->exported) {
            return true;
        }
        for (j = 0; j < it->member_count; j++) {
            if (it->members[j]->exported) {
                return true;
            }
        }
    }
    return false;
}

/* Whether the build writes a program in release mode. */
static bool release_build(const struct options *o)
{
    return !o->dev && !o->library && !o->front_end;
}

/* Whether the checker sees the whole program, which it needs to find an
   abstract class that no class fills. */
static bool whole_program_check(const struct options *o)
{
    return release_build(o) && o->lib == LIB_NONE;
}

/* The checks that ran in this compilation. A check that belongs to
   another build leaves its `allow` alone, since that build decides
   whether the clause silences something. */
static warnings_ran checks_ran(const struct options *o)
{
    warnings_ran ran = WARNINGS_ALL_RAN;

    if (!o->doc_warnings) {
        ran &= ~(WARNINGS_BIT(NAME_DOC_MARKUP) |
                 WARNINGS_BIT(NAME_DOC_UNRESOLVED) |
                 WARNINGS_BIT(NAME_DOC_NOTE_ONLY) |
                 WARNINGS_BIT(NAME_DOC_DROPPED));
    }
    if (!o->doc_warnings || !o->warn_undocumented) {
        ran &= ~WARNINGS_BIT(NAME_UNDOCUMENTED);
    }
    if (!whole_program_check(o)) {
        ran &= ~WARNINGS_BIT(NAME_UNFILLED_ABSTRACT);
    }
    if (!o->library || o->front_end) {
        ran &= ~WARNINGS_BIT(NAME_SINGLE_SEGMENT_PATH);
    }
    if (o->lib != LIB_STATIC) {
        ran &= ~WARNINGS_BIT(NAME_NOTHING_EXPORTED);
    }
    return ran;
}

/* Apply the clauses of the module, turn the warnings into errors where
   the build accepts none, and report. complete says the checker ran to
   its end, so a clause that silenced nothing is known. Returns false
   when an error stands. */
static bool settle_diagnostics(const struct options *o,
                               const struct module *tree,
                               struct diagnostics *diags, bool complete)
{
    size_t i;

    warnings_apply(tree, diags, checks_ran(o), complete);
    if (o->warnings_as_errors || release_build(o)) {
        warnings_promote(diags);
    }
    report_diagnostics(o, diags);
    for (i = 0; i < diags->count; i++) {
        if (!diags->items[i].warning) {
            return false;
        }
    }
    return true;
}

/* Print one token per line: its position, its group and its source text.
   A doc comment spans lines, so it prints its first line. */
static void dump_tokens(const char *source, const struct token_list *tokens)
{
    size_t i;

    for (i = 0; i + 1 < tokens->count; i++) {
        const struct token *t = &tokens->items[i];
        const char *end = memchr(source + t->offset, '\n', t->length);
        size_t length = end != NULL ? (size_t)(end - (source + t->offset))
                                    : t->length;
        char position[32];

        text_format_name(position, sizeof position, "%d:%d", t->line,
                         t->column);
        printf("%-5s %-8s %.*s\n", position, lexer_token_category(t->kind),
               (int)length, source + t->offset);
    }
}

/* The options of lowering that the command line sets. */
static unsigned lower_options(const struct options *o)
{
    unsigned flags = (o->no_reflect ? LOWER_NO_REFLECT : 0u) |
                     (o->dev ? LOWER_DEV : 0u) |
                     (o->no_hooks ? LOWER_NO_HOOKS : 0u) |
                     (o->trace_writes ? LOWER_TRACE_WRITES : 0u);

    /* `trace` follows the mode, as `assert` does, and `--trace` and
       `--no-trace` decide instead of it. */
    if (o->trace == TRACE_ON || (o->trace == TRACE_MODE && o->dev)) {
        flags |= LOWER_TRACE;
    }
    return flags;
}

/* The path a failed assertion or a failed dev-mode check names: the
   input under the first search root that holds it, in memory of arena. */
static const char *recorded_file(const struct options *o,
                                 struct arena *arena)
{
    struct text path = {0};
    char *kept;

    modpath_file_of_source(o->input, o->roots, o->root_count, &path);
    kept = arena_alloc(arena, path.length + 1);
    memcpy(kept, text_cstr(&path), path.length + 1);
    text_free(&path);
    return kept;
}

/* Lower the module into ir and verify the result. file is the path that
   an assertion and a check record, which is not the path a message
   names. */
static bool lower_checked(const char *input, const char *file,
                          struct module *tree, const char *module,
                          struct ir_module *ir, unsigned options,
                          const char *const *patterns, size_t pattern_count,
                          const char *version)
{
    struct text errors = {0};
    bool ok;

    tree->file = file;
    lower_module(tree, module, ir, options, patterns, pattern_count, version);
    if (ir->extern_clash != NULL) {
        say_antic("%s declares `extern fn %s` with another "
                  "signature than a library of the program\n",
                  input, ir->extern_clash);
        return false;
    }
    ok = ir_verify(ir, &errors);
    if (!ok) {
        say_antic("internal error, the IR of %s fails "
                  "verification\n%s", input, text_cstr(&errors));
    }
    text_free(&errors);
    return ok;
}

/* Run the passes over the whole program and print what they report,
   one line each. Returns false after an error. */
static bool whole_checked(const char *input, struct ir_module *program,
                          const char *module, bool release, bool reflect,
                          bool bundled, bool library, bool dev,
                          const struct options *o)
{
    struct whole_options options;
    struct text errors = {0};
    const char *line;
    bool ok;

    memset(&options, 0, sizeof options);
    options.entry = module;
    options.release = release;
    options.reflect = reflect;
    options.bundled = bundled;
    options.library = library;
    options.dev = dev;
    options.plugin = driver_is_plugin(o);
    options.closed = o->closed;
    options.inject = o->inject;
    options.inject_count = o->inject_count;
    ok = whole_program(program, &options, &errors);
    for (line = text_cstr(&errors); *line != '\0';) {
        const char *end = strchr(line, '\n');
        size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
        fprintf(stderr, "%s: error: %.*s\n", input, (int)length, line);
        line += length + (end != NULL ? 1 : 0);
    }
    text_free(&errors);
    return ok;
}

static bool has_main(const struct ir_module *program, const char *module);

/* Lower the module after the loaded libraries and print the whole
   program, after the optimizer passes when the flag asks for them. That
   program has been through the passes over the whole program where the
   build runs them. Returns 2, the status of a finished dump, on success. */
static int dump_ir(const char *input, const char *file, struct module *tree,
                   const char *module, struct ir_module *program,
                   bool optimize, bool release, unsigned options,
                   const char *const *patterns, size_t pattern_count,
                   const struct options *o)
{
    bool no_reflect = (options & LOWER_NO_REFLECT) != 0;
    struct text out = {0};
    struct text errors = {0};

    if (!lower_checked(input, file, tree, module, program, options, patterns,
                       pattern_count, o->package_version)) {
        return 1;
    }
    if (optimize) {
        if ((release || has_main(program, module)) &&
            !whole_checked(input, program, module, release, !no_reflect,
                           false, false, !release, o)) {
            return 1;
        }
        optimize_program(program, module);
        if (!ir_verify(program, &errors)) {
            say_antic("internal error, the optimized IR of %s "
                      "fails verification\n%s", input,
                      text_cstr(&errors));
            text_free(&errors);
            return 1;
        }
    }
    ir_print(&out, program);
    fputs(text_cstr(&out), stdout);
    text_free(&out);
    return 2;
}

/* Whether a function of module name stands in the program. */
static bool holds_module(const struct ir_module *program, const char *name)
{
    size_t i;

    for (i = 0; i < program->function_count; i++) {
        const char *module = program->functions[i]->module;
        if (module != NULL && strcmp(module, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Whether the program has a function main in the main module. */
static bool has_main(const struct ir_module *program, const char *module)
{
    size_t i;

    for (i = 0; i < program->function_count; i++) {
        if (ir_is_main_of(program->functions[i], module)) {
            return true;
        }
    }
    return false;
}

/* Feed the bytes of the file at path to the digest. A file that cannot
   be opened adds nothing, and the link that needs it reports it. A file
   that opens and then fails to read returns false, since a digest of part
   of it would name other code. */
static bool digest_file(struct anti_sha256 *s, const char *path)
{
    FILE *f = platform_open(path, false);
    unsigned char bytes[4096];
    size_t n;
    bool ok;

    if (f == NULL) {
        return true;
    }
    while ((n = ANTI_FREAD(bytes, 1, sizeof bytes, f)) > 0) {
        anti_rt_sha256_update(s, bytes, n);
    }
    ok = !ANTI_FERROR(f);
    ANTI_FCLOSE(f);
    return ok;
}

/* DESIGN: the build id is the SHA-256 of the code that a link takes from
   antic. That is the LLVM text of the module that links, every object the
   command line adds and the runtime library. The notice is left out,
   because it holds the id. The paths are left out as well, so two links
   of one program in two places carry one id. What `-g` added is left out
   too, so a `-g` link and a plain link of one program carry one id.
   `anti symbols` then matches a trace of the one to the archive of the
   other. */
static bool build_id(const struct options *o, const struct text *llvm_text,
                     const struct debug_spans *spans, char hex[65],
                     char *error, size_t size)
{
    struct text library = {0};
    struct anti_sha256 s;
    size_t at = 0;
    size_t i;
    bool ok = true;

    anti_rt_sha256_init(&s);
    for (i = 0; spans != NULL && i < spans->count; i++) {
        if (spans->items[i].start > at) {
            anti_rt_sha256_update(&s, llvm_text->data + at,
                                  spans->items[i].start - at);
        }
        at = spans->items[i].end > at ? spans->items[i].end : at;
    }
    if (at < llvm_text->length) {
        anti_rt_sha256_update(&s, llvm_text->data + at,
                              llvm_text->length - at);
    }
    for (i = 0; ok && i < o->object_count; i++) {
        if (!digest_file(&s, o->objects[i])) {
            text_format(error, size, "cannot read %s", o->objects[i]);
            ok = false;
        }
    }
    if (ok && o->runtime != NULL) {
        link_runtime_library(&library, o->runtime, o->target, o->cpu);
        if (!digest_file(&s, text_cstr(&library))) {
            text_format(error, size, "cannot read %s",
                        text_cstr(&library));
            ok = false;
        }
    }
    anti_rt_sha256_hex(&s, hex);
    text_free(&library);
    return ok;
}

/* The notice with the line `build <id>` after its begin marker, where a
   tool that reads the marker finds it. */
static void identified_notice(struct text *out, const struct text *notice,
                              const char *id)
{
    size_t begin = sizeof ANTI_NOTICE_BEGIN - 1;

    if (notice->length < begin ||
        memcmp(notice->data, ANTI_NOTICE_BEGIN, begin) != 0) {
        text_append_bytes(out, notice->data, notice->length);
        return;
    }
    text_append(out, ANTI_NOTICE_BEGIN);
    text_appendf(out, "%s%s\n", ANTI_NOTICE_BUILD, id);
    text_append_bytes(out, notice->data + begin, notice->length - begin);
}

/* The back end of docs/work-order-llvm-back-end.md. It folds the symbolic
   values of the target and puts in the checks of --memory-checks, then
   translates the module into LLVM IR text. --dump-llvm prints the text.
   Otherwise the text goes to llvm_text with the notice after the build
   id, and driver_run hands it to opt and llc. Returns 0 with the text, 2
   after the dump, DRIVER_OBJECT_ONLY for a dev object of a module without
   main and 1 after an error. */
static int llvm_back_end(const struct options *o, struct ir_module *program,
                         const char *module, struct text *llvm_text,
                         struct extras *extras)
{
    struct layouts layouts;
    struct llvm_emit_options emit;
    struct text out = {0};
    struct debug_spans spans = {0};
    char error[512];
    bool ok;
    int status = 1;
    bool with_notice;
    size_t i;

    /* DESIGN: the optimizer runs again on each function that had a
       symbolic value, so a size folded here reaches the simplifications a
       number would have reached before. */
    ok = layout_init(&layouts, o->target, program, error, sizeof error) &&
         layout_data(&layouts, program);
    for (i = 0; ok && i < program->function_count; i++) {
        struct ir_function *f = program->functions[i];
        bool resolved = false;
        ok = layout_resolve(&layouts, f, &resolved);
        if (ok && resolved && !f->is_extern) {
            optimize_function(f);
        }
        if (ok && program->memory_checks && !f->is_extern) {
            memcheck_function(program, f, &layouts);
        }
    }
    memset(&emit, 0, sizeof emit);
    emit.target = o->target;
    emit.cpu = o->cpu;
    emit.module = module;
    emit.one_module = o->dev || driver_is_plugin(o);
    emit.exports = extras->hosts_plugins;
    emit.local_exports = driver_links_musl(o, extras) &&
                         has_main(program, module) && o->object_count == 0;
    emit.debug = o->debug;
    emit.optimized = !o->dev;
    emit.spans = &spans;
    /* A library for C starts the runtime from a constructor that runs at
       load, so no function it exports checks per call, by decision 5 of
       "Decisions of 2026-10-11". The host has run the runtime's start
       already, so a plugin brings no constructor of its own. */
    if (o->lib != LIB_NONE && !driver_is_plugin(o)) {
        emit.constructor = rt_name(RT_FN_INIT);
    }
    /* The notice stays out of --dump-llvm, so its goldens do not follow
       the runtime library, and out of every object that does not link. */
    with_notice = extras->notice.length > 0 && !o->dump_llvm &&
             !(o->dev && !has_main(program, module)) && !o->assembly_only &&
             o->lib != LIB_STATIC;
    emit.notice = with_notice;
    emit.bundle = extras->marker.length > 0 ? text_cstr(&extras->marker)
                                            : NULL;
    ok = ok && llvm_emit_module(&out, &emit, program, &layouts, error,
                                sizeof error);
    /* A COFF program that hosts plugins exports every name it defines
       through its .def file. Its functions are not internal, so opt keeps
       each of them. */
    if (ok && !o->dump_llvm && extras->hosts_plugins &&
        target_info(o->target)->format == FORMAT_COFF) {
        llvm_emit_names(&extras->host_names, o->target, program);
    }
    if (ok && o->dump_llvm) {
        fputs(text_cstr(&out), stdout);
        status = 2;
    } else if (ok) {
        status = o->dev && !has_main(program, module) ? DRIVER_OBJECT_ONLY : 0;
        /* The build id digests the text without the notice, which holds
           it. */
        if (with_notice) {
            struct text notice = {0};
            char id[65];
            ok = build_id(o, &out, &spans, id, error, sizeof error);
            if (ok) {
                identified_notice(&notice, &extras->notice, id);
                llvm_emit_licenses(&out, o->target, notice.data,
                                   notice.length);
                text_free(&notice);
            }
        }
        for (i = 0; ok && i < program->function_count; i++) {
            const struct ir_function *f = program->functions[i];
            if (f->exported && !f->is_extern) {
                text_appendf(&extras->exports, "%s\n", f->name);
            }
        }
        if (ok) {
            text_append_bytes(llvm_text, out.data, out.length);
        } else {
            status = 1;
        }
    }
    if (!ok) {
        say_antic("%s\n", error);
    }
    layout_free(&layouts);
    text_free(&out);
    llvm_debug_spans_free(&spans);
    return status;
}

/* The package of a component of the runtime archive under the name it
   has in a notice, with the identifier of notice_component_license and
   the text of <runtime>/RUNTIME_LICENSES_DIR/<component>.txt. The text
   stays empty when runtime is NULL. */
static bool archive_package(const char *runtime, const char *name,
                            const char *component, const char *version,
                            struct arena *arena, struct package *out)
{
    struct text path = {0};
    struct text text = {0};
    struct text license = {0};
    char *copy;
    bool ok = true;

    memset(out, 0, sizeof *out);
    out->name = name;
    out->version = version;
    notice_component_license(component, &license);
    copy = arena_alloc(arena, license.length + 1);
    memcpy(copy, text_cstr(&license), license.length + 1);
    out->license = copy;
    out->license_text = "";
    text_appendf(&path, "%s/%s/%s%s", runtime != NULL ? runtime : ".",
                 RUNTIME_LICENSES_DIR, component, RUNTIME_LICENSE_SUFFIX);
    /* DESIGN: a text that cannot be read ends the run, as every other
       file the driver names and cannot open does, so no binary ships a
       notice without the text of a component it links. T151 of
       docs/audit/tests-findings.md. */
    if (runtime != NULL) {
        ok = read_text(text_cstr(&path), &text);
    }
    if (ok && runtime != NULL) {
        copy = arena_alloc(arena, text.length + 1);
        memcpy(copy, text_cstr(&text), text.length);
        out->license_text = copy;
    }
    text_free(&path);
    text_free(&text);
    text_free(&license);
    return ok;
}

bool driver_runtime_packages(const char *runtime, bool musl,
                             struct arena *arena, struct package *out,
                             size_t *count)
{
    size_t n = 0;
    bool ok = archive_package(runtime, RUNTIME_MODULE, RUNTIME_LICENSE_NAME,
                              ANTIC_VERSION, arena, &out[n++]);

    if (ok && musl) {
        ok = archive_package(runtime, MUSL_PACKAGE, MUSL_PACKAGE,
                             ANTIC_MUSL_VERSION, arena, &out[n++]);
    }
    if (ok && musl) {
        ok = archive_package(runtime, MIMALLOC_PACKAGE, MIMALLOC_PACKAGE,
                             ANTIC_MIMALLOC_VERSION, arena, &out[n++]);
    }
    *count = n;
    return ok;
}

/* The licence notice of a linked binary: the runtime, musl and mimalloc
   in a program of musl, every package of the loaded libraries once, and
   the package of the compiled module.

   DESIGN: musl and mimalloc are MIT, whose notice travels with the
   binary, and every program of musl links both. Eddie decided on
   2026-10-08 that its notice names them, each with the version of its
   pin and the text of the runtime archive. A program of the glibc mode
   and of any other target links neither and names neither. */
static bool build_notice(const struct options *o,
                         const struct extras *extras, struct arena *arena,
                         struct text *out)
{
    const struct interface *own = extras->own;
    size_t count = extras->library_count;
    const struct package **list;
    struct package archive[DRIVER_RUNTIME_PACKAGES];
    bool musl = driver_links_musl(o, extras);
    size_t archived = 0;
    size_t n = 0;
    size_t i;
    size_t j;

    if (o->links_musl != NULL) {
        *o->links_musl = musl;
    }
    if (!driver_runtime_packages(o->runtime, musl, arena, archive,
                                 &archived)) {
        return false;
    }
    list = alloc_zeroed(count + DRIVER_RUNTIME_PACKAGES + 1, sizeof *list);
    for (i = 0; i < archived; i++) {
        list[n++] = &archive[i];
    }
    /* DESIGN: the package of the compiled module is always the last
       `package` line, even where a library of the same package came
       first. `anti symbols` reads the version of a binary there. */
    for (i = 0; i < count; i++) {
        const struct interface *library = extras->libraries[i];
        if (own->package.name != NULL &&
            strcmp(own->package.name, library->package.name) == 0) {
            continue;
        }
        for (j = 0; j < n; j++) {
            if (strcmp(list[j]->name, library->package.name) == 0) {
                break;
            }
        }
        if (j == n) {
            list[n++] = &library->package;
        }
    }
    list[n++] = &own->package;
    notice_text(out, list, n);
    free((void *)list);
    return true;
}

/* Lower the program, run the optimizer passes and run llvm_back_end for
   the target. Returns 0 with the text, 2 after a dump, DRIVER_OBJECT_ONLY
   for a dev object of a module without main and 1 after an error. */
static int back_end(const struct options *o, struct module *tree,
                    const char *module, struct ir_module *program,
                    struct text *llvm_text, struct extras *extras)
{
    size_t i;

    if (tree != NULL &&
        !lower_checked(o->input, recorded_file(o, program->arena), tree,
                       module, program,
                       lower_options(o), o->trace_patterns,
                       o->trace_pattern_count, o->package_version)) {
        return 1;
    }
    /* DESIGN: the passes over the whole program run where the program is
       whole. That is every build in release mode. In dev mode it is the
       module that links, which has main, and a library for C. A dev
       object of any other module never links. */
    /* A program that injects an interface or loads a library exports
       its names, and keeps every one of them. This is read before the
       passes over the whole program, which reach what it keeps, and
       before the optimizer, which drops the class records it reads. */
    program->hosts = !o->closed && o->lib == LIB_NONE && !o->library &&
                     whole_hosts_plugins(program);
    if ((!o->dev || o->lib != LIB_NONE || has_main(program, module)) &&
        !whole_checked(o->input, program, module, !o->dev,
                       !o->no_reflect, o->bundle_runtime,
                       o->lib != LIB_NONE, o->dev, o)) {
        return 1;
    }
    /* The `provides` lines of a plugin are read here, because the
       optimizer drops the class records that carry them. */
    extras->hosts_plugins = program->hosts;
    /* DESIGN: a program that loads libraries keeps the hooks as well,
       which Eddie decided for audit finding S29. A closed one loads
       none. */
    if (o->no_hooks && extras->hosts_plugins) {
        fprintf(stderr,
                "%s: error: the program loads libraries, and `--no-hooks` "
                "drops the hooks that count their objects for `unload`; "
                "build it without `--no-hooks`, or with `--closed`\n",
                o->input);
        return 1;
    }
    if (extras->own != NULL &&
        !build_notice(o, extras, program->arena, &extras->notice)) {
        return 1;
    }
    extras->regex = holds_module(program, REGEX_MODULE);
    for (i = 0; driver_is_plugin(o) && i < program->class_count; i++) {
        const struct ir_class *c = program->classes[i];
        size_t j;
        for (j = 0; j < c->provides_count; j++) {
            text_appendf(&extras->provides, "%s\t%s.%s\n",
                         c->provides[j].interface, c->module, c->name);
        }
    }
    /* The build that compiles the program decides, so an assertion of a
       library file follows this build and not the one that wrote it. */
    if (o->asserts == ASSERTS_OFF ||
        (o->asserts == ASSERTS_MODE && !o->dev)) {
        optimize_drop_failures(program, IR_FAIL_ASSERT);
    }
    if (o->checks == CHECKS_OFF || (o->checks == CHECKS_MODE && !o->dev)) {
        optimize_drop_failures(program, IR_FAIL_CHECK);
    }
    /* A plugin holds the code of its own module alone. Every other
       module of the program it was checked against belongs to the host,
       which defines it. */
    if (o->dev || driver_is_plugin(o)) {
        optimize_module(program, module);
    } else {
        optimize_program(program, module);
    }
    if (!o->dump_llvm && !o->assembly_only && !o->dev && o->lib == LIB_NONE &&
        !has_main(program, module)) {
        say_antic("%s: the program has no function `main`\n",
                  o->input);
        return 1;
    }
    program->plugin = driver_is_plugin(o);
    if (o->memory_checks) {
        memcheck_declare(program, module,
                         o->lib == LIB_NONE && has_main(program, module),
                         o->target);
    }
    return llvm_back_end(o, program, module, llvm_text, extras);
}

/* The package header from the options. A field without an option stays
   NULL, and sema_interface fills it in. --license-text names a file whose
   content goes into the header. */
static bool package_header(const struct options *o, struct arena *arena,
                           struct package *out)
{
    struct package_dependency *deps =
        arena_alloc(arena, (o->dependency_count + 1) * sizeof *deps);
    size_t i;

    memset(out, 0, sizeof *out);
    out->name = o->package_name;
    out->version = o->package_version;
    out->license = o->license;
    for (i = 0; i < o->dependency_count; i++) {
        const char *spec = o->dependencies[i];
        const char *first = strchr(spec, ',');
        const char *second = first != NULL ? strchr(first + 1, ',') : NULL;
        char *copy;
        if (second == NULL) {
            say_antic("--dependency %s is not "
                      "<name>,<constraint>,<url>\n", spec);
            return false;
        }
        copy = arena_alloc(arena, strlen(spec) + 1);
        memcpy(copy, spec, strlen(spec));
        copy[first - spec] = '\0';
        copy[second - spec] = '\0';
        deps[i].name = copy;
        deps[i].constraint = copy + (first - spec) + 1;
        deps[i].url = copy + (second - spec) + 1;
    }
    out->dependencies = deps;
    out->dependency_count = o->dependency_count;
    out->attribution = o->attribution;
    out->attribution_count = o->attribution_count;
    if (o->license_text != NULL) {
        struct text text = {0};
        char *copy;
        if (!read_text(o->license_text, &text)) {
            return false;
        }
        copy = arena_alloc(arena, text.length + 1);
        memcpy(copy, text_cstr(&text), text.length);
        out->license_text = copy;
        text_free(&text);
    }
    return true;
}

static bool own_interface(const struct options *o, struct module *tree,
                          const char *module, struct arena *arena,
                          struct interface *out);

/* Write the library file of the module, by default beside the source.
   Returns 2, the status of a finished command without linking. */
static int write_library(const struct options *o, struct module *tree,
                         const char *module, struct arena *arena)
{
    struct ir_module ir;
    struct interface iface;
    struct text bytes = {0};
    struct text path = {0};
    int status = 1;

    ir_module_init(&ir, arena, module);
    if (lower_checked(o->input, recorded_file(o, arena), tree, module, &ir,
                      lower_options(o), o->trace_patterns,
                      o->trace_pattern_count, o->package_version) &&
        own_interface(o, tree, module, arena, &iface)) {
        if (!antl_write(&bytes, &iface, &ir, o->strip_docs)) {
            say_antic("%s is too large for a library file\n",
                      o->input);
            goto done;
        }
        if (o->output != NULL) {
            text_append(&path, o->output);
        } else {
            text_appendf(&path, "%.*s%s",
                         (int)(strlen(o->input) - strlen(SOURCE_SUFFIX)),
                         o->input, ANTL_SUFFIX);
        }
        status = driver_write_file(text_cstr(&path), &bytes) ? 2 : 1;
    }
done:
    text_free(&bytes);
    text_free(&path);
    ir_module_free(&ir);
    return status;
}

static bool defines_main(const struct module *tree)
{
    size_t i;

    for (i = 0; i < tree->item_count; i++) {
        const struct item *it = tree->items[i];
        if (it->kind == ITEM_FN && it->name.length == 4 &&
            memcmp(it->name.text, "main", 4) == 0) {
            return true;
        }
    }
    return false;
}

/* The name of a library for C: the file name of -o without lib and its
   suffix, or the last segment of the module path. */
static void library_name(const struct options *o, const char *module,
                         struct text *out)
{
    const char *base;
    const char *dot;
    size_t n;

    if (o->output == NULL) {
        text_append(out, modpath_last(module));
        return;
    }
    base = platform_file_name(o->output);
    if (target_info(o->target)->format != FORMAT_COFF &&
        strncmp(base, "lib", 3) == 0) {
        base += 3;
    }
    dot = strchr(base, '.');
    n = dot != NULL ? (size_t)(dot - base) : strlen(base);
    text_appendf(out, "%.*s", (int)n, base);
}

/* The interface of the compiled module with the package header of the
   options, as a library file of the module holds it. */
static bool own_interface(const struct options *o, struct module *tree,
                          const char *module, struct arena *arena,
                          struct interface *out)
{
    struct package package;

    if (!package_header(o, arena, &package)) {
        return false;
    }
    sema_interface(tree, module, arena, out);
    if (package.name != NULL) {
        out->package.name = package.name;
    }
    if (package.version != NULL) {
        out->package.version = package.version;
    }
    out->package.dependencies = package.dependencies;
    out->package.dependency_count = package.dependency_count;
    if (package.license != NULL) {
        out->package.license = package.license;
    }
    if (package.license_text != NULL) {
        out->package.license_text = package.license_text;
    }
    out->package.attribution = package.attribution;
    out->package.attribution_count = package.attribution_count;
    return true;
}

static int compile(const struct options *o, struct text *source,
                   struct text *module, struct text *llvm_text,
                   struct text *base, struct extras *extras)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *tree = NULL;
    struct types types;
    struct ir_module program;
    const struct interface **libraries = NULL;
    struct interface own;
    struct paths paths = {0};
    struct sema_inputs in;
    int status = 1;

    ir_module_init(&program, &arena, "");

    if (!read_source(o->input, source)) {
        goto done;
    }
    if (!lexer_lex(text_cstr(source), source->length, &arena, &diags,
                   &tokens)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    if (o->dump_tokens) {
        dump_tokens(text_cstr(source), &tokens);
        status = 2;
        goto done;
    }
    if (!parser_parse(text_cstr(source), &tokens, &arena, &diags, &tree)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    drop_test_blocks(tree, o->tests);
    if (o->dump_ast) {
        struct text dump = {0};
        ast_dump(&dump, tree);
        fputs(text_cstr(&dump), stdout);
        text_free(&dump);
        status = 2;
        goto done;
    }
    if (!module_name(o, module, &diags)) {
        goto done;
    }
    types_init(&types, &arena);
    if (!driver_find_libraries(o, tree, &arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, text_cstr(module), tree, o->input,
                               &arena, &types, &program, libraries)) {
        goto done;
    }
    /* DESIGN: the front end compiles the copies of the generics as a build
       does, so that `--front-end` refuses what `-S` refuses in a copy, by
       R068 of "Rules of 2026-10-13": a generic that names ever deeper
       copies of itself has its message there alone. `--dump-types` prints
       the tree the program wrote and keeps the copies out. */
    tree->compile_copies = o->library || !o->dump_types;
    tree->no_reflect = o->no_reflect;
    memset(&in, 0, sizeof in);
    in.module_name = text_cstr(module);
    in.package = o->package_name;
    in.libraries = libraries;
    in.library_count = paths.count;
    in.types = &types;
    in.arena = &arena;
    in.diags = &diags;
    if (!sema_check(tree, &in, whole_program_check(o))) {
        settle_diagnostics(o, tree, &diags, false);
        goto done;
    }
    /* DESIGN: a static library for C of a module that exports nothing
       offers C no item, which the warning `nothing-exported` says, by R131
       of "Rules of 2026-10-13". It stands at the top of the file, where a
       clause of the whole file covers it, and the release build of the
       library refuses it as every warning. */
    if (o->lib == LIB_STATIC && !exports_something(tree)) {
        diagnostics_warn(&diags, NAME_NOTHING_EXPORTED, 1, 1,
                         "the module exports nothing, and `--lib static` "
                         "writes a library for C");
    }
    if (o->doc_warnings) {
        sema_doc_warnings(tree, &in, o->warn_undocumented);
    }
    if (!settle_diagnostics(o, tree, &diags, true)) {
        goto done;
    }
    diags.count = 0;
    if (o->dump_types) {
        struct text dump = {0};
        ast_dump_typed(&dump, tree);
        fputs(text_cstr(&dump), stdout);
        text_free(&dump);
        status = 2;
        goto done;
    }
    /* A library file holds the IR of the module, so it is written after
       the checker as a program is. `anti check` has one written for each
       module another imports. */
    /* DESIGN: the checker made a compiled copy of every generic the module
       uses with concrete arguments, each an ordinary item. The generics
       then leave the module, with its `type` names and its constraints, so
       the passes after the checker see no type parameter. */
    if (o->library || !o->front_end) {
        sema_strip_generics(tree, &arena);
    }
    if (o->library) {
        status = write_library(o, tree, text_cstr(module), &arena);
        goto done;
    }
    /* The front end ends here, before the first pass that writes a file of
       the program. Status 2 is the status of a command a dump finished. */
    if (o->front_end) {
        status = 2;
        goto done;
    }
    if (o->dump_ir || o->dump_opt) {
        status = dump_ir(o->input, recorded_file(o, program.arena), tree,
                         text_cstr(module),
                         &program, o->dump_opt, !o->dev, lower_options(o),
                         o->trace_patterns, o->trace_pattern_count, o);
        goto done;
    }
    if (o->lib != LIB_NONE && defines_main(tree)) {
        say_antic("%s: a library for C has no function `main`\n",
                  o->input);
        goto done;
    }
    if (o->lib != LIB_NONE || links(o)) {
        const struct interface **all =
            alloc_zeroed(paths.count + 2, sizeof *all);
        if (!own_interface(o, tree, text_cstr(module), &arena, &own)) {
            free((void *)all);
            goto done;
        }
        extras->own = &own;
        extras->libraries = libraries;
        extras->library_count = paths.count;
        if (o->lib != LIB_NONE) {
            memcpy((void *)all, (void *)libraries,
                   paths.count * sizeof *all);
            all[paths.count] = &own;
            library_name(o, text_cstr(module), &extras->name);
            if (o->bundle_runtime && o->lib == LIB_STATIC &&
                target_info(o->target)->format == FORMAT_MACHO) {
                driver_bundle_marker(&extras->marker, own.package.name);
            }
            header_write(&extras->header, text_cstr(&extras->name), all,
                         paths.count + 1, o->bundle_runtime);
            if (!antl_write_header(&extras->package, &own)) {
                say_antic("%s is too large for a library file\n",
                          o->input);
                free((void *)all);
                goto done;
            }
        }
        free((void *)all);
    }
    status = back_end(o, tree, text_cstr(module), &program, llvm_text, extras);
    /* Both lie in this frame and in the list freed below. */
    extras->own = NULL;
    extras->libraries = NULL;
    if (status != 0 && status != DRIVER_OBJECT_ONLY) {
        goto done;
    }
    if (o->lib != LIB_NONE && !o->assembly_only) {
        const char *from = o->output != NULL ? o->output : o->input;
        text_append_bytes(base, from,
                          (size_t)(platform_file_name(from) - from));
        text_append(base, text_cstr(&extras->name));
    } else if (o->output != NULL) {
        text_append(base, o->output);
    } else {
        text_appendf(base, "%.*s",
                     (int)(strlen(o->input) - strlen(SOURCE_SUFFIX)),
                     o->input);
    }

done:
    ir_module_free(&program);
    free((void *)libraries);
    free((void *)paths.items);
    lexer_token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
    return status;
}

/* The interface of the library file o->input, which names its module in
   its header. The other library files load beside it, so its types
   resolve. NULL, with a message, when the file cannot be read or holds
   no such module. */
static const struct interface *library_interface(const struct options *o,
                                                 struct arena *arena,
                                                 struct types *types,
                                                 struct ir_module *program)
{
    struct options with_input = *o;
    const char **listed = driver_libraries_with_input(o);
    const struct interface **libraries = NULL;
    const struct interface *iface = NULL;
    struct module empty;
    struct interface header;
    struct paths paths = {0};
    struct text source = {0};
    char error[200];
    size_t i;

    memset(&empty, 0, sizeof empty);
    with_input.libraries = listed;
    with_input.library_count = o->library_count + 1;
    if (!driver_read_bytes(o->input, &source) ||
        !antl_header((const uint8_t *)source.data, source.length, arena,
                     &header, error, sizeof error)) {
        if (source.length > 0) {
            say_antic("%s %s\n", o->input, error);
        }
        goto done;
    }
    if (!driver_find_libraries(&with_input, &empty, arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, "", NULL, NULL, arena, types,
                               program, libraries)) {
        goto done;
    }
    for (i = 0; i < paths.count; i++) {
        if (strcmp(libraries[i]->module, header.module) == 0) {
            iface = libraries[i];
            break;
        }
    }
    if (iface == NULL) {
        say_antic("%s holds no module `%s`\n", o->input, header.module);
    }

done:
    free(listed);
    free((void *)libraries);
    free((void *)paths.items);
    text_free(&source);
    return iface;
}

/* DESIGN: `anti doc` builds user docs from the public interface of one
   module and nothing else. The interface of a library file and the
   interface of its source are then the same structure. The command
   renders that one structure, so the doc-equivalence test of
   docs/tooling.md measures the library file and not the renderer. Dev
   docs need the private items and the `//#` notes, which live in the
   syntax tree alone. tree is filled for a source input and left NULL for
   a library file. What the interface points at lives in arena, types and
   program. */
static const struct interface *interface_of(const struct options *o,
                                            struct arena *arena,
                                            struct types *types,
                                            struct ir_module *program,
                                            struct module **tree)
{
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *parsed = NULL;
    struct interface *iface = NULL;
    const struct interface **libraries = NULL;
    struct paths paths = {0};
    struct text source = {0};
    struct text module = {0};
    struct sema_inputs in;
    char *kept;
    char *kept_module;
    size_t length = strlen(o->input);
    bool library_file = length > strlen(ANTL_SUFFIX) &&
                        strcmp(o->input + length - strlen(ANTL_SUFFIX),
                               ANTL_SUFFIX) == 0;

    *tree = NULL;
    types_init(types, arena);
    if (library_file) {
        return library_interface(o, arena, types, program);
    }
    if (!read_source(o->input, &source)) {
        goto done;
    }
    /* The names of the interface point into the source, which outlives
       the call in the memory pool and not in this buffer. */
    kept = arena_alloc(arena, source.length + 1);
    memcpy(kept, text_cstr(&source), source.length + 1);
    if (!lexer_lex(kept, source.length, arena, &diags, &tokens) ||
        !parser_parse(kept, &tokens, arena, &diags, &parsed)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    drop_test_blocks(parsed, false);
    if (!module_name(o, &module, NULL)) {
        goto done;
    }
    if (!driver_find_libraries(o, parsed, arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, text_cstr(&module), parsed, o->input,
                               arena, types, program, libraries)) {
        goto done;
    }
    /* The types of the module name its path. A page reads the module of
       the generic a copy names, so the path lives as long as they do. */
    kept_module = arena_alloc(arena, module.length + 1);
    memcpy(kept_module, text_cstr(&module), module.length + 1);
    memset(&in, 0, sizeof in);
    in.module_name = kept_module;
    in.package = o->package_name;
    in.libraries = libraries;
    in.library_count = paths.count;
    in.types = types;
    in.arena = arena;
    in.diags = &diags;
    if (!sema_check(parsed, &in, false)) {
        warnings_apply(parsed, &diags, 0, false);
        report_diagnostics(o, &diags);
        goto done;
    }
    sema_strip_generics(parsed, arena);
    warnings_apply(parsed, &diags, 0, false);
    report_diagnostics(o, &diags);
    iface = arena_alloc(arena, sizeof *iface);
    sema_interface(parsed, kept_module, arena, iface);
    *tree = parsed;

done:
    free((void *)libraries);
    free((void *)paths.items);
    lexer_token_list_free(&tokens);
    diagnostics_free(&diags);
    text_free(&source);
    text_free(&module);
    return iface;
}

bool driver_interface(const struct options *options, driver_interface_use use,
                      void *data)
{
    struct arena arena = {0};
    struct types types;
    struct ir_module program;
    struct module *tree = NULL;
    const struct interface *iface;

    ir_module_init(&program, &arena, "");
    iface = interface_of(options, &arena, &types, &program, &tree);
    if (iface != NULL) {
        use(iface, tree, data);
    }
    ir_module_free(&program);
    arena_free(&arena);
    return iface != NULL;
}

bool driver_library_header(const struct options *o, struct text *out)
{
    struct arena arena = {0};
    struct types types;
    struct ir_module program;
    struct options with_input = *o;
    const char **listed = NULL;
    struct module empty;
    struct interface header;
    struct paths paths = {0};
    struct text source = {0};
    struct text name = {0};
    struct diagnostics diags = {0};
    const struct interface **libraries = NULL;
    const struct interface *own = NULL;
    struct sema_inputs in;
    char error[200];
    size_t kept = 0;
    size_t i;
    bool ok = false;

    memset(&empty, 0, sizeof empty);
    types_init(&types, &arena);
    ir_module_init(&program, &arena, "");
    listed = driver_libraries_with_input(o);
    with_input.libraries = listed;
    with_input.library_count = o->library_count + 1;
    if (!driver_read_bytes(o->input, &source) ||
        !antl_header((const uint8_t *)source.data, source.length, &arena,
                     &header, error, sizeof error)) {
        if (source.length > 0) {
            say_antic("%s %s\n", o->input, error);
        }
        goto done;
    }
    if (!driver_find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, "", NULL, NULL, &arena, &types,
                               &program, libraries)) {
        goto done;
    }
    /* The checker declares the functions of anti.lang.Object, which the
       table of an export class writes. An empty module checks nothing
       else. */
    memset(&in, 0, sizeof in);
    in.module_name = "";
    in.libraries = libraries;
    in.library_count = paths.count;
    in.types = &types;
    in.arena = &arena;
    in.diags = &diags;
    if (!sema_check(&empty, &in, false)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    /* The module of the file goes last, where the compiled module stands
       when antic writes the header of --lib, so both write one order. */
    for (i = 0; i < paths.count; i++) {
        if (strcmp(libraries[i]->module, header.module) == 0) {
            own = libraries[i];
        } else {
            libraries[kept++] = libraries[i];
        }
    }
    if (own == NULL) {
        say_antic("%s holds no module `%s`\n", o->input,
                  header.module);
        goto done;
    }
    libraries[kept] = own;
    library_name(o, header.module, &name);
    header_write(out, text_cstr(&name), libraries, kept + 1, false);
    ok = true;

done:
    free(listed);
    free((void *)libraries);
    free((void *)paths.items);
    text_free(&source);
    text_free(&name);
    diagnostics_free(&diags);
    ir_module_free(&program);
    arena_free(&arena);
    return ok;
}

/* DESIGN: dev mode also compiles a library file of the dependency graph
   into its own object. The IR in the file is the module, and the other
   library files declare what it calls. A main module comes from source,
   so this object never links. */
static int compile_library_file(const struct options *o,
                                struct text *llvm_text, struct text *base,
                                struct extras *extras)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct types types;
    struct ir_module program;
    struct module empty;
    struct options with_input = *o;
    const char **listed = NULL;
    struct interface header;
    struct text bytes = {0};
    struct text verify_errors = {0};
    const struct interface **libraries = NULL;
    struct paths paths = {0};
    char error[200];
    int status = 1;

    memset(&empty, 0, sizeof empty);
    ir_module_init(&program, &arena, "");
    types_init(&types, &arena);
    listed = driver_libraries_with_input(o);
    with_input.libraries = listed;
    with_input.library_count = o->library_count + 1;
    if (!driver_read_bytes(o->input, &bytes)) {
        goto done;
    }
    if (!antl_header((const uint8_t *)bytes.data, bytes.length, &arena,
                     &header, error, sizeof error)) {
        say_antic("%s %s\n", o->input, error);
        goto done;
    }
    if (!driver_find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, "", NULL, NULL, &arena, &types,
                               &program, libraries)) {
        goto done;
    }
    /* No module is lowered here, so lower_checked does not verify the
       program. The passes and the optimizer rely on the verifier, and a
       library file may come from anywhere. */
    if (!ir_verify(&program, &verify_errors)) {
        say_antic("the IR of the library files fails "
                  "verification\n%s", text_cstr(&verify_errors));
        goto done;
    }
    status = back_end(o, NULL, header.module, &program, llvm_text, extras);
    if (status == 0 || status == DRIVER_OBJECT_ONLY) {
        status = DRIVER_OBJECT_ONLY;
        if (o->output != NULL) {
            text_append(base, o->output);
        } else {
            text_appendf(base, "%.*s",
                         (int)(strlen(o->input) - strlen(ANTL_SUFFIX)),
                         o->input);
        }
    }

done:
    ir_module_free(&program);
    free((void *)libraries);
    free((void *)paths.items);
    free(listed);
    text_free(&bytes);
    text_free(&verify_errors);
    diagnostics_free(&diags);
    arena_free(&arena);
    return status;
}

/* Remove the file at path when there is one. */
static void remove_file(const char *path)
{
    if (driver_file_exists(path)) {
        platform_remove(path);
    }
}

bool driver_compile_llvm(const struct options *o, const struct text *text,
                         const char *base, const char *output)
{
    struct text text_path = {0};
    struct text bitcode_path = {0};
    struct text found_opt = {0};
    struct text found_llc = {0};
    struct llvm_run r;
    bool ok;

    text_appendf(&text_path, "%s%s", base, LLVM_TEXT_SUFFIX);
    text_appendf(&bitcode_path, "%s%s", base, LLVM_BITCODE_SUFFIX);
    r.opt = o->opt != NULL ? o->opt
                           : driver_archive_tool(o, "opt", &found_opt);
    r.llc = o->llc != NULL ? o->llc
                           : driver_archive_tool(o, "llc", &found_llc);
    r.target = o->target;
    r.optimize = !o->dev;
    r.assembly = o->assembly_only;
    r.lto = o->lto;
    r.profile_generate = o->profile_generate;
    r.profile_use = o->profile_use;
    ok = r.opt != NULL && r.llc != NULL &&
         driver_write_file(text_cstr(&text_path), text) &&
         llvm_run(&r, text_cstr(&text_path), text_cstr(&bitcode_path),
                  output);
    /* DESIGN: --dump-llvm and --keep-llvm are the two ways to read the
       text, so a build without either leaves no intermediate file. */
    if (!o->keep_llvm) {
        remove_file(text_cstr(&text_path));
        remove_file(text_cstr(&bitcode_path));
    }
    text_free(&text_path);
    text_free(&bitcode_path);
    text_free(&found_opt);
    text_free(&found_llc);
    return ok;
}

/* DESIGN: four options do not stand beside the kind of library they
   meet, by R132 and R150 of "Rules of 2026-10-13". --bundle-runtime puts
   the runtime into an archive, and a shared library links it.
   --no-runtime builds a plugin, a shared library. A library for C is a
   release build, as --lto and the profiles are. --soname names the
   major version of a shared library, and an archive has none. */
static bool library_options_usable(const struct options *o)
{
    if (o->lib == LIB_SHARED && o->bundle_runtime) {
        say_antic("--bundle-runtime puts the runtime into a library of "
                  "--lib static, and a library of --lib shared links it\n");
        return false;
    }
    if (o->lib == LIB_STATIC && o->no_runtime) {
        say_antic("--no-runtime builds a plugin, which is a library of "
                  "--lib shared\n");
        return false;
    }
    if (o->lib == LIB_STATIC && o->soname) {
        say_antic("--soname gives a library of --lib shared its major "
                  "version, and a library of --lib static has none\n");
        return false;
    }
    if (o->lib == LIB_STATIC && o->dev) {
        say_antic("--lib static builds in release mode, without --dev\n");
        return false;
    }
    return true;
}

int driver_run(const struct options *given)
{
    struct options resolved = *given;
    const struct options *o = &resolved;
    struct text source = {0};
    struct text module = {0};
    struct text llvm_text = {0};
    struct text base = {0};
    struct text asm_path = {0};
    struct text obj_path = {0};
    struct extras extras;
    bool object_only = false;
    int status = 1;

    /* DESIGN: a caller that builds its own options sets the level, because
       the zero value of enum cpu_level names v1 rather than the target's
       default. A level of the other architecture is that mistake, and it
       would link a runtime directory that does not exist. */
    if (cpu_arch(o->cpu) != target_info(o->target)->arch) {
        say_antic("%s is no processor level of %s\n",
                  cpu_name(o->cpu), target_name(o->target));
        return 2;
    }

    if (o->memory_checks && !memcheck_available(o->target)) {
        say_antic("`--memory-checks` is not available for %s\n",
                  target_name(o->target));
        return 2;
    }
    resolved.lto = lto_mode(given);
    if (o->lto != LTO_NONE && !lto_usable(o)) {
        return 2;
    }
    if ((o->profile_generate || o->profile_use != NULL) &&
        !profile_usable(o)) {
        return 2;
    }
    /* DESIGN: `unload` refuses while an object of the library is alive,
       and the `created` and `destroyed` hooks count them. The code of the
       library's own classes calls both, so a plugin keeps its hooks. */
    if (o->no_hooks && driver_is_plugin(o)) {
        say_antic("`--no-hooks` drops the hooks that count the objects of a "
                  "plugin for `unload`, and `--no-runtime` builds a plugin\n");
        return 2;
    }
    if (!library_options_usable(o)) {
        return 2;
    }
    memset(&extras, 0, sizeof extras);

    if (!ends_with(o->input, SOURCE_SUFFIX) &&
        !(o->dev && ends_with(o->input, ANTL_SUFFIX))) {
        say_antic("%s: expected a %s file\n", o->input,
                  SOURCE_SUFFIX);
        return 2;
    }
    if (links(o) && !o->dev && o->runtime == NULL) {
        say_antic("linking needs --runtime <dir>, the directory "
                  "that holds %s/<target>/<level>/\n",
                  RUNTIME_LIB_DIR);
        return 2;
    }
    if (links(o) && !o->dev && !can_link(o)) {
        return 2;
    }
    if (o->lib != LIB_NONE && !o->assembly_only &&
        (o->runtime == NULL ||
         ((o->lib == LIB_SHARED ||
           (o->bundle_runtime &&
            target_info(o->target)->format == FORMAT_ELF)) &&
          !can_link(o)))) {
        if (o->runtime == NULL) {
            say_antic("--lib needs --runtime <dir>, the directory "
                      "that holds %s/<target>/<level>/\n",
                      RUNTIME_LIB_DIR);
        }
        return 2;
    }
    switch (ends_with(o->input, ANTL_SUFFIX)
                ? compile_library_file(o, &llvm_text, &base, &extras)
                : compile(o, &source, &module, &llvm_text, &base, &extras)) {
    case 0:
        break;
    case 2: /* a dump or a library file finished the command */
        status = 0;
        goto done;
    case 3: /* dev mode: a module without main stops at its object */
        object_only = true;
        break;
    default:
        goto done;
    }
    /* The .def file of a Windows host lists the COFF symbols of its
       object, which LTO would make bitcode. See lto_mode. */
    if (!o->lto_given && extras.hosts_plugins &&
        target_info(o->target)->os == OS_WINDOWS) {
        resolved.lto = LTO_NONE;
    }
    if (o->dev && !object_only && !o->assembly_only &&
        (o->runtime == NULL || !can_link(o))) {
        if (o->runtime == NULL) {
            say_antic("linking needs --runtime <dir>, the "
                      "directory that holds %s/<target>/\n",
                      RUNTIME_LIB_DIR);
        }
        goto done;
    }
    if (o->assembly_only) {
        text_append(&asm_path, text_cstr(&base));
        if (o->output == NULL) {
            text_append(&asm_path, ASSEMBLY_SUFFIX);
        }
        status = driver_compile_llvm(o, &llvm_text, text_cstr(&base),
                                     text_cstr(&asm_path))
                     ? 0
                     : 1;
        goto done;
    }

    /* DESIGN: the object stays beside the executable, so that a reader
       can open it. The text stays only under --keep-llvm. */
    text_appendf(&obj_path, "%s%s", text_cstr(&base),
                 target_info(o->target)->object_suffix);
    if (!driver_compile_llvm(o, &llvm_text, text_cstr(&base),
                             text_cstr(&obj_path))) {
        goto done;
    }
    if (object_only) {
        status = 0;
        goto done;
    }
    if (o->lib != LIB_NONE) {
        status = driver_build_c_library(o, text_cstr(&obj_path),
                                        text_cstr(&base), &extras)
                     ? 0
                     : 1;
        goto done;
    }
    if (o->output == NULL) {
        text_append(&base, target_info(o->target)->executable_suffix);
    }
    if (driver_link_program(o, text_cstr(&obj_path), text_cstr(&base),
                            &extras)) {
        status = 0;
    }

done:
    extras_free(&extras);
    text_free(&source);
    text_free(&module);
    text_free(&llvm_text);
    text_free(&base);
    text_free(&asm_path);
    text_free(&obj_path);
    return status;
}
