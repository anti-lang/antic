#include "../binary_stdio.h"
#include "check.h"
#include "antl.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "parser.h"
#include "sema.h"
#include "types.h"
#include "whole.h"

/* One program: the modules share the memory pool, the types and the IR,
   as they do in antic. */
struct program {
    struct arena arena;
    struct types types;
    struct diagnostics diags;
    struct token_list tokens[4];
    size_t token_lists;
    const struct interface *libraries[4];
    size_t library_count;
    struct ir_module ir;
};

static void open_program(struct program *p)
{
    memset(p, 0, sizeof *p);
    types_init(&p->types, &p->arena);
    ir_module_init(&p->ir, &p->arena, "");
}

static void close_program(struct program *p)
{
    size_t i;

    ir_module_free(&p->ir);
    for (i = 0; i < p->token_lists; i++) {
        token_list_free(&p->tokens[i]);
    }
    diagnostics_free(&p->diags);
    arena_free(&p->arena);
}

/* Check and lower source as module name into out. */
static struct module *compile(struct program *p, const char *name,
                              const char *source, struct ir_module *out)
{
    struct token_list *tokens = &p->tokens[p->token_lists++];
    struct module *module = NULL;
    bool ok = lex(source, strlen(source), &p->arena, &p->diags, tokens) &&
              parse(source, tokens, &p->arena, &p->diags, &module) &&
              sema_check(module, name, NULL, p->libraries, p->library_count,
                         &p->types, &p->arena, &p->diags, true) &&
              lower_module(module, name, out, &p->diags, false);

    if (!ok) {
        check_failures++;
        fprintf(stderr, "module %s does not compile: %s\n%s\n", name,
                p->diags.count > 0 ? p->diags.items[0].message : "",
                source);
        return NULL;
    }
    return module;
}

/* Compile a library on its own, write its library file and read the
   file into the program. antic loads the files of its command line so. */
static void load_library(struct program *p, const char *name,
                         const char *source)
{
    struct ir_module ir;
    struct interface iface;
    struct text bytes = {0};
    struct module *module;
    char error[160] = "";

    ir_module_init(&ir, &p->arena, name);
    module = compile(p, name, source, &ir);
    if (module != NULL) {
        sema_interface(module, name, &p->arena, &iface);
        antl_write(&bytes, &iface, &ir, false);
        p->libraries[p->library_count] =
            antl_read((const uint8_t *)bytes.data, bytes.length,
                      p->libraries, p->library_count, &p->types, &p->arena,
                      &p->ir, error, sizeof error);
        CHECK_STR(error, "");
        p->library_count++;
    }
    text_free(&bytes);
    ir_module_free(&ir);
}

/* The global of a module that holds a definition, not a reference. */
static uint32_t global_named(const struct ir_module *m, const char *module,
                             const char *name)
{
    size_t i;

    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (!g->is_extern && g->module != NULL &&
            strcmp(g->module, module) == 0 && strcmp(g->name, name) == 0) {
            return (uint32_t)i;
        }
    }
    for (i = 0; i < m->global_count; i++) {
        if (m->globals[i]->module == NULL &&
            strcmp(m->globals[i]->name, name) == 0) {
            return (uint32_t)i;
        }
    }
    check_failures++;
    fprintf(stderr, "no global %s.%s\n", module, name);
    return 0;
}

/* The entries a table call reaches, as the names of their functions in
   the order the pass gives them, separated by spaces. */
static void check_entries(struct whole *w, const struct ir_module *m,
                          uint32_t descriptor, uint32_t slot,
                          const char *expected)
{
    const uint32_t *entries = NULL;
    size_t count = whole_entries(w, descriptor, slot, &entries);
    struct text names = {0};
    size_t i;

    for (i = 0; i < count; i++) {
        text_append(&names, i > 0 ? " " : "");
        text_append(&names, entries[i] == IR_NO_INDEX
                                ? "(none)"
                                : m->functions[entries[i]]->name);
    }
    CHECK_STR(text_cstr(&names), expected);
    text_free(&names);
}

