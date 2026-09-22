#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* The rules of "Sum types" in docs/anti-language-additions.md. A variant
   is a tag and a union of its cases, a literal names its case, `switch`
   binds the fields of a case and covers every case, and `is`, `if let`
   and `tag` read the tag. */

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
        fprintf(stderr, "syntax error in test source: %s\n%s\n",
                c->diags.count > 0 ? c->diags.items[0].message : "",
                source);
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

static void tree(const char *source, const char *expected)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;
    struct text out = {0};

    CHECK(lex(source, strlen(source), &arena, &diags, &tokens));
    CHECK(parse(source, &tokens, &arena, &diags, &module));
    if (module != NULL) {
        ast_dump(&out, module);
        CHECK_STR(text_cstr(&out), expected);
    }
    text_free(&out);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
}

/* The kind of the integer under the tag of a variant of count cases
   without fields. */
static enum type_kind tag_kind(size_t count)
{
    struct checked c;
    struct text source = {0};
    enum type_kind kind = TYPE_ERROR;
    size_t i;

    text_append(&source, "variant V {");
    for (i = 0; i < count; i++) {
        text_appendf(&source, " C%zu,", i);
    }
    text_append(&source, " }\n");
    run(&c, text_cstr(&source));
    CHECK(c.ok);
    if (c.ok) {
        const struct type *v = c.module->items[0]->symbol->type;
        CHECK(v->kind == TYPE_VARIANT && v->param_count == count);
        kind = v->base->base->kind;
    }
    release(&c);
    text_free(&source);
    return kind;
}

/* The variant of every case below, on line 1. */
#define SHAPE "variant Shape { Circle { r: f32 }, Square { side: f32 }, " \
              "Empty }\n"

