#include "../binary_stdio.h"
#include "check.h"
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
        token_list_free(&s->tokens[i]);
    }
    diagnostics_free(&s->diags);
    arena_free(&s->arena);
}

static void print_diagnostics(const struct session *s, const char *source)
{
    size_t i;

    for (i = 0; i < s->diags.count; i++) {
        fprintf(stderr, "  %d:%d: %s\n", s->diags.items[i].line,
                s->diags.items[i].column, s->diags.items[i].message);
    }
    fprintf(stderr, "%s\n", source);
}

/* Parse and check source as module name. */
static struct module *check_module(struct session *s, const char *name,
                                   const char *source, bool *ok)
{
    struct token_list *tokens = &s->tokens[s->token_lists++];
    struct module *module = NULL;

    *ok = lex(source, strlen(source), &s->arena, &s->diags, tokens) &&
          parse(source, tokens, &s->arena, &s->diags, &module) &&
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
        print_diagnostics(s, source);
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

static void accepts(const char *source)
{
    struct session s;
    bool ok;

    open_session(&s);
    library(&s, "geometry", geometry);
    check_module(&s, "main", source, &ok);
    if (!ok) {
        check_failures++;
        fprintf(stderr, "rejected:\n");
        print_diagnostics(&s, source);
    }
    close_session(&s);
}

static void rejects(const char *source, int line, int column,
                    const char *message)
{
    struct session s;
    bool ok;

    open_session(&s);
    library(&s, "geometry", geometry);
    check_module(&s, "main", source, &ok);
    CHECK(!ok);
    if (s.diags.count == 0) {
        check_failures++;
    } else if (s.diags.items[0].line != line ||
               s.diags.items[0].column != column ||
               strcmp(s.diags.items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %d:%d: %s\n", line, column, message);
        print_diagnostics(&s, source);
    }
    close_session(&s);
}

static void imports(void)
{
    accepts("import geometry;\n"
            "import geometry as g;\n"
            "const TWICE: int = geometry.SIDES * 2;\n"
            "fn main() -> int {\n"
            "    let r = g.make() else { return 1; };\n"
            "    let v = g.Rect { w: 1, h: TWICE };\n"
            "    r.release();\n"
            "    g.putchar(65);\n"
            "    return geometry.area(v.w, v.h);\n"
            "}\n");
    rejects("import geometry;\nfn f() -> int {\n    return geometry.hidden();\n}\n",
            3, 12, "`geometry` has no public item `hidden`");
    rejects("import geometry;\nfn f(p: *geometry.Private) {}\n", 2, 10,
            "`geometry` has no public struct `Private`");
    rejects("import geometry;\nfn f() {\n    let m = geometry;\n}\n", 3, 13,
            "`geometry` is a module, not a value");
    rejects("import geometry;\nfn f() {\n    let t = geometry.Rect;\n}\n", 3, 13,
            "`geometry.Rect` is a type, not a value");
    rejects("import geometry;\nfn geometry() {}\n", 2, 4,
            "`geometry` is already declared");
    rejects("import main;\n", 1, 8, "`main` cannot import itself");
    rejects("import shapes;\n", 1, 8, "cannot find module `shapes`");
    /* A method comes from the module that declares the struct. */
    rejects("import geometry;\n"
            "fn area(r: *geometry.Rect) -> int {\n    return r.area();\n}\n",
            3, 12, "`Rect` has no function `area`");
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
    CHECK(ok && lower_module(module, "main", &ir, &s.diags, 0));
    ir_print(&out, &ir);
    CHECK_STR(text_cstr(&out),
              "extern fn geometry.make() -> ptr\n"
              "extern fn geometry.release(ptr)\n"
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
static bool build_library(struct session *s, const char *name,
                          const char *source, struct text *bytes)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    struct ir_module ir;
    struct module *module;
    bool ok;

    module = check_module(s, name, source, &ok);
    ir_module_init(&ir, &s->arena, name);
    ok = ok && lower_module(module, name, &ir, &s->diags, 0);
    if (!ok) {
        check_failures++;
        fprintf(stderr, "library %s does not compile:\n", name);
        print_diagnostics(s, source);
    } else {
        sema_interface(module, name, &s->arena, iface);
        s->libraries[s->library_count++] = iface;
        antl_write(bytes, iface, &ir, false);
    }
    ir_module_free(&ir);
    return ok;
}

static const char scale_source[] = "pub const SCALE: uint = 6;\n"
                                   "pub fn scale(x: uint) -> uint {\n"
                                   "    return x * SCALE;\n"
                                   "}\n";

/* The library file of scale_source, byte by byte. */
static const uint8_t scale_antl[] = {
    'A', 'N', 'T', 'L', 29, 0, 0, 0,                /* magic, version */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* package name */
    5, 0, 0, 0, '0', '.', '0', '.', '0',            /* package version */
    0, 0, 0, 0,                                     /* dependencies */
    0, 0, 0, 0,                                     /* license */
    0, 0, 0, 0,                                     /* license text */
    0, 0, 0, 0,                                     /* attribution */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* module */
    0, 0, 0, 0,                                     /* imports */
    0, 0, 0, 0,                                     /* module doc */
    2, 0, 0, 0,                                     /* types */
    11,                                             /* 0: uint */
    22, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,         /* 1: fn(int) -> int */
    2, 0, 0, 0,                                     /* items */
    2, 5, 0, 0, 0, 'S', 'C', 'A', 'L', 'E', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 6, 0, 0, 0, 0, 0, 0, 0,                      /* const SCALE = 6 */
    3, 5, 0, 0, 0, 's', 'c', 'a', 'l', 'e', 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 'x',                                /* fn scale(x) */
    1, 0, 0, 0,                                     /* source files */
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* the one file */
    0, 0, 0, 0,                                     /* symbolic values */
    0, 0, 0, 0,                                     /* aggregates */
    0, 0, 0, 0,                                     /* globals */
    1, 0, 0, 0,                                     /* functions */
    0, 5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',
    5, 0, 0, 0, 's', 'c', 'a', 'l', 'e',            /* scale.scale */
    4, 255, 255, 255, 255,                          /* -> i64 */
    0, 0, 0, 0, 2, 0, 0, 0,                         /* file 0, line 2 */
    1, 0, 0, 0, 4, 0, 255, 255, 255, 255,           /* one i64 parameter */
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
    61, 4, 3, 0, 0, 0, 255, 255, 255, 255,          /* line 3: ret i64 */
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
        ir_print(&ir_b, &program);
        CHECK_STR(text_cstr(&ir_b),
                  "type anti.rt.Descriptor = struct { name: ptr, name_length: "
                  "i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, "
                  "field_count: i64, fields: ptr, destruct: ptr, offset: i64, "
                  "function_count: i64, functions: ptr }\n"
                  "type vec.V2 = struct { x: i64, y: i64 }\n"
                  "type anti.rt.Field = struct { name: ptr, name_length: i64, "
                  "offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
                  "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
                  "type vec.Hidden = struct { v: vec.V2, next: ptr }\n"
                  "extern fn malloc(i64) -> ptr\n"
                  "global vec.V2.descriptor anti.rt.Descriptor { @vec.1, i64 "
                  "2, ptr 0, size_of vec.V2, i64 0, ptr 0, i64 2, "
                  "@vec.V2.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
                  "global vec.1 size 3 align 1 bytes 56 32 00\n"
                  "global vec.2 size 2 align 1 bytes 78 00\n"
                  "global vec.3 size 2 align 1 bytes 79 00\n"
                  "global vec.V2.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.2, i64 1, offset_of vec.V2.x, i64 6, i64 0, ptr 0 }, "
                  "anti.rt.Field { @vec.3, i64 1, offset_of vec.V2.y, i64 6, "
                  "i64 0, ptr 0 } }\n"
                  "global vec.Hidden.descriptor anti.rt.Descriptor { @vec.6, "
                  "i64 6, ptr 0, size_of vec.Hidden, i64 0, ptr 0, i64 2, "
                  "@vec.Hidden.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
                  "global vec.6 size 7 align 1 bytes 48 69 64 64 65 6e 00\n"
                  "global vec.7 size 2 align 1 bytes 76 00\n"
                  "global vec.8 size 5 align 1 bytes 6e 65 78 74 00\n"
                  "global vec.Hidden.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.7, i64 1, offset_of vec.Hidden.v, i64 21, i64 0, "
                  "@vec.V2.descriptor }, anti.rt.Field { @vec.8, i64 4, "
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
    CHECK(ok && lower_module(module, "main", &program, &b.diags, 0));
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

    if (!module_path_of_source(source, roots, count, &out, error,
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
    CHECK(module_path_reserved("anti"));
    CHECK(module_path_reserved("anti.text"));
    CHECK(!module_path_reserved("antique.text"));
    CHECK(module_path_segments("com.niese.geo") == 3);
    CHECK_STR(module_path_last("com.niese.geo"), "geo");
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
    CHECK(ok && lower_module(module, "com.example.geo", &ir, &s->diags, 0));
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
    CHECK(ok && lower_module(module, "main", &program, &b.diags, 0));
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type str = struct { ptr: ptr, len: i64 }\n"
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
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
              "    jump b2\n"
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
    CHECK(ok && lower_module(module, "main", &program, &b.diags, 0));
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir),
                 "global pair.5 pair.Pair { i8 7, i32 11 }\n") != NULL);
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
                  "pub fn run(f: fn(i16) -> int) -> int {\n"
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
        ir_print(&ir, &program);
        CHECK_STR(text_cstr(&ir),
                  "type anti.rt.Descriptor = struct { name: ptr, name_length: "
                  "i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, "
                  "field_count: i64, fields: ptr, destruct: ptr, offset: i64, "
                  "function_count: i64, functions: ptr }\n"
                  "type vec.V2 = struct { x: i64, y: i64 }\n"
                  "type anti.rt.Field = struct { name: ptr, name_length: i64, "
                  "offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
                  "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
                  "type vec.Hidden = struct { v: vec.V2, next: ptr }\n"
                  "type shapes.Box = struct { corner: vec.V2 }\n"
                  "type [1]anti.rt.Field = array 1 of anti.rt.Field\n"
                  "extern fn malloc(i64) -> ptr\n"
                  "global vec.V2.descriptor anti.rt.Descriptor { @vec.1, i64 "
                  "2, ptr 0, size_of vec.V2, i64 0, ptr 0, i64 2, "
                  "@vec.V2.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
                  "global vec.1 size 3 align 1 bytes 56 32 00\n"
                  "global vec.2 size 2 align 1 bytes 78 00\n"
                  "global vec.3 size 2 align 1 bytes 79 00\n"
                  "global vec.V2.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.2, i64 1, offset_of vec.V2.x, i64 6, i64 0, ptr 0 }, "
                  "anti.rt.Field { @vec.3, i64 1, offset_of vec.V2.y, i64 6, "
                  "i64 0, ptr 0 } }\n"
                  "global vec.Hidden.descriptor anti.rt.Descriptor { @vec.6, "
                  "i64 6, ptr 0, size_of vec.Hidden, i64 0, ptr 0, i64 2, "
                  "@vec.Hidden.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
                  "global vec.6 size 7 align 1 bytes 48 69 64 64 65 6e 00\n"
                  "global vec.7 size 2 align 1 bytes 76 00\n"
                  "global vec.8 size 5 align 1 bytes 6e 65 78 74 00\n"
                  "global vec.Hidden.fields [2]anti.rt.Field { anti.rt.Field { "
                  "@vec.7, i64 1, offset_of vec.Hidden.v, i64 21, i64 0, "
                  "@vec.V2.descriptor }, anti.rt.Field { @vec.8, i64 4, "
                  "offset_of vec.Hidden.next, i64 5393, i64 0, "
                  "@vec.Hidden.descriptor } }\n"
                  "global shapes.Box.descriptor anti.rt.Descriptor { "
                  "@shapes.1, i64 3, ptr 0, size_of shapes.Box, i64 0, ptr 0, "
                  "i64 1, @shapes.Box.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
                  "global shapes.1 size 4 align 1 bytes 42 6f 78 00\n"
                  "global shapes.2 size 7 align 1 bytes 63 6f 72 6e 65 72 00\n"
                  "global vec.V2.descriptor size 0 align 1 bytes\n"
                  "global shapes.Box.fields [1]anti.rt.Field { anti.rt.Field { "
                  "@shapes.2, i64 6, offset_of shapes.Box.corner, i64 21, i64 "
                  "0, @vec.V2.descriptor } }\n"
                  "fn vec.make() -> ptr {\n"
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

static void damaged_files(void)
{
    /* Where the fields a poke below reaches sit, counted back from the end
       of the file: the classes, two instructions of 53 bytes each and the
       15 bytes that open the body, then the one parameter of the
       signature, its count, the source of the function and its result. */
    enum {
        TAIL = 4 + 2 * 53 + 15,
        MUL_OPERAND = 4 + 2 * 53 - 12,
        PARAM_EXT = TAIL + 4 + 1,
        RESULT_AGG = TAIL + 6 + 4 + 8 + 4
    };
    uint8_t copy[sizeof scale_antl];
    size_t n;

    memcpy(copy, scale_antl, sizeof copy);
    copy[4] = 30;
    refuses_file(copy, sizeof copy,
                 "has format version 30, and antic reads version 29");
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
    /* Only a parameter of 8 or 16 bits extends. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - PARAM_EXT] = IR_EXT_SIGN;
    refuses_file(copy, sizeof copy, NULL);
    /* The temporary of the mul instruction points past the temporaries. */
    memcpy(copy, scale_antl, sizeof copy);
    copy[sizeof copy - MUL_OPERAND] = 9;
    refuses_file(copy, sizeof copy, NULL);
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
        "pub final class Square\n"
        "{\n"
        "    inherits Shape,\n"
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
    ok = ok && lower_module(module, "shapes", &ir, &a.diags, 0);
    CHECK(ok);
    iface = arena_alloc(&a.arena, sizeof *iface);
    if (ok) {
        sema_interface(module, "shapes", &a.arena, iface);
        antl_write(&bytes, iface, &ir, false);
        ir_print(&before, &ir);
    }
    CHECK(strstr(text_cstr(&before), "class shapes.Square") != NULL);
    CHECK(strstr(text_cstr(&before), "worker fn shapes.tally") != NULL);
    CHECK(strstr(text_cstr(&before), " table @shapes.Shape.descriptor 8") !=
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
    ok = ok && lower_module(module, "vec", &lib, &s.diags, 0);
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
    ok = ok && lower_module(module, "main", &ir, &s.diags, 0);
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

void test_modules(void)
{
    imports();
    cycles();
    lowers_imports();
    writes_format();
    round_trip();
    round_trip_class();
    keeps_symbolic_sizes();
    package_and_docs();
    private_field_docs();
    module_paths();
    dotted_imports();
    unique_exports();
    keeps_extensions();
    keeps_literals();
    keeps_constants();
    keeps_signatures();
    keeps_extern_aggregates();
    keeps_classes();
    one_struct_descriptor();
    dependencies();
    damaged_files();
}
