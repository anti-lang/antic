#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "text.h"
#include "types.h"

struct checked {
    struct arena arena;
    struct diagnostics diags;
    struct token_list tokens;
    struct module *module;
    struct types types;
    bool ok;
};

static void run(struct checked *c, const char *source)
{
    memset(c, 0, sizeof *c);
    types_init(&c->types, &c->arena);
    c->ok = lex(source, strlen(source), &c->arena, &c->diags, &c->tokens) &&
            parse(source, &c->tokens, &c->arena, &c->diags, &c->module);
    if (!c->ok) {
        fprintf(stderr, "syntax error in test source: %s\n",
                c->diags.items[0].message);
        check_failures++;
        return;
    }
    c->ok = sema_check(c->module, "main", NULL, NULL, 0, &c->types, &c->arena,
                       &c->diags, true);
}

static void release(struct checked *c)
{
    token_list_free(&c->tokens);
    diagnostics_free(&c->diags);
    arena_free(&c->arena);
}

static void accepts(const char *source)
{
    struct checked c;
    size_t i;

    run(&c, source);
    if (!c.ok) {
        check_failures++;
        fprintf(stderr, "rejected:\n%s\n", source);
        for (i = 0; i < c.diags.count; i++) {
            fprintf(stderr, "  %d:%d: %s\n", c.diags.items[i].line,
                    c.diags.items[i].column, c.diags.items[i].message);
        }
    }
    release(&c);
}

static void rejects(const char *source, int line, int column,
                    const char *message)
{
    struct checked c;

    run(&c, source);
    if (c.ok || c.diags.count == 0) {
        check_failures++;
        fprintf(stderr, "accepted, expected %d:%d: %s\n%s\n", line, column,
                message, source);
    } else if (c.diags.items[0].line != line ||
               c.diags.items[0].column != column ||
               strcmp(c.diags.items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %d:%d: %s\ngot      %d:%d: %s\n%s\n", line,
                column, message, c.diags.items[0].line,
                c.diags.items[0].column, c.diags.items[0].message, source);
    }
    release(&c);
}

/* Check source and expect a failure with message at line:column among
   its messages, which need not be the first. */
static void rejects_also(const char *source, int line, int column,
                         const char *message)
{
    struct checked c;
    size_t i;

    run(&c, source);
    for (i = 0; i < c.diags.count; i++) {
        if (c.diags.items[i].line == line &&
            c.diags.items[i].column == column &&
            strcmp(c.diags.items[i].message, message) == 0) {
            break;
        }
    }
    if (c.ok || i == c.diags.count) {
        check_failures++;
        fprintf(stderr, "no message %d:%d: %s\n%s\n", line, column, message,
                source);
        for (i = 0; i < c.diags.count; i++) {
            fprintf(stderr, "  %d:%d: %s\n", c.diags.items[i].line,
                    c.diags.items[i].column, c.diags.items[i].message);
        }
    }
    release(&c);
}

static void typed(const char *source, const char *expected)
{
    struct checked c;
    struct text out = {0};

    run(&c, source);
    CHECK(c.ok);
    if (c.module != NULL) {
        ast_dump_typed(&out, c.module);
        CHECK_STR(text_cstr(&out), expected);
    }
    text_free(&out);
    release(&c);
}

/* Check source and expect success without any message, then run the doc
   warnings with the undocumented option and expect one warning. */
static void warns_option(const char *source, bool undocumented, int line,
                         int column, const char *message)
{
    struct checked c;

    run(&c, source);
    CHECK(c.ok);
    CHECK(c.diags.count == 0);
    sema_doc_warnings(c.module, "main", NULL, 0, &c.types, undocumented,
                      &c.diags);
    CHECK(c.diags.count == 1);
    if (c.diags.count == 1) {
        CHECK(c.diags.items[0].warning);
        CHECK(c.diags.items[0].line == line &&
              c.diags.items[0].column == column);
        CHECK_STR(c.diags.items[0].message, message);
    } else if (c.diags.count > 1) {
        size_t i;
        fprintf(stderr, "expected one warning, got %zu:\n", c.diags.count);
        for (i = 0; i < c.diags.count; i++) {
            fprintf(stderr, "  %d:%d: %s\n", c.diags.items[i].line,
                    c.diags.items[i].column, c.diags.items[i].message);
        }
    }
    release(&c);
}

static void warns(const char *source, int line, int column,
                  const char *message)
{
    warns_option(source, false, line, column, message);
}

/* Check source and expect the doc warnings to say nothing, which is what
   every form inside the markup subset does. */
static void documented(const char *source)
{
    struct checked c;
    size_t i;

    run(&c, source);
    CHECK(c.ok);
    sema_doc_warnings(c.module, "main", NULL, 0, &c.types, false,
                      &c.diags);
    if (c.diags.count != 0) {
        check_failures++;
        fprintf(stderr, "the doc warnings spoke about:\n%s\n", source);
        for (i = 0; i < c.diags.count; i++) {
            fprintf(stderr, "  %d:%d: %s\n", c.diags.items[i].line,
                    c.diags.items[i].column, c.diags.items[i].message);
        }
    }
    release(&c);
}

