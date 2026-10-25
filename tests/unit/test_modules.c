/* The library file written, read back and damaged, the imports across
   modules and the module paths. */

#include "../binary_stdio.h"
#include <stdlib.h>
#include "check.h"
#include "alloc.h"
#include "pipeline.h"
#include "antl.h"
#include "modpath.h"
#include "../../src/antic/platform.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* What the damage of build_damaged changes: the interface it writes, and
   the memory pool of its compilation for what the damage adds. */
struct damaged {
    struct interface *written;
    struct arena *arena;
};

/* Compile a library and add its interface to the compilation. */
static void library(struct compilation *s, const char *name,
                    const char *source)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    bool ok;
    struct module *module = compilation_check(s, name, source, true, &ok);

    if (!ok) {
        check_failures++;
        fprintf(stderr, "library %s does not check:\n", name);
        print_diagnostics(&s->diags, source);
        return;
    }
    sema_interface(module, name, &s->arena, iface);
    compilation_add(s, iface);
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
    struct compilation s;
    bool ok;

    compilation_open(&s);
    library(&s, "geometry", geometry);
    compilation_check(&s, "main", source, true, &ok);
    expect_accepted(ok, &s.diags, source);
    compilation_close(&s);
}

static void geometry_rejects(const char *source, int line, int column,
                                      const char *message)
{
    struct compilation s;
    bool ok;

    compilation_open(&s);
    library(&s, "geometry", geometry);
    compilation_check(&s, "main", source, true, &ok);
    expect_refused(ok, &s.diags, source, line, column, message);
    compilation_close(&s);
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
    struct compilation s;
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
    }
    compilation_open(&s);
    for (i = 0; i < COUNT; i++) {
        compilation_add(&s, &libs[i]);
    }
    compilation_check(&s, "main", source, true, &ok);
    CHECK(ok);
    compilation_close(&s);
}

static void cycles(void)
{
    struct compilation s;
    bool ok;

    /* c was built against an earlier main, and b against c. */
    compilation_open(&s);
    library(&s, "main", "pub fn g() {}\n");
    library(&s, "c", "import main;\npub fn h() {}\n");
    library(&s, "b", "import c;\npub fn f() {}\n");
    s.libraries[0] = s.libraries[1];
    s.libraries[1] = s.libraries[2];
    s.library_count = 2;
    compilation_check(&s, "main", "import b;\n", true, &ok);
    CHECK(!ok);
    CHECK(s.diags.count == 1);
    if (s.diags.count == 1) {
        CHECK(s.diags.items[0].line == 1 && s.diags.items[0].column == 8);
        CHECK_STR(s.diags.items[0].message,
                  "`b` depends on `main`, so the import forms a cycle");
    }
    compilation_close(&s);
    damaged_imports();
}

/* A compilation holds COMPILATION_MODULES modules. One more counts a
   failure, is refused and leaves the arrays of the compilation alone. */
static void compilation_bound(void)
{
    struct compilation s;
    int failures;
    bool ok = false;
    size_t i;

    compilation_open(&s);
    for (i = 0; i < COMPILATION_MODULES; i++) {
        compilation_check(&s, "main", "fn f() {}\n", true, &ok);
        CHECK(ok);
    }
    failures = check_failures;
    CHECK(compilation_check(&s, "main", "fn f() {}\n", true, &ok) == NULL);
    CHECK(!ok);
    CHECK(check_failures == failures + 1);
    check_failures = failures;
    compilation_close(&s);
}

static void lowers_imports(void)
{
    struct compilation s;
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

    compilation_open(&s);
    library(&s, "geometry", geometry);
    module = compilation_check(&s, "main", source, true, &ok);
    CHECK(ok);
    ir_module_init(&ir, &s.arena, "main");
    if (ok) {
        lower_as(module, "main", &ir, 0);
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
    compilation_close(&s);
}

/* Compile a library, add its interface and write its file. damage, when
   set, changes the checked trees of its generics before the writer reads
   them. */
static bool build_damaged(struct compilation *s, const char *name,
                          const char *source,
                          void (*damage)(struct damaged *),
                          struct text *bytes)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    struct ir_module ir;
    struct module *module;
    bool ok;

    module = compilation_check(s, name, source, true, &ok);
    /* The generics leave the module before lowering, as in the driver. */
    if (ok) {
        sema_strip_generics(module, &s->arena);
    }
    ir_module_init(&ir, &s->arena, name);
    if (ok) {
        lower_as(module, name, &ir, 0);
    }
    if (!ok) {
        check_failures++;
        fprintf(stderr, "library %s does not compile:\n", name);
        print_diagnostics(&s->diags, source);
    } else {
        sema_interface(module, name, &s->arena, iface);
        compilation_add(s, iface);
        if (damage != NULL) {
            struct damaged d = {iface, &s->arena};
            damage(&d);
        }
        antl_write(bytes, iface, &ir, false);
    }
    ir_module_free(&ir);
    return ok;
}

static bool build_library(struct compilation *s, const char *name,
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
    'A', 'N', 'T', 'L', 92, 0, 0, 0,                /* magic, version */
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
    2, 5, 0, 0, 0, 'S', 'C', 'A', 'L', 'E', 0, 0, 0, 0, 0,
    8, 0, 0, 0,                                     /* spells `uint` */
    0, 0, 0, 0,
    0, 6, 0, 0, 0, 0, 0, 0, 0,                      /* const SCALE = 6 */
    3, 5, 0, 0, 0, 's', 'c', 'a', 'l', 'e', 1, 0, 0, 0, 0,
    8, 0, 0, 0,                                     /* spells `uint` */
    0, 0, 0, 0,
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
    87, 4, 3, 0, 0, 0, 255, 255, 255, 255,          /* line 3: ret i64 */
    1, 4, 1, 0, 0, 0, 0, 0, 0, 0,                   /* %1 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 255, 255, 255, 255, 0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,                                     /* classes */
    0, 0, 0, 0,                                     /* no locks of functions */
    0, 0, 0, 0,                                     /* no edges of lock orders */
    0, 0, 0, 0,                                     /* no functions that wait */
};

static void writes_format(void)
{
    struct compilation s;
    struct text bytes = {0};
    size_t i;

    compilation_open(&s);
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
    /* A file starts with the magic, and a shorter or other start is no
       library file. */
    CHECK(antl_starts((const uint8_t *)bytes.data, bytes.length));
    CHECK(!antl_starts((const uint8_t *)bytes.data, 3));
    CHECK(!antl_starts((const uint8_t *)"ANTX", 4));
    text_free(&bytes);
    compilation_close(&s);
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
    struct compilation a;
    struct compilation b;
    struct text first = {0};
    struct text second = {0};
    struct text ir_a = {0};
    struct text ir_b = {0};
    struct ir_module program;
    struct interface *vec;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "vec", vec_source, &first);

    compilation_open(&b);
    ir_module_init(&program, &b.arena, "vec");
    vec = compilation_read(&b, (const uint8_t *)first.data, first.length,
                           &program, error, sizeof error);
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
                  "extern fn anti_rt_alloc_aligned(i64, i64, i64, i64) -> ptr\n"
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
                  "    %0 = call ptr @anti_rt_alloc_aligned(1, size_of vec.V2, 16, "
                  "0)\n"
                  "    ret ptr %0\n"
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
    compilation_close(&b);
    compilation_close(&a);
}

/* A library whose symbolic values and aggregates were made interleaved
   comes out of a read and a write as it went in. The descriptor of a
   class takes the size of the class before its field list makes the
   array that holds the records. */
static void round_trip_class(void)
{
    struct compilation a;
    struct compilation b;
    struct text first = {0};
    struct text second = {0};
    struct ir_module program;
    struct interface *iface;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "boxes",
                  "pub class Box { pub w: int = 1, pub h: int = 2 }\n",
                  &first);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "boxes");
    iface = compilation_read(&b, (const uint8_t *)first.data, first.length,
                             &program, error, sizeof error);
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
    compilation_close(&b);
    compilation_close(&a);
}

/* A constant and an array length computed from size_of stay symbolic in
   the interface and in the IR of a library. A module that imports it uses
   them in its own types. */
