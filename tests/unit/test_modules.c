#include "../binary_stdio.h"
#include <stdlib.h>
#include "check.h"
#include "alloc.h"
#include "pipeline.h"
#include "antl.h"
#include "modpath.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* One compilation with its libraries. Every module shares the memory pool
   and the types, as in antic, so a struct keeps one identity. */
struct session {
    struct arena arena;
    struct types types;
    struct diagnostics diags;
    struct token_list tokens[8];
    size_t token_lists;
    const struct interface *libraries[8];
    size_t library_count;
    /* The interface build_damaged writes, which its damage changes. */
    struct interface *written;
};

static void open_session(struct session *s)
{
    memset(s, 0, sizeof *s);
    types_init(&s->types, &s->arena);
}

static void close_session(struct session *s)
{
    size_t i;

    for (i = 0; i < s->token_lists; i++) {
        lexer_token_list_free(&s->tokens[i]);
    }
    diagnostics_free(&s->diags);
    arena_free(&s->arena);
}

/* Parse and check source as module name. */
static struct module *check_module(struct session *s, const char *name,
                                   const char *source, bool *ok)
{
    struct token_list *tokens = &s->tokens[s->token_lists++];
    struct module *module = NULL;

    *ok = lexer_lex(source, strlen(source), &s->arena, &s->diags, tokens) &&
          parser_parse(source, tokens, &s->arena, &s->diags, &module) &&
          sema_check(module, name, NULL, s->libraries, s->library_count, &s->types,
                     &s->arena, &s->diags, true);
    return module;
}

/* Compile a library and add its interface to the session. */
static void library(struct session *s, const char *name, const char *source)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    bool ok;
    struct module *module = check_module(s, name, source, &ok);

    if (!ok) {
        check_failures++;
        fprintf(stderr, "library %s does not check:\n", name);
        print_diagnostics(&s->diags, source);
        return;
    }
    sema_interface(module, name, &s->arena, iface);
    s->libraries[s->library_count++] = iface;
}

static const char geometry[] =
    "pub struct Rect { w: int, h: int }\n"
    "struct Private { x: int }\n"
    "pub const SIDES: int = 4;\n"
    "pub extern fn putchar(c: i32) -> i32;\n"
    "pub fn area(w: int, h: int) -> int {\n"
    "    return w * h;\n"
    "}\n"
    "pub fn make() -> ?*Rect {\n"
    "    return alloc(Rect, 1);\n"
    "}\n"
    "pub fn release(r: *Rect) {\n"
    "    free(r);\n"
    "}\n"
    "fn hidden() -> int {\n"
    "    return 1;\n"
    "}\n";

static void geometry_accepts(const char *source)
{
    struct session s;
    bool ok;

    open_session(&s);
    library(&s, "geometry", geometry);
    check_module(&s, "main", source, &ok);
    expect_accepted(ok, &s.diags, source);
    close_session(&s);
}

static void geometry_rejects(const char *source, int line, int column,
                                      const char *message)
{
    struct session s;
    bool ok;

    open_session(&s);
    library(&s, "geometry", geometry);
    check_module(&s, "main", source, &ok);
    expect_refused(ok, &s.diags, source, line, column, message);
    close_session(&s);
}

static void imports(void)
{
    geometry_accepts("import geometry;\n"
                     "import geometry as g;\n"
                     "const TWICE: int = geometry.SIDES * 2;\n"
                     "fn main() -> int {\n"
                     "    let r = g.make() else { return 1; };\n"
                     "    let v = g.Rect { w: 1, h: TWICE };\n"
                     "    r.release();\n"
                     "    g.putchar(65);\n"
                     "    return geometry.area(v.w, v.h);\n"
                     "}\n");
    geometry_rejects("import geometry;\nfn f() -> int {\n    return geometry.hidden();\n}\n",
                     3, 12, "`geometry` has no public item `hidden`");
    geometry_rejects("import geometry;\nfn f(p: *geometry.Private) {}\n", 2, 10,
                     "`geometry` has no public struct `Private`");
    geometry_rejects("import geometry;\nfn f() {\n    let m = geometry;\n}\n", 3, 13,
                     "`geometry` is a module, not a value");
    geometry_rejects("import geometry;\nfn f() {\n    let t = geometry.Rect;\n}\n", 3, 13,
                     "`geometry.Rect` is a type, not a value");
    geometry_rejects("import geometry;\nfn geometry() {}\n", 2, 4,
                     "`geometry` is already declared");
    geometry_rejects("import main;\n", 1, 8, "`main` cannot import itself");
    geometry_rejects("import shapes;\n", 1, 8, "cannot find module `shapes`");
    /* A method comes from the module that declares the struct. */
    geometry_rejects("import geometry;\n"
                     "fn area(r: *geometry.Rect) -> int {\n    return r.area();\n}\n",
                     3, 12, "`Rect` has no function `area`");
}

/* Sixteen damaged libraries that each import all the others. The search
   for a cycle went down every path to the depth of the library count,
   15^16 of them. */
static void damaged_imports(void)
{
    enum { COUNT = 16 };
    static char names[COUNT][4];
    static const char *imports[COUNT][COUNT - 1];
    static struct interface libs[COUNT];
    static const char source[] = "import l0;\n";
    const struct interface *list[COUNT];
    struct token_list *tokens;
    struct module *module = NULL;
    struct session s;
    bool ok;
    size_t i;
    size_t j;

    for (i = 0; i < COUNT; i++) {
        snprintf(names[i], sizeof names[i], "l%zu", i);
    }
    for (i = 0; i < COUNT; i++) {
        size_t n = 0;
        for (j = 0; j < COUNT; j++) {
            if (j != i) {
                imports[i][n++] = names[j];
            }
        }
        memset(&libs[i], 0, sizeof libs[i]);
        libs[i].module = names[i];
        libs[i].imports = imports[i];
        libs[i].import_count = n;
        list[i] = &libs[i];
    }
    open_session(&s);
    tokens = &s.tokens[s.token_lists++];
    ok = lexer_lex(source, strlen(source), &s.arena, &s.diags, tokens) &&
         parser_parse(source, tokens, &s.arena, &s.diags, &module) &&
         sema_check(module, "main", NULL, list, COUNT, &s.types, &s.arena,
                    &s.diags, true);
    CHECK(ok);
    close_session(&s);
}

static void cycles(void)
{
    struct session s;
    bool ok;

    /* c was built against an earlier main, and b against c. */
    open_session(&s);
    library(&s, "main", "pub fn g() {}\n");
    library(&s, "c", "import main;\npub fn h() {}\n");
    library(&s, "b", "import c;\npub fn f() {}\n");
    s.libraries[0] = s.libraries[1];
    s.libraries[1] = s.libraries[2];
    s.library_count = 2;
    check_module(&s, "main", "import b;\n", &ok);
    CHECK(!ok);
    CHECK(s.diags.count == 1);
    if (s.diags.count == 1) {
        CHECK(s.diags.items[0].line == 1 && s.diags.items[0].column == 8);
        CHECK_STR(s.diags.items[0].message,
                  "`b` depends on `main`, so the import forms a cycle");
    }
    close_session(&s);
    damaged_imports();
}

