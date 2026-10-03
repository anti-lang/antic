/* The driver of antic: it reads the files of a compilation and runs it,
   the front end, lowering, the passes over the IR, the back end and the
   assembler, and writes the library file, the C header and the assembly.
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
#include "emit.h"
#include "ir.h"
#include "lexer.h"
#include "linker.h"
#include "layout.h"
#include "llvm_emit.h"
#include "lower.h"
#include "memcheck.h"
#include "header.h"
#include "modpath.h"
#include "platform.h"
#include "notice.h"
#include "optimize.h"
#include "whole.h"
#include "regalloc.h"
#include "rt_abi.h"
#include "select.h"
#include "sha256.h"
#include "parser.h"
#include "sema.h"
#include "types.h"
#include "text.h"
#include "warnings.h"

static void extras_free(struct extras *e)
{
    text_free(&e->name);
    text_free(&e->header);
    text_free(&e->package);
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

/* Read the whole of a binary file. */
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
        fprintf(stderr, "antic: cannot open %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        if (n > limit - total) {
            fprintf(stderr, "antic: %s is larger than " LEX_SOURCE_MAX_TEXT "\n",
                    path);
            ok = false;
            break;
        }
        total += n;
        text_append_bytes(out, buffer, n);
    }
    if (ferror(f)) {
        fprintf(stderr, "antic: cannot read %s\n", path);
        ok = false;
    }
    fclose(f);
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
        fprintf(stderr, "antic: %s contains a NUL byte\n", path);
        return false;
    }
    return true;
}

bool driver_write_file(const char *path, const struct text *content)
{
    FILE *f = platform_open(path, true);
    bool ok;

    if (f == NULL) {
        fprintf(stderr, "antic: cannot create %s\n", path);
        return false;
    }
    ok = fwrite(text_cstr(content), 1, content->length, f) == content->length;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        fprintf(stderr, "antic: cannot write %s\n", path);
    }
    return ok;
}

/* The module path of the input, from its path under the search roots.
   DESIGN: antic -c refuses a library under the anti root without
   --anti-internal. It warns on a path of one segment, which is for a
   program's own files. No compilation may define the runtime's module. */
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

static bool module_name(const struct options *o, struct text *out,
                        struct diagnostics *diags)
{
    char error[200];