static void keeps_symbolic_sizes(void)
{
    struct compilation a;
    struct compilation b;
    struct text first = {0};
    struct text second = {0};
    struct text ir = {0};
    struct ir_module program;
    struct ir_module reread;
    struct interface *sized;
    struct module *module;
    char error[160] = "";
    bool ok;

    compilation_open(&a);
    build_library(&a, "sized",
                  "pub struct H { tag: u8, n: i32 }\n"
                  "pub const N: int = size_of(H) * 2;\n"
                  "pub struct B { data: [N]byte }\n",
                  &first);
    compilation_open(&b);
    ir_module_init(&reread, &b.arena, "sized");
    sized = compilation_read(&b, (const uint8_t *)first.data, first.length,
                             &reread, error, sizeof error);
    CHECK_STR(error, "");
    if (sized != NULL) {
        antl_write(&second, sized, &reread, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
        CHECK(sized->items[1]->value->kind == CONST_SYMBOLIC);
    }
    ir_module_init(&program, &b.arena, "main");
    compilation_add(&b, sized);
    module = compilation_check(&b, "main",
                               "import sized;\n"
                               "fn main() -> int {\n"
                               "    let b = sized.B { data: [3; sized.N] };\n"
                               "    return b.data.len;\n"
                               "}\n",
                               true, &ok);
    CHECK(ok);
    if (ok) {
        lower_as(module, "main", &program, 0);
    }
    ir_print(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type sized.H = struct { tag: i8, n: i32 }\n"
              "type [size_of(sized.H) * 2]byte = array mul i64(size_of sized.H, 2) of i8\n"
              "type sized.B = struct { data: [size_of(sized.H) * 2]byte }\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %0 = slot sized.B\n"
              "    lifestart %0\n"
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
    compilation_close(&b);
    compilation_close(&a);
}

/* Two modules may not export one name, because an export fn has the
   symbol of its name. */
static void unique_exports(void)
{
    struct compilation a;
    struct text bytes = {0};
    char error[160] = "";
    bool ok;
    struct ir_module program;

    compilation_open(&a);
    build_library(&a, "geo", "export fn dot(a: int) -> int { return a; }\n",
                  &bytes);
    compilation_check(&a, "main",
                      "import geo;\n"
                      "export fn dot(b: int) -> int { return b; }\n",
                      true, &ok);
    CHECK(!ok);
    CHECK(a.diags.count == 1);
    if (a.diags.count == 1) {
        CHECK_STR(a.diags.items[0].message,
                  "export fn `dot` has the symbol of export fn `dot` in "
                  "module `geo`");
    }
    ir_module_init(&program, &a.arena, "main");
    CHECK(compilation_read(&a, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
    CHECK(program.function_count == 1 && program.functions[0]->exported);
    ir_module_free(&program);
    text_free(&bytes);
    compilation_close(&a);
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
    /* A file named for the suffix alone names no module, so its empty
       name is refused as every other name that is no identifier. */
    path_of(".anti", NULL, 0, "`` in .anti is not a lowercase identifier");
    path_of("src/.anti", roots, 2,
            "`` in src/.anti is not a lowercase identifier");
    /* A Windows host separates the parts of a path with `\` as well, as
       its programs and a script that joins a path there write it. */
    if (platform_separator() == '\\') {
        static const char *const windows_roots[] = {"C:\\work\\src"};
        path_of("C:\\work\\bench\\objects.anti", NULL, 0, "objects");
        path_of("C:\\work\\src\\com\\niese\\geo.anti", windows_roots, 1,
                "com.niese.geo");
        path_of("C:\\work\\src/com\\niese/geo.anti", windows_roots, 1,
                "com.niese.geo");
    }
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
    struct compilation s;
    bool ok;

    compilation_open(&s);
    library(&s, "com.example.scale", "pub fn scale(x: int) -> int { return x; }\n");
    library(&s, "org.other.scale", "pub fn half(x: int) -> int { return x; }\n");
    compilation_check(&s, "main",
                      "import com.example.scale;\n"
                      "import org.other.scale as other;\n"
                      "fn main() -> int { return scale.scale(2) + other.half(4); }\n",
                      true, &ok);
    CHECK(ok);
    CHECK(s.diags.count == 0);
    compilation_check(&s, "main",
                      "import com.example.scale;\n"
                      "import org.other.scale;\n",
                      true, &ok);
    CHECK(!ok && s.diags.count == 1 &&
          strcmp(s.diags.items[0].message, "`scale` is already declared") == 0);
    diagnostics_free(&s.diags);
    memset(&s.diags, 0, sizeof s.diags);
    compilation_check(&s, "main", "import com.Example.scale;\n", true, &ok);
    CHECK(!ok && s.diags.count == 1 &&
          strcmp(s.diags.items[0].message,
                 "the module path `com.Example.scale` is not lowercase") == 0);
    compilation_close(&s);
}

/* Write a library of source with the given package header. */
static void write_with(struct compilation *s, const char *source,
                       const struct package *package, bool strip,
                       struct text *bytes)
{
    struct interface *iface = arena_alloc(&s->arena, sizeof *iface);
    struct ir_module ir;
    struct module *module;
    bool ok;

    module = compilation_check(s, "com.example.geo", source, true, &ok);
    ir_module_init(&ir, &s->arena, "com.example.geo");
    CHECK(ok);
    if (ok) {
        lower_as(module, "com.example.geo", &ir, 0);
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
    struct compilation a;
    struct compilation b;
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
    compilation_open(&a);
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
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    read = compilation_read(&b, (const uint8_t *)lines.data, lines.length,
                            &program, error, sizeof error);
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
    compilation_close(&b);
    text_free(&lines);
    text_free(&blocks);
    text_free(&stripped);
    text_free(&bare);
    compilation_close(&a);
}

/* A private struct that a pub item reaches keeps its fields and loses the
   /// text of them, like every other private item. */
static void private_field_docs(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct interface *read;
    struct ir_module program;
    char error[160] = "";

    compilation_open(&a);
    write_with(&a,
               "struct Inner {\n"
               "    /// Private, not in the file.\n"
               "    v: int,\n"
               "}\n"
               "pub fn make() -> Inner { return Inner { v: 1 }; }\n",
               NULL, false, &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    read = compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                            &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(read != NULL && read->item_count == 1);
    if (read != NULL && read->item_count == 1) {
        const struct type *inner = read->items[0]->type->result;
        CHECK(inner->field_count == 1);
        CHECK(inner->field_count == 1 && inner->fields[0].doc.length == 0);
    }
    ir_module_free(&program);
    compilation_close(&b);
    text_free(&bytes);
    compilation_close(&a);
}

/* A library keeps how each narrow parameter extends to 32 bits. */
static void keeps_extensions(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "narrow",
                  "extern fn putb(b: u8) -> i32;\n"
                  "pub fn pick(a: i8, b: u16, c: bool, d: i32) -> i32 {\n"
                  "    return putb(1);\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "narrow");
    CHECK(compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
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
    compilation_close(&b);
    compilation_close(&a);
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

/* A library keeps the type of "Aliasing of views" of every access, a
   field of a struct with its field and an element with its scalar type,
   and writes it again byte for byte. */
static void keeps_access_types(void)
{
    struct compilation a;
    struct compilation b;
    struct text first = {0};
    struct text second = {0};
    struct text ir = {0};
    struct ir_module program;
    struct interface *read;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "geo",
                  "pub struct P { x: int, y: float }\n"
                  "pub fn y(p: *P) -> float { return p.y; }\n"
                  "pub fn at(a: []int, i: int) -> int { return a[i]; }\n",
                  &first);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "geo");
    read = compilation_read(&b, (const uint8_t *)first.data, first.length,
                            &program, error, sizeof error);
    CHECK_STR(error, "");
    CHECK(read != NULL);
    if (read != NULL) {
        antl_write(&second, read, &program, false);
        CHECK(first.length == second.length &&
              memcmp(first.data, second.data, first.length) == 0);
        ir_print_own(&ir, &program);
        CHECK(strstr(text_cstr(&ir), "load f64 %1 !type geo.P.y\n") != NULL);
        CHECK(strstr(text_cstr(&ir), "!type i64\n") != NULL);
    }
    text_free(&first);
    text_free(&second);
    text_free(&ir);
    ir_module_free(&program);
    compilation_close(&b);
    compilation_close(&a);
}

/* A library keeps the memory effects of a runtime function it calls. A
   second library that declares the same function without them leaves the
   program with the weaker of the two, as for a function that never
   returns. */
static void keeps_effects(void)
{
    struct compilation a;
    struct compilation b;
    struct compilation c;
    struct text same = {0};
    struct text mine = {0};
    struct text ir = {0};
    struct ir_module program;
    const struct ir_function *f;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "same",
                  "pub fn eq(a: str, b: str) -> bool {\n"
                  "    return a == b;\n"
                  "}\n",
                  &same);
    compilation_open(&b);
    build_library(&b, "mine",
                  "extern fn anti_rt_same_bytes(a: ?*byte, an: int, "
                  "b: ?*byte, bn: int) -> c_int;\n"
                  "pub fn eq(a: str, b: str) -> bool {\n"
                  "    return anti_rt_same_bytes(a.ptr, a.len, b.ptr, "
                  "b.len) != 0;\n"
                  "}\n",
                  &mine);
    compilation_open(&c);
    ir_module_init(&program, &c.arena, "main");
    CHECK(compilation_read(&c, (const uint8_t *)same.data, same.length,
                           &program, error, sizeof error) != NULL);
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
    CHECK(compilation_read(&c, (const uint8_t *)mine.data, mine.length,
                           &program, error, sizeof error) != NULL);
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
    compilation_close(&c);
    compilation_close(&b);
    compilation_close(&a);
}

/* A library keeps the globals of its literals, and a module that imports
   it numbers its own literals from 0. */
static void keeps_literals(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    compilation_open(&a);
    build_library(&a, "words",
                  "pub fn hi() -> str {\n"
                  "    return \"hi\";\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    compilation_add(&b, compilation_read(&b, (const uint8_t *)bytes.data,
                                         bytes.length, &program, error,
                                         sizeof error));
    CHECK_STR(error, "");
    module = compilation_check(&b, "main",
                               "import words;\n"
                               "fn main() -> int {\n"
                               "    return words.hi().len + \"hi\".len;\n"
                               "}\n",
                               true, &ok);
    CHECK(ok);
    if (ok) {
        lower_as(module, "main", &program, 0);
    }
    ir_print_own(&ir, &program);
    CHECK_STR(text_cstr(&ir),
              "type str = struct { ptr: ptr, len: i64 }\n"
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64) -> never\n"
              "global words.0 size 3 align 1 bytes 68 69 00\n"
              "global main.0 size 3 align 1 bytes 68 69 00\n"
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
              "    %1 = ptradd inbounds %0, offset_of str.len\n"
              "    %2 = load i64 %1 !type i64\n"
              "    %4 = addr @main.0\n"
              "    store ptr %4, %3\n"
              "    %5 = ptradd %3, offset_of str.len\n"
              "    store i64 2, %5\n"
              "    %6 = ptradd inbounds %3, offset_of str.len\n"
              "    %7 = load i64 %6 !type i64\n"
              "    %8 = addov nsw i64 %2, %7\n"
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
    compilation_close(&b);
    compilation_close(&a);
}

/* A library keeps the value of an aggregate constant, which the back end
   turns into bytes for its target. */
static void keeps_constants(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    compilation_open(&a);
    build_library(&a, "pair",
                  "pub struct Pair { a: u8, b: i32 }\n"
                  "const BASE: Pair = Pair { a: 7, b: 11 };\n"
                  "pub fn base() -> Pair {\n"
                  "    return BASE;\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    compilation_add(&b, compilation_read(&b, (const uint8_t *)bytes.data,
                                         bytes.length, &program, error,
                                         sizeof error));
    CHECK_STR(error, "");
    module = compilation_check(&b, "main",
                               "import pair;\n"
                               "fn main() -> int {\n"
                               "    return pair.base().b as int;\n"
                               "}\n",
                               true, &ok);
    CHECK(ok);
    if (ok) {
        lower_as(module, "main", &program, 0);
    }
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir),
                 "global pair.6 pair.Pair { i8 7, i32 11 }\n") != NULL);
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    compilation_close(&b);
    compilation_close(&a);
}

