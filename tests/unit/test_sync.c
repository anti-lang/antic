#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* The rules of "Locking and channels" in
   docs/anti-language-additions.md. `anti.lang.Mutex` and `sync m { }`,
   `chan T` with `send`, `recv` and `close`, and `select` over the
   channels. */

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

/* A source the parser refuses, with its first message. */
static void syntax_error(const char *source, int line, int column,
                         const char *message)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;

    CHECK(lex(source, strlen(source), &arena, &diags, &tokens));
    CHECK(!parse(source, &tokens, &arena, &diags, &module));
    if (diags.count == 0 || diags.items[0].line != line ||
        diags.items[0].column != column ||
        strcmp(diags.items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %d:%d: %s\ngot      %d:%d: %s\n%s\n", line,
                column, message, diags.count > 0 ? diags.items[0].line : 0,
                diags.count > 0 ? diags.items[0].column : 0,
                diags.count > 0 ? diags.items[0].message : "nothing",
                source);
    }
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
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

/* The type of the first `let` of the last function of source. */
static void let_type(const char *source, const char *expected)
{
    struct checked c;
    struct text out = {0};

    run(&c, source);
    CHECK(c.ok);
    if (c.ok) {
        const struct item *f = c.module->items[c.module->item_count - 1];
        const struct stmt *s = f->body->stmts[0];
        CHECK(s->kind == STMT_LET && s->as.let.symbol != NULL);
        if (s->kind == STMT_LET && s->as.let.symbol != NULL) {
            type_name(&out, s->as.let.symbol->type);
            CHECK_STR(text_cstr(&out), expected);
        }
    }
    text_free(&out);
    release(&c);
}

/* A struct with a mutex in it, on line 1 of the cases below. */
#define ACCOUNT "struct Account { lock: Mutex, total: int }\n"

