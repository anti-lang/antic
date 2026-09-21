#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* The rules of "Nullable pointers" in docs/anti-language-additions.md.
   `*T` never holds `none`, `?*T` may, and narrowing is per block. */

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

static void rejects(const char *source, const char *message)
{
    struct checked c;

    run(&c, source);
    if (c.ok || c.diags.count == 0) {
        check_failures++;
        fprintf(stderr, "accepted, expected: %s\n%s\n", message, source);
    } else if (strcmp(c.diags.items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %s\ngot      %s\n%s\n", message,
                c.diags.items[0].message, source);
    }
    release(&c);
}

/* A function body around a fragment, so each case is one line of source. */
static void body_accepts(const char *fragment)
{
    struct text source = {0};

    text_appendf(&source, "fn take(p: *int) { }\n"
                          "fn maybe() -> ?*int { return none; }\n"
                          "fn f() {\n%s\n}\n", fragment);
    accepts(text_cstr(&source));
    text_free(&source);
}

static void body_rejects(const char *fragment, const char *message)
{
    struct text source = {0};

    text_appendf(&source, "fn take(p: *int) { }\n"
                          "fn maybe() -> ?*int { return none; }\n"
                          "fn f() {\n%s\n}\n", fragment);
    rejects(text_cstr(&source), message);
    text_free(&source);
}