/* An f16 constant and a struct of f16 fields cross a library file. The
   constant is its value, which the importer writes as its bits. */
static void keeps_halves(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    struct module *module;
    char error[160] = "";
    bool ok;

    compilation_open(&a);
    build_library(&a, "tone",
                  "pub struct Texel { u: f16, v: f16 }\n"
                  "pub const ONE: f16 = 1.0 as f16;\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    compilation_add(&b, compilation_read(&b, (const uint8_t *)bytes.data,
                                         bytes.length, &program, error,
                                         sizeof error));
    CHECK_STR(error, "");
    module = compilation_check(&b, "main",
                               "import tone;\n"
                               "fn main() -> int {\n"
                               "    let t = tone.Texel { u: tone.ONE as f16,"
                               " v: 0.5 as f16 };\n"
                               "    return (t.u + t.v) as int;\n"
                               "}\n",
                               true, &ok);
    CHECK(ok);
    if (ok) {
        lower_as(module, "main", &program, 0);
    }
    ir_print(&ir, &program);
    CHECK(strstr(text_cstr(&ir), "hext f32 15360\n") != NULL);
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    compilation_close(&b);
    compilation_close(&a);
}

/* A call through a function pointer keeps its signature in the library
   file. */
/* A C function with an aggregate result keeps its aggregate in a library
   file, as a function of the module does. An `extern fn` takes C types
   alone, by R126 of "Rules of 2026-10-13", and a function of the runtime
   is the one that gives a `str`. */
static void keeps_extern_aggregates(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "wrap",
                  "extern fn anti_rt_make() -> str;\n"
                  "pub fn wrap() -> str {\n"
                  "    return anti_rt_make();\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "wrap");
    CHECK(compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
    CHECK_STR(error, "");
    CHECK(program.function_count == 2 &&
          program.functions[0]->result_agg != IR_NO_AGG);
    if (program.function_count == 2 &&
        program.functions[0]->result_agg != IR_NO_AGG) {
        ir_print(&ir, &program);
        CHECK_STR(text_cstr(&ir),
                  "type str = struct { ptr: ptr, len: i64 }\n"
                  "extern fn anti_rt_make() -> agg str\n"
                  "fn wrap.wrap() -> agg str {\n"
                  "b0:\n"
                  "    %0 = call agg @anti_rt_make()\n"
                  "    ret ptr %0\n"
                  "}\n");
    }
    text_free(&bytes);
    text_free(&ir);
    ir_module_free(&program);
    compilation_close(&b);
    compilation_close(&a);
}

/* A parameter that does not keep its argument crosses the library file
   in its form of two words, `concurrent` included. The module that loads
   it passes the code and the context. */
static void keeps_context_signatures(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";
    const struct interface *lib;
    const struct type *f = NULL;

    compilation_open(&a);
    build_library(&a, "calls",
                  "pub fn run(concurrent f: fn(i16) -> int) -> int {\n"
                  "    return f(1);\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "calls");
    lib = compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error);
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
    compilation_close(&b);
    compilation_close(&a);
}

static void keeps_signatures(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "calls",
                  "pub fn run(keep f: fn(i16) -> int) -> int {\n"
                  "    return f(1);\n"
                  "}\n",
                  &bytes);
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "calls");
    CHECK(compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
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
    compilation_close(&b);
    compilation_close(&a);
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
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct ir_module program;
    char error[160] = "";
    const struct interface *lib;
    const struct item *eq;
    const struct item *peek;
    const struct item *mixed;
    const struct expr *call = NULL;
    const struct expr *at = NULL;

    compilation_open(&a);
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
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "marks");
    lib = compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error);
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
    compilation_close(&b);
    compilation_close(&a);
}

/* A struct keeps one identity when two libraries mention it. */
static void dependencies(void)
{
    struct compilation a;
    struct compilation b;
    struct text vec_bytes = {0};
    struct text shapes_bytes = {0};
    struct text ir = {0};
    struct ir_module program;
    const struct interface *libs[2];
    struct antl_context with_vec;
    char error[160] = "";

    compilation_open(&a);
    build_library(&a, "vec", vec_source, &vec_bytes);
    build_library(&a, "shapes", shapes_source, &shapes_bytes);

    compilation_open(&b);
    ir_module_init(&program, &b.arena, "main");
    CHECK(compilation_read(&b, (const uint8_t *)shapes_bytes.data,
                           shapes_bytes.length, &program, error,
                           sizeof error) == NULL);
    CHECK_STR(error, "needs module `vec`");
    libs[0] = compilation_read(&b, (const uint8_t *)vec_bytes.data,
                               vec_bytes.length, &program, error,
                               sizeof error);
    CHECK(libs[0] != NULL);
    with_vec.libraries = libs;
    with_vec.library_count = 1;
    with_vec.types = &b.types;
    with_vec.arena = &b.arena;
    with_vec.program = &program;
    libs[1] = antl_read((const uint8_t *)shapes_bytes.data,
                        shapes_bytes.length, &with_vec, error, sizeof error);
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
                  "extern fn anti_rt_alloc_aligned(i64, i64, i64, i64) -> ptr\n"
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
                  "    %0 = call ptr @anti_rt_alloc_aligned(1, size_of vec.V2, 16, "
                  "0)\n"
                  "    ret ptr %0\n"
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
    compilation_close(&b);
    compilation_close(&a);
}

static void refuses_file(const uint8_t *data, size_t size,
                         const char *expected)
{
    struct compilation s;
    struct ir_module program;
    char error[160] = "";

    compilation_open(&s);
    ir_module_init(&program, &s.arena, "main");
    CHECK(compilation_read(&s, data, size, &program, error,
                           sizeof error) == NULL);
    if (expected != NULL) {
        CHECK_STR(error, expected);
    } else {
        CHECK(error[0] != '\0');
    }
    ir_module_free(&program);
    compilation_close(&s);
}

static bool reads_file(const struct text *bytes);

/* A function that waits, appended to the last count of scale_antl: the
   name `f`, the kind, line 1 and the module `m`. A kind past the last of
   enum wait_kind is damage. */
static void damaged_wait(void)
{
    uint8_t longer[sizeof scale_antl + 18];
    uint8_t *record = longer + sizeof scale_antl;

    memcpy(longer, scale_antl, sizeof scale_antl);
    longer[sizeof scale_antl - 4] = 1;
    memset(record, 0, 18);
    record[0] = 1;
    record[4] = 'f';
    record[5] = WAIT_KIND_COUNT;
    record[9] = 1;
    record[13] = 1;
    record[17] = 'm';
    refuses_file(longer, sizeof longer, NULL);
}