static const char shapes[] =
    "pub abstract class Shape\n"
    "{\n"
    "    abstract fn area(self) -> int;\n"
    "    pub fn name(self) -> int\n"
    "    {\n"
    "        return 0;\n"
    "    }\n"
    "}\n"
    "pub abstract class Named\n"
    "{\n"
    "    abstract fn label(self) -> int;\n"
    "}\n"
    "pub class Square\n"
    "{\n"
    "    inherits Shape,\n"
    "    implements n: Named,\n"
    "    concrete fn area(self) -> int\n"
    "    {\n"
    "        return 1;\n"
    "    }\n"
    "    concrete fn label(self) -> int\n"
    "    {\n"
    "        return 2;\n"
    "    }\n"
    "}\n"
    "pub class Tile\n"
    "{\n"
    "    inherits Square,\n"
    "    concrete fn area(self) -> int\n"
    "    {\n"
    "        return 3;\n"
    "    }\n"
    "}\n";

/* A call through the table of a class reaches the entry at its slot in
   the table of every concrete class at or below it. A call through an
   interface reaches the tables of its sub-objects, and a call through
   the root reaches every table of the program. */
static void finds_entries(void)
{
    struct program p;
    struct whole *w;
    uint32_t shape;

    open_program(&p);
    compile(&p, "main", shapes, &p.ir);
    w = whole_build(&p.ir);
    shape = global_named(&p.ir, "main", "Shape.descriptor");
    check_entries(w, &p.ir, shape, 8, "Square.area Tile.area");
    check_entries(w, &p.ir, shape, 9, "Shape.name");
    check_entries(w, &p.ir, global_named(&p.ir, "main", "Square.descriptor"),
                  8, "Square.area Tile.area");
    check_entries(w, &p.ir, global_named(&p.ir, "main", "Tile.descriptor"), 8,
                  "Tile.area");
    check_entries(w, &p.ir, global_named(&p.ir, "main", "Named.descriptor"),
                  8, "Square.n.label.thunk Tile.n.label.thunk");
    check_entries(w, &p.ir,
                  global_named(&p.ir, "main", "anti_rt_Object_descriptor"), 1,
                  "anti_rt_Object_type_name Square.n.type_name.thunk "
                  "Tile.n.type_name.thunk");
    whole_free(w);
    close_program(&p);
}

/* A module that implements an interface of a library names the
   library's descriptor through a reference of its own. The pass takes
   both for one class. */
static void joins_modules(void)
{
    struct program p;
    struct whole *w;

    open_program(&p);
    load_library(&p, "shapes", shapes);
    load_library(&p, "tags",
                 "import shapes;\n"
                 "pub class Tag\n"
                 "{\n"
                 "    implements n: shapes.Named,\n"
                 "    concrete fn label(self) -> int\n"
                 "    {\n"
                 "        return 4;\n"
                 "    }\n"
                 "}\n");
    w = whole_build(&p.ir);
    check_entries(w, &p.ir, global_named(&p.ir, "shapes", "Named.descriptor"),
                  8, "Square.n.label.thunk Tile.n.label.thunk "
                     "Tag.n.label.thunk");
    whole_free(w);
    close_program(&p);
}

/* The printed body of the function named name. */
static void print_function(struct text *out, const struct ir_module *m,
                           const char *name)
{
    struct text all = {0};
    const char *start;
    const char *end;
    char header[80];

    ir_print(&all, m);
    snprintf(header, sizeof header, "fn main.%s(", name);
    start = strstr(text_cstr(&all), header);
    end = start != NULL ? strstr(start, "\n}\n") : NULL;
    if (end != NULL) {
        text_append_bytes(out, start, (size_t)(end - start) + 3);
    }
    text_free(&all);
}

/* Run the passes over shapes and a function that calls through tables,
   and compare the printed function. */
static void passes(bool release, const char *expected)
{
    struct program p;
    struct text errors = {0};
    struct text out = {0};
    struct text source = {0};
    struct whole_options options;

    text_append(&source, shapes);
    text_append(&source,
                "final class Dot\n"
                "{\n"
                "    pub fn size(self) -> int\n"
                "    {\n"
                "        return 5;\n"
                "    }\n"
                "}\n"
                "fn calls(s: *Shape, n: *Named, t: *Tile) -> int\n"
                "{\n"
                "    return s.area() + s.name() + n.label() + t.area();\n"
                "}\n");
    open_program(&p);
    compile(&p, "main", text_cstr(&source), &p.ir);
    memset(&options, 0, sizeof options);
    options.release = release;
    CHECK(whole_program(&p.ir, &options, &errors));
    CHECK_STR(text_cstr(&errors), "");
    print_function(&out, &p.ir, "calls");
    CHECK_STR(text_cstr(&out), expected);
    text_free(&errors);
    text_free(&out);
    text_free(&source);
    close_program(&p);
}