void test_sync(void)
{
    /* The five words the threading chapter reserved are keywords now.
       `thread` stays reserved, and `close` is a name. */
    {
        static const enum token_kind k[] = {
            TOKEN_SYNC, TOKEN_CHAN, TOKEN_SEND, TOKEN_RECV, TOKEN_SELECT,
            TOKEN_RESERVED, TOKEN_IDENT, TOKEN_IDENT, TOKEN_EOF};
        struct arena arena = {0};
        struct diagnostics diags = {0};
        struct token_list tokens = {0};
        const char *source = "sync chan send recv select thread close Mutex";
        size_t i;

        CHECK(lex(source, strlen(source), &arena, &diags, &tokens));
        CHECK(tokens.count == sizeof k / sizeof k[0]);
        for (i = 0; i < tokens.count && i < sizeof k / sizeof k[0]; i++) {
            CHECK(tokens.items[i].kind == k[i]);
        }
        token_list_free(&tokens);
        diagnostics_free(&diags);
        arena_free(&arena);
    }

    /* `sync m { }` holds an operand and a block. */
    tree("fn f(m: *Mutex) { sync m { g(); } }\n",
         "function f\n"
         "  param m\n"
         "    type *\n"
         "      type Mutex\n"
         "  block\n"
         "    sync_stmt\n"
         "      ident m\n"
         "      block\n"
         "        simple_stmt\n"
         "          call\n"
         "            ident g\n");
    /* `chan T` is a type, `chan T(n)` makes one, and `send` and `recv`
       name their channel. */
    tree("fn f(c: chan int) { let d = chan int(16); send(d, 1); "
         "let v = recv(c); }\n",
         "function f\n"
         "  param c\n"
         "    type chan\n"
         "      type int\n"
         "  block\n"
         "    let_stmt d\n"
         "      chan\n"
         "        type int\n"
         "        int_lit 16\n"
         "    simple_stmt\n"
         "      send\n"
         "        ident d\n"
         "        int_lit 1\n"
         "    let_stmt v\n"
         "      recv\n"
         "        ident c\n");
    /* `select` names a channel in each arm and the name that takes what
       it receives. The name may be left out. */
    tree("fn f(a: chan int, b: chan int) { select { a x => g(x), "
         "b => { }, } }\n",
         "function f\n"
         "  param a\n"
         "    type chan\n"
         "      type int\n"
         "  param b\n"
         "    type chan\n"
         "      type int\n"
         "  block\n"
         "    select_stmt\n"
         "      arm\n"
         "        ident a\n"
         "        binds x\n"
         "        simple_stmt\n"
         "          call\n"
         "            ident g\n"
         "            ident x\n"
         "      arm\n"
         "        ident b\n"
         "        block\n");
    /* `destroy` names the function of a Mutex after `.`. */
    tree("fn f(m: *Mutex) { m.destroy(); }\n",
         "function f\n"
         "  param m\n"
         "    type *\n"
         "      type Mutex\n"
         "  block\n"
         "    simple_stmt\n"
         "      call\n"
         "        field destroy\n"
         "          ident m\n");
    syntax_error("fn f(a: chan int) { select { a x => g(x), else => h() } }\n",
                 1, 43, "a `select` has no `else`");
    syntax_error("fn f() { select { } }\n", 1, 19,
                 "a `select` waits on one channel or more");

    /* A mutex is made, held by `sync` for a block, and destroyed. The
       block may leave by any exit, and `sync` takes the Mutex or a
       pointer to it. */
    accepts(ACCOUNT
            "fn add(a: *Account, n: int) {\n"
            "    sync a.lock { a.total += n; }\n"
            "}\n"
            "fn first(a: *Account, xs: []int) -> int {\n"
            "    for x in xs {\n"
            "        sync a.lock {\n"
            "            if x < 0 { continue; }\n"
            "            if x > 9 { break; }\n"
            "            if x == 5 { return x; }\n"
            "        }\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "fn main() -> int {\n"
            "    let m = Mutex.new();\n"
            "    let a = Account { lock: Mutex.new(), total: 0 };\n"
            "    add(&a, 2);\n"
            "    sync m { if a.total > 1 { return 1; } }\n"
            "    sync &m { a.total = 0; }\n"
            "    sync a.lock { sync m { a.total = 3; } }\n"
            "    a.lock.destroy();\n"
            "    m.destroy();\n"
            "    let xs = [1, 2];\n"
            "    return first(&a, xs[0..2]);\n"
            "}\n");
    /* `sync` as the last statement returns when its block does. */
    accepts("fn read(m: *Mutex, n: *int) -> int {\n"
            "    sync m { return *n; }\n"
            "}\n");
    /* A channel of values: made with its capacity, sent to, closed,
       received from until `recv` gives `none`, and deleted. A worker
       takes a channel and a mutex as it takes a value. */
    accepts("struct Point { x: int, y: int }\n"
            "worker fn produce(chunk: []int, c: chan int, m: Mutex) -> int {\n"
            "    for x in chunk { sync m { send(c, x); } }\n"
            "    return 0;\n"
            "}\n"
            "fn main() -> int {\n"
            "    let c = chan int(16);\n"
            "    let m = Mutex.new();\n"
            "    let data = [1, 2, 3];\n"
            "    let all = data[0..3];\n"
            "    let r = parallel all -> produce(c, m);\n"
            "    free(r.ptr);\n"
            "    close(c);\n"
            "    let total = 0;\n"
            "    while true do {\n"
            "        let v = recv(c) else { break; };\n"
            "        total += *v;\n"
            "    }\n"
            "    let points = chan Point(2);\n"
            "    send(points, Point { x: 1, y: 2 });\n"
            "    let p = recv(points) else { return 1; };\n"
            "    total += p.x;\n"
            "    delete(points);\n"
            "    delete(c);\n"
            "    m.destroy();\n"
            "    return total;\n"
            "}\n");
    /* `select` binds what its arm's channel gives, `none` once that
       channel is closed and empty. */
    accepts("fn drain(a: chan int, b: chan f32) -> int {\n"
            "    let n = 0;\n"
            "    let a_open = true;\n"
            "    let b_open = true;\n"
            "    while a_open || b_open do {\n"
            "        select {\n"
            "            a x => {\n"
            "                let v = x else { a_open = false; continue; };\n"
            "                n += *v;\n"
            "            },\n"
            "            b y => { if y == none { b_open = false; } },\n"
            "        }\n"
            "    }\n"
            "    return n;\n"
            "}\n");
    /* A function named `close` and a type named `Mutex` win over the
       built-in ones. */
    accepts("fn close(n: int) -> int { return n; }\n"
            "fn main() -> int { return close(3); }\n");
    accepts("struct Mutex { n: int }\n"
            "fn main() -> int { let m = Mutex { n: 1 }; return m.n; }\n");

    /* `recv` gives `?*T` of the element, and `chan T(n)` a `chan T`. */
    let_type("fn f(c: chan int) { let v = recv(c); }\n", "?*int");
    let_type("fn f() { let c = chan f32(4); }\n", "chan f32");
    let_type("fn f() { let m = Mutex.new(); }\n", "Mutex");

    rejects("fn main() -> int { let n = 1; sync n { } return 0; }\n", 1, 36,
            "`sync` takes a `Mutex`, a synchronized object or a pointer to "
            "either, found `int`");
    rejects("fn main() -> int {\n"
            "    let m = Mutex.new();\n"
            "    sync m {\n"
            "        sync m { }\n"
            "    }\n"
            "    return 0;\n"
            "}\n",
            4, 9, "`sync m` inside `sync m` deadlocks");
    rejects(ACCOUNT
            "fn f(a: *Account) {\n"
            "    sync a.lock { if a.total > 0 { sync a.lock { } } }\n"
            "}\n",
            3, 36, "`sync a.lock` inside `sync a.lock` deadlocks");
    rejects("fn main() -> int {\n"
            "    let m = Mutex.new();\n"
            "    sync m { defer sync &m { } }\n"
            "    return 0;\n"
            "}\n",
            3, 20, "`sync &m` inside `sync m` deadlocks");
    rejects("fn f(p: ?*Mutex) { sync p { } }\n", 1, 25,
            "`p` may be `none`, check it or use `?*T`");
    rejects("fn main() -> int { let c = chan *int(4); return 0; }\n", 1, 33,
            "a channel carries values alone, and `*int` holds a pointer");
    rejects("fn main() -> int { let c = chan int(true); return 0; }\n", 1, 37,
            "expected `int`, found `bool`");
    rejects("fn f(c: chan int) { send(c, true); }\n", 1, 29,
            "expected `int`, found `bool`");
    rejects("fn f(n: int) { send(n, 1); }\n", 1, 21,
            "`send` takes a channel, found `int`");
    rejects("fn f(n: int) { let v = recv(n); }\n", 1, 29,
            "`recv` takes a channel, found `int`");
    rejects("fn f(n: int) { close(n); }\n", 1, 22,
            "`close` takes a channel, found `int`");
    rejects("fn f(c: chan int) { close(c, c); }\n", 1, 21,
            "`close` takes 1 argument, found 2");
    rejects("fn f(c: chan int) { let v: *int = recv(c); }\n", 1, 35,
            "the value may be `none`, check it or use `?*T`");
    rejects("fn f(n: int) { select { n x => { } } }\n", 1, 25,
            "an arm of a `select` names a channel, found `int`");
    rejects("fn f() { let m = Mutex.new(1); }\n", 1, 18,
            "`Mutex.new` takes no arguments");
    rejects("fn f(c: chan int) { let d = dup(c); }\n", 1, 33,
            "`dup` needs a class pointer, found `chan int`");
    /* C has no form of either, so an export refuses both. */
    rejects("export fn f(m: *Mutex) { }\n", 1, 13,
            "the parameter `m` of export fn `f` has type `*Mutex`, which C "
            "cannot represent");
    rejects("export fn f(c: chan int) { }\n", 1, 13,
            "the parameter `c` of export fn `f` has type `chan int`, which "
            "C cannot represent");
    rejects("worker fn w(chunk: []int, c: chan int) -> int {\n"
            "    delete(c);\n"
            "    return 0;\n"
            "}\n",
            2, 5, "`w` is a `worker fn` and cannot `delete` an object "
            "another one may hold");
}