static void damaged_files(void)
{
    /* Where the fields a poke below reaches sit, counted back from the
       end of the file. First the three counts of the lock orders and the
       classes, two instructions of 53 bytes each and the 15 bytes that
       open the body. Then the one parameter of
       the signature with its facts, its count, the source of the function
       and its result with its extension. */
    enum {
        TAIL = 12 + 4 + 2 * 53 + 15,
        MUL_OPERAND = 12 + 4 + 2 * 53 - 12,
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
    copy[4] = 93;
    refuses_file(copy, sizeof copy,
                 "has format version 93, and antic reads version 92");
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
    /* The flags of a function, right before its effects, take eight
       bits. The sixth marks that it writes tables, and the seventh that
       it allocates, which a C function alone does. The eighth marks the
       range of the result, whose two words follow its extension, and the
       file holds none. */
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
    /* S16 of the first audit: a body without a block. The count of blocks
       follows the temporaries, and the classes follow the blocks. */
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
    struct compilation s;
    struct ir_module program;
    char error[160] = "";
    bool ok;

    compilation_open(&s);
    ir_module_init(&program, &s.arena, "main");
    ok = compilation_read(&s, (const uint8_t *)bytes->data, bytes->length,
                          &program, error, sizeof error) != NULL;
    if (!ok) {
        fprintf(stderr, "  %s\n", error);
    }
    ir_module_free(&program);
    compilation_close(&s);
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
    struct compilation a;
    struct compilation b;
    struct text first = {0};
    struct text second = {0};
    struct ir_module program;
    struct interface *iface;
    char error[160] = "";
    size_t at = SIZE_MAX;
    size_t i;

    compilation_open(&a);
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
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "boxes");
    iface = compilation_read(&b, (const uint8_t *)first.data, first.length,
                             &program, error, sizeof error);
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
    compilation_close(&b);
    compilation_close(&a);
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
    struct compilation s;
    struct text bytes = {0};
    uint8_t value[10];
    size_t at;
    uint32_t int_type;
    uint32_t f64_type;
    uint32_t fn_type;

    compilation_open(&s);
    if (!build_library(&s, "zz", record_source, &bytes)) {
        compilation_close(&s);
        return;
    }
    CHECK(reads_file(&bytes));
    int_type = u32_at(&bytes, name_end(&bytes, "ZZK", false));
    f64_type = u32_at(&bytes, name_end(&bytes, "ZZF", false));
    fn_type = u32_at(&bytes, name_end(&bytes, "zbits", false));

    /* S12 of the first audit: a member of a class body whose type is `int`,
       and one whose type has a parameter the declaration did not write. */
    at = name_end(&bytes, "zzm", false) + 4;
    poke_u32(value, int_type);
    refuses_poke(&bytes, at, value, 4);
    poke_u32(value, fn_type);
    refuses_poke(&bytes, at, value, 4);

    /* S15 of the first audit: a jump and a branch whose target is the integer
       65536. */
    memset(value, 0, sizeof value);
    value[0] = IR_INT;
    value[1] = IR_VOID;
    value[4] = 1;
    refuses_poke(&bytes, block_operand(&bytes, IR_JUMP, 0), value, 10);
    refuses_poke(&bytes, block_operand(&bytes, IR_BRANCH, 1), value, 10);
    refuses_poke(&bytes, block_operand(&bytes, IR_BRANCH, 2), value, 10);

    /* S1 of the first audit: a simd aggregate of the IR whose lane is the unit
       break `_`, and one whose lane is a bitfield. */
    at = name_end(&bytes, "q", true);
    refuses_poke(&bytes, at - 1, "_", 1);
    value[0] = 4;
    refuses_poke(&bytes, at + 5, value, 1);

    /* M3 of the first audit: a bitfield of 40 bits on an `i8` field, in the
       type table and in the aggregate of the IR. */
    value[0] = 40;
    refuses_poke(&bytes, name_end(&bytes, "zw", false) + 4, value, 1);
    refuses_poke(&bytes, name_end(&bytes, "zw", true) + 5, value, 1);

    /* M4 of the first audit: an aggregate of the IR aligned to 3. Before its
       first field stand the count, the empty length text, the length and the
       alignment. */
    at = name_end(&bytes, "zw", true) - 6 - 4 - 4 - 4 - 8;
    memset(value, 0, sizeof value);
    value[0] = 3;
    refuses_poke(&bytes, at, value, 8);

    /* M5 of the first audit: the text constant of `ZZT` typed as a slice of
       `int`. The slice is the one record of its kind in the type table, which
       ends before the items. */
    at = find_slice(&bytes, name_end(&bytes, "ZZK", false));
    poke_u32(value, int_type);
    refuses_poke(&bytes, at, value, 4);

    /* M6 of the first audit: the enum `ZE` over `f64`. */
    at = name_end(&bytes, "ZE", false);
    poke_u32(value, f64_type);
    refuses_poke(&bytes, at, value, 4);

    text_free(&bytes);
    compilation_close(&s);
}

/* Whether the file reads into a program whose IR passes the verifier,
   as the driver takes a library it links. */