void test_nullable(void)
{
    /* `*T` never holds `none`. */
    rejects("fn f() { let p: *int = none; }\n", "`*int` cannot hold `none`");
    rejects("fn f() -> *int { return none; }\n", "`*int` cannot hold `none`");
    accepts("fn f() { let p: ?*int = none; }\n");
    accepts("fn f() -> ?*int { return none; }\n");

    /* `none` has type `?*T` for every T, and a context gives it the T. */
    rejects("fn f() { let p = none; }\n",
            "`none` needs a pointer type from its context");

    /* `?*T` cannot be dereferenced, called, indexed or passed where `*T`
       is expected until the program has checked it. */
    body_rejects("    let p = maybe();\n    let n = *p;",
                 "`p` may be `none`, check it or use `?*T`");
    body_rejects("    let p = maybe();\n    take(p);",
                 "`p` may be `none`, check it or use `?*T`");
    body_rejects("    let p = maybe();\n    let q: *int = p;",
                 "`p` may be `none`, check it or use `?*T`");

    /* `*T` passes where `?*T` is expected: the value is a pointer that
       happens never to be `none`. */
    accepts("fn g(p: ?*int) { }\n"
            "fn f(q: *int) { g(q); }\n");

    /* Narrowing is per block. Inside `if p != none { }` the name is
       `*T`, and after the block it is `?*T` again. */
    body_accepts("    let p = maybe();\n"
                 "    if p != none {\n        take(p);\n    }");
    body_rejects("    let p = maybe();\n"
                 "    if p != none {\n        take(p);\n    }\n"
                 "    take(p);",
                 "`p` may be `none`, check it or use `?*T`");

    /* `none != p` narrows as `p != none` does. */
    body_accepts("    let p = maybe();\n"
                 "    if none != p {\n        take(p);\n    }");

    /* After `if p == none { return; }` the name is `*T` for the rest of
       the enclosing block. */
    body_accepts("    let p = maybe();\n"
                 "    if p == none {\n        return;\n    }\n"
                 "    take(p);");
    /* A branch that does not leave narrows nothing after it. */
    body_rejects("    let p = maybe();\n"
                 "    if p == none {\n        take(maybe() as *int);\n    }\n"
                 "    take(p);",
                 "`p` may be `none`, check it or use `?*T`");

    /* The `else` of `if p == none` narrows, and the body of `if p != none`
       leaves its own `else` alone. */
    body_accepts("    let p = maybe();\n"
                 "    if p == none {\n        return;\n    } else {\n"
                 "        take(p);\n    }");
    body_rejects("    let p = maybe();\n"
                 "    if p != none {\n        take(p);\n    } else {\n"
                 "        take(p);\n    }",
                 "`p` may be `none`, check it or use `?*T`");

    /* Nested blocks keep the narrowing of the block that made it. */
    body_accepts("    let p = maybe();\n"
                 "    if p != none {\n"
                 "        {\n            take(p);\n        }\n"
                 "    }");
    body_accepts("    let p = maybe();\n"
                 "    let q = maybe();\n"
                 "    if p != none {\n"
                 "        if q != none {\n"
                 "            take(p);\n            take(q);\n"
                 "        }\n"
                 "        take(p);\n"
                 "    }");
    body_rejects("    let p = maybe();\n"
                 "    let q = maybe();\n"
                 "    if p != none {\n"
                 "        if q != none {\n            take(p);\n        }\n"
                 "        take(q);\n"
                 "    }",
                 "`q` may be `none`, check it or use `?*T`");

    /* Assigning to p inside a narrowed block ends the narrowing for that
       block, because the new value is a `?*T` again. */
    body_rejects("    let p = maybe();\n"
                 "    if p != none {\n"
                 "        take(p);\n"
                 "        p = maybe();\n"
                 "        take(p);\n"
                 "    }",
                 "`p` may be `none`, check it or use `?*T`");
    /* The narrowing of an outer block ends as well, because the name the
       inner block assigned is the same name. */
    body_rejects("    let p = maybe();\n"
                 "    if p != none {\n"
                 "        {\n            p = maybe();\n        }\n"
                 "        take(p);\n"
                 "    }",
                 "`p` may be `none`, check it or use `?*T`");
    /* An assignment after the block that narrowed it is no error: the
       name is `?*T` there in any case. */
    body_accepts("    let p = maybe();\n"
                 "    if p != none {\n        take(p);\n    }\n"
                 "    p = maybe();");

    /* Narrowing follows `&&` and `||`. The right operand of `&&` sees
       what the left proved true, and the right of `||` what it proved
       false. The body of the `if` keeps an `&&` chain and not an `||`. */
    body_accepts("    let p = maybe();\n"
                 "    if p != none && *p > 0 {\n        take(p);\n    }");
    body_accepts("    let p = maybe();\n"
                 "    if p == none || *p > 0 {\n        return;\n    }");
    body_accepts("    let p = maybe();\n    let q = maybe();\n"
                 "    if p != none && q != none && *p + *q > 0 {\n"
                 "        take(p);\n        take(q);\n    }");
    /* An `||` chain proves nothing when it holds. */
    body_rejects("    let p = maybe();\n    let q = maybe();\n"
                 "    if p != none || q != none {\n        take(p);\n    }",
                 "`p` may be `none`, check it or use `?*T`");
    /* Nor does the right operand of an `||` see the left as checked. */
    body_rejects("    let p = maybe();\n"
                 "    if p != none || *p > 0 {\n        return;\n    }",
                 "`p` may be `none`, check it or use `?*T`");
    /* Nested: the chain narrows the body, and a block inside keeps it. */
    body_accepts("    let p = maybe();\n    let q = maybe();\n"
                 "    if p != none && q != none {\n"
                 "        if *p > 0 {\n            take(q);\n        }\n"
                 "    }");

    /* A function value follows the pointer rule. */
    rejects("fn f() { let g: fn() = none; }\n", "`fn()` cannot hold `none`");
    accepts("fn f() { let g: ?fn() = none; }\n");
    rejects("fn one() { }\n"
            "fn f() { let g: ?fn() = one; g(); }\n",
            "`g` may be `none`, check it or use `?fn(...)`");
    accepts("fn one() { }\n"
            "fn f() { let g: ?fn() = one; if g != none { g(); } }\n");
    accepts("fn one() { }\n"
            "fn f() { let g: ?fn() = one; let h = g else { return; }; h(); }\n");
    /* `fn()` passes where `?fn()` is expected, as `*T` does for `?*T`. */
    accepts("fn takes(g: ?fn()) { }\n"
            "fn f(h: fn()) { takes(h); }\n");
    rejects("fn takes(g: fn()) { }\n"
            "fn f(h: ?fn()) { takes(h); }\n",
            "`h` may be `none`, check it or use `?fn(...)`");

    /* `while p != none { }` narrows its body by the same rule. */
    body_accepts("    let p = maybe();\n"
                 "    while p != none do {\n        take(p);\n    }");

    /* `let m = p else { }` binds m as `*T`, and the else block leaves. */
    body_accepts("    let p = maybe();\n"
                 "    let m = p else { return; };\n"
                 "    take(m);");
    body_rejects("    let p = maybe();\n"
                 "    let m = p else { };\n"
                 "    take(m);",
                 "the `else` of a `let` leaves the block it stands in");
    body_rejects("    let n = 1 else { return; };",
                 "the `else` of a `let` follows a value of type `?*T`, "
                 "found `int`");

    /* `p catch fatal` and `p catch e { }` follow the error forms, with
       the error `anti.lang.NoneDereference`. The class is an ordinary
       imported one, so a module that writes the form imports it. */
    body_rejects("    let p = maybe();\n    let m = p catch fatal;",
                 "`catch` on a `?*T` gives an `anti.lang.NoneDereference`, so "
                 "the module imports `anti.lang`");
    body_rejects("    let n = 1 catch fatal;",
                 "`catch` here guards a `?*T`, found `int`");

    /* alloc T { } and alloc T(args) give `*T`. alloc(T, n) gives `?*T`,
       because malloc does. */
    accepts("struct P { x: int }\n"
            "fn f() { let p: *P = alloc P { x: 1 }; free(p); }\n");
    accepts("struct P { x: int }\n"
            "fn f() { let p: ?*P = alloc(P, 4); free(p); }\n");
    rejects("struct P { x: int }\n"
            "fn f() { let p: *P = alloc(P, 4); free(p); }\n",
            "the value may be `none`, check it or use `?*T`");

    /* A `?*T` from C: every pointer of an `extern fn` is one, and the
       program checks it before it uses it. */
    accepts("extern fn getenv(name: ?*byte) -> ?*byte;\n"
            "fn f() -> int {\n"
            "    let found = getenv(\"HOME\".ptr);\n"
            "    if found == none {\n        return 0;\n    }\n"
            "    return found[0] as int;\n"
            "}\n");
    rejects("extern fn getenv(name: ?*byte) -> ?*byte;\n"
            "fn f() -> int { return getenv(\"HOME\".ptr)[0] as int; }\n",
            "the value may be `none`, check it or use `?*T`");

    /* `is`, `as` and `as?` take a `?*T`: `none` is of no class. `dup`,
       `delete` and `destroy` read the table and need a checked one. */
    accepts("class Shape { pub n: int = 0 }\n"
            "class Circle { inherits Shape, pub r: int = 0 }\n"
            "fn f(p: ?*Shape) -> bool { return p is *Circle; }\n");
    accepts("class Shape { pub n: int = 0 }\n"
            "class Circle { inherits Shape, pub r: int = 0 }\n"
            "fn f(p: ?*Shape) -> ?*Circle { return p as? *Circle; }\n");
    rejects("class Shape { pub n: int = 0 }\n"
            "fn f(p: ?*Shape) { delete(p); }\n",
            "`p` may be `none`, check it or use `?*T`");
    rejects("class Shape { pub n: int = 0 }\n"
            "fn f(p: ?*Shape) -> *Shape { return dup(p); }\n",
            "`p` may be `none`, check it or use `?*T`");

    /* A field of a class follows the same rule as a local. */
    rejects("class Link { pub own next: ?*Link = none, pub n: int = 0 }\n"
            "fn f(l: *Link) -> int { return l.next.n; }\n",
            "`l.next` may be `none`, check it or use `?*T`");

    /* The `ptr` of a str and of a slice is `?*T`: neither holds an
       address when it holds no bytes. */
    rejects("fn take(p: *byte) { }\n"
            "fn f(s: str) { take(s.ptr); }\n",
            "`s.ptr` may be `none`, check it or use `?*T`");

    /* Every pointer of an `extern fn` is `?*T`. */
    accepts("extern fn malloc(n: u64) -> ?*byte;\n"
            "fn f() { let p = malloc(8); if p != none { free(p); } }\n");
    rejects("extern fn malloc(n: u64) -> *byte;\n"
            "fn f() { let p = malloc(8); free(p); }\n",
            "every pointer of the result of `extern fn malloc` is `?*T`");

    /* `p ?? q` gives p as `*T` when it is not `none` and q otherwise, and
       the result is `?*T` when q may be `none`. It binds tighter than
       `==` and groups from the right. */
    body_accepts("    let n = 1;\n    take(maybe() ?? &n);");
    body_accepts("    let n = 1;\n    take(maybe() ?? maybe() ?? &n);");
    body_accepts("    let n = 1;\n    let same: bool = maybe() ?? &n == &n;");
    body_accepts("    let p: ?*int = maybe() ?? maybe();");
    body_rejects("    let q = maybe() ?? maybe();\n    take(q);",
                 "`q` may be `none`, check it or use `?*T`");
    body_rejects("    let n = 1;\n    let p = &n;\n    take(p ?? &n);",
                 "`??` follows a value of type `?*T`, found `*int`");
    body_rejects("    let p = maybe();\n"
                 "    if p != none {\n        take(p ?? p);\n    }",
                 "`??` follows a value of type `?*T`, found `*int`");
    body_rejects("    let b = true;\n    take(maybe() ?? &b);",
                 "expected `?*int`, found `*bool`");
    /* A function value follows the pointer rule. */
    accepts("fn g(f: ?fn(int) -> int, h: fn(int) -> int) -> int {\n"
            "    let k = f ?? h;\n"
            "    return k(1);\n"
            "}\n");

    /* `p?.x` and `p?.f(args)` give `none` when p is `none` and the field
       or the call otherwise. The result is `?*U`, and a field or result
       that is no pointer is refused. */
    accepts("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: ?*Node) -> ?*Node { return p?.next?.next; }\n");
    accepts("struct Node { value: int, next: ?*Node }\n"
            "fn after(n: *Node, k: int) -> ?*Node { return n.next; }\n"
            "fn f(p: ?*Node) -> ?*Node { return p?.after(1); }\n");
    accepts("struct Node { value: int, next: ?*Node }\n"
            "struct Tag { owner: *Node }\n"
            "fn f(t: ?*Tag) -> ?*Node { return t?.owner; }\n");
    accepts("struct Hooks { done: fn(int) -> int }\n"
            "fn f(h: ?*Hooks) -> ?fn(int) -> int { return h?.done; }\n");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: ?*Node) -> int { let v = p?.value; return 0; }\n",
            "`?.` needs a field or a result that is a pointer, found `int`");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn touch(n: *Node) { }\n"
            "fn f(p: ?*Node) { p?.touch(); }\n",
            "`?.` needs a field or a result that is a pointer, and the call "
            "returns no value");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: *Node) -> ?*Node { return p?.next; }\n",
            "`?.` follows a value of type `?*T`, found `*Node`");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: ?*Node) -> ?*Node {\n"
            "    if p != none { return p?.next; }\n"
            "    return none;\n"
            "}\n",
            "`?.` follows a value of type `?*T`, found `*Node`");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: ?*Node) -> *Node { return p?.next; }\n",
            "the value may be `none`, check it or use `?*T`");
    rejects("struct Node { value: int, next: ?*Node }\n"
            "fn f(p: ?*Node) -> int { return p?.next.value; }\n",
            "the value may be `none`, check it or use `?*T`");
}