void test_sema(void)
{
    /* A developer note on a public item needs a user comment beside it. */
    warns("//# Invariant.\npub fn f() {}\n"
          "//# Private.\nfn g() {}\n"
          "/// Doc.\n//# Noted.\npub fn h() {}\n",
          2, 8, "the pub item `f` has a `//#` note and no `///` comment");
    /* A doc comment that no item, field or module takes is lost, and the
       warning names the marker as the reader wrote it. The openers are
       split so that a comment scanner does not read them as comments. */
    warns("fn f() {\n    /" "** Local. */\n    let x = 1;\n}\n", 2, 5,
          "the `/" "**` comment is dropped, because no item or field "
          "follows it");
    warns("fn f() {}\n//! Late.\n", 2, 1,
          "the `//!` comment is dropped, because it stands after the first "
          "import or item");
    /* Markup outside the doc subset, one kind per comment, reported at
       the position of the comment. */
    warns("/// # Heading\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds a heading, which the doc markup has "
          "not");
    warns("/// A table:\n///\n/// | a | b |\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds a table, which the doc markup has "
          "not");
    warns("/// Such as *this*.\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds emphasis, which the doc markup has "
          "not");
    warns("/// A list:\n///\n/// 1. one\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds a numbered list, which the doc markup "
          "has not");
    warns("/// A picture: ![alt](p.png)\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds an image, which the doc markup has "
          "not");
    warns("/// Such as <b> here.\npub fn f() {}\n", 1, 1,
          "the doc comment of `f` holds HTML, which the doc markup has not");
    warns("//! A heading of the module:\n//!\n//! # Title\nfn f() {}\n", 1,
          1, "the doc comment of the module holds a heading, which the doc "
          "markup has not");
    /* A fenced block holds what it likes, and the forms of the subset are
       silent: paragraphs, inline code, `-` lists and links. */
    documented("/// Text with `f`, a list and a link.\n"
               "///\n"
               "/// ```text\n"
               "/// # not a heading, *not* emphasis, <b> no tag\n"
               "/// ```\n"
               "///\n"
               "/// - an item\n"
               "/// - [a link](https://anti-lang.com)\n"
               "pub fn f() {}\n");
    /* A backtick name resolves against the module, its items and the
       names they declare. A name that resolves nowhere is a warning, and
       inline code that is no name is none. */
    warns("/// `nowhere` is no name of this module.\npub fn f() {}\n", 1, 1,
          "`nowhere` in the doc comment of `f` resolves to nothing");
    warns("//! `nowhere` is no name of this module.\nfn f() {}\n", 1, 1,
          "`nowhere` in the doc comment of the module resolves to nothing");
    documented("/// `f`, `n`, `Point`, `x`, `main.f`, `int`, `return`,\n"
               "/// `f()` and `--release` all pass.\n"
               "pub fn f(n: int) -> int { return n; }\n"
               "struct Point { x: int, y: int }\n");
    /* --warn-undocumented reports a `pub` item without a `///` comment,
       and a private item is no concern of it. */
    warns_option("pub fn f() {}\nfn g() {}\n", true, 1, 8,
                 "the pub item `f` has no `///` comment");
    warns_option("/// Doc.\npub class Thing { pub fn f(self) {} }\n", true,
                 2, 26, "the pub item `f` has no `///` comment");

    typed("fn scale(x: int) -> int {\n"
          "    let k = 2 + 4;\n"
          "    return x * k;\n"
          "}\n",
          "function scale             fn(int) -> int\n"
          "  param x                  int\n"
          "    type int\n"
          "  result\n"
          "    type int\n"
          "  block\n"
          "    let_stmt k             int\n"
          "      additive +           int\n"
          "        int_lit 2          int\n"
          "        int_lit 4          int\n"
          "    return_stmt\n"
          "      multiplicative *     int\n"
          "        ident x            int\n"
          "        ident k            int\n");

    /* Literal types from context. */
    typed("fn f(a: u32) -> u32 { let x: i8 = -128; return a + 1; }",
          "function f                 fn(u32) -> u32\n"
          "  param a                  u32\n"
          "    type u32\n"
          "  result\n"
          "    type u32\n"
          "  block\n"
          "    let_stmt x             i8\n"
          "      type i8\n"
          "      unary -              i8\n"
          "        int_lit 128        i8\n"
          "    return_stmt\n"
          "      additive +           u32\n"
          "        ident a            u32\n"
          "        int_lit 1          u32\n");
    /* uint and the C types with a fixed size are other spellings of the
       sized types. */
    typed("extern fn f(a: c_int, b: c_uchar, c: c_size_t, d: c_double) -> uint;",
          "extern_fn f                fn(i32, byte, u64, float) -> u64\n"
          "  param a\n"
          "    type c_int\n"
          "  param b\n"
          "    type c_uchar\n"
          "  param c\n"
          "    type c_size_t\n"
          "  param d\n"
          "    type c_double\n"
          "  result\n"
          "    type uint\n");
    accepts("fn f(a: c_char, b: c_short, c: c_ushort, d: c_uint) {\n"
            "    let x: i8 = a; let y: i16 = b; let z: u16 = c;\n"
            "    let w: u32 = d; let v: c_float = 1.5; let u: f32 = v;\n"
            "    let s: i64 = 1 as c_longlong; let t: u64 = 1 as c_ulonglong;\n"
            "}");
    /* A length computed from size_of stays symbolic until the back end
       folds it. Equal lengths make one array type. */
    accepts("struct H { tag: u8, n: i32 }\n"
            "const N: int = size_of(H) * 2;\n"
            "struct B { bytes: [N]byte, more: [size_of(H) * 2]byte }\n"
            "fn f(b: *B) -> int {\n"
            "    let copy: [N]byte = b.more;\n"
            "    let fill = [0; N - 1];\n"
            "    return copy.len + fill.len;\n"
            "}\n");
    rejects("struct H { n: i32 }\nfn f(a: [size_of(H)]byte) {}", 2, 10,
            "a length computed from `size_of` is allowed only in a struct "
            "field or a local variable");
    rejects("struct H { n: i32 }\nconst Z: [size_of(H)]byte = [0; 4];", 2, 11,
            "a length computed from `size_of` is allowed only in a struct "
            "field or a local variable");
    rejects("struct H { n: i32 }\nconst F: f64 = size_of(H) as f64;", 2, 16,
            "a value computed from `size_of` converts only to an integer type "
            "in a constant expression");
    rejects("struct H { n: i32 }\nconst D: int = size_of(H) / 0;", 2, 16,
            "`size_of(H) / 0` divides by zero");
    rejects("struct H { n: i32 }\nconst A: [2]int = [1, 2];\n"
            "const X: int = A[size_of(H) - 4];", 3, 18,
            "an index computed from `size_of` is not a constant expression");
    rejects("struct S { a: [size_of(S)]byte }", 1, 8,
            "struct `S` contains itself");
    /* An export fn takes and returns only types with a C representation,
       and an exported struct holds only such fields. */
    accepts("export struct Vec2 { x: c_int, y: c_int, tags: [4]u8 }\n"
            "export union Num { i: i64, d: f64 }\n"
            "export const LIMIT: int = 10;\n"
            "export const NAME: str = \"geo\";\n"
            "export fn dot(a: Vec2, b: *Vec2, n: *Num, cb: fn(i32) -> bool,\n"
            "              p: *byte, f: float, u: uint, q: **Vec2) -> c_int {\n"
            "    return a.x;\n"
            "}\n");
    rejects("export fn f(s: str) {}", 1, 13,
            "the parameter `s` of export fn `f` has type `str`, which C cannot "
            "represent");
    rejects("export fn f(c: char) {}", 1, 13,
            "the parameter `c` of export fn `f` has type `char`, which C cannot "
            "represent");
    rejects("export fn f(a: [4]int) {}", 1, 13,
            "the parameter `a` of export fn `f` has type `[4]int`, which C "
            "cannot represent");
    rejects("export fn f(p: *[]int) {}", 1, 13,
            "the parameter `p` of export fn `f` has type `*[]int`, which C "
            "cannot represent");
    rejects("export fn f(cb: fn(str)) {}", 1, 13,
            "the parameter `cb` of export fn `f` has type `fn(str)`, which C "
            "cannot represent");
    rejects("struct V { x: int }\nexport fn f(v: *V) {}", 2, 13,
            "the parameter `v` of export fn `f` has type `*V`, and `V` is not "
            "exported");
    rejects("export fn f() -> str { return \"x\"; }", 1, 18,
            "the result of export fn `f` has type `str`, which C cannot "
            "represent");
    rejects("export struct S { s: str }", 1, 19,
            "the field `s` of export struct `S` has type `str`, which C "
            "cannot represent");
    rejects("struct B { a: i32 }\nexport struct S { d: [size_of(B)]u8 }", 2, 19,
            "the field `d` of export struct `S` has type `[size_of(B)]byte`, "
            "which C cannot represent");
    rejects("export const P: ?*byte = none;", 1, 17,
            "export const `P` has type `?*byte`, and an export const is a "
            "number, a bool or a str");
    rejects("export fn main() -> int { return 0; }", 1, 11,
            "`main` is the entry of the program and cannot be exported");
    accepts("export struct S align(8) { a: u32, b: u32 : 3 }");
    rejects("export struct S align(8) { a: u32 : 3, b: u32 }", 1, 28,
            "the first field `a` of export struct `S` is a bitfield, and the "
            "C header aligns the struct on its first field");
    /* A bitfield has a sized integer type and 1 to that many bits. It has
       no address. */
    accepts("struct Flags { visible: u32 : 1, layer: u32 : 4, level: i8 : 3 }\n"
            "fn f(p: *Flags) -> int {\n"
            "    p.layer += 1;\n"
            "    let f = Flags { visible: 1, layer: 2, level: -1 };\n"
            "    f.level = p.level;\n"
            "    return f.visible as int + f.layer as int;\n"
            "}\n");
    rejects("struct S { a: f32 : 3 }", 1, 15,
            "a bitfield has a sized integer type, found `f32`");
    rejects("struct S { a: c_long : 3 }", 1, 15,
            "a bitfield has a sized integer type, found `c_long`");
    rejects("struct S { a: u8 : 9 }", 1, 20,
            "a bitfield of `byte` has 1 to 8 bits");
    rejects("struct S { a: u8 : 0 }", 1, 20,
            "a bitfield of `byte` has 1 to 8 bits");
    rejects("struct S { a: u8 : 3 }\nfn f(s: S) { let p = &s.a; }", 2, 23,
            "a bitfield has no address");
    /* A zero-width bitfield `_: T : 0` breaks the unit. It holds no value,
       so no literal, access or constant names it. */
    accepts("struct S { a: u8, _: u32 : 0, b: u8 : 3, _: u16 : 0, c: u8 }\n"
            "const K: S = S { a: 1, b: 2, c: 3 };\n"
            "fn f(s: S) -> int {\n"
            "    let t = S { a: 4, b: 5, c: 6 };\n"
            "    return s.a as int + t.b as int + K.c as int;\n"
            "}\n");
    rejects("struct S { _: u32, a: u8 }", 1, 12,
            "the field `_` is a zero-width bitfield, written `_: T : 0`");
    rejects("struct S { _: u32 : 2, a: u8 }", 1, 12,
            "the field `_` is a zero-width bitfield, written `_: T : 0`");
    rejects("union U { a: u8, _: u32 : 0 }", 1, 18,
            "a union holds no zero-width bitfield");
    rejects("struct S { a: u8, _: u32 : 0 }\nfn f(s: S) -> u32 { return s._; }",
            2, 28, "`S` has no field `_`");
    rejects("struct S { a: u8, _: u32 : 0 }\n"
            "fn f() -> S { return S { a: 1, _: 0 }; }", 2, 32,
            "`S` has no field `_`");
    rejects("export struct S align(8) { _: u32 : 0, b: u32 }", 1, 28,
            "the first field `_` of export struct `S` is a bitfield, and the "
            "C header aligns the struct on its first field");
    /* An alignment is a constant power of two. */
    accepts("const N: int = 8;\npacked struct P { a: u8, b: i32 }\n"
            "struct A align(N * 2) { a: u8, p: P }\n"
            "fn f(a: A) -> i32 { return a.p.b; }\n");
    rejects("struct A align(3) { a: u8 }", 1, 16,
            "an alignment is a power of two");
    rejects("struct B { a: i32 }\nstruct A align(size_of(B)) { a: u8 }", 2, 16,
            "an alignment is a constant, not a value computed from `size_of`");
    /* A union literal names one field, and a union has no methods. */
    accepts("union Value { i: int, f: f64 }\n"
            "struct Tagged { tag: u8, v: Value }\n"
            "fn f(x: f64) -> int {\n"
            "    let t = Tagged { tag: 1, v: Value { f: x } };\n"
            "    t.v.i = t.v.i + 1;\n"
            "    return t.v.i;\n"
            "}\n");
    rejects("union Value { i: int, f: f64 }\n"
            "fn f() { let v = Value { i: 1, f: 2.0 }; }", 2, 18,
            "a literal of union `Value` names exactly one field");
    rejects("union Value { i: int, f: f64 }\n"
            "fn get(v: Value) -> int { return v.i; }\n"
            "fn f(v: Value) -> int { return v.get(); }", 3, 32,
            "`Value` has no field `get`");
    rejects("union Value { i: int, f: f64 }\n"
            "const V: Value = Value { i: 1 };", 2, 18,
            "a union is not a constant expression");
    /* c_long, c_ulong and c_wchar are types of their own whose width the
       target decides. A literal fits the narrower width. */
    typed("extern fn labs(n: c_long) -> c_long;\n"
          "fn f(a: i32, w: c_wchar) -> c_ulong {\n"
          "    return (labs(a as c_long) + 1) as c_ulong + w as c_ulong;\n"
          "}\n",
          "extern_fn labs             fn(c_long) -> c_long\n"
          "  param n\n"
          "    type c_long\n"
          "  result\n"
          "    type c_long\n"
          "function f                 fn(i32, c_wchar) -> c_ulong\n"
          "  param a                  i32\n"
          "    type i32\n"
          "  param w                  c_wchar\n"
          "    type c_wchar\n"
          "  result\n"
          "    type c_ulong\n"
          "  block\n"
          "    return_stmt\n"
          "      additive +           c_ulong\n"
          "        cast               c_ulong\n"
          "          additive +       c_long\n"
          "            call           c_long\n"
          "              ident labs   fn(c_long) -> c_long\n"
          "              cast         c_long\n"
          "                ident a    i32\n"
          "                type c_long\n"
          "            int_lit 1      c_long\n"
          "          type c_ulong\n"
          "        cast               c_ulong\n"
          "          ident w          c_wchar\n"
          "          type c_ulong\n");
    accepts("fn f() { let a: c_long = -2147483648; let b: c_ulong = 4294967295;"
            " let c: c_wchar = 65535; }");
    rejects("fn f() { let a: c_long = 2147483648; }", 1, 26,
            "`2147483648` does not fit `c_long` on every target");
    rejects("fn f() { let c: c_wchar = 65536; }", 1, 27,
            "`65536` does not fit `c_wchar` on every target");
    rejects("fn f(a: c_long) -> i64 { return a; }", 1, 33,
            "expected `int`, found `c_long`");
    rejects("const BIG: c_ulong = 65536 as c_ulong * 65536 as c_ulong;", 1, 22,
            "the value does not fit `c_ulong` on every target");
    accepts("fn f() { let big = 5_000_000_000; let half: f32 = 0.5; }");
    accepts("fn f() { let x: u8 = 250 + 10; let p: ?*int = none; }");
    accepts("fn f(p: *int) -> bool { return p == none; }");
    rejects("fn f() { let n: u8 = 300; }", 1, 22, "`300` does not fit `byte`");
    rejects("fn f() { let m: u8 = -1; }", 1, 22, "`-1` does not fit `byte`");
    rejects("fn f() { let g: f32 = 1; }", 1, 23,
            "expected `f32`, found an integer literal");
    rejects("fn f() { let x: int = 1.5; }", 1, 23,
            "expected `int`, found a float literal");
    rejects("fn f() { let p = none; }", 1, 18,
            "`none` needs a type that may be `none` from its context");
    rejects("fn f(a: u8) { let b = -a; }", 1, 23,
            "unary `-` needs a signed integer or a float, found `byte`");

    /* Operators and conversions. */
    accepts("fn f(x: int) -> bool { return (x & 1) == 0 && x < 10 || !true; }");
    accepts("fn f(x: int) -> u8 { return (x as u8) << 3; }");
    accepts("fn f() -> u32 { return 'A' as u32 + 1; }");
    accepts("fn f(p: *int) -> *byte { return p as *byte; }");
    accepts("fn f(b: bool, c: u32) -> char { let n = b as int; return c as char; }");
    rejects("fn f(a: int, b: u8) -> int { return a + b; }", 1, 37,
            "the operands of `+` have the types `int` and `byte`");
    rejects("fn f(x: int) -> bool { return x & 1 == 0; }", 1, 31,
            "the operands of `&` have the types `int` and `bool`");
    rejects("fn f(a: float) -> float { return a % 2.0; }", 1, 34,
            "`%` needs integer operands, found `float`");
    rejects("fn f(x: int) -> bool { return x as bool; }", 1, 31,
            "cannot convert `int` to `bool`");
    rejects("fn f(a: bool, b: bool) -> bool { return a < b; }", 1, 41,
            "`<` needs numeric or `char` operands, found `bool`");

    /* `x in lo..hi` is `x >= lo && x < hi`, so the bounds take the type
       of the value, which the two comparisons take. */
    accepts("fn f(c: char) -> bool { return c in 'a'..'{'; }");
    accepts("fn f(x: u8, n: u8) -> bool { return x in 1..n + 1; }");
    accepts("fn f(x: float) -> bool { return x in 0.0..1.0; }");
    accepts("fn f(n: u8) -> bool { return 3 in 0..n; }");
    accepts("const SMALL: bool = 3 in 0..8;");
    accepts("fn f(in: int) -> bool { return in in 0..in; }");
    rejects("fn f(x: i32, n: int) -> bool { return x in 0..n; }", 1, 47,
            "expected `i32`, found `int`");
    rejects("fn f(x: int) -> bool { return x in 0..2.5; }", 1, 39,
            "expected `int`, found a float literal");
    rejects("fn f(b: bool) -> bool { return b in false..true; }", 1, 32,
            "`in` needs numeric or `char` operands, found `bool`");
    rejects("fn f(x: int) -> int { return x in 0..2; }", 1, 30,
            "expected `int`, found `bool`");
    rejects("fn f(x: int) { const C: bool = x in 0..2; }", 1, 32,
            "a variable is not a constant expression");

    /* Names, scopes and shadowing. */
    accepts("fn f(x: int) -> int { let x = x + 1; return x; }");
    accepts("fn f(x: int) -> int { let t = 0; { let x = x + 1; t += x; } "
            "return t + x; }");
    accepts("fn f() -> int { return g(); }\nfn g() -> int { return 1; }");
    rejects("fn f() -> int { return y; }", 1, 24, "unknown name `y`");
    rejects("fn f() { let a = 1; let a = 2; }", 1, 25,
            "`a` is already declared in this block");
    rejects("fn f() {}\nfn f() {}", 2, 4, "`f` is already declared");
    rejects("fn f(a: int, a: int) {}", 1, 14,
            "`a` is already declared in this block");

    /* Statements. */
    accepts("fn f(n: int) -> int { let i = 0; while i < n do { i += 1; "
            "if i == 3 { continue; } if i > 5 { break; } } do { i -= 1; } "
            "while i > 0 return i; }");
    rejects("fn f(n: int) { if n { } }", 1, 19,
            "a condition has type `bool`, found `int`");
    rejects("fn f() { break; }", 1, 10, "`break` outside a loop");
    rejects("fn f() -> int { return; }", 1, 17,
            "`return` needs a value of type `int`");
    rejects("fn f() { return 1; }", 1, 17, "`f` returns no value");
    rejects("fn f() -> int { return true; }", 1, 24,
            "expected `int`, found `bool`");
    rejects("fn f(n: int) -> int { if n > 0 { return 1; } }", 1, 4,
            "`f` can reach its end without `return`");
    accepts("fn f(n: int) -> int { if n > 0 { return 1; } else if n < 0 "
            "{ return -1; } else { return 0; } }");
    rejects("fn f() -> int { while true do { return 1; } }", 1, 4,
            "`f` can reach its end without `return`");
    rejects("fn f() { 1 = 2; }", 1, 10, "cannot assign to this expression");
    rejects("fn f(s: str) { s[0] = 65; }", 1, 16,
            "cannot assign to this expression");
    rejects("fn f(s: []int) { s.len = 3; }", 1, 18,
            "cannot assign to this expression");

    /* Structs, fields, methods and literals. */
    accepts("struct Vec2 { x: f32, y: f32 }\n"
            "fn length(v: *Vec2) -> f32 { return v.x * v.x + v.y * v.y; }\n"
            "fn scaled(v: Vec2, k: f32) -> Vec2 { return Vec2 { y: v.y * k, x: v.x * k }; }\n"
            "fn f() -> f32 { let v = Vec2 { x: 3.0, y: 4.0 }; let p = &v; "
            "let w = v.scaled(2.0); return v.length() + p.length() + p.scaled(1.0).x; }");
    accepts("struct Shape { area: fn(*Shape) -> f64, size: f64 }\n"
            "fn square_area(s: *Shape) -> f64 { return s.size * s.size; }\n"
            "fn scale(s: *Shape, k: f64) { s.size *= k; }\n"
            "fn demo() -> f64 { let sq = Shape { area: square_area, size: 2.0 }; "
            "sq.scale(1.5); return sq.area(&sq); }");
    rejects("struct V { x: int }\nfn f() { let v = V { x: 1, y: 2 }; }", 2, 28,
            "`V` has no field `y`");
    rejects("struct V { x: int, y: int }\nfn f() { let v = V { x: 1 }; }", 2,
            18, "the literal of `V` misses the field `y`");
    rejects("struct V { x: int }\nfn f(v: V) -> int { return v.z; }", 2, 28,
            "`V` has no field `z`");
    rejects("struct V { x: int }\nfn f(v: V) { v.go(); }", 2, 14,
            "`V` has no function `go`");
    rejects("struct Node { next: *Node, value: Node }", 1, 8,
            "struct `Node` contains itself");
    rejects("fn f(v: Vec2) {}", 1, 9, "unknown type `Vec2`");

    /* Pointers, arrays, slices and strings. */
    accepts("fn f(a: [4]int, s: []int, p: *int, t: str) -> int {\n"
            "    let x = a[1] + s[2] + p[3] + *p + a.len + s.len;\n"
            "    let b: byte = t[0];\n"
            "    let part: []byte = t[1..3];\n"
            "    let whole: []int = a[0..4];\n"
            "    let raw: []int = []int { ptr: p, len: 4 };\n"
            "    let q: ?*byte = t.ptr;\n"
            "    return x;\n"
            "}");
    accepts("fn f() -> int { let a = [1, 2, 3]; let z = [0; 16]; "
            "let m: [2]f32 = [0.5, 1.5]; return a[0] + z.len; }");
    accepts("fn f() { let p = alloc(int, 4) else { return; }; p[0] = 1; "
            "free(p); let n = size_of([3]i16); }");
    rejects("fn f(x: int) -> int { return x[0]; }", 1, 30,
            "cannot index `int`");
    rejects("fn f(a: [4]int) -> int { return a[true]; }", 1, 35,
            "expected `int`, found `bool`");
    rejects("fn f(x: int) -> int { return *x; }", 1, 30,
            "unary `*` needs a pointer, found `int`");
    rejects("fn f() { let a = [0; 0]; }", 1, 22,
            "an array length is at least 1");
    rejects("fn f() { free(3); }", 1, 15, "`free` needs a pointer, found `int`");

    /* Calls, externs and function pointers. */
    accepts("extern fn printf(fmt: ?*byte, ...) -> i32;\n"
            "fn f() { printf(\"%d %f\\n\".ptr, 1, 2.5); }");
    accepts("fn add(a: i32, b: i32) -> i32 { return a + b; }\n"
            "fn f() -> i32 { let op: fn(i32, i32) -> i32 = add; "
            "return op(1, 2); }");
    rejects("fn g(a: int) {}\nfn f() { g(1, 2); }", 2, 10,
            "`g` takes 1 argument, found 2");
    rejects("fn g(a: int) {}\nfn f() { g(true); }", 2, 12,
            "expected `int`, found `bool`");
    rejects("extern fn printf(fmt: ?*byte, ...) -> i32;\n"
            "fn f(x: f32) { printf(\"\".ptr, x); }", 2, 31,
            "a variadic argument has type i32, u32, int, u64, float or a "
            "pointer, found `f32`");
    rejects("fn g() {}\nfn f() { let x = g(); }", 2, 18,
            "`g` returns no value");

    /* Constants. */
    accepts("const WIDTH: int = 320;\n"
            "const HEIGHT: int = WIDTH * 3 / 4;\n"
            "const SCREEN: [2]int = [WIDTH, HEIGHT];\n"
            "const PIXELS: int = SCREEN[0] * SCREEN[1];\n"
            "const WIDE: bool = WIDTH > HEIGHT && HEIGHT >= 200;\n"
            "struct P { tag: u8, value: i32 }\n"
            "const BYTES: int = size_of(P) * 2;\n"
            "const NAME: str = \"antic\";\n"
            "const LETTERS: int = \"antic\".len;\n"
            "fn f() -> [PIXELS]byte { const LOCAL: u32 = 1 << 4; "
            "return [0; PIXELS]; }");
    rejects("fn g() -> int { return 1; }\nconst X: int = g();", 2, 16,
            "a call is not a constant expression");
    rejects("const A: int = B;\nconst B: int = A;", 2, 16,
            "`A` depends on itself");
    rejects("const X: int = 1 / 0;", 1, 16, "`1 / 0` divides by zero");
    /* An operation on constant operands that C leaves undefined is a
       compile error in a function body too, and names its values. */
    rejects("fn f() -> int { return 7 / 0; }", 1, 24,
            "`7 / 0` divides by zero");
    rejects("fn f() -> int { return 7 % 0; }", 1, 24,
            "`7 % 0` divides by zero");
    rejects("const ZERO: int = 0;\nfn f() -> int { return 5 / ZERO; }", 2, 24,
            "`5 / 0` divides by zero");
    rejects("fn f() -> int { return (1 / 0) + 2; }", 1, 25,
            "`1 / 0` divides by zero");
    /* A literal operand keeps the spelling of the source, and any other
       operand shows its value as a literal would write it. */
    rejects("fn f() -> int { return 0x10 / 0; }", 1, 24,
            "`0x10 / 0` divides by zero");
    rejects("const BIG: f64 = 1.5e10;\nfn f() -> i32 { return BIG as i32; }",
            2, 24, "`15000000000.0 as i32` does not fit `i32`");
    rejects("const HUGE: f64 = 2.5e300;\nfn f() -> int { return HUGE as int; }",
            2, 24, "`2.5e300 as int` does not fit `int`");
    rejects("fn f() -> i8 { let m: i8 = -128; const N: i8 = -128 / -1; "
            "return m; }", 1, 48, "`-128 / -1` does not fit `i8`");
    rejects("fn f() -> i16 { return -32768 % -1; }", 1, 24,
            "`-32768 % -1` does not fit `i16`");
    rejects("fn f() -> int { return 1 << 64; }", 1, 24,
            "`1 << 64` shifts out of range");
    rejects("fn f() -> i32 { return 1 >> -1; }", 1, 24,
            "`1 >> -1` shifts out of range");
    rejects("fn f() -> i32 { return 30000000000.0 as i32; }", 1, 24,
            "`30000000000.0 as i32` does not fit `i32`");
    rejects("fn f() -> u8 { return -1.5 as u8; }", 1, 23,
            "`-1.5 as byte` does not fit `byte`");
    rejects("fn f() -> int { return (0.0 / 0.0) as int; }", 1, 25,
            "`nan as int` does not fit `int`");
    rejects("struct H { n: i32 }\nfn f() -> int { return size_of(H) >> 64; }",
            2, 24, "`size_of(H) >> 64` shifts out of range");
    /* A variable operand leaves the operation to run time, and a float
       division by zero gives an infinity. */
    accepts("fn f(x: int, n: int) -> int { return x / 0 + (1 << n); }\n"
            "fn g(x: f64) -> int { return x as int; }\n"
            "const INF: f64 = 1.0 / 0.0;\n"
            "const LAST: i8 = -127 / -1;\n"
            "const ALMOST: u8 = 255.9 as u8;");
    /* The wrapping and saturating operators take two integers of one
       type. `<<%` has a value for every count, so a constant count at
       or above the width is no error. */
    accepts("fn f(a: u8, b: u8, n: int) -> u8 {\n"
            "    let x = 1 <<% n;\n"
            "    return a +% b -% a *% b +| a -| b *| (a <<% b);\n"
            "}\n"
            "const Z: int = 1 <<% 64;\n"
            "const N: i32 = 1 <<% -1;\n"
            "const S: u8 = 200 +| 100;");
    rejects("fn f(a: f64) -> f64 { return a +% a; }", 1, 30,
            "`+%` needs integer operands, found `float`");
    rejects("fn f(a: f32) -> f32 { return a *| a; }", 1, 30,
            "`*|` needs integer operands, found `f32`");
    rejects("fn f(a: u8, b: i8) -> u8 { return a +| b; }", 1, 35,
            "the operands of `+|` have the types `byte` and `i8`");
    rejects("fn f(a: bool) -> bool { return a <<% a; }", 1, 32,
            "`<<%` needs integer operands, found `bool`");
    /* `mul_high` is a built-in of two integers of one type. A function
       of that name in the module wins over it. */
    accepts("fn f(a: u64, b: u64, c: i8) -> u64 {\n"
            "    let h = mul_high(c, c);\n"
            "    return mul_high(a, b);\n"
            "}\n"
            "const H: u64 = mul_high(1 << 40, 1 << 40);");
    rejects("fn f(a: u64) -> u64 { return mul_high(a); }", 1, 30,
            "`mul_high` takes 2 arguments, found 1");
    rejects("fn f(a: f64) -> f64 { return mul_high(a, a); }", 1, 30,
            "`mul_high` needs integer operands, found `float`");
    rejects("fn f(a: u64, b: u32) -> u64 { return mul_high(a, b); }", 1, 38,
            "the operands of `mul_high` have the types `u64` and `u32`");
    accepts("fn mul_high(a: int) -> int { return a; }\n"
            "fn f() -> int { return mul_high(3); }");
    /* `let (result, flags) = e;` destructures the `(T, Flags)` of one
       arithmetic operation on integers. The flags name may be a Flags
       variable in scope, which the statement assigns. A carry into `+`
       and `-` is the `carry` field of a Flags value. */
    typed("fn f(a: u8, b: u8) {\n"
          "    let (r, f) = a + b;\n"
          "}\n",
          "function f                 fn(byte, byte)\n"
          "  param a                  byte\n"
          "    type u8\n"
          "  param b                  byte\n"
          "    type u8\n"
          "  block\n"
          "    let_stmt               (byte, Flags)\n"
          "      name r               byte\n"
          "      name f               Flags\n"
          "      additive +           byte\n"
          "        ident a            byte\n"
          "        ident b            byte\n");
    accepts("fn f(a: int, b: int) -> bool {\n"
            "    let (r, f) = a + b;\n"
            "    let (s, f) = a - b - f.carry;\n"
            "    let (t, g) = -a;\n"
            "    let (u, h) = a << 3;\n"
            "    let (v, k) = a >> b;\n"
            "    let (w, m) = a * b + f.carry;\n"
            "    (r, f) = a * b;\n"
            "    let x: Flags = f;\n"
            "    let y = Flags { overflow: false, carry: true, zero: false,\n"
            "                    negative: false };\n"
            "    let z = a + b + y.carry + x.carry;\n"
            "    return f.carry || g.zero || h.negative || k.overflow ||\n"
            "           m.carry || x.overflow || r + s + t + u + v + w + z == 0;\n"
            "}");
    rejects("fn f(a: int, b: int) { let (r, f) = a / b; }", 1, 37,
            "the flags form takes one `+`, `-`, `*`, `<<`, `>>` or unary "
            "`-`, found `/`");
    rejects("fn f(a: int, b: int) { let (r, f) = a +% b; }", 1, 37,
            "the flags form takes one `+`, `-`, `*`, `<<`, `>>` or unary "
            "`-`, found `+%`");
    rejects("fn f(a: f64, b: f64) { let (r, f) = a + b; }", 1, 37,
            "the flags form takes an integer type, found `float`");
    rejects("fn f() { let (r, f) = -5; }", 1, 23,
            "a `-` before a literal forms a constant, which has no flags");
    rejects("fn f(a: int) { let x = 1; let (r, x) = a + a; }", 1, 35,
            "`x` is already declared in this block");
    rejects("fn f(a: int) { let r = 1; let (r, g) = a + a; }", 1, 32,
            "`r` is already declared in this block");
    rejects("fn f(a: int) { let r = 1; let g = 2; (r, g) = a + a; }", 1, 42,
            "expected `Flags`, found `int`");
    rejects("fn f(a: int, b: u8) { let (r, g) = a + a; (b, g) = a + a; }", 1,
            52, "expected `byte`, found `int`");
    rejects("fn f(a: int) { let (r, g) = a + a; (r, g) = (a, g); }", 1, 45,
            "the flags form takes one `+`, `-`, `*`, `<<`, `>>` or unary "
            "`-`, found a tuple");
    rejects("fn f(a: int, p: *int) { let (r, g) = a + a; (*p, g) = a + a; }",
            1, 45, "the flags form assigns to two names");
    rejects("fn f(a: u8, g: Flags) -> u8 { return a +% g.carry; }", 1, 38,
            "the operands of `+%` have the types `byte` and `bool`");
    rejects("fn f(a: u8, g: Flags) -> u8 { return a * g.carry; }", 1, 38,
            "the operands of `*` have the types `byte` and `bool`");
    rejects("fn f(a: f64, g: Flags) -> f64 { return a + g.carry; }", 1, 40,
            "a carry goes into an integer, found `float`");
    rejects("fn f(a: u8, g: bool) -> u8 { return a + g; }", 1, 37,
            "the operands of `+` have the types `byte` and `bool`");
    rejects("const X: int = 1;\nfn f() -> *int { return &X; }", 2, 26,
            "a constant has no address");
    accepts("const GRID: [3]int = [1, 2, 3];\n"
            "const CELLS: int = GRID.len;");
    rejects("fn f() -> int { let a = [1, 2, 3]; const N: int = a.len; "
            "return N; }", 1, 51, "a variable is not a constant expression");

    /* main */
    accepts("fn main(args: []str, env: []str) -> int { return args.len; }");
    /* Threads, chapter 22. */
    accepts("worker fn sum(chunk: []int) -> int { return chunk.len; }\n"
            "fn f(a: []int) -> int { let r = parallel a -> sum; "
            "return r.len; }\n");
    accepts("worker fn scale(chunk: []int, k: int) -> int { return k; }\n"
            "fn f(a: []int) -> int { let r = parallel a by 4 -> scale(2); "
            "return r.len; }\n");
    typed("worker fn sum(chunk: []int) -> int { return 0; }\n"
          "fn f(a: []int) { let r = parallel a -> sum; }\n",
          "worker function sum        fn([]int) -> int\n"
          "  param chunk              []int\n"
          "    type []\n"
          "      type int\n"
          "  result\n"
          "    type int\n"
          "  block\n"
          "    return_stmt\n"
          "      int_lit 0            int\n"
          "function f                 fn([]int)\n"
          "  param a                  []int\n"
          "    type []\n"
          "      type int\n"
          "  block\n"
          "    let_stmt r             []int\n"
          "      parallel             []int\n"
          "        ident a            []int\n"
          "        ident sum          fn([]int) -> int\n");
    rejects("fn sum(chunk: []int) -> int { return 0; }\n"
            "fn f(a: []int) { let r = parallel a -> sum; }\n", 2, 40,
            "`sum` is not a `worker fn`");
    rejects("worker fn sum(chunk: []int) -> int { return 0; }\n"
            "fn f(a: int) { let r = parallel a -> sum; }\n", 2, 33,
            "`parallel` splits a slice, and this is `int`");
    rejects("worker fn sum(chunk: []float) -> int { return 0; }\n"
            "fn f(a: []int) { let r = parallel a -> sum; }\n", 2, 40,
            "`sum` takes `[]float` as its chunk, and the slice is `[]int`");
    rejects("worker fn sum(chunk: []int, k: int) -> int { return 0; }\n"
            "fn f(a: []int) { let r = parallel a -> sum; }\n", 2, 40,
            "`sum` takes 1 argument beside the chunk, found 0");
    rejects("worker fn sum(chunk: []*int) -> int { return 0; }\n"
            "fn f(a: []*int) { let r = parallel a -> sum; }\n", 2, 41,
            "the chunk has type `*int`, which holds a pointer. A worker "
            "takes and returns values alone");

    rejects("fn main(n: int) -> int { return n; }", 1, 4,
            "`main` must be fn main() -> int, fn main(args: []str) -> int "
            "or fn main(args: []str, env: []str) -> int");
    rejects("import geometry;\nfn f() {}", 1, 8,
            "cannot find module `geometry`");

    /* A class below calls the `construct` of its base through
       `self.super`, although it has no level of its own. Nothing else
       calls a `construct`. */
    accepts("class Pet { pub n: int = 0, fn construct(self, n: int) "
            "{ self.n = n; } }\n"
            "class Cat inherits Pet { fn construct(self, n: int) "
            "{ self.super.construct(n * 2); } }\n");
    rejects("class Pet { pub n: int = 0, fn construct(self, n: int) "
            "{ self.n = n; }\n"
            "    pub fn reset(self) { self.construct(0); } }\n", 2, 26,
            "`construct` runs after a literal and after `alloc`, and is not "
            "called directly");
    rejects("class Pet { pub n: int = 0, fn construct(self, n: int) "
            "{ self.n = n; } }\n"
            "fn f(p: *Pet) { p.construct(1); }\n", 2, 17,
            "`construct` is private to `Pet`");

    /* A static function is namespaced by its class, so a class below may
       declare a static of the same name. `Class.f` names one of them and
       never the other. A function that takes `self` and a field are
       another matter, and the chain keeps its one name for each. */
    accepts("class Pet { pub n: int = 0,\n"
            "    pub fn new(n: int) -> Pet { return Pet { n: n }; } }\n"
            "class Cat inherits Pet {\n"
            "    pub fn new() -> Cat { return Cat { n: 1 }; } }\n"
            "fn f() -> int { let a = Pet.new(2); let b = Cat.new();\n"
            "    return a.n + b.n; }\n");
    rejects("class Pet { pub n: int = 0, pub fn tag(self) -> int "
            "{ return self.n; } }\n"
            "class Cat inherits Pet { pub fn tag(self) -> int "
            "{ return 1; } }\n", 2, 33,
            "`Pet` already has `tag`");
    rejects("class Pet { pub n: int = 0,\n"
            "    pub fn make() -> Pet { return Pet { n: 1 }; } }\n"
            "class Cat inherits Pet { pub make: int = 0 }\n", 3, 26,
            "`Pet` already has `make`");
    rejects("class Pet { pub n: int = 0, pub fn tag(self) -> int "
            "{ return self.n; } }\n"
            "class Cat inherits Pet { pub fn tag() -> int "
            "{ return 1; } }\n", 2, 33,
            "`Pet` already has `tag`");

    /* A `concrete fn` takes the signature of the entry it fills, `own`
       included, from the base chain, from an interface and through a
       qualifier. errors/concrete_signature.anti holds the refusals. */
    accepts("class Point { pub x: int = 0 }\n"
            "abstract class Shape { abstract fn area(self, k: int) -> int;\n"
            "    abstract fn take(self, own p: ?*Point); }\n"
            "abstract class Named { abstract fn label(self, k: i32) -> i32;\n"
            "    abstract fn size(self) -> int; }\n"
            "class Square inherits Shape { implements n: Named,\n"
            "    concrete fn area(self, k: int) -> int { return k; }\n"
            "    concrete fn take(self, own p: ?*Point) { }\n"
            "    concrete fn size(self) -> int { return 1; }\n"
            "    concrete fn Named::label(self, k: i32) -> i32 "
            "{ return k; } }\n");
    rejects("abstract class Shape { abstract fn area(self, k: int) -> int; }\n"
            "class Square inherits Shape {\n"
            "    concrete fn area(self, k: i32) -> int { return 0; } }\n", 3,
            31, "`k` of `concrete fn area` has type `i32`, and `Shape.area` "
            "takes `int`");

    /* Tuples. The type stands wherever a type stands, the elements are
       `t.0` upwards, and `let (a, b) = e;` and `for i, x in items` are
       the two forms that take one apart. */
    accepts("fn pair(n: int) -> (int, str) { return (n, \"two\"); }\n"
            "fn add(p: (int, int)) -> int { return p.0 + p.1; }\n"
            "fn f(items: []int) -> int {\n"
            "    let t = pair(1);\n"
            "    let (n, word) = t;\n"
            "    let sum = add((n, word.len));\n"
            "    for i, x in items { sum = sum + i + x; }\n"
            "    for i, p in &items { sum = sum + i + *p; }\n"
            "    return sum + t.0;\n"
            "}\n");
    /* Two tuples of the same elements in the same order are one type. */
    accepts("fn pair() -> (int, int) { return (1, 2); }\n"
            "fn f() -> (int, int) { let t = pair(); return t; }\n");
    rejects("fn f() { let (a, b) = 5; }\n", 1, 14,
            "a destructuring takes a tuple, found `int`");
    rejects("fn pair() -> (int, int) { return (1, 2); }\n"
            "fn f() { let (a, b, c) = pair(); }\n", 2, 14,
            "`(int, int)` has 2 elements, and the destructuring names 3");
    rejects("fn pair() -> (int, int) { return (1, 2); }\n"
            "fn f() -> int { let t = pair(); return t.5; }\n", 2, 40,
            "`(int, int)` has 2 elements, and `5` is none of them");
    rejects("fn f() -> int { let n = 1; return n.0; }\n", 1, 35,
            "`int` is not a tuple, so it has no element `0`");
    rejects("fn f() { for i, x in 0..10 { } }\n", 1, 14,
            "`for i, x` binds the index and the element of a slice or an "
            "array");
    /* The index is an `int` and the element has the element's type, a
       `*T` over `&items`. Both are read-only, as the variable of every
       `for` is. */
    accepts("fn f(items: []u8) -> u8 {\n"
            "    let n: u8 = 0;\n"
            "    for i, x in items { n = n + x; }\n"
            "    for i, p in &items { *p = 0; }\n"
            "    return n;\n"
            "}\n");
    rejects("fn f(items: []u8) -> u8 {\n"
            "    let n: u8 = 0;\n"
            "    for i, x in items { n = n + i; }\n"
            "    return n;\n"
            "}\n", 3, 29, "the operands of `+` have the types `byte` and `int`");
    rejects("fn f(items: []int) { for i, x in items { i = 0; } }\n", 1, 42,
            "`i` is the variable of a `for` and is read-only");
    rejects("fn f(items: []int) { for i, x in items { x = 0; } }\n", 1, 42,
            "`x` is a copy of each element of `items`. Walk with `&items` "
            "to change the elements");
    rejects("fn f(items: []int) { for i, p in &items { p = &items[0]; } }\n",
            1, 43, "`p` is the variable of a `for` and is read-only");
    /* A tuple crosses to C as the struct the header writes for it, so it
       crosses when every element does. */
    accepts("export fn divmod(a: int, b: int) -> (int, int) {\n"
            "    return (a / b, a % b);\n"
            "}\n");
    rejects("export fn bad(n: int) -> (int, str) { return (n, \"x\"); }\n",
            1, 26,
            "the result of export fn `bad` has type `(int, str)`, which C "
            "cannot represent");
    /* `FieldDescriptor` is a struct the compiler declares, as `Flags`
       is, so a literal of it names it without an import. */
    accepts("fn f() -> int {\n"
            "    let r = FieldDescriptor { name: \"n\", offset: 8,\n"
            "                              type_id: 0, owned: 0,\n"
            "                              descriptor: none };\n"
            "    return r.offset;\n"
            "}\n");
    /* A `tests` or `fixtures` block sees every private item of its own
       module, the insides of its classes included. */
    accepts("class Counter {\n"
            "    n: int = 0,\n"
            "    fn bump(self) { self.n = self.n + 1; }\n"
            "}\n"
            "fixtures {\n"
            "    fn one() -> *Counter { return alloc Counter { n: 1 }; }\n"
            "}\n"
            "tests {\n"
            "    fn counts() {\n"
            "        let c = one();\n"
            "        c.bump();\n"
            "        assert(c.n == 2);\n"
            "    }\n"
            "}\n");
    /* Outside those blocks the levels stand. */
    rejects("class Counter { n: int = 0, }\n"
            "fn f(c: *Counter) -> int { return c.n; }\n", 2, 35,
            "`n` is private to `Counter`");
}