static bool verifies_file(const uint8_t *data, size_t size)
{
    struct compilation s;
    struct ir_module program;
    struct text errors = {0};
    char error[160] = "";
    bool ok;

    compilation_open(&s);
    ir_module_init(&program, &s.arena, "main");
    ok = compilation_read(&s, data, size, &program, error,
                          sizeof error) != NULL &&
         ir_verify(&program, &errors);
    text_free(&errors);
    ir_module_free(&program);
    compilation_close(&s);
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
    struct compilation s;
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
    compilation_open(&s);
    if (!build_library(&s, "zs", text_cstr(&source), &bytes)) {
        text_free(&source);
        compilation_close(&s);
        return;
    }
    CHECK(verifies_file((const uint8_t *)bytes.data, bytes.length));

    /* S03 of the second audit: the lane `zq` of `ZV` in the type table, as
       every other type of the table, `ZZ` of no bytes and `f64` among them. */
    lane = name_end(&bytes, "zq", false);
    own = u32_at(&bytes, lane);
    for (i = 0; i < 64; i++) {
        if (i != own) {
            poke_u32(value, i);
            refuses_poke(&bytes, lane, value, 4);
        }
    }
    /* S03 of the second audit: the same lane in the IR, as every other scalar
       and as each aggregate, `ZZ` among them. */
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

    /* S24 of the second audit: three lanes, and one lane of four bytes, in
       both tables. */
    value[0] = 16;
    refuses_poke(&bytes, type_flags(&bytes, "za"), value, 1);
    refuses_poke(&bytes, type_flags(&bytes, "zo"), value, 1);
    value[0] = 2;
    refuses_poke(&bytes, agg_flags(&bytes, "za"), value, 1);
    refuses_poke(&bytes, agg_flags(&bytes, "zo"), value, 1);
    /* S24 of the second audit: `ZV` packed, and `ZV` aligned to 16, in both
       tables. The alignment follows the flags, the safety and the empty
       `compatible` line in the type table, and the flags in the IR. */
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

    /* S24 of the second audit: the sum over `ZV` made a sum over each other
       aggregate, `ZW` above the vector cap among them. */
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
    compilation_close(&s);
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
                                        struct compilation *s,
                                        const char *name,
                                        const char *field)
{
    struct ir_module program;
    const struct interface *iface;
    const struct type *found = NULL;
    char error[160] = "";
    size_t i;
    size_t j;

    ir_module_init(&program, &s->arena, "main");
    iface = compilation_read(s, (const uint8_t *)bytes->data, bytes->length,
                             &program, error, sizeof error);
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
    struct compilation s;
    struct compilation r;
    struct text bytes = {0};
    struct ir_module program;
    const struct ir_global *g;
    char error[160] = "";
    uint8_t value[8];
    size_t at;
    uint32_t own;
    uint32_t i;

    compilation_open(&s);
    if (!build_library(&s, "zk", const_source, &bytes)) {
        compilation_close(&s);
        return;
    }
    CHECK(reads_file(&bytes));

    /* The default `none` of `zp` given each other type of the table:
       a file that reads has a pointer or a function that may be `none`
       there, and never `*ZD`. */
    at = name_end(&bytes, "zp", false);
    own = u32_at(&bytes, at);
    for (i = 0; i < 64; i++) {
        struct compilation t;
        struct text poked = {0};
        const struct type *type;
        if (i == own) {
            continue;
        }
        text_append_bytes(&poked, bytes.data, bytes.length);
        poke_u32((uint8_t *)poked.data + at, i);
        compilation_open(&t);
        type = field_type_of(&poked, &t, "ZN", "zp");
        CHECK(type == NULL || types_is_nullable(type));
        compilation_close(&t);
        text_free(&poked);
    }

    /* S23 of the second audit: the scalar of the first item of a global's
       value made an aggregate and void. The global's name, its size, its
       alignment, its bytes, its relocations and its marks come before the
       value, and the value opens with its kind, its scalar, its count and its
       aggregate. */
    compilation_open(&r);
    ir_module_init(&program, &r.arena, "main");
    CHECK(compilation_read(&r, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
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
    compilation_close(&r);

    text_free(&bytes);
    compilation_close(&s);
}

/* bytes with the relocations of the global named name, which has none,
   replaced by count addresses at offsets, each of global 0. */
static void with_relocations(const struct text *bytes, const char *name,
                             const uint64_t *offsets, uint32_t count,
                             uint8_t to_function, struct text *out)
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
        word[0] = to_function;
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
    struct compilation s;
    struct text bytes = {0};
    struct text changed = {0};

    compilation_open(&s);
    if (!build_library(&s, "zr", source, &bytes)) {
        compilation_close(&s);
        return;
    }
    with_relocations(&bytes, "0", apart, 2, 0, &changed);
    CHECK(reads_file(&changed));
    with_relocations(&bytes, "0", overlap, 2, 0, &changed);
    refuses_file((const uint8_t *)changed.data, changed.length, NULL);
    with_relocations(&bytes, "0", past, 1, 0, &changed);
    refuses_file((const uint8_t *)changed.data, changed.length, NULL);
    /* Rule 14: whether the target is a function is a flag, and the marks
       of a global take bits 0 to 3. */
    with_relocations(&bytes, "0", apart, 2, 2, &changed);
    refuses_file((const uint8_t *)changed.data, changed.length, NULL);
    {
        size_t at = name_end(&bytes, "0", false);
        uint64_t size = (uint64_t)u32_at(&bytes, at) |
                        (uint64_t)u32_at(&bytes, at + 4) << 32;
        size_t marks = at + 8 + 8 + (size_t)size + 4;
        const uint8_t unknown = (uint8_t)(bytes.data[marks] | 16);
        refuses_poke(&bytes, marks, &unknown, 1);
    }
    text_free(&changed);
    text_free(&bytes);
    compilation_close(&s);
}

/* Rule 14: the address of a constant of the IR names a global by an index
   of 64 bits, which the reader holds to the count of the globals whole.
   The descriptor of `V2` starts with the address of its name. */
static void wide_addresses(void)
{
    struct compilation s;
    struct text bytes = {0};
    size_t at;
    uint64_t size;
    size_t item;
    const uint8_t high = 1;

    compilation_open(&s);
    if (!build_library(&s, "vec", vec_source, &bytes)) {
        compilation_close(&s);
        return;
    }
    at = name_end(&bytes, "V2.descriptor", false);
    size = (uint64_t)u32_at(&bytes, at) | (uint64_t)u32_at(&bytes, at + 4)
                                              << 32;
    /* The size, the alignment, the data, no addresses, the marks, and
       the kind, the type, the count and the aggregate of the value. */
    item = at + 8 + 8 + (size_t)size + 4 + 1 + 1 + 1 + 8 + 5;
    CHECK(item + 10 <= bytes.length &&
          (uint8_t)bytes.data[item] == IR_CONST_ADDR);
    CHECK(reads_file(&bytes));
    refuses_poke(&bytes, item + 2 + 4, &high, 1);
    text_free(&bytes);
    compilation_close(&s);
}

/* M336: every type of `anti.lang` that the compiler declares, in the
   signature of a function, in the hidden lock of a synchronized class
   and in the Job of a generic. */
static const char declared_source[] =
    "pub class Task { pub n: int = 0, }\n"
    "pub worker fn add(t: *Task, k: int) -> int {\n"
    "    return t.n + k;\n"
    "}\n"
    "pub synchronized class Guarded\n"
    "{\n"
    "    pub n: int = 0,\n"
    "    pub fn get(self) -> int\n"
    "    {\n"
    "        return self.n;\n"
    "    }\n"
    "}\n"
    "pub fn every(o: *Object, f: *Flags, m: *Mutex, r: *Regex,"
    " br: *ByteRegex, mt: *Match, bm: *ByteMatch, fd: *FieldDescriptor,"
    " c: *chan int) -> int {\n"
    "    return 0;\n"
    "}\n"
    "pub fn job<T>(k: int, extra: T) -> int {\n"
    "    let a = alloc Task { n: 1 };\n"
    "    let one = dispatch a -> add(k);\n"
    "    let n = join(one);\n"
    "    delete(a);\n"
    "    return n;\n"
    "}\n";

/* The symbol of the item name of lib, or NULL. */
static const struct symbol *item_of(const struct interface *lib,
                                    const char *name)
{
    size_t i;

    for (i = 0; lib != NULL && i < lib->item_count; i++) {
        if (lib->items[i]->name.length == strlen(name) &&
            memcmp(lib->items[i]->name.text, name, strlen(name)) == 0) {
            return lib->items[i];
        }
    }
    return NULL;
}

/* M336: a library file names each type the compiler declares and
   declares none, and the reader gives the compiler's own type for each
   name. The file reads back and writes again byte for byte. */
static void declared_types(void)
{
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct text again = {0};
    struct ir_module program;
    const struct interface *read;
    const struct symbol *every;
    const struct symbol *guarded;
    char error[160] = "";

    compilation_open(&a);
    if (!build_library(&a, "decl", declared_source, &bytes)) {
        compilation_close(&a);
        return;
    }
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "decl");
    read = compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                            &program, error, sizeof error);
    CHECK_STR(error, "");
    every = item_of(read, "every");
    guarded = item_of(read, "Guarded");
    CHECK(every != NULL && every->type->param_count == 9);
    if (every != NULL && every->type->param_count == 9) {
        struct type *const *p = every->type->params;
        CHECK(p[0]->element == types_object(&b.types));
        CHECK(p[1]->element == types_flags(&b.types));
        CHECK(p[2]->element == types_mutex(&b.types));
        CHECK(p[3]->element == types_regex(&b.types));
        CHECK(p[4]->element == types_byte_regex(&b.types));
        CHECK(p[5]->element == types_match(&b.types, NULL));
        CHECK(p[6]->element == types_match_of(&b.types, true));
        CHECK(p[7]->element == types_field_descriptor(&b.types));
        CHECK(p[8]->element ==
              types_chan(&b.types, types_builtin(&b.types, TYPE_I64)));
    }
    CHECK(guarded != NULL && guarded->type->field_count > 0);
    if (guarded != NULL) {
        size_t i;
        size_t locks = 0;
        for (i = 0; i < guarded->type->field_count; i++) {
            locks += guarded->type->fields[i].type ==
                     types_object_lock(&b.types);
        }
        CHECK(locks == 1);
    }
    if (read != NULL) {
        antl_write(&again, read, &program, false);
        CHECK(again.length == bytes.length &&
              memcmp(again.data, bytes.data, bytes.length) == 0);
    }
    ir_module_free(&program);
    compilation_close(&b);
    text_free(&again);
    text_free(&bytes);
    compilation_close(&a);
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
    struct compilation a;
    struct compilation b;
    struct interface *iface;
    struct ir_module ir;
    struct ir_module program;
    struct module *module;
    struct text bytes = {0};
    struct text before = {0};
    struct text after = {0};
    char error[160] = "";
    bool ok;

    compilation_open(&a);
    module = compilation_check(&a, "shapes", source, true, &ok);
    ir_module_init(&ir, &a.arena, "shapes");
    if (ok) {
        lower_as(module, "shapes", &ir, 0);
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
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "");
    CHECK(compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                           &program, error, sizeof error) != NULL);
    CHECK_STR(error, "");
    ir_print(&after, &program);
    CHECK_STR(text_cstr(&after), text_cstr(&before));
    text_free(&bytes);
    text_free(&before);
    text_free(&after);
    ir_module_free(&program);
    ir_module_free(&ir);
    compilation_close(&b);
    compilation_close(&a);
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
    struct compilation s;
    struct interface *iface;
    struct ir_module lib;
    struct ir_module ir;
    struct module *module;
    const struct ir_global *g;
    size_t i;
    bool ok;

    compilation_open(&s);
    module = compilation_check(&s, "vec", vec, true, &ok);
    ir_module_init(&lib, &s.arena, "vec");
    if (ok) {
        lower_as(module, "vec", &lib, 0);
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
        compilation_add(&s, iface);
    }

    module = compilation_check(&s, "main", source, true, &ok);
    ir_module_init(&ir, &s.arena, "main");
    if (ok) {
        lower_as(module, "main", &ir, 0);
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
    compilation_close(&s);
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
enum { SCALE_TYPES = 67, SCALE_ITEMS = 86, SCALE_SYMS = 183, SCALE_AGGS = 187 };

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
        put_u8(b, 0);                   /* no bitfield */
        put_u8(b, 0);                   /* no extension */
        put_u8(b, 0);                   /* not atomic */
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
        put_u32(b, 0);                  /* no functions */
        put_u32(b, 0);                  /* no private functions */
        put_u32(b, 0);                  /* no constants or static fields */
    }
    /* No field has a default value. */
    for (k = 0; k < n; k++) {
        put_u8(b, 0);
    }
    text_append_bytes(b, scale_antl + SCALE_ITEMS,
                      sizeof scale_antl - SCALE_ITEMS);
}

/* S13 of the first audit: a table of a library file that nests deeper than the
   reader allows is refused. Its entries may point forward, which the reader
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

/* scale_antl with n entries of kind after its two types, each built over
   the entry before it and the first over `uint`. Each stands one level
   above the entry it is built over. */
static void type_chain(struct text *b, uint32_t n, uint8_t kind)
{
    uint32_t k;

    text_append_bytes(b, scale_antl, SCALE_TYPES);
    put_u32(b, 2 + n);
    text_append_bytes(b, scale_antl + SCALE_TYPES + 4,
                      SCALE_ITEMS - SCALE_TYPES - 4);
    for (k = 0; k < n; k++) {
        uint32_t below = k == 0 ? 0 : 1 + k;
        put_u8(b, kind);
        switch (kind) {
        case TYPE_POINTER:
        case TYPE_SLICE:
            put_u32(b, below);
            put_u8(b, 0);               /* no marks */
            break;
        case TYPE_ARRAY:
            put_u32(b, below);
            put_u8(b, 0);               /* a number of elements */
            put_u64(b, 2);
            break;
        case TYPE_FN:
            put_u32(b, 0);              /* no parameters */
            put_u32(b, below);
            put_u8(b, 0);
            break;
        default:                        /* a tuple of two */
            put_u32(b, 2);
            put_u32(b, below);
            put_u32(b, below);
            break;
        }
    }
    text_append_bytes(b, scale_antl + SCALE_ITEMS,
                      sizeof scale_antl - SCALE_ITEMS);
}