/* Release mode calls a function directly when every table that the
   pointer may point at holds that one function at the slot. Two classes
   fill `area`, and two sub-objects give `label` a thunk each, so those
   two calls stay indirect. Dev mode changes no call. */
static void devirtualises(void)
{
    passes(true,
           "fn main.calls(%0: ptr, %1: ptr, %2: ptr) -> i64 {\n"
           "b0:\n"
           "    %3 = load ptr %0\n"
           "    %4 = mul i64 8, size_of ptr\n"
           "    %5 = ptradd %3, %4\n"
           "    %6 = load ptr %5\n"
           "    %7 = call i64 %6 via @main.fn.0(%0) table "
           "@main.Shape.descriptor 8\n"
           "    %8 = load ptr %0\n"
           "    %9 = mul i64 9, size_of ptr\n"
           "    %10 = ptradd %8, %9\n"
           "    %11 = load ptr %10\n"
           "    %12 = call i64 @main.Shape.name(%0)\n"
           "    %13 = add i64 %7, %12\n"
           "    %14 = load ptr %1\n"
           "    %15 = mul i64 8, size_of ptr\n"
           "    %16 = ptradd %14, %15\n"
           "    %17 = load ptr %16\n"
           "    %18 = call i64 %17 via @main.fn.0(%1) table "
           "@main.Named.descriptor 8\n"
           "    %19 = add i64 %13, %18\n"
           "    %20 = load ptr %2\n"
           "    %21 = mul i64 8, size_of ptr\n"
           "    %22 = ptradd %20, %21\n"
           "    %23 = load ptr %22\n"
           "    %24 = call i64 @main.Tile.area(%2)\n"
           "    %25 = add i64 %19, %24\n"
           "    ret i64 %25\n"
           "}\n");
    passes(false,
           "fn main.calls(%0: ptr, %1: ptr, %2: ptr) -> i64 {\n"
           "b0:\n"
           "    %3 = load ptr %0\n"
           "    %4 = mul i64 8, size_of ptr\n"
           "    %5 = ptradd %3, %4\n"
           "    %6 = load ptr %5\n"
           "    %7 = call i64 %6 via @main.fn.0(%0) table "
           "@main.Shape.descriptor 8\n"
           "    %8 = load ptr %0\n"
           "    %9 = mul i64 9, size_of ptr\n"
           "    %10 = ptradd %8, %9\n"
           "    %11 = load ptr %10\n"
           "    %12 = call i64 %11 via @main.fn.0(%0) table "
           "@main.Shape.descriptor 9\n"
           "    %13 = add i64 %7, %12\n"
           "    %14 = load ptr %1\n"
           "    %15 = mul i64 8, size_of ptr\n"
           "    %16 = ptradd %14, %15\n"
           "    %17 = load ptr %16\n"
           "    %18 = call i64 %17 via @main.fn.0(%1) table "
           "@main.Named.descriptor 8\n"
           "    %19 = add i64 %13, %18\n"
           "    %20 = load ptr %2\n"
           "    %21 = mul i64 8, size_of ptr\n"
           "    %22 = ptradd %20, %21\n"
           "    %23 = load ptr %22\n"
           "    %24 = call i64 %23 via @main.fn.0(%2) table "
           "@main.Tile.descriptor 8\n"
           "    %25 = add i64 %19, %24\n"
           "    ret i64 %25\n"
           "}\n");
}

/* The printed lines of the program that start with prefix. */
static void print_lines(struct text *out, const struct ir_module *m,
                        const char *prefix)
{
    struct text all = {0};
    const char *line;

    ir_print(&all, m);
    for (line = text_cstr(&all); *line != '\0';) {
        const char *end = strchr(line, '\n');
        size_t length = (size_t)(end - line) + 1;
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            text_append_bytes(out, line, length);
        }
        line += length;
    }
    text_free(&all);
}

/* Run the passes over a program that reads the registry when reads is
   set, and compare the printed globals of the registry. */