/* A type that contains itself, or a class that inherits itself, is
   refused with one message. No walk over fields or bases after it
   recurses or loops without end. */
void test_sema_cycles(void)
{
    static const char vec4[] =
        "simd struct V { x: f32, y: f32, z: f32, w: f32 }\n";
    char source[512];

    rejects("class A { a: A, }", 1, 7, "class `A` contains itself");
    rejects("class A { x: int, a: A, }", 1, 7, "class `A` contains itself");
    rejects("class A { b: B, }\nclass B { a: A, }\n"
            "fn f() { let c = chan A(1); }", 1, 7,
            "class `A` contains itself");
    rejects("class A { a: A }\nfn f() { let x = A { }; }", 1, 7,
            "class `A` contains itself");
    rejects("class A { use a: A }\nfn f(p: *A) -> int { return p.zz; }", 1,
            7, "class `A` contains itself");
    rejects("struct T { s: S }\nstruct S { t: T }\n"
            "class C { s: S }\nfn f() { let x = C { }; }", 1, 8,
            "struct `T` contains itself");
    rejects("variant V { A { v: V }, Empty }\n"
            "fn g(a: V.A) -> int { return 1; }\n"
            "fn f(x: V) -> int { switch x { A a => g(a), Empty => g(x) } "
            "return 0; }", 1, 9,
            "variant `V` contains itself");
    snprintf(source, sizeof source,
             "struct S { a: S }\n%s"
             "fn f(v: V) -> int { let s = v as S; return 0; }", vec4);
    rejects(source, 1, 8, "struct `S` contains itself");
    rejects("class A inherits A { }", 1, 9, "class `A` inherits itself");
    rejects("class A inherits B { }\nclass B inherits A { }\n"
            "fn f(b: *B) -> *A { return b; }", 2, 9,
            "class `B` inherits itself");
    rejects("class A inherits C { }\nclass B inherits A { }\n"
            "class C inherits B { n: int = 0, }\n"
            "fn f(a: *A) -> int { return a.n; }", 3, 9,
            "class `C` inherits itself");
    /* A refused base leaves the base field without a type, which the
       search for a cycle passes over. */
    rejects("class A inherits N { a: A, }", 1, 9, "`N` is not a class");
    /* A unit break is a bitfield of no width, and a lane is no
       bitfield. */
    rejects("simd struct V { _: i64 : 0 }", 1, 17,
            "a lane of a `simd struct` is no bitfield");
    /* A size past 2^64 bytes is no size, where it wrapped to 16. */
    snprintf(source, sizeof source,
             "%sfn f(v: V) { let s = v as [16][1152921504606846977]u8; }",
             vec4);
    rejects(source, 2, 22, "cannot convert `V` to "
            "`[16][1152921504606846977]byte`, a `simd struct` converts to "
            "an array or a plain struct of the same bytes");
    snprintf(source, sizeof source,
             "struct P { a: [4611686018427387904][2]u8, "
             "b: [4611686018427387912][2]u8 }\n"
             "%sfn f(v: V) { let s = v as P; }", vec4);
    rejects(source, 3, 22, "cannot convert `V` to `P`, a `simd struct` "
            "converts to an array or a plain struct of the same bytes");
}