/* S305: a pointer, a slice, an array, a function type or a tuple of the
   type table nests at most TYPES_NEST_MAX levels deep, as the checker
   holds a type the program writes. A deeper one is refused before a walk
   over its parts recurses once per level. */
static void deep_types(void)
{
    static const uint8_t kinds[] = {TYPE_POINTER, TYPE_SLICE, TYPE_ARRAY,
                                    TYPE_FN, TYPE_TUPLE};
    size_t i;

    for (i = 0; i < sizeof kinds; i++) {
        struct text b = {0};
        type_chain(&b, TYPES_NEST_MAX, kinds[i]);
        CHECK(reads_file(&b));
        text_free(&b);
        type_chain(&b, TYPES_NEST_MAX + 1, kinds[i]);
        refuses_file((const uint8_t *)b.data, b.length, NULL);
        text_free(&b);
    }
}

/* The type of parameter 0 of the item name of lib, or NULL. */
static const struct type *param_of(const struct interface *lib,
                                   const char *name)
{
    size_t i;

    for (i = 0; lib != NULL && i < lib->item_count; i++) {
        const struct symbol *sym = lib->items[i];
        if (sym->name.length == strlen(name) &&
            memcmp(sym->name.text, name, sym->name.length) == 0 &&
            sym->type != NULL && sym->type->kind == TYPE_FN &&
            sym->type->param_count > 0) {
            return sym->type->params[0];
        }
    }
    return NULL;
}

/* S305: a copy of a generic the reader makes stands one level above its
   deepest argument, as the copy the checker makes does, so the limit
   holds a chain of copies as well. */
static void copy_depths(void)
{
    static const char source[] = "pub struct Box<T> { v: T }\n"
                                 "pub fn deep(b: Box<Box<Box<int>>>) -> int {\n"
                                 "    return 0;\n"
                                 "}\n";
    struct compilation a;
    struct compilation b;
    struct text bytes = {0};
    struct ir_module program;
    const struct interface *read;
    const struct type *written;
    const struct type *got;
    char error[160] = "";

    compilation_open(&a);
    if (!build_library(&a, "boxes", source, &bytes)) {
        compilation_close(&a);
        return;
    }
    written = param_of(a.libraries[0], "deep");
    compilation_open(&b);
    ir_module_init(&program, &b.arena, "boxes");
    read = compilation_read(&b, (const uint8_t *)bytes.data, bytes.length,
                            &program, error, sizeof error);
    CHECK_STR(error, "");
    got = param_of(read, "deep");
    CHECK(written != NULL && written->depth == 3);
    CHECK(got != NULL && got->depth == 3);
    ir_module_free(&program);
    compilation_close(&b);
    text_free(&bytes);
    compilation_close(&a);
}

/* M329: a library of structs that nest levels deep, a constant of the
   outermost, which a function keeps as a global of the IR, and an array
   whose length is a sum of levels terms of `size_of`. */
static void deep_source(struct text *out, int levels)
{
    char line[96];
    int k;

    for (k = 0; k < levels; k++) {
        if (k + 1 < levels) {
            snprintf(line, sizeof line, "pub struct D%d { f: D%d }\n", k,
                     k + 1);
        } else {
            snprintf(line, sizeof line, "pub struct D%d { v: int }\n", k);
        }
        text_append(out, line);
    }
    text_append(out, "pub struct Wide { b: [size_of(int)");
    for (k = 1; k < levels; k++) {
        text_append(out, " + size_of(int)");
    }
    text_append(out, "]u8 }\npub const DEEP: D0 = ");
    for (k = 0; k + 1 < levels; k++) {
        snprintf(line, sizeof line, "D%d { f: ", k);
        text_append(out, line);
    }
    snprintf(line, sizeof line, "D%d { v: 1 }", levels - 1);
    text_append(out, line);
    for (k = 0; k + 1 < levels; k++) {
        text_append(out, " }");
    }
    text_append(out, ";\n"
                     "pub fn deep() -> D0 {\n"
                     "    return DEEP;\n"
                     "}\n"
                     "pub fn wide(w: Wide) -> int {\n"
                     "    return 0;\n"
                     "}\n");
}

/* M329: one limit bounds how deep the tables of a library file nest, the
   limit of the checker. A constant of the type table, a constant of the
   IR and a symbolic value that the checker took read back. */
static void deep_values(void)
{
    struct compilation s;
    struct text source = {0};
    struct text bytes = {0};

    deep_source(&source, 100);
    compilation_open(&s);
    if (build_library(&s, "deepv", text_cstr(&source), &bytes)) {
        CHECK(reads_file(&bytes));
    }
    text_free(&bytes);
    text_free(&source);
    compilation_close(&s);
}

/* scale_antl with an entry of the builtin kind and a struct or a class
   whose one field has that type and form. */
static void field_of_kind(struct text *b, uint8_t kind, uint8_t holder,
                          uint8_t form)
{
    text_append_bytes(b, scale_antl, SCALE_TYPES);
    put_u32(b, 4);
    text_append_bytes(b, scale_antl + SCALE_TYPES + 4,
                      SCALE_ITEMS - SCALE_TYPES - 4);
    put_u8(b, kind);
    put_u8(b, holder);
    put_str(b, "scale");
    put_str(b, "s0");
    put_u8(b, 0);                       /* a plain struct */
    put_u8(b, 0);
    put_u8(b, 0);
    put_str(b, "");
    put_u64(b, 0);
    put_u32(b, 1);
    put_str(b, "f");
    put_u32(b, 2);
    put_u8(b, 0);
    put_u8(b, form);
    put_u8(b, VIS_PUB);
    put_u8(b, 0);
    put_str(b, "");
    put_str(b, "");
    put_u32(b, 0);                      /* no functions */
    put_u32(b, 0);                      /* no private functions */
    put_u32(b, 0);                      /* no constants or static fields */
    put_u8(b, 0);                       /* no default */
    text_append_bytes(b, scale_antl + SCALE_ITEMS,
                      sizeof scale_antl - SCALE_ITEMS);
}

/* Rule 14: the type table carries the type of `none` and the error type
   for the trees, and no field of a struct has either, as none has
   `void`. The base of a class is a class. A field of `int` reads. */
static void field_kinds(void)
{
    static const uint8_t refused[] = {TYPE_VOID, TYPE_NONE, TYPE_ERROR};
    struct text b = {0};
    size_t i;

    field_of_kind(&b, TYPE_I64, TYPE_STRUCT, FIELD_PLAIN);
    CHECK(reads_file(&b));
    text_free(&b);
    for (i = 0; i < sizeof refused; i++) {
        field_of_kind(&b, refused[i], TYPE_STRUCT, FIELD_PLAIN);
        refuses_file((const uint8_t *)b.data, b.length, NULL);
        text_free(&b);
    }
    field_of_kind(&b, TYPE_I64, TYPE_CLASS, FIELD_BASE);
    refuses_file((const uint8_t *)b.data, b.length, NULL);
    text_free(&b);
}

/* Read the library file bytes of module name into a new session and
   write it again into out. False when the reader refuses it. */
static bool read_and_write(const uint8_t *bytes, size_t length,
                           const char *name, struct text *out)
{
    struct compilation s;
    struct ir_module program;
    struct interface *read;
    char error[160] = "";

    compilation_open(&s);
    ir_module_init(&program, &s.arena, name);
    read = compilation_read(&s, bytes, length, &program, error, sizeof error);
    out->length = 0;
    if (read != NULL) {
        antl_write(out, read, &program, false);
    }
    ir_module_free(&program);
    compilation_close(&s);
    return read != NULL;
}

/* Rule 14: a byte that holds a flag holds 0 or 1, and the reader refuses
   any other value. Each byte of 1 is changed to 2 in turn. A reader that
   took the 2 for a 1 would give the file it was written from when it
   writes what it read, so the test refuses that outcome. */
static void flags_refused(const char *name, const char *source)
{
    struct compilation s;
    struct text bytes = {0};
    struct text again = {0};
    uint8_t *copy;
    size_t at;

    compilation_open(&s);
    if (!build_library(&s, name, source, &bytes)) {
        compilation_close(&s);
        return;
    }
    CHECK(read_and_write((const uint8_t *)bytes.data, bytes.length, name,
                         &again));
    CHECK(again.length == bytes.length &&
          memcmp(again.data, bytes.data, bytes.length) == 0);
    copy = alloc_zeroed(bytes.length, 1);
    for (at = 0; at < bytes.length; at++) {
        if ((uint8_t)bytes.data[at] != 1) {
            continue;
        }
        memcpy(copy, bytes.data, bytes.length);
        copy[at] = 2;
        if (read_and_write(copy, bytes.length, name, &again) &&
            again.length == bytes.length &&
            memcmp(again.data, bytes.data, bytes.length) == 0) {
            check_failures++;
            fprintf(stderr, "%s: the flag at byte %zu reads 2 as 1\n", name,
                    at);
        }
    }
    free(copy);
    text_free(&again);
    text_free(&bytes);
    compilation_close(&s);
}