    if (!modpath_of_source(o->input, o->roots, o->root_count, out, error,
                               sizeof error)) {
        fprintf(stderr, "antic: %s\n", error);
        return false;
    }
    if (strcmp(text_cstr(out), RUNTIME_MODULE) == 0) {
        fprintf(stderr, "antic: %s: the module path `%s` is the runtime's\n",
                o->input, RUNTIME_MODULE);
        return false;
    }
    if (o->library && !o->front_end && !o->internal &&
        modpath_reserved(text_cstr(out))) {
        fprintf(stderr, "antic: %s: the module path `%s` is reserved for the "
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
           !o->dump_types && !o->dump_ir && !o->dump_opt && !o->dump_select &&
           !o->dump_alloc && !o->dump_llvm && !o->library && !o->front_end &&
           o->lib == LIB_NONE;
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
        fprintf(stderr, "antic: linking for %s with the platform linker needs "
                        "a host with the same operating system. -S writes "
                        "the assembly without linking.\n",
                target_name(o->target));
        return false;
    }
    return true;
}

bool driver_file_exists(const char *path)
{
    FILE *f = platform_open(path, false);

    if (f != NULL) {
        fclose(f);
    }
    return f != NULL;
}

static void print_diagnostics(const char *input,
                              const struct diagnostics *diags)
{
    size_t i;

    for (i = 0; i < diags->count; i++) {
        const struct diagnostic *d = &diags->items[i];
        fprintf(stderr, "%s:%d:%d: %s: %s", input, d->line, d->column,
                d->warning ? "warning" : "error", d->message);
        if (d->name != NAME_NONE) {
            fprintf(stderr, " [%s]", warnings_name(d->name));
        }
        fputc('\n', stderr);
    }
}

/* Print the diagnostics of the compilation and count them for the caller
   that asked, which is `anti check`. */
static void report_diagnostics(const struct options *o,
                               const struct diagnostics *diags)
{
    size_t i;

    print_diagnostics(o->input, diags);
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
   A doc comment spans lines, so it prints its first line. Chapter 1 shows
   this output for the function scale. */
static void dump_tokens(const char *source, const struct token_list *tokens)
{
    size_t i;

    for (i = 0; i + 1 < tokens->count; i++) {
        const struct token *t = &tokens->items[i];
        const char *end = memchr(source + t->offset, '\n', t->length);
        size_t length = end != NULL ? (size_t)(end - (source + t->offset))
                                    : t->length;
        char position[32];

        snprintf(position, sizeof position, "%d:%d", t->line, t->column);
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
   input under the first search root that holds it. */
static const char *recorded_file(const struct options *o)
{
    return modpath_file_of_source(o->input, o->roots, o->root_count);
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
    ok = ir_verify(ir, &errors);
    if (!ok) {
        fprintf(stderr, "antic: internal error, the IR of %s fails "
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
            fprintf(stderr, "antic: internal error, the optimized IR of %s "
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
        const struct ir_function *f = program->functions[i];
        if (!f->is_extern && f->module != NULL &&
            strcmp(f->module, module) == 0 && strcmp(f->name, "main") == 0) {
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
    while ((n = fread(bytes, 1, sizeof bytes, f)) > 0) {
        anti_rt_sha256_update(s, bytes, n);
    }
    ok = !ferror(f);
    fclose(f);
    return ok;
}

/* DESIGN: the build id is the SHA-256 of the code that a link takes from
   antic. That is the assembly of the module that links, every object the
   command line adds and the runtime library. The notice is left out,
   because it holds the id. The paths are left out as well, so two links
   of one program in two places carry one id. What `-g` added is left out
   too, so a `-g` link and a plain link of one program carry one id.
   `anti symbols` then matches a trace of the one to the archive of the
   other. */
static bool build_id(const struct options *o, const struct text *assembly,
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
            anti_rt_sha256_update(&s, assembly->data + at,
                                  spans->items[i].start - at);
        }
        at = spans->items[i].end > at ? spans->items[i].end : at;
    }
    if (at < assembly->length) {
        anti_rt_sha256_update(&s, assembly->data + at, assembly->length - at);
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

/* The LLVM back end, which docs/work-order-llvm-back-end.md builds step
   by step. It folds the symbolic values of the target and puts in the
   checks of --memory-checks as select_module does, then translates the
   module into LLVM IR text. --dump-llvm prints the text. The step
   emit-run adds opt and llc, which write the object, so until then a
   build that wants one is refused. Returns 2 after the dump and 1 after
   an error, as back_end does. */
static int llvm_back_end(const struct options *o, struct ir_module *program,
                         const char *module, const struct extras *extras)
{
    struct layouts layouts;
    struct llvm_emit_options emit;
    struct text out = {0};
    char error[512];
    bool ok;
    int status = 1;
    size_t i;

    /* DESIGN: as in select_module, the optimizer runs again on each
       function that had a symbolic value, so a size folded here reaches
       the simplifications a number would have reached before. */
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
    /* The host has run the runtime's start already, so a plugin brings
       no constructor of its own. */
    if (o->lib == LIB_SHARED && !driver_is_plugin(o)) {
        emit.constructor = rt_name(RT_FN_INIT);
    }
    ok = ok && llvm_emit_module(&out, &emit, program, &layouts, error,
                                sizeof error);
    if (ok && o->dump_llvm) {
        fputs(text_cstr(&out), stdout);
        status = 2;
    } else if (ok) {
        text_format(error, sizeof error,
                    "the LLVM back end writes no object before the step "
                    "emit-run, and --dump-llvm prints its text");
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "antic: %s\n", error);
    }
    layout_free(&layouts);
    text_free(&out);
    return status;
}

/* Lower the program, run the optimizer passes and run the back end for
   the target: instruction selection, register allocation and emission.
   The dumps print the machine code instead, before allocation for
   --dump-select. Returns 0 with the assembly, 2 after a dump and 1 after
   an error. */
static int back_end(const struct options *o, struct module *tree,
                    const char *module, struct ir_module *program,
                    struct text *assembly, struct extras *extras)
{
    struct mach_function **functions;
    struct text out = {0};
    struct debug_spans spans = {0};
    char error[200];
    bool dump = o->dump_select || o->dump_alloc || o->dump_llvm;
    bool ok;
    int status = 1;
    size_t i;

    if (tree != NULL &&
        !lower_checked(o->input, recorded_file(o), tree, module, program,
                       lower_options(o), o->trace_patterns,
                       o->trace_pattern_count, o->package_version)) {
        return 1;
    }
    /* DESIGN: the passes over the whole program run where the program is
       whole. That is every build in release mode. In dev mode it is the
       module that links, which has main, and a library for C. A dev
       object of any other module never links. */
    if ((!o->dev || o->lib != LIB_NONE || has_main(program, module)) &&
        !whole_checked(o->input, program, module, !o->dev,
                       !o->no_reflect, o->bundle_runtime,
                       o->lib != LIB_NONE, o->dev, o)) {
        return 1;
    }
    /* A program that injects an interface or loads a library exports
       its names. Both this and the `provides` lines of a plugin are
       read here, because the optimizer drops the class records that
       carry them. */
    extras->hosts_plugins = !o->closed && o->lib == LIB_NONE && !o->library &&
                            whole_hosts_plugins(program);
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
    if (!dump && !o->assembly_only && !o->dev && o->lib == LIB_NONE &&
        !has_main(program, module)) {
        fprintf(stderr, "antic: %s: the program has no function `main`\n",
                o->input);
        return 1;
    }
    program->plugin = driver_is_plugin(o);
    if (o->memory_checks) {
        memcheck_declare(program, module,
                         o->lib == LIB_NONE && has_main(program, module),
                         o->target);
    }
    /* --dump-llvm prints the text of the LLVM back end, whichever back end
       --backend names. */
    if (o->backend == BACKEND_LLVM || o->dump_llvm) {
        return llvm_back_end(o, program, module, extras);
    }
    functions = alloc_zeroed(program->function_count + 1, sizeof *functions);
    ok = select_module(o->target, o->cpu, program, functions, error,
                       sizeof error);
    for (i = 0; ok && !(o->dump_select && !o->dump_alloc) &&
                i < program->function_count;
         i++) {
        if (functions[i] != NULL) {
            ok = regalloc_function(o->target, functions[i], error,
                                   sizeof error);
        }
    }
    if (ok && dump) {
        for (i = 0; i < program->function_count; i++) {
            if (functions[i] != NULL) {
                mach_print(&out, target_desc(o->target), o->cpu, program,
                           functions[i]);
            }
        }
        fputs(text_cstr(&out), stdout);
        status = 2;
    } else if (ok) {
        if (o->dev || driver_is_plugin(o)) {
            emit_module(assembly, o->target, o->cpu, program, functions,
                        module, extras->hosts_plugins, o->debug, &spans);
        } else {
            emit_program(assembly, o->target, o->cpu, program, functions,
                         module, extras->hosts_plugins, o->debug, &spans);
        }
        if (ok && o->dev && !has_main(program, module)) {
            status = 3;
        }
        /* The host has run the runtime's start already, so a plugin
           brings no constructor of its own. */
        if (ok && o->lib == LIB_SHARED && !driver_is_plugin(o)) {
            emit_constructor(assembly, o->target, rt_name(RT_FN_INIT));
        }
        if (ok && extras->notice.length > 0 && status != 3 &&
            !o->assembly_only && o->lib != LIB_STATIC) {
            struct text notice = {0};
            char id[65];
            ok = build_id(o, assembly, &spans, id, error, sizeof error);
            if (ok) {
                identified_notice(&notice, &extras->notice, id);
                emit_licenses(assembly, o->target, notice.data,
                              notice.length);
                text_free(&notice);
            }
        }
        if (ok && extras->hosts_plugins &&
            target_info(o->target)->format == FORMAT_COFF) {
            emit_names(&extras->host_names, o->target, program, functions);
        }
        for (i = 0; ok && i < program->function_count; i++) {
            const struct ir_function *f = program->functions[i];
            if (f->exported && !f->is_extern) {
                text_appendf(&extras->exports, "%s\n", f->name);
            }
        }
        status = !ok ? 1 : status == 3 ? 3 : 0;
    }
    if (!ok) {
        fprintf(stderr, "antic: %s\n", error);
    }
    for (i = 0; i < program->function_count; i++) {
        if (functions[i] != NULL) {
            mach_function_free(functions[i]);
            free(functions[i]);
        }
    }
    free(functions);
    text_free(&out);
    debug_spans_free(&spans);
    return status;
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
            fprintf(stderr, "antic: --dependency %s is not "
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
    if (lower_checked(o->input, recorded_file(o), tree, module, &ir,
                      lower_options(o), o->trace_patterns,
                      o->trace_pattern_count, o->package_version) &&
        own_interface(o, tree, module, arena, &iface)) {
        if (!antl_write(&bytes, &iface, &ir, o->strip_docs)) {
            fprintf(stderr, "antic: %s is too large for a library file\n",
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
    base = strrchr(o->output, '/');
    base = base != NULL ? base + 1 : o->output;
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

/* The licence notice of a linked binary: the runtime, every package of
   the loaded libraries once, and the package of the compiled module. */
static bool build_notice(const struct options *o, const struct interface *own,
                         const struct interface *const *libraries,
                         size_t count, struct arena *arena, struct text *out)
{
    const struct package **list = alloc_zeroed(count + 3, sizeof *list);
    struct package runtime;
    struct text path = {0};
    struct text text = {0};
    size_t n = 0;
    size_t i;
    size_t j;

    memset(&runtime, 0, sizeof runtime);
    runtime.name = RUNTIME_MODULE;
    runtime.version = ANTIC_VERSION;
    runtime.license = "0BSD";
    runtime.license_text = "";
    text_appendf(&path, "%s/licenses/anti_rt.txt",
                 o->runtime != NULL ? o->runtime : ".");
    if (o->runtime != NULL && read_text(text_cstr(&path), &text)) {
        char *copy = arena_alloc(arena, text.length + 1);
        memcpy(copy, text_cstr(&text), text.length);
        runtime.license_text = copy;
    }
    list[n++] = &runtime;
    /* DESIGN: the package of the compiled module is always the last
       `package` line, even where a library of the same package came
       first. `anti symbols` reads the version of a binary there. */
    for (i = 0; i < count; i++) {
        if (own->package.name != NULL &&
            strcmp(own->package.name, libraries[i]->package.name) == 0) {
            continue;
        }
        for (j = 0; j < n; j++) {
            if (strcmp(list[j]->name, libraries[i]->package.name) == 0) {
                break;
            }
        }
        if (j == n) {
            list[n++] = &libraries[i]->package;
        }
    }
    list[n++] = &own->package;
    notice_text(out, list, n);
    free((void *)list);
    text_free(&path);
    text_free(&text);
    return true;
}

static int compile(const struct options *o, struct text *source,
                   struct text *module, struct text *assembly,
                   struct text *base, struct extras *extras)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *tree = NULL;
    struct types types;
    struct ir_module program;
    const struct interface **libraries = NULL;
    struct paths paths = {0};
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
    if (!driver_load_libraries(&paths, text_cstr(module), &arena, &types,
                               &program, libraries)) {
        goto done;
    }
    tree->compile_copies = o->library || (!o->front_end && !o->dump_types);
    tree->no_reflect = o->no_reflect;
    if (!sema_check(tree, text_cstr(module), o->package_name, libraries,
                    paths.count, &types, &arena, &diags,
                    whole_program_check(o))) {
        settle_diagnostics(o, tree, &diags, false);
        goto done;
    }
    if (o->doc_warnings) {
        sema_doc_warnings(tree, text_cstr(module), libraries, paths.count,
                          &types, o->warn_undocumented, &diags);
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
        status = dump_ir(o->input, recorded_file(o), tree, text_cstr(module),
                         &program, o->dump_opt, !o->dev, lower_options(o),
                         o->trace_patterns, o->trace_pattern_count, o);
        goto done;
    }
    if (o->lib != LIB_NONE && defines_main(tree)) {
        fprintf(stderr, "antic: %s: a library for C has no function `main`\n",
                o->input);
        goto done;
    }
    if (o->lib != LIB_NONE || links(o)) {
        struct interface own;
        const struct interface **all =
            alloc_zeroed(paths.count + 2, sizeof *all);
        if (!own_interface(o, tree, text_cstr(module), &arena, &own) ||
            !build_notice(o, &own, libraries, paths.count, &arena,
                          &extras->notice)) {
            free((void *)all);
            goto done;
        }
        if (o->lib != LIB_NONE) {
            memcpy((void *)all, (void *)libraries,
                   paths.count * sizeof *all);
            all[paths.count] = &own;
            library_name(o, text_cstr(module), &extras->name);
            header_write(&extras->header, text_cstr(&extras->name), all,
                         paths.count + 1, o->bundle_runtime);
            if (!antl_write_header(&extras->package, &own)) {
                fprintf(stderr, "antic: %s is too large for a library file\n",
                        o->input);
                free((void *)all);
                goto done;
            }
        }
        free((void *)all);
    }
    status = back_end(o, tree, text_cstr(module), &program, assembly, extras);
    if (status != 0 && status != 3) {
        goto done;
    }
    if (o->lib != LIB_NONE && !o->assembly_only) {
        const char *slash = strrchr(o->output != NULL ? o->output : o->input,
                                    '/');
        const char *from = o->output != NULL ? o->output : o->input;
        if (slash != NULL) {
            text_appendf(base, "%.*s/", (int)(slash - from), from);
        }
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

/* DESIGN: `anti doc` builds user docs from the public interface of one
   module and nothing else. The interface of a library file and the
   interface of its source are then the same structure. The command
   renders that one structure, so the doc-equivalence test of
   docs/tooling.md measures the library file and not the renderer. Dev
   docs need the private items and the `//#` notes, which live in the
   syntax tree alone. tree is filled for a source input and left NULL for
   a library file. */
const struct interface *driver_interface(const struct options *o,
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
    char *kept;
    char *kept_module;
    size_t length = strlen(o->input);
    bool library_file = length > strlen(ANTL_SUFFIX) &&
                        strcmp(o->input + length - strlen(ANTL_SUFFIX),
                               ANTL_SUFFIX) == 0;
    char error[200];
    size_t i;

    *tree = NULL;
    types_init(types, arena);
    if (library_file) {
        struct options with_input = *o;
        const char **listed = driver_libraries_with_input(o);
        struct module empty;
        struct interface header;
        memset(&empty, 0, sizeof empty);
        with_input.libraries = listed;
        with_input.library_count = o->library_count + 1;
        if (!driver_read_bytes(o->input, &source) ||
            !antl_header((const uint8_t *)source.data, source.length, arena,
                         &header, error, sizeof error)) {
            if (source.length > 0) {
                fprintf(stderr, "antic: %s %s\n", o->input, error);
            }
            free(listed);
            goto done;
        }
        text_append(&module, header.module);
        if (!driver_find_libraries(&with_input, &empty, arena, &paths)) {
            free(listed);
            goto done;
        }
        free(listed);
        libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
        if (!driver_load_libraries(&paths, "", arena, types, program,
                                   libraries)) {
            goto done;
        }
        for (i = 0; i < paths.count; i++) {
            if (strcmp(libraries[i]->module, text_cstr(&module)) == 0) {
                iface = (struct interface *)libraries[i];
                break;
            }
        }
        if (iface == NULL) {
            fprintf(stderr, "antic: %s holds no module `%s`\n", o->input,
                    text_cstr(&module));
        }
        goto done;
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
    if (!driver_load_libraries(&paths, text_cstr(&module), arena, types,
                               program, libraries)) {
        goto done;
    }
    /* The types of the module name its path. A page reads the module of
       the generic a copy names, so the path lives as long as they do. */
    kept_module = arena_alloc(arena, module.length + 1);
    memcpy(kept_module, text_cstr(&module), module.length + 1);
    if (!sema_check(parsed, kept_module, o->package_name, libraries,
                    paths.count, types, arena, &diags, false)) {
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
            fprintf(stderr, "antic: %s %s\n", o->input, error);
        }
        goto done;
    }
    if (!driver_find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, "", &arena, &types, &program,
                               libraries)) {
        goto done;
    }
    /* The checker declares the functions of anti.lang.Object, which the
       table of an export class writes. An empty module checks nothing
       else. */
    if (!sema_check(&empty, "", NULL, libraries, paths.count, &types,
                    &arena, &diags, false)) {
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
        fprintf(stderr, "antic: %s holds no module `%s`\n", o->input,
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
                                struct text *assembly, struct text *base,
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
        fprintf(stderr, "antic: %s %s\n", o->input, error);
        goto done;
    }
    if (!driver_find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = alloc_zeroed(paths.count + 1, sizeof *libraries);
    if (!driver_load_libraries(&paths, "", &arena, &types, &program,
                               libraries)) {
        goto done;
    }
    /* No module is lowered here, so lower_checked does not verify the
       program. The passes and the optimizer rely on the verifier, and a
       library file may come from anywhere. */
    if (!ir_verify(&program, &verify_errors)) {
        fprintf(stderr, "antic: the IR of the library files fails "
                        "verification\n%s", text_cstr(&verify_errors));
        goto done;
    }
    status = back_end(o, NULL, header.module, &program, assembly, extras);
    if (status == 0 || status == 3) {
        status = 3;
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

/* The tool of the runtime archive, or its bare name for the search path.
   DESIGN: an installed antic finds llvm-mc and llvm-ar beside itself in
   bin/, the rule that already holds for the lld programs. The text holds
   the path, so it lives until the caller frees it. */
static const char *archive_tool(const struct options *o, const char *name,
                                struct text *path)
{
    enum target host;

    if (o->runtime == NULL) {
        return name;
    }

    if (!target_host(&host)) {
        return name;
    }
    text_appendf(path, "%s/%s/%s%s", o->runtime, RUNTIME_BIN_DIR, name,
                 target_info(host)->executable_suffix);
    if (driver_file_exists(text_cstr(path))) {
        return text_cstr(path);
    }
    return name;
}

/* Run llvm-mc on the assembly file for the target. */
bool driver_assemble(const struct options *o, const char *assembly,
                     const char *object)
{
    struct text triple = {0};
    struct text attributes = {0};
    struct text found = {0};
    const char *argv[] = {NULL, NULL, "-filetype=obj", "-o", object, assembly,
                          NULL, NULL};
    int run;

    argv[0] = o->llvm_mc != NULL ? o->llvm_mc
                                 : archive_tool(o, "llvm-mc", &found);
    text_appendf(&triple, "-triple=%s", target_info(o->target)->triple);
    argv[1] = text_cstr(&triple);
    /* The assembler of the level, so that it takes the instructions the
       level adds. The x86_64 assembler needs no attributes. */
    if (cpu_attributes(o->cpu)[0] != '\0') {
        text_appendf(&attributes, "-mattr=%s", cpu_attributes(o->cpu));
        argv[6] = argv[5];
        argv[5] = text_cstr(&attributes);
        argv[7] = NULL;
    }
    run = process_run(argv);
    text_free(&attributes);
    text_free(&triple);
    text_free(&found);
    if (run != 0) {
        fprintf(stderr, "antic: llvm-mc failed\n");
        return false;
    }
    return true;
}

int driver_run(const struct options *o)
{
    struct text source = {0};
    struct text module = {0};
    struct text assembly = {0};
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
        fprintf(stderr, "antic: %s is no processor level of %s\n",
                cpu_name(o->cpu), target_name(o->target));
        return 2;
    }

    if (o->memory_checks && !memcheck_available(o->target)) {
        fprintf(stderr, "antic: `--memory-checks` is not available for %s\n",
                target_name(o->target));
        return 2;
    }
    /* DESIGN: `unload` refuses while an object of the library is alive,
       and the `created` and `destroyed` hooks count them. The code of the
       library's own classes calls both, so a plugin keeps its hooks. */
    if (o->no_hooks && driver_is_plugin(o)) {
        fputs("antic: `--no-hooks` drops the hooks that count the objects of "
              "a plugin for `unload`, and `--no-runtime` builds a plugin\n",
              stderr);
        return 2;
    }
    memset(&extras, 0, sizeof extras);

    if (!ends_with(o->input, SOURCE_SUFFIX) &&
        !(o->dev && ends_with(o->input, ANTL_SUFFIX))) {
        fprintf(stderr, "antic: %s: expected a %s file\n", o->input,
                SOURCE_SUFFIX);
        return 2;
    }
    if (links(o) && !o->dev && o->runtime == NULL) {
        fprintf(stderr, "antic: linking needs --runtime <dir>, the directory "
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
            target_info(o->target)->format != FORMAT_COFF)) &&
          !can_link(o)))) {
        if (o->runtime == NULL) {
            fprintf(stderr, "antic: --lib needs --runtime <dir>, the directory "
                            "that holds %s/<target>/<level>/\n",
                    RUNTIME_LIB_DIR);
        }
        return 2;
    }
    switch (ends_with(o->input, ANTL_SUFFIX)
                ? compile_library_file(o, &assembly, &base, &extras)
                : compile(o, &source, &module, &assembly, &base, &extras)) {
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
    if (o->dev && !object_only && !o->assembly_only &&
        (o->runtime == NULL || !can_link(o))) {
        if (o->runtime == NULL) {
            fprintf(stderr, "antic: linking needs --runtime <dir>, the "
                            "directory that holds %s/<target>/\n",
                    RUNTIME_LIB_DIR);
        }
        goto done;
    }
    if (o->assembly_only) {
        if (o->output == NULL) {
            text_append(&base, ASSEMBLY_SUFFIX);
        }
        status = driver_write_file(text_cstr(&base), &assembly) ? 0 : 1;
        goto done;
    }

    /* DESIGN: the assembly and object files stay beside the executable,
       so that a reader can open them. */
    text_appendf(&asm_path, "%s%s", text_cstr(&base), ASSEMBLY_SUFFIX);
    text_appendf(&obj_path, "%s%s", text_cstr(&base),
                 target_info(o->target)->object_suffix);
    if (!driver_write_file(text_cstr(&asm_path), &assembly)) {
        goto done;
    }
    if (!driver_assemble(o, text_cstr(&asm_path), text_cstr(&obj_path))) {
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
    text_free(&assembly);
    text_free(&base);
    text_free(&asm_path);
    text_free(&obj_path);
    return status;
}