static void lowers_imports(void)
{
    struct session s;
    struct ir_module ir;
    struct text out = {0};
    struct text errors = {0};
    struct module *module;
    bool ok;
    const char *source = "import geometry;\n"
                         "fn main() -> int {\n"
                         "    let r = geometry.make();\n"
                         "    r.release();\n"
                         "    geometry.putchar(10);\n"
                         "    return geometry.area(geometry.SIDES, 2);\n"
                         "}\n";

    open_session(&s);
    library(&s, "geometry", geometry);
    module = check_module(&s, "main", source, &ok);
    CHECK(ok);
    ir_module_init(&ir, &s.arena, "main");
    if (ok) {
        lower_module(module, "main", &ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    ir_print(&out, &ir);
    CHECK_STR(text_cstr(&out),
              "type geometry.Rect = struct { w: i64, h: i64 }\n"
              "extern fn geometry.make() -> ptr\n"
              "extern fn geometry.release(ptr nonnull "
              "deref(size_of geometry.Rect))\n"
              "extern fn putchar(i32) -> i32\n"
              "extern fn geometry.area(i64, i64) -> i64\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %0 = call ptr @geometry.make()\n"
              "    %1 = copy ptr %0\n"
              "    call void @geometry.release(%1)\n"
              "    %2 = call i32 @putchar(10)\n"
              "    %3 = call i64 @geometry.area(4, 2)\n"
              "    ret i64 %3\n"
              "}\n");
    CHECK(ir_verify(&ir, &errors));
    text_free(&out);
    text_free(&errors);
    ir_module_free(&ir);
    close_session(&s);
}

/* Check, lower and write a library, and add its interface. */
/* Compile a library and write its file. damage, when set, changes the
   checked trees of its generics before the writer reads them. */
static bool build_damaged(struct session *s, const char *name,
                          const char *source,
                          void (*damage)(struct session *),
                          struct text *bytes)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    struct ir_module ir;
    struct module *module;
    bool ok;

    module = check_module(s, name, source, &ok);
    /* The generics leave the module before lowering, as in the driver. */
    if (ok) {
        sema_strip_generics(module, &s->arena);
    }
    ir_module_init(&ir, &s->arena, name);
    if (ok) {
        lower_module(module, name, &ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    if (!ok) {
        check_failures++;
        fprintf(stderr, "library %s does not compile:\n", name);
        print_diagnostics(&s->diags, source);
    } else {
        sema_interface(module, name, &s->arena, iface);
        s->libraries[s->library_count++] = iface;
        if (damage != NULL) {
            s->written = iface;
            damage(s);
        }
        antl_write(bytes, iface, &ir, false);
    }
    ir_module_free(&ir);
    return ok;
}

static bool build_library(struct session *s, const char *name,
                          const char *source, struct text *bytes)
{
    return build_damaged(s, name, source, NULL, bytes);
}

static const char scale_source[] = "pub const SCALE: uint = 6;\n"
                                   "pub fn scale(x: uint) -> uint {\n"
                                   "    return x * SCALE;\n"
                                   "}\n";

/* The library file of scale_source, byte by byte. */
static const uint8_t scale_antl[] = {
    'A', 'N', 'T', 'L', 79, 0, 0, 0,                /* magic, version */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* package name */
    5, 0, 0, 0, '0', '.', '0', '.', '0',            /* package version */
    0, 0, 0, 0,                                     /* dependencies */
    0, 0, 0, 0,                                     /* license */
    0, 0, 0, 0,                                     /* license text */
    0, 0, 0, 0,                                     /* attribution */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* module */
    0, 0, 0, 0,                                     /* imports */
    0, 0, 0, 0,                                     /* frameworks */
    0, 0, 0, 0,                                     /* linux libraries */
    0, 0, 0, 0,                                     /* module doc */
    2, 0, 0, 0,                                     /* types */
    11,                                             /* 0: uint */
    23, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* 1: fn(int) -> int */
    2, 0, 0, 0,                                     /* items */
    2, 5, 0, 0, 0, 'S', 'C', 'A', 'L', 'E', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 6, 0, 0, 0, 0, 0, 0, 0,                      /* const SCALE = 6 */
    3, 5, 0, 0, 0, 's', 'c', 'a', 'l', 'e', 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 'x',                                /* fn scale(x) */
    0, 0, 0, 0,                                     /* no defaults */
    0, 0, 0, 0,                                     /* no own parameters */
    0, 0, 0, 0,                                     /* generics */
    0, 0, 0, 0,                                     /* extern symbols */
    0, 0, 0, 0,                                     /* generic bodies */
    1, 0, 0, 0,                                     /* source files */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* the one file */
    0, 0, 0, 0,                                     /* symbolic values */
    0, 0, 0, 0,                                     /* aggregates */
    0, 0, 0, 0,                                     /* globals */
    1, 0, 0, 0,                                     /* functions */
    0, 0, 5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',      /* flags, effects */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* scale.scale */
    4, 255, 255, 255, 255, 0,                       /* -> i64 */
    0, 0, 0, 0, 2, 0, 0, 0,                         /* file 0, line 2 */
    1, 0, 0, 0, 4, 0, 255, 255, 255, 255,           /* one i64 parameter */
    0, 255, 255, 255, 255,                          /* with no facts */
    2, 0, 0, 0, 4, 4,                               /* temporaries */
    1, 0, 0, 0,                                     /* blocks */
    0,                                              /* not an assert arm */
    2, 0, 0, 0,                                     /* instructions */
    2, 4, 3, 0, 0, 0, 1, 0, 0, 0,                   /* line 3: %1 = mul i64 */
    1, 4, 0, 0, 0, 0, 0, 0, 0, 0,                   /* %0 */
    2, 4, 6, 0, 0, 0, 0, 0, 0, 0,                   /* 6 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 255, 255, 255, 255, 0, 0, 0, 0,              /* no type, field 0 */
    0, 0, 0, 0,                                     /* no arguments */
    85, 4, 3, 0, 0, 0, 255, 255, 255, 255,          /* line 3: ret i64 */
    1, 4, 1, 0, 0, 0, 0, 0, 0, 0,                   /* %1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 255, 255, 255, 255, 0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,                                     /* classes */
};

static void writes_format(void)
{
    struct session s;
    struct text bytes = {0};
    size_t i;

    open_session(&s);
    build_library(&s, "scale", scale_source, &bytes);
    CHECK(bytes.length == sizeof scale_antl);
    for (i = 0; i < bytes.length && i < sizeof scale_antl; i++) {
        if ((uint8_t)bytes.data[i] != scale_antl[i]) {
            check_failures++;
            fprintf(stderr, "byte %zu is %u, expected %u\n", i,
                    (uint8_t)bytes.data[i], scale_antl[i]);
            break;
        }
    }
    text_free(&bytes);
    close_session(&s);
}

static const char vec_source[] = "pub struct V2 { x: int, y: int }\n"
                                 "struct Hidden { v: V2, next: ?*Hidden }\n"
                                 "pub const ORIGIN: V2 = V2 { x: 0, y: 0 };\n"
                                 "pub const NAME: str = \"vec\";\n"
                                 "pub const HALF: f64 = 0.5;\n"
                                 "pub fn make() -> ?*V2 {\n"
                                 "    return alloc(V2, 1);\n"
                                 "}\n"
                                 "pub fn hidden() -> ?*Hidden {\n"
                                 "    return none;\n"
                                 "}\n";

static const char shapes_source[] = "import vec;\n"
                                    "pub struct Box { corner: vec.V2 }\n"
                                    "pub fn make() -> ?*vec.V2 {\n"
                                    "    return vec.make();\n"
                                    "}\n";

/* Write a library, read it into a new session and write it again. */
static void round_trip(void)
{
    struct session a;
    struct session b;
    struct text first = {0};
    struct text second = {0};
    struct text ir_a = {0};
    struct text ir_b = {0};
    struct ir_module program;
    struct interface *vec;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "vec", vec_source, &first);

    open_session(&b);
    ir_module_init(&program, &b.arena, "vec");
    vec = antl_read((const uint8_t *)first.data, first.length, NULL, 0,
                    &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(vec != NULL);
    if (vec != NULL) {
        antl_write(&second, vec, &program, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
        CHECK_STR(vec->module, "vec");
        CHECK(vec->item_count == 6);
        ir_print_own(&ir_b, &program);
        CHECK_STR(text_cstr(&ir_b),
                  "type vec.V2 = struct { x: i64, y: i64 }\n"
                  "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
                  "type vec.Hidden = struct { v: vec.V2, next: ptr }\n"
                  "extern fn malloc(i64) -> ptr\n"
                  "global vec.V2.descriptor anti.rt.Descriptor { @vec.1, i64 "
                  "2, ptr 0, size_of vec.V2, i64 0, ptr 0, i64 2, "
                  "@vec.V2.fields, ptr 0, i64 0, i64 0, ptr 0, @vec.package.version, i64 5, ptr 0, i64 0, ptr 0 }\n"
                  "global vec.1 size 3 align 1 bytes 56 32 00\n"
                  "global vec.2 size 2 align 1 bytes 78 00\n"
                  "global vec.3 size 2 align 1 bytes 79 00\n"
                  "global vec.V2.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.2, i64 1, offset_of vec.V2.x, i64 6, i64 0, ptr 0 }, "
                  "anti.rt.Field { @vec.3, i64 1, offset_of vec.V2.y, i64 6, "
                  "i64 0, ptr 0 } }\n"
                  "global vec.package.version size 6 align 1 bytes 30 2e 30 2e 30 00\nglobal vec.Hidden.descriptor anti.rt.Descriptor { @vec.7, "
                  "i64 6, ptr 0, size_of vec.Hidden, i64 0, ptr 0, i64 2, "
                  "@vec.Hidden.fields, ptr 0, i64 0, i64 0, ptr 0, @vec.package.version, i64 5, ptr 0, i64 0, ptr 0 }\n"
                  "global vec.7 size 7 align 1 bytes 48 69 64 64 65 6e 00\n"
                  "global vec.8 size 2 align 1 bytes 76 00\n"
                  "global vec.9 size 5 align 1 bytes 6e 65 78 74 00\n"
                  "global vec.Hidden.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.8, i64 1, offset_of vec.Hidden.v, i64 21, i64 0, "
                  "@vec.V2.descriptor }, anti.rt.Field { @vec.9, i64 4, "
                  "offset_of vec.Hidden.next, i64 5393, i64 0, "
                  "@vec.Hidden.descriptor } }\n"
                  "fn vec.make() -> ptr {\n"
                  "b0:\n"
                  "    %0 = mul i64 1, size_of vec.V2\n"
                  "    %1 = call ptr @malloc(%0)\n"
                  "    ret ptr %1\n"
                  "}\n"
                  "fn vec.hidden() -> ptr {\n"
                  "b0:\n"
                  "    ret ptr 0\n"
                  "}\n");
    }
    text_free(&first);
    text_free(&second);
    text_free(&ir_a);
    text_free(&ir_b);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A library whose symbolic values and aggregates were made interleaved
   comes out of a read and a write as it went in. The descriptor of a
   class takes the size of the class before its field list makes the
   array that holds the records. */
static void round_trip_class(void)
{
    struct session a;
    struct session b;
    struct text first = {0};
    struct text second = {0};
    struct ir_module program;
    struct interface *iface;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "boxes",
                  "pub class Box { pub w: int = 1, pub h: int = 2 }\n",
                  &first);
    open_session(&b);
    ir_module_init(&program, &b.arena, "boxes");
    iface = antl_read((const uint8_t *)first.data, first.length, NULL, 0,
                      &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(iface != NULL);
    if (iface != NULL) {
        antl_write(&second, iface, &program, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
    }
    text_free(&first);
    text_free(&second);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A constant and an array length computed from size_of stay symbolic in
   the interface and in the IR of a library. A module that imports it uses
   them in its own types. */
static void keeps_symbolic_sizes(void)
{
    struct session a;
    struct session b;
    struct text first = {0};
    struct text second = {0};
    struct text ir = {0};
    struct ir_module program;
    struct ir_module reread;
    struct interface *sized;
    struct module *module;
    char error[160] = "";
    bool ok;

    open_session(&a);
    build_library(&a, "sized",
                  "pub struct H { tag: u8, n: i32 }\n"
                  "pub const N: int = size_of(H) * 2;\n"
                  "pub struct B { data: [N]byte }\n",
                  &first);
    open_session(&b);
    ir_module_init(&reread, &b.arena, "sized");
    sized = antl_read((const uint8_t *)first.data, first.length, NULL, 0,
                      &b.types, &b.arena, &reread, error, sizeof error);
    CHECK_STR(error, "");
    if (sized != NULL) {
        antl_write(&second, sized, &reread, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
        CHECK(sized->items[1]->value->kind == CONST_SYMBOLIC);
    }
    ir_module_init(&program, &b.arena, "main");
    b.libraries[b.library_count++] = sized;
    module = check_module(&b, "main",
                          "import sized;\n"
                          "fn main() -> int {\n"
                          "    let b = sized.B { data: [3; sized.N] };\n"
                          "    return b.data.len;\n"
                          "}\n",
                          &ok);
    CHECK(ok);
    if (ok) {
        lower_module(module, "main", &program, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type sized.H = struct { tag: i8, n: i32 }\n"
              "type [size_of(sized.H) * 2]byte = array "
              "mul i64(size_of sized.H, 2) of i8\n"
              "type sized.B = struct { data: [size_of(sized.H) * 2]byte }\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %0 = slot sized.B\n"
              "    %1 = copy i64 0\n"
              "    jump b1\n"
              "b1:\n"
              "    %2 = slt i8 %1, mul i64(size_of sized.H, 2)\n"
              "    branch %2, b2, b3\n"
              "b2:\n"
              "    %3 = mul i64 %1, size_of i8\n"
              "    %4 = ptradd %0, %3\n"
              "    store i8 3, %4\n"
              "    %5 = add i64 %1, 1\n"
              "    %1 = copy i64 %5\n"
              "    jump b1\n"
              "b3:\n"
              "    ret i64 mul i64(size_of sized.H, 2)\n"
              "}\n");
    text_free(&first);
    text_free(&second);
    text_free(&ir);
    ir_module_free(&program);
    ir_module_free(&reread);
    close_session(&b);
    close_session(&a);
}

/* Two modules may not export one name, because an export fn has the
   symbol of its name. */
static void unique_exports(void)
{
    struct session a;
    struct text bytes = {0};
    char error[160] = "";
    bool ok;
    struct ir_module program;

    open_session(&a);
    build_library(&a, "geo", "export fn dot(a: int) -> int { return a; }\n",
                  &bytes);
    check_module(&a, "main",
                 "import geo;\n"
                 "export fn dot(b: int) -> int { return b; }\n",
                 &ok);
    CHECK(!ok);
    CHECK(a.diags.count == 1);
    if (a.diags.count == 1) {
        CHECK_STR(a.diags.items[0].message,
                  "export fn `dot` has the symbol of export fn `dot` in "
                  "module `geo`");
    }
    ir_module_init(&program, &a.arena, "main");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &a.types, &a.arena, &program, error, sizeof error) != NULL);
    CHECK(program.function_count == 1 && program.functions[0]->exported);
    ir_module_free(&program);
    text_free(&bytes);
    close_session(&a);
}

static void path_of(const char *source, const char *const *roots,
                    size_t count, const char *expected)
{
    struct text out = {0};
    char error[160] = "";

    if (!modpath_of_source(source, roots, count, &out, error,
                               sizeof error)) {
        CHECK_STR(error, expected);
    } else {
        CHECK_STR(text_cstr(&out), expected);
    }
    text_free(&out);
}

/* The module path of a source file is its path under the search root that
   holds it, with dots for slashes. Outside every root it is the file name
   alone. */
static void module_paths(void)
{
    static const char *const roots[] = {"lib", "src/"};

    path_of("src/com/niese/geo.anti", roots, 2, "com.niese.geo");
    path_of("lib/anti/text.anti", roots, 2, "anti.text");
    path_of("tools/geo.anti", roots, 2, "geo");
    path_of("geo.anti", NULL, 0, "geo");
    path_of("src/com/my-lib/geo.anti", roots, 2,
            "`my-lib` in src/com/my-lib/geo.anti is not a lowercase "
            "identifier");
    path_of("src/Com/geo.anti", roots, 2,
            "`Com` in src/Com/geo.anti is not a lowercase identifier");
    path_of("src/com/fn/geo.anti", roots, 2,
            "`fn` in src/com/fn/geo.anti is a keyword");
    CHECK(modpath_reserved("anti"));
    CHECK(modpath_reserved("anti.text"));
    CHECK(!modpath_reserved("antique.text"));
    CHECK(modpath_segments("com.niese.geo") == 3);
    CHECK_STR(modpath_last("com.niese.geo"), "geo");
}

/* An import names the full module path. The last segment, or the name
   after as, is the name in the importing module. */
static void dotted_imports(void)
{
    struct session s;
    bool ok;

    open_session(&s);
    library(&s, "com.example.scale", "pub fn scale(x: int) -> int { return x; }\n");
    library(&s, "org.other.scale", "pub fn half(x: int) -> int { return x; }\n");
    check_module(&s, "main",
                 "import com.example.scale;\n"
                 "import org.other.scale as other;\n"
                 "fn main() -> int { return scale.scale(2) + other.half(4); }\n",
                 &ok);
    CHECK(ok);
    CHECK(s.diags.count == 0);
    check_module(&s, "main",
                 "import com.example.scale;\n"
                 "import org.other.scale;\n",
                 &ok);
    CHECK(!ok && s.diags.count == 1 &&
          strcmp(s.diags.items[0].message, "`scale` is already declared") == 0);
    diagnostics_free(&s.diags);
    memset(&s.diags, 0, sizeof s.diags);
    check_module(&s, "main", "import com.Example.scale;\n", &ok);
    CHECK(!ok && s.diags.count == 1 &&
          strcmp(s.diags.items[0].message,
                 "the module path `com.Example.scale` is not lowercase") == 0);
    close_session(&s);
}

/* Write a library of source with the given package header. */
static void write_with(struct session *s, const char *source,
                       const struct package *package, bool strip,
                       struct text *bytes)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    struct ir_module ir;
    struct module *module;
    bool ok;

    module = check_module(s, "com.example.geo", source, &ok);
    ir_module_init(&ir, &s->arena, "com.example.geo");
    CHECK(ok);
    if (ok) {
        lower_module(module, "com.example.geo", &ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    sema_interface(module, "com.example.geo", &s->arena, iface);
    if (package != NULL) {
        iface->package = *package;
    }
    antl_write(bytes, iface, &ir, strip);
    ir_module_free(&ir);
}

/* DESIGN: the three sources below put every item on the same line,
   because a library file records the line of every statement. The blank
   lines are what makes the files comparable byte for byte: what is
   compared is then the doc text alone. */
static const char line_docs[] =
    "//! Plane geometry.\n"                            /* 1 */
    "//#! Built for the tests.\n"                      /* 2 */
    "\n\n\n\n\n\n"                                     /* 3 to 8 */
    "/// A point.\n"                                   /* 9 */
    "///\n"                                            /* 10 */
    "///     Two fields.\n"                            /* 11 */
    "pub struct Point {\n"                             /* 12 */
    "\n\n"                                             /* 13, 14 */
    "    /// Across.\n"                                /* 15 */
    "    x: f64,\n"                                    /* 16 */
    "    y: f64,\n"                                    /* 17 */
    "}\n"                                              /* 18 */
    "\n\n\n\n"                                         /* 19 to 22 */
    "/// Dot product.\n"                               /* 23 */
    "//# Not a note in the file.\n"                    /* 24 */
    "pub fn dot(a: Point, b: Point) -> f64 { return a.x * b.x + a.y * b.y; }\n"
    "\n\n"                                             /* 26, 27 */
    "/// Private, not in the file.\n"                  /* 28 */
    "fn hidden() -> f64 { return 1.0; }\n";            /* 29 */

static const char block_docs[] =
    "/" "*!\n"
    "    Plane geometry.\n"
    "*/\n"
    "/" "*#!\n"
    "    Built for the tests.\n"
    "*/\n"
    "/" "**\n"
    "  A point.\n"
    "\n"
    "      Two fields.\n"
    "*/\n"
    "pub struct Point {\n"
    "    /" "**\n"
    "        Across.\n"
    "    *" "/\n"
    "    x: f64,\n"
    "    y: f64,\n"
    "}\n"
    "/" "** \n"
    "Dot product.\n"
    "*/\n"
    "/" "*#\n"
    "  Not a note in the file.\n"
    "*/\n"
    "pub fn dot(a: Point, b: Point) -> f64 { return a.x * b.x + a.y * b.y; }\n"
    "/" "**\n"
    "  Private, not in the file.\n"
    "*/\n"
    "fn hidden() -> f64 { return 1.0; }\n";

static const char no_docs[] =
    "\n\n\n\n\n\n\n\n\n\n\n"                              /* 1 to 11 */
    "pub struct Point {\n"                             /* 12 */
    "\n\n\n"                                           /* 13 to 15 */
    "    x: f64,\n"                                    /* 16 */
    "    y: f64,\n"                                    /* 17 */
    "}\n"                                              /* 18 */
    "\n\n\n\n\n\n"                                     /* 19 to 24 */
    "pub fn dot(a: Point, b: Point) -> f64 { return a.x * b.x + a.y * b.y; }\n"
    "\n\n\n"                                           /* 26 to 28 */
    "fn hidden() -> f64 { return 1.0; }\n";            /* 29 */

/* The package header and the doc text of the public interface. The line
   and the block form of the comments write the same bytes. Without doc
   text a file equals the file of the source without comments. */
static void package_and_docs(void)
{
    struct session a;
    struct session b;
    struct text lines = {0};
    struct text blocks = {0};
    struct text stripped = {0};
    struct text bare = {0};
    struct interface header;
    struct interface *read;
    struct ir_module program;
    struct package package;
    static struct package_dependency deps[1] = {
        {"com.example.vec", "^1.2", "https://anti.example.com/repo"}};
    static const char *const attribution[2] = {"Copyright 2026 Example",
                                               "Contains code from Other"};
    char error[160] = "";

    memset(&package, 0, sizeof package);
    package.name = "com.example";
    package.version = "1.2.4";
    package.dependencies = deps;
    package.dependency_count = 1;
    package.license = "MIT";
    package.license_text = "Permission is hereby granted.\n";
    package.attribution = attribution;
    package.attribution_count = 2;
    open_session(&a);
    write_with(&a, line_docs, &package, false, &lines);
    write_with(&a, block_docs, &package, false, &blocks);
    write_with(&a, line_docs, &package, true, &stripped);
    write_with(&a, no_docs, &package, false, &bare);
    CHECK(lines.length == blocks.length &&
          memcmp(lines.data, blocks.data, lines.length) == 0);
    CHECK(stripped.length == bare.length &&
          memcmp(stripped.data, bare.data, bare.length) == 0);
    CHECK(antl_header((const uint8_t *)lines.data, lines.length, &a.arena,
                      &header, error, sizeof error));
    CHECK_STR(error, "");
    CHECK_STR(header.module, "com.example.geo");
    CHECK_STR(header.package.name, "com.example");
    CHECK_STR(header.package.version, "1.2.4");
    CHECK(header.package.dependency_count == 1);
    if (header.package.dependency_count == 1) {
        CHECK_STR(header.package.dependencies[0].url,
                  "https://anti.example.com/repo");
    }
    CHECK_STR(header.package.license, "MIT");
    CHECK_STR(header.package.license_text, "Permission is hereby granted.\n");
    CHECK(header.package.attribution_count == 2);
    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    read = antl_read((const uint8_t *)lines.data, lines.length, NULL, 0,
                     &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    if (read != NULL) {
        CHECK_STR(read->doc, "Plane geometry.");
        CHECK(read->item_count == 2);
        CHECK_STR(read->items[0]->doc.text, "A point.\n\n    Two fields.");
        CHECK_STR(read->items[0]->type->fields[0].doc.text, "Across.");
        CHECK(read->items[0]->type->fields[1].doc.length == 0);
        CHECK_STR(read->items[1]->doc.text, "Dot product.");
        CHECK(read->items[1]->params != NULL &&
              read->items[1]->params[0].length == 1 &&
              read->items[1]->params[1].length == 1 &&
              read->items[1]->params[0].text[0] == 'a' &&
              read->items[1]->params[1].text[0] == 'b');
    }
    ir_module_free(&program);
    close_session(&b);
    text_free(&lines);
    text_free(&blocks);
    text_free(&stripped);
    text_free(&bare);
    close_session(&a);
}

/* A private struct that a pub item reaches keeps its fields and loses the
   /// text of them, like every other private item. */
static void private_field_docs(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct interface *read;
    struct ir_module program;
    char error[160] = "";

    open_session(&a);
    write_with(&a,
               "struct Inner {\n"
               "    /// Private, not in the file.\n"
               "    v: int,\n"
               "}\n"
               "pub fn make() -> Inner { return Inner { v: 1 }; }\n",
               NULL, false, &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    read = antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                     &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(read != NULL && read->item_count == 1);
    if (read != NULL && read->item_count == 1) {
        const struct type *inner = read->items[0]->type->result;
        CHECK(inner->field_count == 1);
        CHECK(inner->field_count == 1 && inner->fields[0].doc.length == 0);
    }
    ir_module_free(&program);
    close_session(&b);
    text_free(&bytes);
    close_session(&a);
}

/* A library keeps how each narrow parameter extends to 32 bits. */
static void keeps_extensions(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "narrow",
                  "extern fn putb(b: u8) -> i32;\n"
                  "pub fn pick(a: i8, b: u16, c: bool, d: i32) -> i32 {\n"
                  "    return putb(1);\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "narrow");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "extern fn putb(i8 zeroext) -> i32\n"
              "fn narrow.pick(%0: i8 signext, %1: i16 zeroext, %2: i8 zeroext, "
              "%3: i32) -> i32 {\n"
              "b0:\n"
              "    %4 = call i32 @putb(1)\n"
              "    ret i32 %4\n"
              "}\n");
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* The C function name of program, or NULL. */
static const struct ir_function *c_function(const struct ir_module *program,
                                            const char *name)
{
    size_t i;

    for (i = 0; i < program->function_count; i++) {
        const struct ir_function *f = program->functions[i];
        if (f->module == NULL && strcmp(f->name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

/* A library keeps the memory effects of a runtime function it calls. A
   second library that declares the same function without them leaves the
   program with the weaker of the two, as for a function that never
   returns. */
static void keeps_effects(void)
{
    struct session a;
    struct session b;
    struct session c;
    struct text same = {0};
    struct text mine = {0};
    struct text ir = {0};
    struct ir_module program;
    const struct ir_function *f;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "same",
                  "pub fn eq(a: str, b: str) -> bool {\n"
                  "    return a == b;\n"
                  "}\n",
                  &same);
    open_session(&b);
    build_library(&b, "mine",
                  "extern fn anti_rt_same_bytes(a: ?*byte, an: int, "
                  "b: ?*byte, bn: int) -> c_int;\n"
                  "pub fn eq(a: str, b: str) -> bool {\n"
                  "    return anti_rt_same_bytes(a.ptr, a.len, b.ptr, "
                  "b.len) != 0;\n"
                  "}\n",
                  &mine);
    open_session(&c);
    ir_module_init(&program, &c.arena, "main");
    CHECK(antl_read((const uint8_t *)same.data, same.length, NULL, 0,
                    &c.types, &c.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    f = c_function(&program, "anti_rt_same_bytes");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(f->effects == IR_EFFECTS_READS_ARGS);
        CHECK(f->guarantees == (IR_WILLRETURN | IR_NOSYNC | IR_NOFREE));
    }
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir),
                 "extern fn anti_rt_same_bytes(ptr, i64, ptr, i64) -> i32 "
                 "effects reads willreturn nosync nofree\n") != NULL);
    CHECK(antl_read((const uint8_t *)mine.data, mine.length, NULL, 0,
                    &c.types, &c.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    f = c_function(&program, "anti_rt_same_bytes");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(f->effects == IR_EFFECTS_ANY);
        CHECK(f->guarantees == 0);
    }
    text_free(&same);
    text_free(&mine);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&c);
    close_session(&b);
    close_session(&a);
}

/* A library keeps the globals of its literals, and a module that imports
   it numbers its own literals from 0. */
static void keeps_literals(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    open_session(&a);
    build_library(&a, "words",
                  "pub fn hi() -> str {\n"
                  "    return \"hi\";\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    b.libraries[b.library_count++] =
        antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                  &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    module = check_module(&b, "main",
                          "import words;\n"
                          "fn main() -> int {\n"
                          "    return words.hi().len + \"hi\".len;\n"
                          "}\n",
                          &ok);
    CHECK(ok);
    if (ok) {
        lower_module(module, "main", &program, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type str = struct { ptr: ptr, len: i64 }\n"
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64) -> never\n"
              "global words.0 size 3 align 1 bytes 68 69 00\n"
              "global main.0 size 3 align 1 bytes 68 69 00\n"
              "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "fn words.hi() -> agg str {\n"
              "b0:\n"
              "    %0 = slot str\n"
              "    %1 = addr @words.0\n"
              "    store ptr %1, %0\n"
              "    %2 = ptradd %0, offset_of str.len\n"
              "    store i64 2, %2\n"
              "    ret ptr %0\n"
              "}\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %3 = slot str\n"
              "    %0 = call agg @words.hi()\n"
              "    %1 = ptradd %0, offset_of str.len\n"
              "    %2 = load i64 %1\n"
              "    %4 = addr @main.0\n"
              "    store ptr %4, %3\n"
              "    %5 = ptradd %3, offset_of str.len\n"
              "    store i64 2, %5\n"
              "    %6 = ptradd %3, offset_of str.len\n"
              "    %7 = load i64 %6\n"
              "    %8 = addov i64 %2, %7\n"
              "    branchov %8, b1, b2\n"
              "b1:\n"
              "    %9 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%9, 21, 1, %2, %7)\n"
              "    unreachable\n"
              "b2:\n"
              "    ret i64 %8\n"
              "}\n");
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A library keeps the value of an aggregate constant, which the back end
   turns into bytes for its target. */
static void keeps_constants(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    open_session(&a);
    build_library(&a, "pair",
                  "pub struct Pair { a: u8, b: i32 }\n"
                  "const BASE: Pair = Pair { a: 7, b: 11 };\n"
                  "pub fn base() -> Pair {\n"
                  "    return BASE;\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    b.libraries[b.library_count++] =
        antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                  &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    module = check_module(&b, "main",
                          "import pair;\n"
                          "fn main() -> int {\n"
                          "    return pair.base().b as int;\n"
                          "}\n",
                          &ok);
    CHECK(ok);
    if (ok) {
        lower_module(module, "main", &program, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir),
                 "global pair.6 pair.Pair { i8 7, i32 11 }\n") != NULL);
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* An f16 constant and a struct of f16 fields cross a library file. The
   constant is its value, which the importer writes as its bits. */
static void keeps_halves(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    open_session(&a);
    build_library(&a, "tone",
                  "pub struct Texel { u: f16, v: f16 }\n"
                  "pub const ONE: f16 = 1.0 as f16;\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    b.libraries[b.library_count++] =
        antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                  &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    module = check_module(&b, "main",
                          "import tone;\n"
                          "fn main() -> int {\n"
                          "    let t = tone.Texel { u: tone.ONE as f16,"
                          " v: 0.5 as f16 };\n"
                          "    return (t.u + t.v) as int;\n"
                          "}\n",
                          &ok);
    CHECK(ok);
    if (ok) {
        lower_module(module, "main", &program, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir), "hext f32 15360\n") != NULL);
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A call through a function pointer keeps its signature in the library
   file. */
/* A C function with an aggregate result keeps its aggregate in a library
   file, as a function of the module does. */
static void keeps_extern_aggregates(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "wrap",
                  "extern fn make() -> str;\n"
                  "pub fn wrap() -> str {\n"
                  "    return make();\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "wrap");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    CHECK(program.function_count == 2 &&
          program.functions[0]->result_agg != IR_NO_AGG);
    if (program.function_count == 2 &&
        program.functions[0]->result_agg != IR_NO_AGG) {
        ir_print(&ir, &program);
        CHECK_STR(text_cstr(&ir),
                  "type str = struct { ptr: ptr, len: i64 }\n"
                  "extern fn make() -> agg str\n"
                  "fn wrap.wrap() -> agg str {\n"
                  "b0:\n"
                  "    %0 = call agg @make()\n"
                  "    ret ptr %0\n"
                  "}\n");
    }
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A parameter that does not keep its argument crosses the library file
   in its form of two words, `concurrent` included. The module that loads
   it passes the code and the context. */
static void keeps_context_signatures(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";
    const struct interface *lib;
    const struct type *f = NULL;

    open_session(&a);
    build_library(&a, "calls",
                  "pub fn run(concurrent f: fn(i16) -> int) -> int {\n"
                  "    return f(1);\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "calls");
    lib = antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error, sizeof error);
    CHECK(lib != NULL);
    CHECK_STR(error, "");
    if (lib != NULL && lib->item_count == 1 &&
        lib->items[0]->type->param_count == 1) {
        f = lib->items[0]->type->params[0];
    }
    CHECK(f != NULL && f->context && f->concurrent);
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type fn(...) = struct { code: ptr, context: ptr }\n"
              "extern fn calls.fn.0(i16 signext, ptr) -> i64\n"
              "fn calls.run(%0: ptr, %1: ptr) -> i64 {\n"
              "b0:\n"
              "    %2 = slot fn(...)\n"
              "    store ptr %0, %2\n"
              "    %3 = ptradd %2, offset_of fn(...).context\n"
              "    store ptr %1, %3\n"
              "    %4 = load ptr %2\n"
              "    %5 = ptradd %2, offset_of fn(...).context\n"
              "    %6 = load ptr %5\n"
              "    %7 = call i64 %4 via @calls.fn.0(1, %6)\n"
              "    ret i64 %7\n"
              "}\n");
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

static void keeps_signatures(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    open_session(&a);
    build_library(&a, "calls",
                  "pub fn run(keep f: fn(i16) -> int) -> int {\n"
                  "    return f(1);\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "calls");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "extern fn calls.fn.0(i16 signext) -> i64\n"
              "fn calls.run(%0: ptr) -> i64 {\n"
              "b0:\n"
              "    %1 = call i64 %0 via @calls.fn.0(1)\n"
              "    ret i64 %1\n"
              "}\n");
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* The generic named name among the generics of lib, or NULL. */
static const struct item *generic_of(const struct interface *lib,
                                     const char *name)
{
    size_t i;

    for (i = 0; lib != NULL && i < lib->generic_count; i++) {
        const struct item *it = lib->generics[i];
        if (it->name.length == strlen(name) &&
            memcmp(it->name.text, name, it->name.length) == 0) {
            return it;
        }
    }
    return NULL;
}

/* Every mark the checker leaves on a generic survives the library file.
   The declaration and its symbol keep `operator`, a parameter keeps
   `lent`, and the receiver of a nested hash call keeps `prechecked`. A
   copy hands that receiver to the checker again as it stands. */
static void keeps_generic_marks(void)
{
    struct session a;
    struct session b;
    struct text bytes = {0};
    struct ir_module program;
    char error[160] = "";
    const struct interface *lib;
    const struct item *eq;
    const struct item *peek;
    const struct item *mixed;
    const struct expr *call = NULL;
    const struct expr *at = NULL;

    open_session(&a);
    build_library(&a, "marks",
                  "pub struct Box<T> { v: T }\n"
                  "pub operator fn eq<T: eq>(a: Box<T>, b: Box<T>) -> bool {\n"
                  "    return a.v == b.v;\n"
                  "}\n"
                  "pub fn peek<T>(lent p: *T) -> T {\n"
                  "    return *p;\n"
                  "}\n"
                  "pub struct Name { n: int }\n"
                  "pub operator fn hash(a: Name) -> u64 {\n"
                  "    return a.n as u64;\n"
                  "}\n"
                  "pub fn mixed<T: hash>(own v: T) -> u64 {\n"
                  "    return (v, Name { n: 1 }).hash();\n"
                  "}\n",
                  &bytes);
    open_session(&b);
    ir_module_init(&program, &b.arena, "marks");
    lib = antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error, sizeof error);
    CHECK(lib != NULL);
    CHECK_STR(error, "");
    eq = generic_of(lib, "eq");
    CHECK(eq != NULL && eq->is_operator && eq->symbol->is_operator);
    peek = generic_of(lib, "peek");
    CHECK(peek != NULL && peek->param_count == 1 && peek->params[0].lent);
    mixed = generic_of(lib, "mixed");
    if (mixed != NULL && mixed->body != NULL && mixed->body->count == 1 &&
        mixed->body->stmts[0]->kind == STMT_RETURN) {
        call = mixed->body->stmts[0]->as.return_value;
    }
    /* The hash of the `Name` in the tuple is `hash(*hole)`. */
    if (call != NULL && call->kind == EXPR_CALL &&
        call->as.call.hash_count == 1 &&
        call->as.call.hash_calls[0]->as.call.arg_count == 1) {
        at = call->as.call.hash_calls[0]->as.call.args[0];
    }
    CHECK(at != NULL && at->kind == EXPR_UNARY &&
          at->as.unary.operand->kind == EXPR_NONE);
    CHECK(at != NULL && at->prechecked && at->as.unary.operand->prechecked);
    text_free(&bytes);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

/* A struct keeps one identity when two libraries mention it. */
static void dependencies(void)
{
    struct session a;
    struct session b;
    struct text vec_bytes = {0};
    struct text shapes_bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    const struct interface *libs[2];
    char error[160] = "";

    open_session(&a);
    build_library(&a, "vec", vec_source, &vec_bytes);
    build_library(&a, "shapes", shapes_source, &shapes_bytes);

    open_session(&b);
    ir_module_init(&program, &b.arena, "main");
    CHECK(antl_read((const uint8_t *)shapes_bytes.data, shapes_bytes.length,
                    NULL, 0, &b.types, &b.arena, &program, error,
                    sizeof error) == NULL);
    CHECK_STR(error, "needs module `vec`");
    libs[0] = antl_read((const uint8_t *)vec_bytes.data, vec_bytes.length,
                        NULL, 0, &b.types, &b.arena, &program, error,
                        sizeof error);
    CHECK(libs[0] != NULL);
    libs[1] = antl_read((const uint8_t *)shapes_bytes.data,
                        shapes_bytes.length, libs, 1, &b.types, &b.arena,
                        &program, error, sizeof error);
    CHECK(libs[1] != NULL);
    if (libs[0] != NULL && libs[1] != NULL) {
        const struct type *v2 = libs[0]->items[0]->type;
        const struct type *box = libs[1]->items[0]->type;
        CHECK(box->kind == TYPE_STRUCT && box->field_count == 1 &&
              box->fields[0].type == v2);
        ir_print_own(&ir, &program);
        CHECK_STR(text_cstr(&ir),
                  "type vec.V2 = struct { x: i64, y: i64 }\n"
                  "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
                  "type vec.Hidden = struct { v: vec.V2, next: ptr }\n"
                  "type shapes.Box = struct { corner: vec.V2 }\n"
                  "type [1]anti.rt.Field = array 1 of anti.rt.Field\n"
                  "extern fn malloc(i64) -> ptr\n"
                  "global vec.V2.descriptor anti.rt.Descriptor { @vec.1, i64 "
                  "2, ptr 0, size_of vec.V2, i64 0, ptr 0, i64 2, "
                  "@vec.V2.fields, ptr 0, i64 0, i64 0, ptr 0, @vec.package.version, i64 5, ptr 0, i64 0, ptr 0 }\n"
                  "global vec.1 size 3 align 1 bytes 56 32 00\n"
                  "global vec.2 size 2 align 1 bytes 78 00\n"
                  "global vec.3 size 2 align 1 bytes 79 00\n"
                  "global vec.V2.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.2, i64 1, offset_of vec.V2.x, i64 6, i64 0, ptr 0 }, "
                  "anti.rt.Field { @vec.3, i64 1, offset_of vec.V2.y, i64 6, "
                  "i64 0, ptr 0 } }\n"
                  "global vec.package.version size 6 align 1 bytes 30 2e 30 2e 30 00\nglobal vec.Hidden.descriptor anti.rt.Descriptor { @vec.7, "
                  "i64 6, ptr 0, size_of vec.Hidden, i64 0, ptr 0, i64 2, "
                  "@vec.Hidden.fields, ptr 0, i64 0, i64 0, ptr 0, @vec.package.version, i64 5, ptr 0, i64 0, ptr 0 }\n"
                  "global vec.7 size 7 align 1 bytes 48 69 64 64 65 6e 00\n"
                  "global vec.8 size 2 align 1 bytes 76 00\n"
                  "global vec.9 size 5 align 1 bytes 6e 65 78 74 00\n"
                  "global vec.Hidden.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.8, i64 1, offset_of vec.Hidden.v, i64 21, i64 0, "
                  "@vec.V2.descriptor }, anti.rt.Field { @vec.9, i64 4, "
                  "offset_of vec.Hidden.next, i64 5393, i64 0, "
                  "@vec.Hidden.descriptor } }\n"
                  "global shapes.Box.descriptor anti.rt.Descriptor { "
                  "@shapes.1, i64 3, ptr 0, size_of shapes.Box, i64 0, ptr 0, "
                  "i64 1, @shapes.Box.fields, ptr 0, i64 0, i64 0, ptr 0, @shapes.package.version, i64 5, ptr 0, i64 0, ptr 0 }\n"
                  "global shapes.1 size 4 align 1 bytes 42 6f 78 00\n"
                  "global shapes.2 size 7 align 1 bytes 63 6f 72 6e 65 72 00\n"
                  "global vec.V2.descriptor size 0 align 1 bytes\n"
                  "global shapes.Box.fields [1]anti.rt.Field { anti.rt.Field { "
                  "@shapes.2, i64 6, offset_of shapes.Box.corner, i64 21, i64 "
                  "0, @vec.V2.descriptor } }\n"
                  "global shapes.package.version size 6 align 1 bytes 30 2e 30 2e 30 00\nfn vec.make() -> ptr {\n"
                  "b0:\n"
                  "    %0 = mul i64 1, size_of vec.V2\n"
                  "    %1 = call ptr @malloc(%0)\n"
                  "    ret ptr %1\n"
                  "}\n"
                  "fn vec.hidden() -> ptr {\n"
                  "b0:\n"
                  "    ret ptr 0\n"
                  "}\n"
                  "fn shapes.make() -> ptr {\n"
                  "b0:\n"
                  "    %0 = call ptr @vec.make()\n"
                  "    ret ptr %0\n"
                  "}\n");
    }
    text_free(&vec_bytes);
    text_free(&shapes_bytes);
    text_free(&ir);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

static void refuses_file(const uint8_t *data, size_t size,
                         const char *expected)
{
    struct session s;
    struct ir_module program;
    char error[160] = "";

    open_session(&s);
    ir_module_init(&program, &s.arena, "main");
    CHECK(antl_read(data, size, NULL, 0, &s.types, &s.arena, &program, error,
                    sizeof error) == NULL);
    if (expected != NULL) {
        CHECK_STR(error, expected);
    } else {
        CHECK(error[0] != '\0');
    }
    ir_module_free(&program);
    close_session(&s);
}

static bool reads_file(const struct text *bytes);

static void damaged_files(void)
{
    /* Where the fields a poke below reaches sit, counted back from the
       end of the file. First the classes, two instructions of 53 bytes
       each and the 15 bytes that open the body. Then the one parameter of
       the signature with its facts, its count, the source of the function
       and its result with its extension. */
    enum {
        TAIL = 4 + 2 * 53 + 15,
        MUL_OPERAND = 4 + 2 * 53 - 12,
        PARAM_FACTS = TAIL + 4 + 1,
        PARAM_DEREF = TAIL + 4,
        PARAM_EXT = PARAM_FACTS + 4 + 1,
        RESULT_EXT = TAIL + 11 + 4 + 8 + 1,
        RESULT_AGG = RESULT_EXT + 4,
        /* The memory effects of the function, before its module and its
           name, each a count and five bytes, and its result type. */
        FN_EFFECTS = RESULT_AGG + 1 + 2 * 9 + 1,
        /* The flags, the last byte of type 1, which starts at byte 72
           after the header and the one byte of type 0. */
        FN_FLAGS = 85
    };
    uint8_t copy[sizeof scale_antl];
    struct text good = {0};
    size_t n;

    memcpy(copy, scale_antl, sizeof copy);
    copy[4] = 80;
    refuses_file(copy, sizeof copy,
                 "has format version 80, and antic reads version 79");
    memcpy(copy, scale_antl, sizeof copy);
    copy[3] = 'X';
    refuses_file(copy, sizeof copy, "is not a library file");
    /* Every shorter file fails without reading past its end. */
    for (n = 8; n < sizeof scale_antl; n++) {
        refuses_file(scale_antl, n, NULL);
    }
    /* The result type agg needs an aggregate of the type table. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - RESULT_AGG] = IR_AGG;
    refuses_file(copy, sizeof copy, NULL);
    /* The memory effects take two bits of a class and three of the
       guarantees, and no more. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - FN_EFFECTS] = IR_EFFECTS_READS_ARGS |
                                     IR_GUARANTEES << 2;
    text_append_bytes(&good, (const char *)copy, sizeof copy);
    CHECK(reads_file(&good));
    text_free(&good);
    copy[sizeof copy - FN_EFFECTS] = 8 << 2;
    refuses_file(copy, sizeof copy, NULL);
    /* The flags of a function, right before its effects, take seven
       bits. The sixth marks that it writes tables, and the seventh that
       it allocates, which a C function alone does. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - FN_EFFECTS - 1] |= 32;
    text_append_bytes(&good, (const char *)copy, sizeof copy);
    CHECK(reads_file(&good));
    text_free(&good);
    copy[sizeof copy - FN_EFFECTS - 1] |= 64;
    refuses_file(copy, sizeof copy, NULL);
    copy[sizeof copy - FN_EFFECTS - 1] = 128;
    refuses_file(copy, sizeof copy, NULL);
    /* Only a parameter or a result of 8 or 16 bits extends. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - PARAM_EXT] = IR_EXT_SIGN;
    refuses_file(copy, sizeof copy, NULL);
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - RESULT_EXT] = IR_EXT_SIGN;
    refuses_file(copy, sizeof copy, NULL);
    /* The facts of a pointer stand on a pointer alone, and a
       dereferenceable size needs nonnull. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - PARAM_FACTS] = 1;
    refuses_file(copy, sizeof copy, NULL);
    memcpy(copy, scale_antl, sizeof copy);
    memset(copy + sizeof copy - PARAM_DEREF, 0, 4);
    refuses_file(copy, sizeof copy, NULL);
    /* The temporary of the mul instruction points past the temporaries. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - MUL_OPERAND] = 9;
    refuses_file(copy, sizeof copy, NULL);
    /* The flags of the function type: `-> never` with `may fail`, which
       a function that never returns cannot be, and an out pointer without
       `may fail`. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[FN_FLAGS] = 128 | 4;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    /* `own fn` without the form of two words and `concurrent`, which it
       always stands with. */
    copy[FN_FLAGS] = 64;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    copy[FN_FLAGS] = 64 | 16;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    /* `concurrent` without the form of two words, and that form on a
       bound function. */
    copy[FN_FLAGS] = 32;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    copy[FN_FLAGS] = 18;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    copy[FN_FLAGS] = 8;
    refuses_file(copy, sizeof copy, "is damaged at byte 86");
    /* S16: a body without a block. The count of blocks follows the
       temporaries, and the classes follow the blocks. */
    n = sizeof scale_antl - TAIL + 6;
    memcpy(copy, scale_antl, n);
    memset(copy + n, 0, 4);
    memcpy(copy + n + 4, scale_antl + sizeof scale_antl - 4, 4);
    refuses_file(copy, n + 8, NULL);
    /* A block whose failure kind names nothing. The kind opens the block,
       after the temporaries and the count of blocks. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - TAIL + 10] = IR_FAIL_GUARD + 1;
    refuses_file(copy, sizeof copy, NULL);
    /* A module path with a NUL inside, which a C string would cut to
       `sc`. The path starts at byte 46. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[48] = '\0';
    refuses_file(copy, sizeof copy, NULL);
}

/* A file writes a name as a 32-bit length and the bytes. This is the
   offset past the first copy of name, or past the last when last is set.
   SIZE_MAX when there is none. */
static size_t name_end(const struct text *bytes, const char *name, bool last)
{
    size_t n = strlen(name);
    size_t found = SIZE_MAX;
    size_t i;

    for (i = 0; i + 4 + n <= bytes->length; i++) {
        const uint8_t *p = (const uint8_t *)bytes->data + i;
        if (p[0] == n && p[1] == 0 && p[2] == 0 && p[3] == 0 &&
            memcmp(p + 4, name, n) == 0) {
            found = i + 4 + n;
            if (!last) {
                break;
            }
        }
    }
    CHECK(found != SIZE_MAX);
    return found;
}

/* The unsigned 32-bit number at byte at. */
static uint32_t u32_at(const struct text *bytes, size_t at)
{
    const uint8_t *p = (const uint8_t *)bytes->data + at;

    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static bool reads_file(const struct text *bytes)
{
    struct session s;
    struct ir_module program;
    char error[160] = "";
    bool ok;

    open_session(&s);
    ir_module_init(&program, &s.arena, "main");
    ok = antl_read((const uint8_t *)bytes->data, bytes->length, NULL, 0, &s.types, &s.arena,
                   &program, error, sizeof error) != NULL;
    if (!ok) {
        fprintf(stderr, "  %s\n", error);
    }
    ir_module_free(&program);
    close_session(&s);
    return ok;
}

/* The file with the count bytes of value written at byte at is refused. */
static void refuses_poke(const struct text *bytes, size_t at,
                         const void *value, size_t count)
{
    uint8_t *copy;

    if (at == SIZE_MAX || at + count > bytes->length) {
        check_failures++;
        fprintf(stderr, "no place to poke at %zu\n", at);
        return;
    }
    copy = alloc_zeroed(bytes->length, 1);
    memcpy(copy, bytes->data, bytes->length);
    memcpy(copy + at, value, count);
    refuses_file(copy, bytes->length, NULL);
    free(copy);
}

/* The value of a class literal that defaults a field: its base is a value
   of the base, and a field the literal leaves out is CONST_DEFAULT. The
   file reads back to the same bytes. A CONST_DEFAULT in place of the base,
   or as the whole default, is refused. */
static void class_literal_defaults(void)
{
    /* Box { w: 3 }: the struct of three fields, the base Object with its
       table left out, w as the integer 3 and h left out. */
    static const uint8_t value[] = {
        CONST_STRUCT, 3, 0, 0, 0,
        CONST_STRUCT, 1, 0, 0, 0, CONST_DEFAULT,
        CONST_INT, 3, 0, 0, 0, 0, 0, 0, 0,
        CONST_DEFAULT,
    };
    static const uint8_t left_out = CONST_DEFAULT;
    struct session a;
    struct session b;
    struct text first = {0};
    struct text second = {0};
    struct ir_module program;
    struct interface *iface;
    char error[160] = "";
    size_t at = SIZE_MAX;
    size_t i;

    open_session(&a);
    build_library(&a, "boxes",
                  "pub class Box { pub w: int = 1, pub h: int = 2 }\n"
                  "pub class Shelf { pub box: Box = Box { w: 3 }, "
                  "pub n: int = 4 }\n",
                  &first);
    for (i = 0; i + sizeof value <= first.length; i++) {
        if (memcmp(first.data + i, value, sizeof value) == 0) {
            at = i;
        }
    }
    CHECK(at != SIZE_MAX);
    open_session(&b);
    ir_module_init(&program, &b.arena, "boxes");
    iface = antl_read((const uint8_t *)first.data, first.length, NULL, 0,
                      &b.types, &b.arena, &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(iface != NULL);
    if (iface != NULL) {
        antl_write(&second, iface, &program, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
    }
    if (at != SIZE_MAX) {
        refuses_poke(&first, at + 5, &left_out, 1);
        refuses_poke(&first, at, &left_out, 1);
    }
    text_free(&first);
    text_free(&second);
    ir_module_free(&program);
    close_session(&b);
    close_session(&a);
}

static void poke_u32(uint8_t out[4], uint32_t v)
{
    out[0] = (uint8_t)v;
    out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)(v >> 16);
    out[3] = (uint8_t)(v >> 24);
}

/* A library whose records name what a well-formed file never does. Each
   file below is written from source and read back first, then refused
   with one record changed. */
static const char record_source[] =
    "pub const ZZK: int = 1;\n"
    "pub const ZZF: f64 = 1.5;\n"
    "pub const ZZT: []u8 = b\"abc\";\n"
    "pub enum ZE: u8 { A, B }\n"
    "pub struct ZB { zw: i8 : 3, zv: i8 : 2 }\n"
    "pub simd struct ZV { q: f32, r: f32 }\n"
    "pub class ZC\n"
    "{\n"
    "    n: int = 0,\n"
    "    pub fn zzm(self) -> int\n"
    "    {\n"
    "        return self.n;\n"
    "    }\n"
    "}\n"
    "pub fn zbits(b: ZB) -> int { return b.zw as int; }\n"
    "pub fn zlane(v: ZV) -> f32 { return v.q; }\n"
    "pub fn zloop(x: int) -> int\n"
    "{\n"
    "    let t = 0;\n"
    "    for i in 0..x {\n"
    "        if i > 2 {\n"
    "            return i;\n"
    "        }\n"
    "    }\n"
    "    return t;\n"
    "}\n";

/* The first instruction of op at or after from whose operand number
   operand is a block, or SIZE_MAX. An instruction starts with its op,
   its type, its line and its result, and each operand is 10 bytes. */
static size_t block_operand(const struct text *bytes, uint8_t op,
                            int operand)
{
    const uint8_t *p = (const uint8_t *)bytes->data;
    size_t at = (size_t)(10 + 10 * operand);
    size_t i;

    for (i = 0; i + 40 <= bytes->length; i++) {
        if (p[i] == op && p[i + 1] == IR_VOID && p[i + 6] == 0xff &&
            p[i + 7] == 0xff && p[i + 8] == 0xff && p[i + 9] == 0xff &&
            p[i + at] == IR_BLOCK && p[i + at + 1] == IR_VOID) {
            return i + at;
        }
    }
    check_failures++;
    fprintf(stderr, "no block operand of op %u\n", op);
    return SIZE_MAX;
}

/* The element index of the first slice record before end, which is the
   one slice of record_source. */
static size_t find_slice(const struct text *bytes, size_t end)
{
    const uint8_t *p = (const uint8_t *)bytes->data;
    size_t i;

    for (i = 0; i + 5 <= end; i++) {
        if (p[i] == TYPE_SLICE && p[i + 2] == 0 && p[i + 3] == 0 &&
            p[i + 4] == 0 && p[i + 1] < 16) {
            return i + 1;
        }
    }
    check_failures++;
    fprintf(stderr, "no slice record\n");
    return SIZE_MAX;
}

static void damaged_records(void)
{
    struct session s;
    struct text bytes = {0};
    uint8_t value[10];
    size_t at;
    uint32_t int_type;
    uint32_t f64_type;
    uint32_t fn_type;

    open_session(&s);
    if (!build_library(&s, "zz", record_source, &bytes)) {
        close_session(&s);
        return;
    }
    CHECK(reads_file(&bytes));
    int_type = u32_at(&bytes, name_end(&bytes, "ZZK", false));
    f64_type = u32_at(&bytes, name_end(&bytes, "ZZF", false));
    fn_type = u32_at(&bytes, name_end(&bytes, "zbits", false));

    /* S12: a member of a class body whose type is `int`, and one whose
       type has a parameter the declaration did not write. */
    at = name_end(&bytes, "zzm", false) + 4;
    poke_u32(value, int_type);
    refuses_poke(&bytes, at, value, 4);
    poke_u32(value, fn_type);
    refuses_poke(&bytes, at, value, 4);

    /* S15: a jump and a branch whose target is the integer 65536. */
    memset(value, 0, sizeof value);
    value[0] = IR_INT;
    value[1] = IR_VOID;
    value[4] = 1;
    refuses_poke(&bytes, block_operand(&bytes, IR_JUMP, 0), value, 10);
    refuses_poke(&bytes, block_operand(&bytes, IR_BRANCH, 1), value, 10);
    refuses_poke(&bytes, block_operand(&bytes, IR_BRANCH, 2), value, 10);

    /* S1: a simd aggregate of the IR whose lane is the unit break `_`,
       and one whose lane is a bitfield. */
    at = name_end(&bytes, "q", true);
    refuses_poke(&bytes, at - 1, "_", 1);
    value[0] = 4;
    refuses_poke(&bytes, at + 5, value, 1);

    /* M3: a bitfield of 40 bits on an `i8` field, in the type table and
       in the aggregate of the IR. */
    value[0] = 40;
    refuses_poke(&bytes, name_end(&bytes, "zw", false) + 4, value, 1);
    refuses_poke(&bytes, name_end(&bytes, "zw", true) + 5, value, 1);

    /* M4: an aggregate of the IR aligned to 3. Before its first field
       stand the count, the empty length text, the length and the
       alignment. */
    at = name_end(&bytes, "zw", true) - 6 - 4 - 4 - 4 - 8;
    memset(value, 0, sizeof value);
    value[0] = 3;
    refuses_poke(&bytes, at, value, 8);

    /* M5: the text constant of `ZZT` typed as a slice of `int`. The
       slice is the one record of its kind in the type table, which ends
       before the items. */
    at = find_slice(&bytes, name_end(&bytes, "ZZK", false));
    poke_u32(value, int_type);
    refuses_poke(&bytes, at, value, 4);

    /* M6: the enum `ZE` over `f64`. */
    at = name_end(&bytes, "ZE", false);
    poke_u32(value, f64_type);
    refuses_poke(&bytes, at, value, 4);

    text_free(&bytes);
    close_session(&s);
}

/* Whether the file reads into a program whose IR passes the verifier,
   as the driver takes a library it links. */
static bool verifies_file(const uint8_t *data, size_t size)
{
    struct session s;
    struct ir_module program;
    struct text errors = {0};
    char error[160] = "";
    bool ok;

    open_session(&s);
    ir_module_init(&program, &s.arena, "main");
    ok = antl_read(data, size, NULL, 0, &s.types, &s.arena, &program, error,
                   sizeof error) != NULL &&
         ir_verify(&program, &errors);
    text_free(&errors);
    ir_module_free(&program);
    close_session(&s);
    return ok;
}

/* Whether the file with the count bytes of value written at byte at
   verifies as verifies_file does. */
static bool verifies_poke(const struct text *bytes, size_t at,
                          const void *value, size_t count)
{
    uint8_t *copy;
    bool ok;

    if (at == SIZE_MAX || at + count > bytes->length) {
        check_failures++;
        fprintf(stderr, "no place to poke at %zu\n", at);
        return false;
    }
    copy = alloc_zeroed(bytes->length, 1);
    memcpy(copy, bytes->data, bytes->length);
    memcpy(copy + at, value, count);
    ok = verifies_file(copy, bytes->length);
    free(copy);
    return ok;
}

/* The flags of the struct whose first field is field, in the type table.
   Between them stand the byte of its thread safety, its empty
   `compatible` line, its alignment and the count of its fields. */
static size_t type_flags(const struct text *bytes, const char *field)
{
    return name_end(bytes, field, false) - strlen(field) - 4 - 4 - 8 - 4 -
           1 - 1;
}

/* The flags of the aggregate of the IR whose first field is field.
   Between them stand its alignment, its length, its empty length text
   and the count of its fields. */
static size_t agg_flags(const struct text *bytes, const char *field)
{
    return name_end(bytes, field, true) - strlen(field) - 4 - 4 - 4 - 4 -
           8 - 1;
}

/* The aggregate a `vreduce` of f32 lanes names, as an offset into the
   file. An instruction is its op, its type, its line, its result and
   three operands of 10 bytes, then the type and the index of its
   aggregate and its field, which is the operation of a lane. */
static size_t vreduce_agg(const struct text *bytes)
{
    const uint8_t *p = (const uint8_t *)bytes->data;
    size_t i;

    for (i = 0; i + 49 <= bytes->length; i++) {
        if (p[i] == IR_VREDUCE && p[i + 1] == IR_F32 &&
            p[i + 40] == IR_AGG && u32_at(bytes, i + 45) == IR_FADD) {
            return i + 41;
        }
    }
    check_failures++;
    fprintf(stderr, "no vreduce\n");
    return SIZE_MAX;
}

/* A library of simd structs. The checker holds a simd struct to its
   rules, and a library file reaches lowering and the back ends without
   the checker. `ZZ` has no bytes. `Z3` and `Z1` break the rules of a
   simd struct once their flags say they are one. */
static const char simd_source[] =
    "pub struct ZZ { _: i64 : 0 }\n"
    "pub struct Z3 { za: i32, zb: i32, zc: i32 }\n"
    "pub struct Z1 { zo: i32 }\n"
    "pub simd struct ZV { zq: f32, zr: f32 }\n"
    "pub fn zz(z: ZZ, t: Z3, o: Z1) -> int { return 0; }\n"
    "pub fn zlane(v: ZV) -> f32 { return v.zq; }\n"
    "pub fn zsum(v: ZV) -> f32 { return v.sum(); }\n";

static void damaged_simd(void)
{
    struct session s;
    struct text source = {0};
    struct text bytes = {0};
    uint8_t value[8];
    size_t lane;
    size_t at;
    uint32_t own;
    uint32_t i;

    /* `ZW` is 512 bytes of f32 lanes, above the vector cap, and each
       operation on it is a loop. */
    text_append(&source, simd_source);
    text_append(&source, "pub simd struct ZW { w0: f32");
    for (i = 1; i < 128; i++) {
        text_appendf(&source, ", w%u: f32", i);
    }
    text_append(&source, " }\npub fn zwide(w: ZW) -> f32 { return w.w0; }\n");
    open_session(&s);
    if (!build_library(&s, "zs", text_cstr(&source), &bytes)) {
        text_free(&source);
        close_session(&s);
        return;
    }
    CHECK(verifies_file((const uint8_t *)bytes.data, bytes.length));

    /* S03: the lane `zq` of `ZV` in the type table, as every other type
       of the table, `ZZ` of no bytes and `f64` among them. */
    lane = name_end(&bytes, "zq", false);
    own = u32_at(&bytes, lane);
    for (i = 0; i < 64; i++) {
        if (i != own) {
            poke_u32(value, i);
            refuses_poke(&bytes, lane, value, 4);
        }
    }
    /* S03: the same lane in the IR, as every other scalar and as each
       aggregate, `ZZ` among them. */
    lane = name_end(&bytes, "zq", true);
    for (i = IR_VOID; i <= IR_LOCK; i++) {
        if (i != IR_F32 && i != IR_AGG) {
            value[0] = (uint8_t)i;
            poke_u32(value + 1, IR_NO_AGG);
            refuses_poke(&bytes, lane, value, 5);
        }
    }
    for (i = 0; i < 64; i++) {
        value[0] = IR_AGG;
        poke_u32(value + 1, i);
        refuses_poke(&bytes, lane, value, 5);
    }

    /* The places of the flags hold what the source gave. */
    CHECK(((const uint8_t *)bytes.data)[type_flags(&bytes, "zq")] == 16);
    CHECK(((const uint8_t *)bytes.data)[agg_flags(&bytes, "zq")] == 2);
    CHECK(((const uint8_t *)bytes.data)[type_flags(&bytes, "za")] == 0);
    CHECK(((const uint8_t *)bytes.data)[agg_flags(&bytes, "za")] == 0);

    /* S24: three lanes, and one lane of four bytes, in both tables. */
    value[0] = 16;
    refuses_poke(&bytes, type_flags(&bytes, "za"), value, 1);
    refuses_poke(&bytes, type_flags(&bytes, "zo"), value, 1);
    value[0] = 2;
    refuses_poke(&bytes, agg_flags(&bytes, "za"), value, 1);
    refuses_poke(&bytes, agg_flags(&bytes, "zo"), value, 1);
    /* S24: `ZV` packed, and `ZV` aligned to 16, in both tables. The
       alignment follows the flags, the safety and the empty `compatible`
       line in the type table, and the flags in the IR. */
    value[0] = 16 | 2;
    refuses_poke(&bytes, type_flags(&bytes, "zq"), value, 1);
    value[0] = 2 | 1;
    refuses_poke(&bytes, agg_flags(&bytes, "zq"), value, 1);
    memset(value, 0, sizeof value);
    value[0] = 16;
    refuses_poke(&bytes, type_flags(&bytes, "zq") + 1 + 1 + 4, value, 8);
    refuses_poke(&bytes, agg_flags(&bytes, "zq") + 1, value, 8);

    /* The count of the lanes of `ZV` in the IR, one short, one over and
       past every byte of the file. It follows the alignment, the length
       and the empty length text. */
    poke_u32(value, 1);
    refuses_poke(&bytes, agg_flags(&bytes, "zq") + 1 + 8 + 4 + 4, value, 4);
    poke_u32(value, 3);
    refuses_poke(&bytes, agg_flags(&bytes, "zq") + 1 + 8 + 4 + 4, value, 4);
    poke_u32(value, UINT32_MAX);
    refuses_poke(&bytes, agg_flags(&bytes, "zq") + 1 + 8 + 4 + 4, value, 4);
    /* Every shorter file fails without reading past its end. */
    for (at = 8; at < bytes.length; at++) {
        refuses_file((const uint8_t *)bytes.data, at, NULL);
    }

    /* S24: the sum over `ZV` made a sum over each other aggregate, `ZW`
       above the vector cap among them. */
    at = vreduce_agg(&bytes);
    own = at == SIZE_MAX ? 0 : u32_at(&bytes, at);
    for (i = 0; i < 64 && at != SIZE_MAX; i++) {
        if (i != own) {
            poke_u32(value, i);
            CHECK(!verifies_poke(&bytes, at, value, 4));
        }
    }

    text_free(&bytes);
    text_free(&source);
    close_session(&s);
}

/* A library of constants: a class with defaults, and a class whose
   descriptor is a global with a value. */
static const char const_source[] =
    "pub class ZN { zp: ?*int = none, zi: int = 3 }\n"
    "pub class ZD\n"
    "{\n"
    "    n: int = 0,\n"
    "    pub fn zdn(self) -> int\n"
    "    {\n"
    "        return self.n;\n"
    "    }\n"
    "}\n";

static bool same_name(const struct name *n, const char *text)
{
    return n->length == strlen(text) && memcmp(n->text, text, n->length) == 0;
}

/* The type of the field field of the class or struct name of the file,
   or NULL when the file is refused. */
static const struct type *field_type_of(const struct text *bytes,
                                        struct session *s, const char *name,
                                        const char *field)
{
    struct ir_module program;
    const struct interface *iface;
    const struct type *found = NULL;
    char error[160] = "";
    size_t i;
    size_t j;

    ir_module_init(&program, &s->arena, "main");
    iface = antl_read((const uint8_t *)bytes->data, bytes->length, NULL, 0,
                      &s->types, &s->arena, &program, error, sizeof error);
    for (i = 0; iface != NULL && i < iface->item_count; i++) {
        const struct type *t = iface->items[i]->type;
        if (iface->items[i]->kind != SYMBOL_STRUCT ||
            !same_name(&t->name, name)) {
            continue;
        }
        for (j = 0; j < t->field_count; j++) {
            if (same_name(&t->fields[j].name, field)) {
                found = t->fields[j].type;
            }
        }
    }
    ir_module_free(&program);
    return found;
}

/* The first global of the program whose value is an aggregate that
   opens with a scalar item. */
static const struct ir_global *valued_global(const struct ir_module *program)
{
    size_t i;

    for (i = 0; i < program->global_count; i++) {
        const struct ir_global *g = program->globals[i];
        if (g->value != NULL && g->value->kind == IR_CONST_AGG &&
            g->value->item_count > 0 &&
            g->value->items[0].kind != IR_CONST_NONE &&
            g->value->items[0].kind != IR_CONST_AGG) {
            return g;
        }
    }
    return NULL;
}

static void damaged_constants(void)
{
    struct session s;
    struct session r;
    struct text bytes = {0};
    struct ir_module program;
    const struct ir_global *g;
    char error[160] = "";
    uint8_t value[8];
    size_t at;
    uint32_t own;
    uint32_t i;

    open_session(&s);
    if (!build_library(&s, "zk", const_source, &bytes)) {
        close_session(&s);
        return;
    }
    CHECK(reads_file(&bytes));

    /* The default `none` of `zp` given each other type of the table:
       a file that reads has a pointer or a function that may be `none`
       there, and never `*ZD`. */
    at = name_end(&bytes, "zp", false);
    own = u32_at(&bytes, at);
    for (i = 0; i < 64; i++) {
        struct session t;
        struct text poked = {0};
        const struct type *type;
        if (i == own) {
            continue;
        }
        text_append_bytes(&poked, bytes.data, bytes.length);
        poke_u32((uint8_t *)poked.data + at, i);
        open_session(&t);
        type = field_type_of(&poked, &t, "ZN", "zp");
        CHECK(type == NULL || types_is_nullable(type));
        close_session(&t);
        text_free(&poked);
    }

    /* S23: the scalar of the first item of a global's value made an
       aggregate and void. The global's name, its size, its alignment,
       its bytes, its relocations and its marks come before the value,
       and the value opens with its kind, its scalar, its count and its
       aggregate. */
    open_session(&r);
    ir_module_init(&program, &r.arena, "main");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &r.types, &r.arena, &program, error,
                    sizeof error) != NULL);
    g = valued_global(&program);
    CHECK(g != NULL);
    if (g != NULL) {
        size_t name = name_end(&bytes, g->name, false);
        uint64_t size = (uint64_t)u32_at(&bytes, name) |
                        (uint64_t)u32_at(&bytes, name + 4) << 32;
        size_t relocs = name + 8 + 8 + (size_t)size;
        size_t item = relocs + 4 + 13 * (size_t)u32_at(&bytes, relocs) + 1 +
                      1 + 1 + 8 + 5;
        CHECK(((const uint8_t *)bytes.data)[item] ==
              (uint8_t)g->value->items[0].kind);
        value[0] = IR_AGG;
        refuses_poke(&bytes, item + 1, value, 1);
        value[0] = IR_VOID;
        refuses_poke(&bytes, item + 1, value, 1);
        /* The alignment of the global, 3. */
        memset(value, 0, sizeof value);
        value[0] = 3;
        refuses_poke(&bytes, name + 8, value, 8);
    }
    ir_module_free(&program);
    close_session(&r);

    text_free(&bytes);
    close_session(&s);
}

/* bytes with the relocations of the global named name, which has none,
   replaced by count addresses at offsets, each of global 0. */
static void with_relocations(const struct text *bytes, const char *name,
                             const uint64_t *offsets, uint32_t count,
                             struct text *out)
{
    size_t at = name_end(bytes, name, false);
    uint64_t size = (uint64_t)u32_at(bytes, at) |
                    (uint64_t)u32_at(bytes, at + 4) << 32;
    size_t relocs = at + 8 + 8 + (size_t)size;
    uint8_t word[8];
    uint32_t i;
    int k;

    CHECK(u32_at(bytes, relocs) == 0);
    out->length = 0;
    text_append_bytes(out, bytes->data, relocs);
    poke_u32(word, count);
    text_append_bytes(out, word, 4);
    for (i = 0; i < count; i++) {
        for (k = 0; k < 8; k++) {
            word[k] = (uint8_t)(offsets[i] >> (8 * k));
        }
        text_append_bytes(out, word, 8);
        poke_u32(word, 0);
        text_append_bytes(out, word, 4);
        word[0] = 0;
        text_append_bytes(out, word, 1);
    }
    text_append_bytes(out, bytes->data + relocs + 4,
                      bytes->length - relocs - 4);
}

/* The addresses of a global each take eight bytes inside its data, apart
   from the others. A library file is the one source of a global whose
   addresses break that, so the reader refuses one that ends past the
   data or overlaps another. */
static void damaged_relocations(void)
{
    static const char source[] =
        "pub fn text() -> str\n"
        "{\n"
        "    return \"abcdefghijklmnopqrstuvwxyz\";\n"
        "}\n";
    static const uint64_t apart[] = {0, 8};
    static const uint64_t overlap[] = {0, 4};
    static const uint64_t past[] = {24};
    struct session s;
    struct text bytes = {0};
    struct text changed = {0};

    open_session(&s);
    if (!build_library(&s, "zr", source, &bytes)) {
        close_session(&s);
        return;
    }
    with_relocations(&bytes, "0", apart, 2, &changed);
    CHECK(reads_file(&changed));
    with_relocations(&bytes, "0", overlap, 2, &changed);
    refuses_file((const uint8_t *)changed.data, changed.length, NULL);
    with_relocations(&bytes, "0", past, 1, &changed);
    refuses_file((const uint8_t *)changed.data, changed.length, NULL);
    text_free(&changed);
    text_free(&bytes);
    close_session(&s);
}

/* A library file keeps the class records, the mark of a `worker fn` and
   the class and slot of a call through a table. The passes over the
   whole program read them. The program read back prints as the module
   did. */
static void keeps_classes(void)
{
    static const char source[] =
        "pub abstract class Shape\n"
        "{\n"
        "    abstract fn area(self) -> int;\n"
        "}\n"
        "pub abstract class Named\n"
        "{\n"
        "    abstract fn label(self) -> int;\n"
        "}\n"
        "pub final class Square inherits Shape\n"
        "{\n"
        "    implements n: Named,\n"
        "    side: int = 2,\n"
        "    concrete fn area(self) -> int\n"
        "    {\n"
        "        return self.side * self.side;\n"
        "    }\n"
        "    concrete fn label(self) -> int\n"
        "    {\n"
        "        return 1;\n"
        "    }\n"
        "}\n"
        "pub singleton class Board\n"
        "{\n"
        "    atomic hits: int = 0,\n"
        "    mutable score: int = 0,\n"
        "}\n"
        "pub worker fn tally(part: []int) -> int\n"
        "{\n"
        "    return part.len;\n"
        "}\n"
        "pub fn total(s: *Shape) -> int\n"
        "{\n"
        "    return s.area();\n"
        "}\n";
    struct session a;
    struct session b;
    struct interface *iface;
    struct ir_module ir;
    struct ir_module program;
    struct module *module;
    struct text bytes = {0};
    struct text before = {0};
    struct text after = {0};
    char error[160] = "";
    bool ok;

    open_session(&a);
    module = check_module(&a, "shapes", source, &ok);
    ir_module_init(&ir, &a.arena, "shapes");
    if (ok) {
        lower_module(module, "shapes", &ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    CHECK(ok);
    iface = arena_alloc(&a.arena, sizeof *iface);
    if (ok) {
        sema_interface(module, "shapes", &a.arena, iface);
        antl_write(&bytes, iface, &ir, false);
        ir_print(&before, &ir);
    }
    CHECK(strstr(text_cstr(&before), "class shapes.Square") != NULL);
    CHECK(strstr(text_cstr(&before), "worker fn shapes.tally") != NULL);
    CHECK(strstr(text_cstr(&before), " table @shapes.Shape.descriptor 17") !=
          NULL);
    open_session(&b);
    ir_module_init(&program, &b.arena, "");
    CHECK(antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                    &b.types, &b.arena, &program, error,
                    sizeof error) != NULL);
    CHECK_STR(error, "");
    ir_print(&after, &program);
    CHECK_STR(text_cstr(&after), text_cstr(&before));
    text_free(&bytes);
    text_free(&before);
    text_free(&after);
    ir_module_free(&program);
    ir_module_free(&ir);
    close_session(&b);
    close_session(&a);
}

/* The global of ir named name in module, or NULL. */
static const struct ir_global *global_in(const struct ir_module *ir,
                                         const char *module, const char *name)
{
    size_t i;

    for (i = 0; i < ir->global_count; i++) {
        const struct ir_global *g = ir->globals[i];
        if (g->module != NULL && strcmp(g->module, module) == 0 &&
            strcmp(g->name, name) == 0) {
            return g;
        }
    }
    return NULL;
}

/* A struct has one descriptor in a program, which the module that
   declares it writes whether a class there names it or not. A module
   that names the struct of another in a field record refers to that
   one, so the two records hold one address. */
static void one_struct_descriptor(void)
{
    static const char vec[] = "pub struct V2 { x: int, y: int }\n"
                              "struct Hidden { v: V2 }\n"
                              "union Bits { i: i32, f: f32 }\n";
    static const char source[] =
        "import vec;\n"
        "class Holder {\n"
        "    pub at: vec.V2 = vec.V2 { x: 0, y: 0 },\n"
        "    pub p: ?*vec.V2 = none,\n"
        "}\n";
    struct session s;
    struct interface *iface;
    struct ir_module lib;
    struct ir_module ir;
    struct module *module;
    const struct ir_global *g;
    size_t i;
    bool ok;

    open_session(&s);
    module = check_module(&s, "vec", vec, &ok);
    ir_module_init(&lib, &s.arena, "vec");
    if (ok) {
        lower_module(module, "vec", &lib, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    CHECK(ok);
    g = global_in(&lib, "vec", "V2.descriptor");
    CHECK(g != NULL && !g->is_extern && g->value != NULL);
    g = global_in(&lib, "vec", "Hidden.descriptor");
    CHECK(g != NULL && !g->is_extern && g->value != NULL);
    CHECK(global_in(&lib, "vec", "Bits.descriptor") == NULL);
    iface = arena_alloc(&s.arena, sizeof *iface);
    if (ok) {
        sema_interface(module, "vec", &s.arena, iface);
        s.libraries[s.library_count++] = iface;
    }

    module = check_module(&s, "main", source, &ok);
    ir_module_init(&ir, &s.arena, "main");
    if (ok) {
        lower_module(module, "main", &ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    }
    CHECK(ok);
    g = global_in(&ir, "vec", "V2.descriptor");
    CHECK(g != NULL && g->is_extern && g->value == NULL);
    for (i = 0; i < ir.global_count; i++) {
        const struct ir_global *own = ir.globals[i];
        if (own->module != NULL && strcmp(own->module, "main") == 0 &&
            strstr(own->name, "V2.") != NULL) {
            fprintf(stderr, "main writes %s\n", own->name);
            check_failures++;
        }
    }
    ir_module_free(&ir);
    ir_module_free(&lib);
    close_session(&s);
}

static void put_u8(struct text *b, uint8_t v)
{
    text_append_bytes(b, &v, 1);
}

static void put_u32(struct text *b, uint32_t v)
{
    uint8_t out[4];

    poke_u32(out, v);
    text_append_bytes(b, out, 4);
}

static void put_u64(struct text *b, uint64_t v)
{
    put_u32(b, (uint32_t)v);
    put_u32(b, (uint32_t)(v >> 32));
}

static void put_str(struct text *b, const char *s)
{
    put_u32(b, (uint32_t)strlen(s));
    text_append_bytes(b, s, strlen(s));
}

/* Where the tables sit in scale_antl. The count of the type table comes
   first and the items follow the table. Then come the counts of the
   symbolic values and of the aggregates of the IR. */
enum { SCALE_TYPES = 67, SCALE_ITEMS = 86, SCALE_SYMS = 175, SCALE_AGGS = 179 };

/* The index of link k of a chain of n: each link names the next one
   forward, or the one before it backward. The end of the chain is the
   link that names none. */
static uint32_t chain_next(uint32_t k, uint32_t n, bool forward)
{
    if (forward) {
        return k + 1 < n ? k + 1 : UINT32_MAX;
    }
    return k > 0 ? k - 1 : UINT32_MAX;
}

/* scale_antl with n symbolic values, each the negation of the next. */
static void sym_chain(struct text *b, uint32_t n, bool forward)
{
    uint32_t k;

    text_append_bytes(b, scale_antl, SCALE_SYMS);
    put_u32(b, n);
    for (k = 0; k < n; k++) {
        uint32_t next = chain_next(k, n, forward);
        put_u8(b, next == UINT32_MAX ? IR_SYM_INT : IR_SYM_OP);
        put_u8(b, IR_I64);
        put_u64(b, 1);
        put_u8(b, IR_VOID);
        put_u32(b, IR_NO_AGG);
        put_u32(b, 0);
        put_u8(b, next == UINT32_MAX ? 0 : IR_NEG);
        put_u32(b, next == UINT32_MAX ? 0 : next);
        put_u32(b, IR_NO_AGG);
    }
    text_append_bytes(b, scale_antl + SCALE_AGGS,
                      sizeof scale_antl - SCALE_AGGS);
}

/* scale_antl with n IR structs, each holding the next. */
static void agg_chain(struct text *b, uint32_t n, bool forward)
{
    char name[16];
    uint32_t k;

    text_append_bytes(b, scale_antl, SCALE_AGGS);
    put_u32(b, n);
    for (k = 0; k < n; k++) {
        uint32_t next = chain_next(k, n, forward);
        snprintf(name, sizeof name, "a%u", (unsigned)k);
        put_u8(b, IR_AGG_STRUCT);
        put_str(b, name);
        put_u8(b, 0);
        put_u64(b, 0);
        put_u32(b, IR_NO_AGG);
        put_str(b, "");
        put_u32(b, 1);
        put_str(b, "f");
        put_u8(b, next == UINT32_MAX ? IR_I64 : IR_AGG);
        put_u32(b, next);
        put_u8(b, 0);
        put_u8(b, 0);
    }
    text_append_bytes(b, scale_antl + SCALE_AGGS + 4,
                      sizeof scale_antl - SCALE_AGGS - 4);
}

/* scale_antl with n structs after its two types, each holding the next,
   and the last a uint. */
static void struct_chain(struct text *b, uint32_t n, bool forward)
{
    char name[16];
    uint32_t k;

    text_append_bytes(b, scale_antl, SCALE_TYPES);
    put_u32(b, 2 + n);
    text_append_bytes(b, scale_antl + SCALE_TYPES + 4,
                      SCALE_ITEMS - SCALE_TYPES - 4);
    for (k = 0; k < n; k++) {
        uint32_t next = chain_next(k, n, forward);
        snprintf(name, sizeof name, "s%u", (unsigned)k);
        put_u8(b, TYPE_STRUCT);
        put_str(b, "scale");
        put_str(b, name);
        put_u8(b, 0);                   /* a plain struct */
        put_u8(b, 0);
        put_u8(b, 0);
        put_str(b, "");
        put_u64(b, 0);
        put_u32(b, 1);
        put_str(b, "f");
        put_u32(b, next == UINT32_MAX ? 0 : 2 + next);
        put_u8(b, 0);
        put_u8(b, FIELD_PLAIN);
        put_u8(b, VIS_PUB);
        put_u8(b, 0);
        put_str(b, "");
        put_str(b, "");
        put_u32(b, 0);
    }
    /* No field has a default value. */
    for (k = 0; k < n; k++) {
        put_u8(b, 0);
    }
    text_append_bytes(b, scale_antl + SCALE_ITEMS,
                      sizeof scale_antl - SCALE_ITEMS);
}

/* S13: a table of a library file that nests deeper than the reader
   allows is refused. Its entries may point forward, which the reader
   follows on demand, or back. A short chain reads either way. */
static void deep_tables(void)
{
    void (*const chains[])(struct text *, uint32_t, bool) = {
        sym_chain, agg_chain, struct_chain};
    size_t i;

    for (i = 0; i < sizeof chains / sizeof chains[0]; i++) {
        int forward;
        for (forward = 0; forward < 2; forward++) {
            struct text b = {0};
            chains[i](&b, 16, forward != 0);
            CHECK(reads_file(&b));
            text_free(&b);
            chains[i](&b, 300000, forward != 0);
            refuses_file((const uint8_t *)b.data, b.length, NULL);
            text_free(&b);
        }
    }
}

/* S01: the checked tree of a generic that a library file carries. Each
   test below writes the tree of `tree` with one relation the checker
   never leaves, and the reader refuses the file. */
static const char tree_source[] =
    "pub variant TShape { Circle { r: int }, Empty }\n"
    "pub enum TColor: u8 { Red, Green }\n"
    "pub fn tpair<T, U>(own x: T, y: U) -> T {\n"
    "    return x;\n"
    "}\n"
    "fn thelper(k: int) -> int {\n"
    "    return k;\n"
    "}\n"
    "const TLIMIT: int = 5;\n"
    "pub fn tree<T>(own x: T, n: int, s: TShape, pairs: [](int, int),"
    " xs: []int) -> int {\n"
    "    let total = 0;\n"
    "    switch s {\n"
    "        Circle c => total = total + c.r,\n"
    "        else => total = total + 1,\n"
    "    }\n"
    "    let tested = s is TShape.Circle;\n"
    "    let made = TShape.Circle { r: n };\n"
    "    let color = TColor.Green;\n"
    "    for (a, b) in pairs {\n"
    "        total = total + a + b;\n"
    "    }\n"
    "    for v in xs {\n"
    "        total = total + v;\n"
    "    }\n"
    "    for j in 0..10 by -3 {\n"
    "        total = total + j;\n"
    "    }\n"
    "    while total > 100 do {\n"
    "        break;\n"
    "    }\n"
    "    {\n"
    "        total = total + 3;\n"
    "    }\n"
    "    let kept = tpair(x, n);\n"
    "    let neg = -n;\n"
    "    let sum = total + n;\n"
    "    let helped = thelper(sum) + TLIMIT;\n"
    "    let f = fn(k: int) -> int { return k + 1; };\n"
    "    return f(sum);\n"
    "}\n";

/* The tree of the generic `tree` among the generics s writes. */
static struct item *tree_of(struct session *s)
{
    size_t i;

    for (i = 0; i < s->written->generic_count; i++) {
        struct item *it = s->written->generics[i];
        if (it->name.length == 4 && memcmp(it->name.text, "tree", 4) == 0) {
            return it;
        }
    }
    check_failures++;
    fprintf(stderr, "no generic `tree`\n");
    return NULL;
}

/* The statement of kind among the top statements of the body of `tree`,
   the nth of that kind. */
static struct stmt *tree_stmt(struct session *s, enum stmt_kind kind,
                              size_t nth)
{
    struct item *it = tree_of(s);
    size_t i;

    for (i = 0; it != NULL && i < it->body->count; i++) {
        struct stmt *st = it->body->stmts[i];
        if (st->kind == kind && nth-- == 0) {
            return st;
        }
    }
    check_failures++;
    fprintf(stderr, "no statement of kind %d\n", (int)kind);
    return NULL;
}

/* The value of `let name` in the body of `tree`. */
static struct expr *tree_let(struct session *s, const char *name)
{
    struct item *it = tree_of(s);
    size_t i;

    for (i = 0; it != NULL && i < it->body->count; i++) {
        struct stmt *st = it->body->stmts[i];
        if (st->kind == STMT_LET && st->as.let.name.length == strlen(name) &&
            memcmp(st->as.let.name.text, name, strlen(name)) == 0) {
            return st->as.let.value;
        }
    }
    check_failures++;
    fprintf(stderr, "no `let %s`\n", name);
    return NULL;
}

static void otherwise_past_arms(struct session *s)
{
    tree_stmt(s, STMT_SWITCH, 0)->as.switch_stmt.otherwise_at = 2;
}

static void arm_case_past_cases(struct session *s)
{
    tree_stmt(s, STMT_SWITCH, 0)->as.switch_stmt.arms[0].variant_case =
        65536;
}

static void cast_case_past_cases(struct session *s)
{
    tree_let(s, "tested")->as.cast.variant_case = 3;
}

static void literal_case_past_cases(struct session *s)
{
    tree_let(s, "made")->as.struct_lit.variant_case = 7;
}

static void enum_value_past_values(struct session *s)
{
    tree_let(s, "color")->as.field.enum_value = 9;
}

static void name_without_symbol(struct session *s)
{
    tree_let(s, "sum")->as.binary.right->symbol = NULL;
}

static void operand_without_type(struct session *s)
{
    tree_let(s, "neg")->as.unary.operand->type = NULL;
}

static void pattern_without_element(struct session *s)
{
    tree_stmt(s, STMT_FOR, 0)->as.for_loop.element = NULL;
}

/* Three names for the two parts of `(int, int)`. */
static void pattern_past_parts(struct session *s)
{
    struct stmt *loop = tree_stmt(s, STMT_FOR, 0);
    struct binding *names = arena_alloc(&s->arena, 3 * sizeof *names);

    names[0] = loop->as.for_loop.names[0];
    names[1] = loop->as.for_loop.names[1];
    names[2] = loop->as.for_loop.names[1];
    loop->as.for_loop.names = names;
    loop->as.for_loop.name_count = 3;
}

static void walk_without_name(struct session *s)
{
    tree_stmt(s, STMT_FOR, 1)->as.for_loop.name_count = 0;
}

/* The `break` of the `while` in place of the loop, outside it. */
static void break_outside_loop(struct session *s)
{
    struct item *it = tree_of(s);
    size_t i;

    for (i = 0; i < it->body->count; i++) {
        struct stmt *loop = it->body->stmts[i];
        if (loop->kind == STMT_WHILE) {
            it->body->stmts[i] = loop->as.loop.body->stmts[0];
            return;
        }
    }
}

/* Two parameters on a function whose type takes one. */
static void closure_past_params(struct session *s)
{
    struct item *fn = tree_let(s, "f")->as.fn;
    struct param *params = arena_alloc(&s->arena, 2 * sizeof *params);

    params[0] = fn->params[0];
    params[1] = fn->params[0];
    fn->params = params;
    fn->param_count = 2;
}

static void generic_short_of_params(struct session *s)
{
    tree_of(s)->param_count = 4;
}

/* One argument recorded for a copy of a generic of two. */
static void copy_short_of_args(struct session *s)
{
    tree_let(s, "kept")->as.call.copy_count = 1;
}

/* The nested block holds the statement that holds it. */
static void block_holds_itself(struct session *s)
{
    struct stmt *nested = tree_stmt(s, STMT_BLOCK, 0);

    nested->as.block->stmts[0] = nested;
}

static void expr_holds_itself(struct session *s)
{
    struct expr *sum = tree_let(s, "sum");

    sum->as.binary.left = sum;
}

/* One statement in two places of the body. */
static void stmt_twice(struct session *s)
{
    struct stmt *nested = tree_stmt(s, STMT_BLOCK, 0);
    struct item *it = tree_of(s);

    it->body->stmts[0] = nested;
}

/* A copy of the symbol that e names, which the file then writes as an
   extern of its own. */
static struct symbol *own_symbol(struct session *s, struct expr *e)
{
    struct symbol *copy = arena_alloc(&s->arena, sizeof *copy);

    *copy = *e->symbol;
    e->symbol = copy;
    return copy;
}

/* The private function `thelper` written with the type `int`. */
static void extern_fn_not_fn(struct session *s)
{
    struct expr *call = tree_let(s, "helped")->as.binary.left;

    own_symbol(s, call->as.call.callee)->type = call->type;
}

/* The private constant `TLIMIT` written without its value. */
static void extern_const_without_value(struct session *s)
{
    own_symbol(s, tree_let(s, "helped")->as.binary.right)->value = NULL;
}

/* A line above INT_MAX, which a negative line of the compiler writes. */
static void line_negative(struct session *s)
{
    tree_let(s, "sum")->pos.line = -1;
}

/* A chain of `-` as deep as depth, with the operand of `-n` at its end. */
static void unary_chain(struct session *s, size_t depth)
{
    struct expr *neg = tree_let(s, "neg");
    struct expr *chain = neg->as.unary.operand;
    size_t k;

    for (k = 0; k < depth; k++) {
        struct expr *link = arena_alloc(&s->arena, sizeof *link);
        *link = *neg;
        link->as.unary.operand = chain;
        chain = link;
    }
    neg->as.unary.operand = chain;
}

static void short_chain(struct session *s)
{
    unary_chain(s, 16);
}

static void deep_chain(struct session *s)
{
    unary_chain(s, ANTL_TREE_DEPTH_MAX + 16);
}

/* Read the library of tree_source written with damage, or refuse it. */
static void tree_file(void (*damage)(struct session *),
                      bool reads)
{
    struct session s;
    struct text bytes = {0};

    open_session(&s);
    if (build_damaged(&s, "tt", tree_source, damage, &bytes)) {
        if (reads) {
            CHECK(reads_file(&bytes));
        } else {
            refuses_file((const uint8_t *)bytes.data, bytes.length, NULL);
        }
    }
    text_free(&bytes);
    close_session(&s);
}

/* `tpair(x, n, n)`, the last argument shared. Every table keeps its
   size, and the first byte that differs is the count of the arguments. */
static void arg_repeated(struct session *s)
{
    struct expr *call = tree_let(s, "kept");
    struct expr **args = arena_alloc(&s->arena, 3 * sizeof *args);

    args[0] = call->as.call.args[0];
    args[1] = call->as.call.args[1];
    args[2] = call->as.call.args[1];
    call->as.call.args = args;
    call->as.call.arg_count = 3;
}

/* `tpair(x)`: the table of expressions of the tree loses `n`, and the
   first byte that differs is the count of that table. */
static void arg_dropped(struct session *s)
{
    tree_let(s, "kept")->as.call.arg_count = 1;
}

/* The first byte where the file of tree_source written with damage
   differs from the one written without, or SIZE_MAX. */
static size_t tree_difference(const struct text *plain,
                              void (*damage)(struct session *))
{
    struct session s;
    struct text other = {0};
    size_t at = SIZE_MAX;
    size_t i;

    open_session(&s);
    if (build_damaged(&s, "tt", tree_source, damage, &other)) {
        for (i = 0; i < plain->length && i < other.length; i++) {
            if (plain->data[i] != other.data[i]) {
                at = i;
                break;
            }
        }
    }
    text_free(&other);
    close_session(&s);
    return at;
}

/* plain with the u32 value at byte at is refused at the byte after it,
   where the count stands, and not later. */
static void refuses_count(const struct text *plain, size_t at, uint32_t value)
{
    uint8_t *copy;
    char expected[64];
    int k;

    if (at == SIZE_MAX || at + 4 > plain->length) {
        check_failures++;
        fprintf(stderr, "no count at %zu\n", at);
        return;
    }
    copy = alloc_zeroed(plain->length, 1);
    memcpy(copy, plain->data, plain->length);
    for (k = 0; k < 4; k++) {
        copy[at + (size_t)k] = (uint8_t)(value >> (8 * k));
    }
    snprintf(expected, sizeof expected, "is damaged at byte %zu", at + 4);
    refuses_file(copy, plain->length, expected);
    free(copy);
}

/* M33: a count of a tree is measured against the rest of the file before
   the reader allocates for it. A list of arguments takes four bytes per
   element and a record of the table of expressions more than two. */
static void tree_counts(void)
{
    struct session s;
    struct text plain = {0};
    size_t list;
    size_t table;

    open_session(&s);
    if (!build_damaged(&s, "tt", tree_source, NULL, &plain)) {
        close_session(&s);
        return;
    }
    close_session(&s);
    list = tree_difference(&plain, arg_repeated);
    table = tree_difference(&plain, arg_dropped);
    if (list == SIZE_MAX || table == SIZE_MAX) {
        check_failures++;
        fprintf(stderr, "no counts in the tree\n");
        text_free(&plain);
        return;
    }
    CHECK((uint8_t)plain.data[list] == 2);
    CHECK((uint8_t)plain.data[table] > 2);
    refuses_count(&plain, list, (uint32_t)((plain.length - list - 4) / 3));
    refuses_count(&plain, table, (uint32_t)((plain.length - table - 4) / 2));
    refuses_count(&plain, list, UINT32_MAX);
    refuses_count(&plain, table, UINT32_MAX);
    text_free(&plain);
}

/* Every prefix of a file that carries trees is refused, and the reader
   reads no byte past its end. Each prefix stands in memory of its own
   length, so a sanitizer sees a read past it. */
static void truncated_trees(void)
{
    struct session s;
    struct text bytes = {0};
    size_t n;

    open_session(&s);
    if (build_library(&s, "tt", tree_source, &bytes)) {
        CHECK(reads_file(&bytes));
        for (n = 0; n < bytes.length; n++) {
            uint8_t *prefix = alloc_zeroed(n, 1);
            memcpy(prefix, bytes.data, n);
            refuses_file(prefix, n, NULL);
            free(prefix);
        }
    }
    text_free(&bytes);
    close_session(&s);
}

/* The negative step of `for j in 0..10 by -3` reads back as -3. */
static void signed_numbers(void)
{
    struct session s;
    struct text bytes = {0};
    struct ir_module program;
    const struct interface *read = NULL;
    char error[160] = "";
    size_t found = 0;
    size_t i;

    open_session(&s);
    if (build_library(&s, "tt", tree_source, &bytes)) {
        ir_module_init(&program, &s.arena, "main");
        read = antl_read((const uint8_t *)bytes.data, bytes.length, NULL, 0,
                         &s.types, &s.arena, &program, error, sizeof error);
        CHECK(read != NULL);
        for (i = 0; read != NULL && i < read->generic_count; i++) {
            const struct item *it = read->generics[i];
            size_t k;
            if (it->name.length != 4 || memcmp(it->name.text, "tree", 4) != 0) {
                continue;
            }
            for (k = 0; k < it->body->count; k++) {
                const struct stmt *st = it->body->stmts[k];
                if (st->kind == STMT_FOR && st->as.for_loop.step != NULL) {
                    CHECK(st->as.for_loop.step_value == -3);
                    found++;
                }
            }
        }
        ir_module_free(&program);
        CHECK(found == 1);
    }
    text_free(&bytes);
    close_session(&s);
}

static void damaged_trees(void)
{
    void (*const damages[])(struct session *) = {
        otherwise_past_arms,     arm_case_past_cases,
        cast_case_past_cases,    literal_case_past_cases,
        enum_value_past_values,  name_without_symbol,
        operand_without_type,    pattern_without_element,
        pattern_past_parts,      walk_without_name,
        break_outside_loop,      closure_past_params,
        generic_short_of_params, copy_short_of_args,
        block_holds_itself,      expr_holds_itself,
        stmt_twice,              deep_chain,
        extern_fn_not_fn,        extern_const_without_value,
        line_negative};
    size_t i;

    tree_file(NULL, true);
    tree_file(short_chain, true);
    for (i = 0; i < sizeof damages / sizeof damages[0]; i++) {
        tree_file(damages[i], false);
    }
}

void test_modules(void)
{
    damaged_trees();
    tree_counts();
    signed_numbers();
    truncated_trees();
    deep_tables();
    imports();
    cycles();
    lowers_imports();
    writes_format();
    round_trip();
    round_trip_class();
    class_literal_defaults();
    keeps_symbolic_sizes();
    package_and_docs();
    private_field_docs();
    module_paths();
    dotted_imports();
    unique_exports();
    keeps_extensions();
    keeps_effects();
    keeps_literals();
    keeps_constants();
    keeps_halves();
    keeps_signatures();
    keeps_generic_marks();
    keeps_context_signatures();
    keeps_extern_aggregates();
    keeps_classes();
    one_struct_descriptor();
    dependencies();
    damaged_files();
    damaged_records();
    damaged_simd();
    damaged_constants();
    damaged_relocations();
}