static void registry(bool reads, bool reflect, bool bundled,
                     const char *expected)
{
    struct program p;
    struct text errors = {0};
    struct text out = {0};
    struct text source = {0};
    struct whole_options options;

    text_append(&source, shapes);
    text_append(&source, "singleton class Only\n"
                         "{\n"
                         "    n: int = 1,\n"
                         "}\n");
    if (reads) {
        text_append(&source,
                    "extern fn anti_rt_reflect_new(name: *byte, length: int)"
                    " -> *Object;\n"
                    "fn make() -> *Object\n"
                    "{\n"
                    "    return anti_rt_reflect_new(\"Tile\".ptr, 4);\n"
                    "}\n");
    }
    open_program(&p);
    compile(&p, "main", text_cstr(&source), &p.ir);
    memset(&options, 0, sizeof options);
    options.reflect = reflect;
    options.bundled = bundled;
    CHECK(whole_program(&p.ir, &options, &errors));
    print_lines(&out, &p.ir, "global anti.rt.");
    print_lines(&out, &p.ir, "global (null).anti_rt_registry");
    CHECK_STR(text_cstr(&out), expected);
    text_free(&errors);
    text_free(&out);
    text_free(&source);
    close_program(&p);
}

/* The registry lists every class that `reflect.new` may build. That is
   each complete class that is not a singleton, with its descriptor, the
   function that prepares an object and the path of its module. A program
   that never reads the registry gets none, and `--no-reflect` leaves it
   empty. A library with the runtime bundled holds the readers whatever it
   reads, and gets an empty one. */
static void writes_registry(void)
{
    registry(true, true, false,
             "global anti.rt.registry.0 size 5 align 1 bytes 6d 61 69 6e 00\n"
             "global anti.rt.registry.classes [2]anti.rt.Class { "
             "anti.rt.Class { @main.Square.descriptor, @main.Square.init, "
             "@anti.rt.registry.0, i64 4, i64 0 }, "
             "anti.rt.Class { @main.Tile.descriptor, @main.Tile.init, "
             "@anti.rt.registry.0, i64 4, i64 0 } }\n"
             "global (null).anti_rt_registry anti.rt.Registry { i64 2, "
             "@anti.rt.registry.classes }\n");
    registry(true, false, false,
             "global (null).anti_rt_registry anti.rt.Registry { i64 0, "
             "ptr 0 }\n");
    registry(false, true, false, "");
    registry(false, true, true,
             "global (null).anti_rt_registry anti.rt.Registry { i64 0, "
             "ptr 0 }\n");
}

static const char board[] =
    "pub singleton class Board\n"
    "{\n"
    "    pub mutable score: int = 0,\n"
    "    pub atomic hits: int = 0,\n"
    "}\n"
    "pub fn bump()\n"
    "{\n"
    "    Board.get().score = 1;\n"
    "}\n"
    "pub fn hit()\n"
    "{\n"
    "    Board.get().hits.add(1);\n"
    "}\n"
    "pub abstract class Task\n"
    "{\n"
    "    abstract fn run(self) -> int;\n"
    "}\n"
    "pub class Scoring\n"
    "{\n"
    "    inherits Task,\n"
    "    concrete fn run(self) -> int\n"
    "    {\n"
    "        return Board.get().score;\n"
    "    }\n"
    "}\n"
    "pub class Quiet\n"
    "{\n"
    "    inherits Task,\n"
    "    concrete fn run(self) -> int\n"
    "    {\n"
    "        return 0;\n"
    "    }\n"
    "}\n";

/* DESIGN: a `worker fn` may not reach a `mutable` field of a singleton,
   in any function it calls directly or through a table, in any module.
   The checker of each module walks its own functions. The pass sees
   what they call in the other modules and through the tables. */
static void checks_singletons(void)
{
    struct program p;
    struct text errors = {0};
    struct whole_options options;

    open_program(&p);
    load_library(&p, "board", board);
    compile(&p, "main",
            "import board;\n"
            "worker fn tally(part: []int) -> int\n"
            "{\n"
            "    board.bump();\n"
            "    return part.len;\n"
            "}\n"
            "worker fn spin(part: []int) -> int\n"
            "{\n"
            "    let s = board.Scoring { };\n"
            "    let t: *board.Task = &s;\n"
            "    return t.run() + part.len;\n"
            "}\n"
            "worker fn count(part: []int) -> int\n"
            "{\n"
            "    board.hit();\n"
            "    return part.len;\n"
            "}\n",
            &p.ir);
    memset(&options, 0, sizeof options);
    CHECK(!whole_program(&p.ir, &options, &errors));
    CHECK_STR(text_cstr(&errors),
              "`score` is `mutable` in singleton `Board` and `worker fn "
              "tally` reaches it\n"
              "`score` is `mutable` in singleton `Board` and `worker fn "
              "spin` reaches it\n");
    text_free(&errors);
    close_program(&p);
}

void test_whole(void)
{
    finds_entries();
    joins_modules();
    devirtualises();
    writes_registry();
    checks_singletons();
}