/* A chain of constants and a chain of calls from a worker are checked
   without a recursion per link. A constant never reads a value its
   literal did not fill. */
void test_sema_constants(void)
{
    struct text source = {0};
    int i;

    /* Each constant names the next, so the first needs the whole chain. */
    for (i = 0; i < 3000; i++) {
        text_appendf(&source, "const C%d: i64 = C%d + 1;\n", i, i + 1);
    }
    text_append(&source, "const C3000: i64 = 0;\n"
                         "fn f() -> i64 { return C0; }\n");
    accepts(text_cstr(&source));
    text_free(&source);
    /* A cycle through the whole chain is refused where the evaluation
       nests too deep. */
    for (i = 0; i < 3000; i++) {
        text_appendf(&source, "const C%d: i64 = C%d + 1;\n", i,
                     (i + 1) % 3000);
    }
    rejects(text_cstr(&source), 62, 18,
            "`C62` needs a chain of more than 64 constants");
    text_free(&source);
    /* A worker that starts a chain of 60000 calls, whose last function
       deletes an object. */
    text_append(&source, "class Cell { pub n: int = 0, }\n"
                         "worker fn w(chunk: []int) -> int { g0(); "
                         "return 0; }\n");
    for (i = 0; i < 60000; i++) {
        text_appendf(&source, "fn g%d() { g%d(); }\n", i, i + 1);
    }
    text_append(&source, "fn g60000() { let c = alloc Cell { n: 1 }; "
                         "delete(c); }\n");
    rejects(text_cstr(&source), 60003, 44,
            "`w` is a `worker fn` and cannot `delete` an object another "
            "one may hold");
    text_free(&source);

    /* A class holds its table and its base, which no constant carries,
       and it read the defaults as 0. */
    rejects("class P { pub x: i64 = 3, pub y: i64, }\n"
            "const Q: P = P { y: 1 };\n"
            "const X: i64 = Q.x;\n",
            2, 14, "a class is not a constant expression");
    rejects("struct In { x: i64 }\n"
            "class Out { pub inner: In = In { x: 1 }, }\n"
            "const O: Out = Out { };\n"
            "const Z: i64 = O.inner.x;\n",
            3, 16, "a class is not a constant expression");
    rejects("class P { pub x: i64 = 3, pub y: i64, }\n"
            "struct S { p: P, n: i64 }\n"
            "const Q: S = S { p: P { y: 1 }, n: 2 };\n",
            3, 14, "a class is not a constant expression");
    rejects("class P { pub x: i64 = 3, pub y: i64, }\n"
            "const X: i64 = P { y: 1 }.x;\n",
            2, 16, "a field of a class is not a constant expression");
    accepts("struct In { x: i64 }\nstruct Out { inner: In, y: i64 }\n"
            "const O: Out = Out { inner: In { x: 5 }, y: 2 };\n"
            "const Z: i64 = O.inner.x + O.y;\n");

    /* The product of the length and the size of an item wrapped to 0. */
    rejects("const A: [576460752303423488]i64 = [0; 576460752303423488];\n",
            1, 36, "an array of more than 1048576 elements is not a "
            "constant expression");
    rejects("const A: [1025][1024]u8 = [[0; 1024]; 1025];\n", 1, 27,
            "an array of more than 1048576 elements is not a constant "
            "expression");
    accepts("const A: [1024][1024]u8 = [[0; 1024]; 1024];\n");

    /* A variant without cases has no case to name as the example. */
    rejects_also("variant V { }\nfn f(v: V) -> bool { return v is X; }\n",
                 2, 29, "`is` on `V` names one of its cases");
}