void test_variant(void)
{
    /* `variant` is a keyword. */
    {
        struct arena arena = {0};
        struct diagnostics diags = {0};
        struct token_list tokens = {0};
        CHECK(lex("variant", 7, &arena, &diags, &tokens));
        CHECK(tokens.count == 2 && tokens.items[0].kind == TOKEN_VARIANT);
        token_list_free(&tokens);
        diagnostics_free(&diags);
        arena_free(&arena);
    }

    /* The declaration names each case and its fields, and an arm names
       the case and the name that binds its fields. `if let` is a switch
       of one arm and an else. */
    tree("packed variant Shape align(4) { Circle { r: f32 }, Empty, }\n",
         "variant_decl Shape\n"
         "  packed\n"
         "  align\n"
         "    int_lit 4\n"
         "  case Circle\n"
         "    field r\n"
         "      type f32\n"
         "  case Empty\n");
    tree("fn f(s: Shape) { switch s { Circle c => g(c), Empty => h() } }\n",
         "function f\n"
         "  param s\n"
         "    type Shape\n"
         "  block\n"
         "    switch_stmt\n"
         "      ident s\n"
         "      arm\n"
         "        ident Circle\n"
         "        binds c\n"
         "        simple_stmt\n"
         "          call\n"
         "            ident g\n"
         "            ident c\n"
         "      arm\n"
         "        ident Empty\n"
         "        simple_stmt\n"
         "          call\n"
         "            ident h\n");
    tree("fn f(s: Shape) { if let Circle c = s { g(c); } }\n",
         "function f\n"
         "  param s\n"
         "    type Shape\n"
         "  block\n"
         "    if_let\n"
         "      ident s\n"
         "      arm\n"
         "        ident Circle\n"
         "        binds c\n"
         "        block\n"
         "          simple_stmt\n"
         "            call\n"
         "              ident g\n"
         "              ident c\n"
         "      else\n"
         "        block\n");

    /* Declaration, literals, a switch that covers every case, one with
       `else`, `if let`, `is` and `tag`. */
    accepts(SHAPE
            "fn area(s: Shape) -> f32 {\n"
            "    let a: f32 = 0.0;\n"
            "    switch s {\n"
            "        Circle c => a = 3.0 * c.r * c.r,\n"
            "        Square q => a = q.side * q.side,\n"
            "        Empty => a = 0.0,\n"
            "    }\n"
            "    return a;\n"
            "}\n"
            "fn side(s: Shape) -> f32 {\n"
            "    if let Square q = s { return q.side; }\n"
            "    else if let Circle c = s { return c.r; }\n"
            "    else { return 0.0; }\n"
            "}\n"
            "fn round(s: Shape) -> bool {\n"
            "    let b = false;\n"
            "    switch s { Circle => b = true, else => b = false }\n"
            "    return b;\n"
            "}\n"
            "fn main() -> int {\n"
            "    let s = Shape.Circle { r: 2.0 };\n"
            "    let t = Shape.Empty;\n"
            "    let u: Shape = Shape.Square { side: 1.0 };\n"
            "    if s is Shape.Circle && !(t is Shape.Circle) &&\n"
            "       s.tag != t.tag && u.tag as int == 1 {\n"
            "        return area(s) as int;\n"
            "    }\n"
            "    return 0;\n"
            "}\n");
    /* A variant is a value: a field, an element, a parameter and a
       result. */
    accepts(SHAPE
            "struct Holder { first: Shape, many: [3]Shape }\n"
            "fn make(r: f32) -> Shape { return Shape.Circle { r: r }; }\n"
            "fn main() -> int {\n"
            "    let h = Holder { first: make(1.0),\n"
            "        many: [Shape.Empty, make(2.0), Shape.Square { side: 3.0 }] };\n"
            "    let p = &h.first;\n"
            "    return p.tag as int + h.many[2].tag as int;\n"
            "}\n");
    /* A case with no field has no union member, and a variant of such
       cases alone is its tag. */
    accepts("variant Light { Red, Amber, Green }\n"
            "fn next(l: Light) -> Light {\n"
            "    let n = l;\n"
            "    switch l {\n"
            "        Red => n = Light.Green,\n"
            "        Amber => n = Light.Red,\n"
            "        Green => n = Light.Amber,\n"
            "    }\n"
            "    return n;\n"
            "}\n");
    /* `fallthrough` enters an arm that binds nothing. */
    accepts(SHAPE
            "fn f(s: Shape) -> int {\n"
            "    let n = 0;\n"
            "    switch s { Square => { n = 1; fallthrough; }, Empty => n += 2,\n"
            "        Circle c => n = 3 }\n"
            "    return n;\n"
            "}\n");

    /* A switch without `else` covers every case, and the message names
       the ones it misses. */
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { Circle c => g(c.r) }\n"
            "}\n"
            "fn g(r: f32) { }\n",
            3, 5, "this `switch` on `Shape` has no arm for `Square`, `Empty`");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { Empty => g(), Circle => g(), Empty => g(),\n"
            "        Square => g() }\n"
            "}\n"
            "fn g() { }\n",
            3, 45, "this case already has an arm");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { Triangle t => g(), else => g() }\n"
            "}\n"
            "fn g() { }\n",
            3, 16, "`Shape` has no case `Triangle`");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { Empty e => g(), else => g() }\n"
            "}\n"
            "fn g() { }\n",
            3, 22, "case `Empty` of `Shape` has no fields to bind");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { 1 => g(), else => g() }\n"
            "}\n"
            "fn g() { }\n",
            3, 16, "an arm of a `switch` on `Shape` names one of its cases");
    rejects("fn f(n: int) {\n"
            "    switch n { Circle c => g(), else => g() }\n"
            "}\n"
            "fn g() { }\n",
            2, 23, "an arm binds the fields of a case in a `switch` on a "
                   "variant alone");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    switch s { Empty => { fallthrough; }, Circle c => g(c.r),\n"
            "        else => g(0.0) }\n"
            "}\n"
            "fn g(r: f32) { }\n",
            3, 27, "`fallthrough` into an arm that binds the fields of "
                   "`Circle`");
    rejects(SHAPE
            "fn f(s: Shape) {\n"
            "    if let Circle c = s { fallthrough; }\n"
            "}\n",
            3, 27, "`fallthrough` is allowed as the last statement of a "
                   "`switch` arm only");
    rejects("fn f(n: int) {\n"
            "    if let Circle c = n { }\n"
            "}\n",
            2, 23, "`if let` takes a variant, found `int`");
    /* The name an arm binds lives in the arm alone. */
    rejects(SHAPE
            "fn f(s: Shape) -> f32 {\n"
            "    if let Circle c = s { }\n"
            "    return c.r;\n"
            "}\n",
            4, 12, "unknown name `c`");

    /* `switch` alone reads the fields of a case, and the tag is read
       and never written. */
    rejects(SHAPE
            "fn f(s: Shape) -> f32 { return s.r; }\n",
            2, 32, "a variant has the field `tag` alone, and `switch` reads "
                   "the fields of its cases");
    rejects(SHAPE
            "fn f(s: Shape) { let x = s.u; }\n",
            2, 26, "a variant has the field `tag` alone, and `switch` reads "
                   "the fields of its cases");
    rejects(SHAPE
            "fn f(s: Shape, t: Shape) { s.tag = t.tag; }\n",
            2, 28, "the `tag` of a variant is read-only");
    rejects(SHAPE
            "fn f(s: Shape, t: Shape) -> bool { return s == t; }\n",
            2, 43, "`==` is not defined on `Shape`");

    /* A literal names its case. */
    rejects(SHAPE
            "fn f() -> Shape { return Shape.Circle; }\n",
            2, 26, "`Shape.Circle` has fields, which its literal names in "
                   "braces");
    rejects(SHAPE
            "fn f() -> Shape { return Shape.Triangle; }\n",
            2, 26, "`Shape` has no case `Triangle`");
    rejects(SHAPE
            "fn f() -> Shape { return Shape.Triangle { }; }\n",
            2, 26, "`Shape` has no case `Triangle`");
    rejects(SHAPE
            "fn f() -> Shape { return Shape { }; }\n",
            2, 26, "a literal of variant `Shape` names one of its cases");
    rejects(SHAPE
            "fn f() -> Shape { return Shape.Circle { }; }\n",
            2, 26, "the literal of `Shape.Circle` misses the field `r`");
    rejects(SHAPE
            "fn f() -> Shape { return Shape.Circle { side: 1.0 }; }\n",
            2, 41, "`Shape.Circle` has no field `side`");
    rejects(SHAPE
            "const S: Shape = Shape.Empty;\n",
            2, 18, "a variant is not a constant expression");

    /* `is` names a case of the variant of its operand. */
    rejects(SHAPE
            "fn f(s: Shape) -> bool { return s is Circle; }\n",
            2, 33, "`is` on `Shape` names one of its cases, as "
                   "`Shape.Circle`");
    rejects(SHAPE
            "variant Other { A, B }\n"
            "fn f(s: Shape) -> bool { return s is Other.A; }\n",
            3, 33, "`Other.A` is not a case of `Shape`");
    rejects(SHAPE
            "fn f(s: Shape) -> bool { return s is Shape.Triangle; }\n",
            2, 33, "`Shape` has no case `Triangle`");
    rejects(SHAPE
            "fn f(p: *Shape) -> bool { return p is Shape.Circle; }\n",
            2, 34, "`is` takes a variant as a value, found the pointer "
                   "`*Shape`");
    rejects(SHAPE
            "fn f(p: *Shape) { switch p { Empty => { }, else => { } } }\n",
            2, 26, "`switch` takes an enum, an integer, a `str` or a "
                   "variant, found `*Shape`");

    /* The tag is the smallest unsigned integer that holds the number of
       cases. */
    CHECK(tag_kind(3) == TYPE_U8);
    CHECK(tag_kind(255) == TYPE_U8);
    CHECK(tag_kind(256) == TYPE_U16);

    /* The declaration. */
    rejects("variant V { }\n", 1, 9, "variant `V` has no case");
    rejects("variant V { A, B { x: int }, A }\n", 1, 30,
            "variant `V` has two cases named `A`");
    rejects("variant V { A { x: int, x: f32 } }\n", 1, 25,
            "case `A` of `V` has two fields named `x`");
    rejects("variant V { A { v: V }, B }\n", 1, 9, "variant `V` contains "
            "itself");
    rejects("struct P { x: int }\n"
            "export variant V { A { p: P }, B }\n",
            2, 24, "the field `p` of case `A` of export variant `V` has type "
                   "`P`, and `P` is not exported");
}