/* S01 of the second audit: the checked tree of a generic that a library file
   carries. Each test below writes the tree of `tree` with one relation the
   checker never leaves, and the reader refuses the file. */
static const char tree_source[] =
    "pub variant TShape { Circle { r: int }, Empty }\n"
    "pub enum TColor: u8 { Red, Green }\n"
    "pub struct TPoint { x: int, y: int }\n"
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
    "    let pt = TPoint { x: n, y: 1 };\n"
    "    let px = pt.x;\n"
    "    let sl = []int { ptr: none, len: 0 };\n"
    "    total = total + px + sl.len;\n"
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
static struct item *tree_of(struct damaged *s)
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
static struct stmt *tree_stmt(struct damaged *s, enum stmt_kind kind,
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
static struct expr *tree_let(struct damaged *s, const char *name)
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

static void otherwise_past_arms(struct damaged *s)
{
    tree_stmt(s, STMT_SWITCH, 0)->as.switch_stmt.otherwise_at = 2;
}

static void arm_case_past_cases(struct damaged *s)
{
    tree_stmt(s, STMT_SWITCH, 0)->as.switch_stmt.arms[0].variant_case =
        65536;
}

static void cast_case_past_cases(struct damaged *s)
{
    tree_let(s, "tested")->as.cast.variant_case = 3;
}

static void literal_case_past_cases(struct damaged *s)
{
    tree_let(s, "made")->as.struct_lit.variant_case = 7;
}

static void enum_value_past_values(struct damaged *s)
{
    tree_let(s, "color")->as.field.enum_value = 9;
}

static void name_without_symbol(struct damaged *s)
{
    tree_let(s, "sum")->as.binary.right->symbol = NULL;
}

static void operand_without_type(struct damaged *s)
{
    tree_let(s, "neg")->as.unary.operand->type = NULL;
}

static void pattern_without_element(struct damaged *s)
{
    tree_stmt(s, STMT_FOR, 0)->as.for_loop.element = NULL;
}

/* Three names for the two parts of `(int, int)`. */
static void pattern_past_parts(struct damaged *s)
{
    struct stmt *loop = tree_stmt(s, STMT_FOR, 0);
    struct binding *names = arena_alloc(s->arena, 3 * sizeof *names);

    names[0] = loop->as.for_loop.names[0];
    names[1] = loop->as.for_loop.names[1];
    names[2] = loop->as.for_loop.names[1];
    loop->as.for_loop.names = names;
    loop->as.for_loop.name_count = 3;
}

static void walk_without_name(struct damaged *s)
{
    tree_stmt(s, STMT_FOR, 1)->as.for_loop.name_count = 0;
}

/* The `break` of the `while` in place of the loop, outside it. */
static void break_outside_loop(struct damaged *s)
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
static void closure_past_params(struct damaged *s)
{
    struct item *fn = tree_let(s, "f")->as.fn;
    struct param *params = arena_alloc(s->arena, 2 * sizeof *params);

    params[0] = fn->params[0];
    params[1] = fn->params[0];
    fn->params = params;
    fn->param_count = 2;
}

static void generic_short_of_params(struct damaged *s)
{
    tree_of(s)->param_count = 4;
}

/* One argument recorded for a copy of a generic of two. */
static void copy_short_of_args(struct damaged *s)
{
    tree_let(s, "kept")->as.call.copy_count = 1;
}

/* The nested block holds the statement that holds it. */
static void block_holds_itself(struct damaged *s)
{
    struct stmt *nested = tree_stmt(s, STMT_BLOCK, 0);

    nested->as.block->stmts[0] = nested;
}

static void expr_holds_itself(struct damaged *s)
{
    struct expr *sum = tree_let(s, "sum");

    sum->as.binary.left = sum;
}

/* One statement in two places of the body. */
static void stmt_twice(struct damaged *s)
{
    struct stmt *nested = tree_stmt(s, STMT_BLOCK, 0);
    struct item *it = tree_of(s);

    it->body->stmts[0] = nested;
}

/* S304: a name that no field of the struct holds. */
static const struct name no_field = {"zz", 2};

/* `pt.zz`, which lowering finds no field for. */
static void field_past_struct(struct damaged *s)
{
    tree_let(s, "px")->as.field.name = no_field;
}

/* `TPoint { zz: n, y: 1 }`. */
static void init_past_struct(struct damaged *s)
{
    tree_let(s, "pt")->as.struct_lit.fields[0].name = no_field;
}

/* `TShape.Circle { zz: n }`, a name the fields of the case lack. */
static void init_past_case(struct damaged *s)
{
    tree_let(s, "made")->as.struct_lit.fields[0].name = no_field;
}

/* A literal of the variant that names no case, whose case lowering
   counts from 1. */
static void variant_without_case(struct damaged *s)
{
    tree_let(s, "made")->as.struct_lit.variant_case = 0;
}

/* `[]int { zz: none, len: 0 }`. */
static void init_past_slice(struct damaged *s)
{
    tree_let(s, "sl")->as.slice_lit.fields[0].name = no_field;
}

/* A copy of the symbol that e names, which the file then writes as an
   extern of its own. */
static struct symbol *own_symbol(struct damaged *s, struct expr *e)
{
    struct symbol *copy = arena_alloc(s->arena, sizeof *copy);

    *copy = *e->symbol;
    e->symbol = copy;
    return copy;
}

/* The private function `thelper` written with the type `int`. */
static void extern_fn_not_fn(struct damaged *s)
{
    struct expr *call = tree_let(s, "helped")->as.binary.left;

    own_symbol(s, call->as.call.callee)->type = call->type;
}

/* The private constant `TLIMIT` written without its value. */
static void extern_const_without_value(struct damaged *s)
{
    own_symbol(s, tree_let(s, "helped")->as.binary.right)->value = NULL;
}

/* A line above INT_MAX, which a negative line of the compiler writes. */
static void line_negative(struct damaged *s)
{
    tree_let(s, "sum")->pos.line = -1;
}

/* A chain of `-` as deep as depth, with the operand of `-n` at its end. */
static void unary_chain(struct damaged *s, size_t depth)
{
    struct expr *neg = tree_let(s, "neg");
    struct expr *chain = neg->as.unary.operand;
    size_t k;

    for (k = 0; k < depth; k++) {
        struct expr *link = arena_alloc(s->arena, sizeof *link);
        *link = *neg;
        link->as.unary.operand = chain;
        chain = link;
    }
    neg->as.unary.operand = chain;
}

static void short_chain(struct damaged *s)
{
    unary_chain(s, 16);
}

static void deep_chain(struct damaged *s)
{
    unary_chain(s, ANTL_TREE_DEPTH_MAX + 16);
}

/* Read the library of tree_source written with damage, or refuse it. */
static void tree_file(void (*damage)(struct damaged *),
                      bool reads)
{
    struct compilation s;
    struct text bytes = {0};

    compilation_open(&s);
    if (build_damaged(&s, "tt", tree_source, damage, &bytes)) {
        if (reads) {
            CHECK(reads_file(&bytes));
        } else {
            refuses_file((const uint8_t *)bytes.data, bytes.length, NULL);
        }
    }
    text_free(&bytes);
    compilation_close(&s);
}

/* `tpair(x, n, n)`, the last argument shared. Every table keeps its
   size, and the first byte that differs is the count of the arguments. */
static void arg_repeated(struct damaged *s)
{
    struct expr *call = tree_let(s, "kept");
    struct expr **args = arena_alloc(s->arena, 3 * sizeof *args);

    args[0] = call->as.call.args[0];
    args[1] = call->as.call.args[1];
    args[2] = call->as.call.args[1];
    call->as.call.args = args;
    call->as.call.arg_count = 3;
}

/* `tpair(x)`: the table of expressions of the tree loses `n`, and the
   first byte that differs is the count of that table. */
static void arg_dropped(struct damaged *s)
{
    tree_let(s, "kept")->as.call.arg_count = 1;
}

/* The first byte where the file of tree_source written with damage
   differs from the one written without, or SIZE_MAX. */
static size_t tree_difference(const struct text *plain,
                              void (*damage)(struct damaged *))
{
    struct compilation s;
    struct text other = {0};
    size_t at = SIZE_MAX;
    size_t i;

    compilation_open(&s);
    if (build_damaged(&s, "tt", tree_source, damage, &other)) {
        for (i = 0; i < plain->length && i < other.length; i++) {
            if (plain->data[i] != other.data[i]) {
                at = i;
                break;
            }
        }
    }
    text_free(&other);
    compilation_close(&s);
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

/* M33 of the second audit: a count of a tree is measured against the rest of
   the file before the reader allocates for it. A list of arguments takes four
   bytes per element and a record of the table of expressions more than two. */
static void tree_counts(void)
{
    struct compilation s;
    struct text plain = {0};
    size_t list;
    size_t table;

    compilation_open(&s);
    if (!build_damaged(&s, "tt", tree_source, NULL, &plain)) {
        compilation_close(&s);
        return;
    }
    compilation_close(&s);
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
    struct compilation s;
    struct text bytes = {0};
    size_t n;

    compilation_open(&s);
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
    compilation_close(&s);
}

/* The negative step of `for j in 0..10 by -3` reads back as -3. */
static void signed_numbers(void)
{
    struct compilation s;
    struct text bytes = {0};
    struct ir_module program;
    const struct interface *read = NULL;
    char error[160] = "";
    size_t found = 0;
    size_t i;

    compilation_open(&s);
    if (build_library(&s, "tt", tree_source, &bytes)) {
        ir_module_init(&program, &s.arena, "main");
        read = compilation_read(&s, (const uint8_t *)bytes.data, bytes.length,
                                &program, error, sizeof error);
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
    compilation_close(&s);
}

/* A library of the records the sweep below damages: a constraint, a
   generic that names it, a struct of a constant parameter, a simd struct,
   a variant, an enum, a class with defaults, a `?T`, a tuple, a length of
   `size_of` and a hook, and a default of a parameter. */
static const char sweep_source[] =
    "pub constraint Small = eq + lt;\n"
    "pub struct Pair<T, N: int> { v: [N]T, n: int }\n"
    "pub simd struct Lanes { x: f32, y: f32, z: f32, w: f32 }\n"
    "pub variant Shape { Circle { r: int }, Empty }\n"
    "pub enum Color: u8 { Red, Green }\n"
    "pub class Box\n"
    "{\n"
    "    pub w: int = 3,\n"
    "    pub at: (int, bool) = (1, true),\n"
    "    pub maybe: ?int,\n"
    "    pub bytes: [size_of(int)]u8,\n"
    "    pub operator fn index(self, i: int) -> int\n"
    "    {\n"
    "        return self.w + i;\n"
    "    }\n"
    "}\n"
    "pub fn least<T: Small>(own a: T, own b: T) -> T {\n"
    "    if a < b {\n"
    "        return a;\n"
    "    }\n"
    "    return b;\n"
    "}\n"
    "pub fn first<T, N: int>(p: Pair<T, N>) -> T {\n"
    "    return p.v[0];\n"
    "}\n"
    "pub fn half(x: int, s: Shape, c: Color = Color.Green) -> ?int {\n"
    "    if x < 0 {\n"
    "        return none;\n"
    "    }\n"
    "    return x / 2;\n"
    "}\n"
    "pub fn sum(l: Lanes, c: Color) -> f32 {\n"
    "    return l.x + l.w;\n"
    "}\n";

/* DESIGN: one byte of a library file changed, at every position, once to
   the byte with its lowest bit turned and once to 0. The reader either
   takes the file or refuses it with a message, and a file it takes goes
   through the verifier as in the driver. The test holds no result per
   byte. It checks that no change ends the process, and a sanitizer build
   checks every read of memory on the way. Two values keep the test
   short: 0xff and the byte plus 16 as well reached three more refusals of
   the reader for twice the time. */
static void sweep_file(const char *name, const char *source)
{
    struct compilation s;
    struct text bytes = {0};
    uint8_t *copy;
    size_t at;
    size_t pokes = 0;
    size_t refused = 0;

    compilation_open(&s);
    if (!build_library(&s, name, source, &bytes)) {
        compilation_close(&s);
        return;
    }
    CHECK(reads_file(&bytes));
    CHECK(verifies_file((const uint8_t *)bytes.data, bytes.length));
    copy = alloc_zeroed(bytes.length, 1);
    for (at = 0; at < bytes.length; at++) {
        const uint8_t old = (uint8_t)bytes.data[at];
        const uint8_t values[] = {(uint8_t)(old ^ 1), 0};
        size_t k;

        for (k = 0; k < sizeof values; k++) {
            if (values[k] == old) {
                continue;
            }
            memcpy(copy, bytes.data, bytes.length);
            copy[at] = values[k];
            pokes++;
            if (!verifies_file(copy, bytes.length)) {
                refused++;
            }
        }
    }
    CHECK(refused > 0 && refused < pokes);
    free(copy);
    text_free(&bytes);
    compilation_close(&s);
}

static void damaged_bytes(void)
{
    sweep_file("sweep", sweep_source);
    sweep_file("tt", tree_source);
}

static void damaged_trees(void)
{
    void (*const damages[])(struct damaged *) = {
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
        line_negative,           field_past_struct,
        init_past_struct,        init_past_case,
        variant_without_case,    init_past_slice};
    size_t i;

    tree_file(NULL, true);
    tree_file(short_chain, true);
    for (i = 0; i < sizeof damages / sizeof damages[0]; i++) {
        tree_file(damages[i], false);
    }
}

/* A generic that calls a function of the root, whose C symbol the file
   carries beside its name. */
static const char root_source[] =
    "pub class RC { }\n"
    "pub fn rname<T>(x: T, c: *RC) -> str {\n"
    "    return c.type_name();\n"
    "}\n";

/* A generic whose closure captures a local. */
static const char capture_source[] =
    "pub fn grab<T>(v: T) -> int {\n"
    "    let n = 1;\n"
    "    let f = fn(k: int) -> int { return k + n; };\n"
    "    return f(2);\n"
    "}\n";

/* The closure of `grab`, with the room for captures the checker gave
   it, as find_closure finds it before the writer runs. */
static struct item *capturing;
static size_t capture_room;

static void find_closure(struct damaged *s)
{
    const struct item *grab = generic_of(s->written, "grab");
    size_t i;

    capturing = NULL;
    for (i = 0; grab != NULL && i < grab->body->count; i++) {
        const struct stmt *st = grab->body->stmts[i];
        if (st->kind == STMT_LET && st->as.let.value != NULL &&
            st->as.let.value->kind == EXPR_FN) {
            capturing = st->as.let.value->as.fn;
        }
    }
    if (capturing != NULL) {
        capture_room = capturing->capture_capacity;
    }
}

/* The writer reads the trees it writes and changes none of them: the
   room of the captures of a closure stays as the checker left it. */
static void writer_keeps_trees(void)
{
    struct compilation s;
    struct text bytes = {0};

    compilation_open(&s);
    CHECK(build_damaged(&s, "cap", capture_source, find_closure, &bytes));
    CHECK(capturing != NULL && capturing->capture_count == 1);
    CHECK(capturing != NULL && capture_room > 1 &&
          capturing->capture_capacity == capture_room);
    text_free(&bytes);
    compilation_close(&s);
}

/* Rule 14: the C symbol of a function of the root becomes a C string,
   so one with a NUL inside is refused, where it would name a shorter
   symbol. */
static void runtime_names(void)
{
    static const char name[] = "type_name";
    struct compilation s;
    struct text bytes = {0};
    size_t at;
    size_t found = SIZE_MAX;

    compilation_open(&s);
    if (!build_library(&s, "rn", root_source, &bytes)) {
        compilation_close(&s);
        return;
    }
    CHECK(reads_file(&bytes));
    /* The symbol stands before the mark of the item and its name. */
    for (at = 4; at + 18 + 2 * strlen(name) <= bytes.length; at++) {
        const uint8_t *p = (const uint8_t *)bytes.data + at;
        if (u32_at(&bytes, at) == strlen(name) &&
            memcmp(p + 4, name, strlen(name)) == 0 &&
            p[4 + strlen(name)] == 1 &&
            u32_at(&bytes, at + 5 + strlen(name)) == strlen(name)) {
            found = at + 4 + 4;
            break;
        }
    }
    CHECK(found != SIZE_MAX);
    refuses_poke(&bytes, found, "", 1);
    text_free(&bytes);
    compilation_close(&s);
}

/* A variadic C function that a generic calls. */
static const char variadic_source[] =
    "extern fn printf(format: ?*c_char, ...) -> c_int;\n"
    "pub fn say<T>(x: T) -> c_int {\n"
    "    return printf(none, 1);\n"
    "}\n";

static void flag_bytes(void)
{
    flags_refused("sweep", sweep_source);
    flags_refused("tt", tree_source);
    flags_refused("rn", root_source);
    flags_refused("va", variadic_source);
}

/* A pattern literal across a carriage return and a line feed counts the
   two as one line break, so the error stands at the byte PCRE2 names on
   the second line, as in a file of line feeds alone. The literal asks
   for `anti.regex`, which an empty library of that name stands for. */
static void pattern_lines(void)
{
    static const char source[] = "import anti.regex;\r\n"
                                 "fn f() {\r\n"
                                 "\tlet p = re\"first\r\nsecond(?<\";\r\n"
                                 "}\r\n";
    struct compilation s;
    bool ok;

    compilation_open(&s);
    library(&s, "anti.regex", "");
    compilation_check(&s, "main", source, true, &ok);
    expect_refused(ok, &s.diags, source, 4, 10,
                   "malformed pattern: subpattern name expected");
    compilation_close(&s);
}

void test_modules(void)
{
    deep_types();
    copy_depths();
    deep_values();
    field_kinds();
    runtime_names();
    writer_keeps_trees();
    flag_bytes();
    wide_addresses();
    declared_types();
    damaged_trees();
    damaged_bytes();
    tree_counts();
    signed_numbers();
    truncated_trees();
    deep_tables();
    imports();
    cycles();
    compilation_bound();
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
    keeps_access_types();
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
    damaged_wait();
    damaged_records();
    damaged_simd();
    damaged_constants();
    damaged_relocations();
    pattern_lines();
}
