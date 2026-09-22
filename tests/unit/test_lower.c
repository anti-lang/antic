#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "object.h"
#include "std.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

struct lowered {
    struct arena arena;
    struct diagnostics diags;
    struct token_list tokens;
    struct module *module;
    struct types types;
    struct ir_module ir;
    bool ok;
};

static void run(struct lowered *l, const char *source)
{
    memset(l, 0, sizeof *l);
    types_init(&l->types, &l->arena);
    ir_module_init(&l->ir, &l->arena, "main");
    if (!lex(source, strlen(source), &l->arena, &l->diags, &l->tokens) ||
        !parse(source, &l->tokens, &l->arena, &l->diags, &l->module) ||
        !sema_check(l->module, "main", NULL, NULL, 0, &l->types, &l->arena,
                    &l->diags, true)) {
        fprintf(stderr, "test source does not check: %d:%d: %s\n%s\n",
                l->diags.items[0].line, l->diags.items[0].column,
                l->diags.items[0].message, source);
        check_failures++;
        return;
    }
    l->ok = lower_module(l->module, "main", &l->ir, &l->diags, 0);
}

static void release(struct lowered *l)
{
    ir_module_free(&l->ir);
    token_list_free(&l->tokens);
    diagnostics_free(&l->diags);
    arena_free(&l->arena);
}

static void lowers(const char *source, const char *expected)
{
    struct lowered l;
    struct text out = {0};
    struct text errors = {0};

    run(&l, source);
    CHECK(l.ok);
    ir_print(&out, &l.ir);
    CHECK_STR(text_cstr(&out), expected);
    if (!ir_verify(&l.ir, &errors)) {
        check_failures++;
        fprintf(stderr, "verifier:\n%s", text_cstr(&errors));
    }
    text_free(&out);
    text_free(&errors);
    release(&l);
}

/* The class record of the module named name, or NULL. */
static const struct ir_class *class_named(const struct ir_module *m,
                                          const char *name)
{
    size_t i;

    for (i = 0; i < m->class_count; i++) {
        if (strcmp(m->classes[i]->name, name) == 0) {
            return m->classes[i];
        }
    }
    return NULL;
}

static const char *global_name(const struct ir_module *m, uint32_t g)
{
    return g < m->global_count ? m->globals[g]->name : "(none)";
}

static const struct ir_function *function_named(const struct ir_module *m,
                                                const char *name)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        if (strcmp(m->functions[i]->name, name) == 0) {
            return m->functions[i];
        }
    }
    return NULL;
}

/* Whether the printed body of function name in the IR of source holds
   fragment. */
static bool body_holds(const char *source, const char *name,
                       const char *fragment)
{
    struct lowered l;
    struct text out = {0};
    char header[128];
    const char *start;
    const char *end;
    bool found = false;

    run(&l, source);
    CHECK(l.ok);
    ir_print(&out, &l.ir);
    snprintf(header, sizeof header, "fn %s(", name);
    start = strstr(text_cstr(&out), header);
    end = start != NULL ? strstr(start, "\n}\n") : NULL;
    if (end != NULL) {
        const char *at = strstr(start, fragment);
        found = at != NULL && at < end;
    }
    if (!found) {
        fprintf(stderr, "%s holds no `%s`:\n%s", name, fragment,
                text_cstr(&out));
    }
    text_free(&out);
    release(&l);
    return found;
}

/* alloc(T, n) of a class fills the memory with zeros, so an element the
   program has not filled has a zero table. A struct and a primitive keep
   malloc. */
static void allocates_zeroed_classes(void)
{
    static const char source[] =
        "class Item { pub n: int = 0 }\n"
        "struct Point { x: int, y: int }\n"
        "fn items(n: int) -> ?*Item { return alloc(Item, n); }\n"
        "fn points(n: int) -> ?*Point { return alloc(Point, n); }\n"
        "fn bytes(n: int) -> ?*u8 { return alloc(u8, n); }\n"
        "fn one() -> *Item { return alloc Item { n: 1 }; }\n";

    CHECK(body_holds(source, "main.items", "call ptr @calloc(%0, "));
    CHECK(body_holds(source, "main.points", "call ptr @malloc("));
    CHECK(body_holds(source, "main.bytes", "call ptr @malloc("));
    CHECK(body_holds(source, "main.one", "call ptr @malloc("));
}

/* A class record names the tables of a class, its base, its interfaces
   and the `mutable` fields of a singleton. A `worker fn` keeps its mark.
   A call through a table names the class and the slot. */
/* Lower source and check that every call of the failure routine names
   kind. */
static void checks_kind_of(const char *source, int kind)
{
    struct lowered l;
    bool found = false;
    size_t i;
    size_t b;
    size_t k;

    run(&l, source);
    CHECK(l.ok);
    for (i = 0; i < l.ir.function_count; i++) {
        const struct ir_function *f = l.ir.functions[i];
        for (b = 0; b < f->block_count; b++) {
            for (k = 0; k < f->blocks[b]->count; k++) {
                const struct ir_inst *inst = &f->blocks[b]->insts[k];
                if (inst->op != IR_CALL || inst->a.kind != IR_FUNC ||
                    strcmp(l.ir.functions[inst->a.as.index]->name,
                           "anti_rt_check_failed") != 0) {
                    continue;
                }
                CHECK(inst->arg_count == 5);
                CHECK(inst->args[2].as.integer == (uint64_t)kind);
                found = true;
            }
        }
    }
    CHECK(found);
    release(&l);
}

/* DESIGN: a failed check names the values it prints by the kind of enum
   anti_check in rt/std.h, which the runtime reads. Lowering writes that
   number into the call, and this test pins the two together. */
static void records_check_kinds(void)
{
    checks_kind_of("fn f(a: []int, i: int) -> int { return a[i]; }",
                   ANTI_CHECK_BOUNDS);
    checks_kind_of("fn f(a: int, b: int) -> int { return a + b; }",
                   ANTI_CHECK_OVERFLOW);
    checks_kind_of("fn f(a: int) -> i8 { return a as i8; }",
                   ANTI_CHECK_VALUE);
    checks_kind_of("fn f(a: uint) -> i8 { return a as i8; }",
                   ANTI_CHECK_VALUE_U);
    checks_kind_of("fn f(a: int, b: int) -> int { return a / b; }",
                   ANTI_CHECK_LEFT);
    checks_kind_of("fn f(a: uint, b: uint) -> uint { return a / b; }",
                   ANTI_CHECK_LEFT_U);
    checks_kind_of("fn f(a: int, b: int) -> int { return a << b; }",
                   ANTI_CHECK_SHIFT);
}

static void records_classes(void)
{
    struct lowered l;
    const struct ir_class *shape;
    const struct ir_class *square;
    const struct ir_class *board;
    const struct ir_function *total;
    const struct ir_inst *call = NULL;
    size_t b;
    size_t i;

    run(&l, "abstract class Shape\n"
            "{\n"
            "    abstract fn area(self) -> int;\n"
            "}\n"
            "abstract class Named\n"
            "{\n"
            "    abstract fn label(self) -> int;\n"
            "}\n"
            "final class Square inherits Shape\n"
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
            "singleton class Board\n"
            "{\n"
            "    atomic hits: int = 0,\n"
            "    mutable score: int = 0,\n"
            "}\n"
            "worker fn tally(part: []int) -> int\n"
            "{\n"
            "    return part.len;\n"
            "}\n"
            "fn total(s: *Shape) -> int\n"
            "{\n"
            "    return s.area();\n"
            "}\n");
    CHECK(l.ok);
    shape = class_named(&l.ir, "Shape");
    square = class_named(&l.ir, "Square");
    board = class_named(&l.ir, "Board");
    CHECK(l.ir.class_count == 4 && class_named(&l.ir, "Named") != NULL);
    CHECK(shape != NULL && square != NULL && board != NULL);
    if (shape != NULL) {
        CHECK_STR(shape->module, "main");
        CHECK(shape->flags == IR_CLASS_ABSTRACT);
        CHECK(shape->table == IR_NO_INDEX);
        CHECK_STR(global_name(&l.ir, shape->descriptor), "Shape.descriptor");
        CHECK_STR(global_name(&l.ir, shape->base),
                  "anti_lang_Object_descriptor");
    }
    if (square != NULL) {
        CHECK(square->flags == IR_CLASS_FINAL);
        CHECK_STR(global_name(&l.ir, square->table), "Square.table");
        CHECK_STR(global_name(&l.ir, square->base), "Shape.descriptor");
        CHECK(square->subtable_count == 1);
        if (square->subtable_count == 1) {
            CHECK_STR(global_name(&l.ir, square->subtables[0].interface),
                      "Named.descriptor");
            CHECK_STR(global_name(&l.ir, square->subtables[0].table),
                      "Square.n.table");
        }
        CHECK(square->mutable_count == 0);
    }
    if (board != NULL) {
        CHECK(board->flags == IR_CLASS_SINGLETON);
        CHECK(board->mutable_count == 1);
        if (board->mutable_count == 1 && board->agg < l.ir.agg_count) {
            CHECK_STR(l.ir.aggs[board->agg]
                          ->fields[board->mutable_fields[0]].name,
                      "score");
        }
    }
    CHECK(function_named(&l.ir, "tally") != NULL &&
          function_named(&l.ir, "tally")->worker);
    total = function_named(&l.ir, "total");
    CHECK(total != NULL && !total->worker);
    for (b = 0; total != NULL && b < total->block_count; b++) {
        for (i = 0; i < total->blocks[b]->count; i++) {
            if (total->blocks[b]->insts[i].op == IR_CALL) {
                call = &total->blocks[b]->insts[i];
            }
        }
    }
    CHECK(call != NULL);
    if (call != NULL) {
        CHECK(call->c.kind == IR_GLOBAL);
        CHECK_STR(global_name(&l.ir, call->c.as.index), "Shape.descriptor");
        CHECK(call->field == 8);
    }
    release(&l);
}

/* The global of the module named name, or NULL. */
static const struct ir_global *global_of(const struct ir_module *m,
                                         const char *name)
{
    size_t i;

    for (i = 0; i < m->global_count; i++) {
        if (strcmp(m->globals[i]->name, name) == 0) {
            return m->globals[i];
        }
    }
    return NULL;
}

/* One field record: the type id the compiler wrote and the descriptor
   it names, or "(none)". */
struct field_expect {
    uint64_t type;
    const char *descriptor;
};

static void check_field_records(const struct ir_module *m,
                                const char *list,
                                const struct field_expect *expected,
                                size_t count)
{
    const struct ir_global *g = global_of(m, list);
    size_t i;

    CHECK(g != NULL && g->value != NULL);
    if (g == NULL || g->value == NULL) {
        return;
    }
    CHECK(g->value->item_count == count);
    for (i = 0; i < count && i < g->value->item_count; i++) {
        const struct ir_const *item = &g->value->items[i];
        const char *descriptor =
            item->items[5].kind == IR_CONST_ADDR
                ? global_name(m, item->items[5].global)
                : "(none)";
        if (item->items[3].integer != expected[i].type) {
            fprintf(stderr, "%s record %zu: type id %llu, expected %llu\n",
                    list, i, (unsigned long long)item->items[3].integer,
                    (unsigned long long)expected[i].type);
            check_failures++;
        }
        CHECK_STR(descriptor, expected[i].descriptor);
    }
}

/* DESIGN: a field record names the type of its field by the type id of
   rt/object.h and never by a width. The test pins the numbers that the
   compiler writes to the ones the runtime reads. A struct that a field
   names has a descriptor with a field list of its own. */
static void records_type_ids(void)
{
    static const struct field_expect holder[] = {
        {ANTI_TYPE_BOOL, "(none)"},
        {ANTI_TYPE_CHAR, "(none)"},
        {ANTI_TYPE_I8, "(none)"},
        {ANTI_TYPE_I16, "(none)"},
        {ANTI_TYPE_I32, "(none)"},
        {ANTI_TYPE_I64, "(none)"},
        {ANTI_TYPE_CLONG, "(none)"},
        {ANTI_TYPE_U8, "(none)"},
        {ANTI_TYPE_U16, "(none)"},
        {ANTI_TYPE_U32, "(none)"},
        {ANTI_TYPE_U64, "(none)"},
        {ANTI_TYPE_CULONG, "(none)"},
        {ANTI_TYPE_CWCHAR, "(none)"},
        {ANTI_TYPE_F32, "(none)"},
        {ANTI_TYPE_F64, "(none)"},
        {ANTI_TYPE_STR, "(none)"},
        {ANTI_TYPE_PTR | ANTI_TYPE_I32 << 8, "(none)"},
        {ANTI_TYPE_FN, "(none)"},
        {ANTI_TYPE_SLICE | ANTI_TYPE_U16 << 8, "(none)"},
        {ANTI_TYPE_ARRAY | ANTI_TYPE_I8 << 8, "(none)"},
        {ANTI_TYPE_STRUCT, "Size.descriptor"},
        {ANTI_TYPE_UNION, "(none)"},
        {ANTI_TYPE_ENUM | ANTI_TYPE_U8 << 8, "(none)"},
        {ANTI_TYPE_CLASS, "Inner.descriptor"},
        {ANTI_TYPE_PTR | ANTI_TYPE_STRUCT << 8, "Size.descriptor"},
        {ANTI_TYPE_SLICE | ANTI_TYPE_STRUCT << 8, "Size.descriptor"},
        {ANTI_TYPE_PTR | ANTI_TYPE_CLASS << 8, "Inner.descriptor"},
        {ANTI_TYPE_SLICE | ANTI_TYPE_U8 << 8, "(none)"},
        {ANTI_TYPE_F16, "(none)"},
        {ANTI_TYPE_SLICE | ANTI_TYPE_F16 << 8, "(none)"},
    };
    static const struct field_expect size[] = {
        {ANTI_TYPE_I32, "(none)"},
        {ANTI_TYPE_F64, "(none)"},
    };
    struct lowered l;
    const struct ir_global *d;

    run(&l,
        "struct Size { w: i32, h: f64 }\n"
        "union Bits { i: i32, f: f32 }\n"
        "enum Mode: u8 { A, B }\n"
        "class Inner { pub v: int = 0 }\n"
        "class Holder {\n"
        "    a: bool, b: char, c: i8, d: i16, e: i32, f: i64, g: c_long,\n"
        "    h: u8, i: u16, j: u32, k: u64, l: c_ulong, m: c_wchar,\n"
        "    n: f32, o: f64, p: str, q: *i32, r: fn(i32) -> i32,\n"
        "    s: []u16, t: [4]i8, u: Size, v: Bits, w: Mode, x: Inner,\n"
        "    y: *Size, z: []Size, own next: *Inner, modes: []Mode,\n"
        "    half: f16, halves: []f16,\n"
        "}\n");
    CHECK(l.ok);
    check_field_records(&l.ir, "Holder.fields", holder,
                        sizeof holder / sizeof *holder);
    check_field_records(&l.ir, "Size.fields", size,
                        sizeof size / sizeof *size);
    d = global_of(&l.ir, "Size.descriptor");
    CHECK(d != NULL && d->value != NULL);
    if (d != NULL && d->value != NULL) {
        CHECK(d->value->items[6].integer == 2);
        CHECK(d->value->items[7].kind == IR_CONST_ADDR);
        CHECK_STR(global_name(&l.ir, d->value->items[7].global),
                  "Size.fields");
    }
    CHECK(global_of(&l.ir, "Bits.descriptor") == NULL);
    release(&l);
}

void test_lower(void)
{
    /* A bitfield is read and written through the address of its struct and
       the field, and the back end lowers both to shifts and masks. */
    lowers("struct Flags { visible: u32 : 1, level: i8 : 3 }\n"
           "fn f(p: *Flags) -> i8 {\n"
           "    p.visible = 1;\n"
           "    p.level -= 2;\n"
           "    return p.level;\n"
           "}\n",
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr }\n"
           "type main.Flags = struct { visible: i32 : 1 zeroext, level: i8 : 3 signext }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.Flags.descriptor anti.rt.Descriptor { @main.1, i64 5, ptr 0, size_of main.Flags, i64 0, ptr 0, i64 2, @main.Flags.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 6 align 1 bytes 46 6c 61 67 73 00\n"
           "global main.2 size 8 align 1 bytes 76 69 73 69 62 6c 65 00\n"
           "global main.3 size 6 align 1 bytes 6c 65 76 65 6c 00\n"
           "global main.Flags.fields [2]anti.rt.Field { anti.rt.Field { @main.2, i64 7, offset_of main.Flags.visible, i64 0, i64 0, ptr 0 }, anti.rt.Field { @main.3, i64 5, offset_of main.Flags.level, i64 0, i64 0, ptr 0 } }\n"
           "global main.5 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
           "fn main.f(%0: ptr) -> i8 {\n"
           "b0:\n"
           "    bitstore i32 1, %0, main.Flags.visible\n"
           "    %1 = bitload i8 %0, main.Flags.level\n"
           "    %2 = subov i8 %1, 2\n"
           "    branchov %2, b1, b2\n"
           "b1:\n"
           "    %3 = sext i64 %1\n"
           "    %4 = sext i64 2\n"
           "    %5 = addr @main.5\n"
           "    call void @anti_rt_check_failed(%5, 21, 1, %3, %4)\n"
           "    jump b2\n"
           "b2:\n"
           "    bitstore i8 %2, %0, main.Flags.level\n"
           "    %6 = bitload i8 %0, main.Flags.level\n"
           "    ret i8 %6\n"
           "}\n");
    /* packed and align(N) reach the type table, and the back end lays the
       types out. */
    lowers("packed struct P { a: u8, b: i32 }\n"
           "struct A align(16) { a: u8 }\n"
           "fn f(p: *P, a: A) -> i32 {\n"
           "    return p.b + a.a as i32;\n"
           "}\n",
           "type main.A = struct align(16) { a: i8 }\n"
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr }\n"
           "type main.P = packed struct { a: i8, b: i32 }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "type [1]anti.rt.Field = array 1 of anti.rt.Field\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.P.descriptor anti.rt.Descriptor { @main.1, i64 1, ptr 0, size_of main.P, i64 0, ptr 0, i64 2, @main.P.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 2 align 1 bytes 50 00\n"
           "global main.2 size 2 align 1 bytes 61 00\n"
           "global main.3 size 2 align 1 bytes 62 00\n"
           "global main.P.fields [2]anti.rt.Field { anti.rt.Field { @main.2, i64 1, offset_of main.P.a, i64 8, i64 0, ptr 0 }, anti.rt.Field { @main.3, i64 1, offset_of main.P.b, i64 5, i64 0, ptr 0 } }\n"
           "global main.A.descriptor anti.rt.Descriptor { @main.6, i64 1, ptr 0, size_of main.A, i64 0, ptr 0, i64 1, @main.A.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.6 size 2 align 1 bytes 41 00\n"
           "global main.A.fields [1]anti.rt.Field { anti.rt.Field { @main.2, i64 1, offset_of main.A.a, i64 8, i64 0, ptr 0 } }\n"
           "global main.8 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.f(%0: ptr, %1: agg main.A) -> i32 {\n"
           "b0:\n"
           "    %2 = ptradd %0, offset_of main.P.b\n"
           "    %3 = load i32 %2\n"
           "    %4 = load i8 %1\n"
           "    %5 = zext i32 %4\n"
           "    %6 = addov i32 %3, %5\n"
           "    branchov %6, b1, b2\n"
           "b1:\n"
           "    %7 = sext i64 %3\n"
           "    %8 = sext i64 %5\n"
           "    %9 = addr @main.8\n"
           "    call void @anti_rt_check_failed(%9, 21, 1, %7, %8)\n"
           "    jump b2\n"
           "b2:\n"
           "    ret i32 %6\n"
           "}\n");
    /* Every field of a union lies at offset 0. */
    lowers("union Value { i: int, f: f64 }\n"
           "fn bits(x: f64) -> int {\n"
           "    let v = Value { f: x };\n"
           "    return v.i;\n"
           "}\n",
           "type main.Value = union { i: i64, f: f64 }\n"
           "fn main.bits(%0: f64) -> i64 {\n"
           "b0:\n"
           "    %1 = slot main.Value\n"
           "    store f64 %0, %1\n"
           "    %2 = load i64 %1\n"
           "    ret i64 %2\n"
           "}\n");
    /* A target-sized C type has an IR type of its own. A conversion extends
       from a type that is never wider and truncates to one that is never
       wider. */
    lowers("extern fn labs(n: c_long) -> c_long;\n"
           "fn f(a: i32, w: c_wchar) -> i64 {\n"
           "    let x = labs(a as c_long) + 1;\n"
           "    return x as i64 + w as i64 + (x as i32) as i64;\n"
           "}\n",
           "extern fn labs(clong) -> clong\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "global main.2 size 35 align 1 bytes 6d 61 69 6e 3a 34 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 69 33 32 00\n"
           "fn main.f(%0: i32, %1: cwchar) -> i64 {\n"
           "b0:\n"
           "    %2 = sext clong %0\n"
           "    %3 = call clong @labs(%2)\n"
           "    %4 = addov clong %3, 1\n"
           "    branchov %4, b1, b2\n"
           "b1:\n"
           "    %5 = sext i64 %3\n"
           "    %6 = sext i64 1\n"
           "    %7 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%7, 21, 1, %5, %6)\n"
           "    jump b2\n"
           "b2:\n"
           "    %8 = copy clong %4\n"
           "    %9 = sext i64 %8\n"
           "    %10 = zext i64 %1\n"
           "    %11 = addov i64 %9, %10\n"
           "    branchov %11, b3, b4\n"
           "b3:\n"
           "    %12 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%12, 21, 1, %9, %10)\n"
           "    jump b4\n"
           "b4:\n"
           "    %13 = trunc i32 %8\n"
           "    %14 = sext clong %13\n"
           "    %15 = eq i8 %14, %8\n"
           "    branch %15, b6, b5\n"
           "b5:\n"
           "    %16 = sext i64 %8\n"
           "    %17 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%17, 34, 2, %16, 0)\n"
           "    jump b6\n"
           "b6:\n"
           "    %18 = trunc i32 %8\n"
           "    %19 = sext i64 %18\n"
           "    %20 = addov i64 %11, %19\n"
           "    branchov %20, b7, b8\n"
           "b7:\n"
           "    %21 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%21, 21, 1, %11, %19)\n"
           "    jump b8\n"
           "b8:\n"
           "    ret i64 %20\n"
           "}\n");
    /* The length, the stride and .len of an array whose length comes from
       size_of are symbolic values. */
    lowers("struct H { tag: u8, n: i32 }\n"
           "fn f() -> int {\n"
           "    let a: [size_of(H) - 4]byte = [7; size_of(H) - 4];\n"
           "    return a.len + a[1] as int;\n"
           "}\n",
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr }\n"
           "type main.H = struct { tag: i8, n: i32 }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "type [size_of(main.H) - 4]byte = array sub i64(size_of main.H, 4) of i8\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.H.descriptor anti.rt.Descriptor { @main.1, i64 1, ptr 0, size_of main.H, i64 0, ptr 0, i64 2, @main.H.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 2 align 1 bytes 48 00\n"
           "global main.2 size 4 align 1 bytes 74 61 67 00\n"
           "global main.3 size 2 align 1 bytes 6e 00\n"
           "global main.H.fields [2]anti.rt.Field { anti.rt.Field { @main.2, i64 3, offset_of main.H.tag, i64 8, i64 0, ptr 0 }, anti.rt.Field { @main.3, i64 1, offset_of main.H.n, i64 5, i64 0, ptr 0 } }\n"
           "global main.5 size 28 align 1 bytes 6d 61 69 6e 3a 34 3a 20 69 6e 64 65 78 20 6f 75 74 20 6f 66 20 62 6f 75 6e 64 73 00\n"
           "global main.6 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.f() -> i64 {\n"
           "b0:\n"
           "    %0 = slot [size_of(main.H) - 4]byte\n"
           "    %1 = copy i64 0\n"
           "    jump b1\n"
           "b1:\n"
           "    %2 = slt i8 %1, sub i64(size_of main.H, 4)\n"
           "    branch %2, b2, b3\n"
           "b2:\n"
           "    %3 = mul i64 %1, size_of i8\n"
           "    %4 = ptradd %0, %3\n"
           "    store i8 7, %4\n"
           "    %5 = add i64 %1, 1\n"
           "    %1 = copy i64 %5\n"
           "    jump b1\n"
           "b3:\n"
           "    %6 = ult i8 1, sub i64(size_of main.H, 4)\n"
           "    branch %6, b5, b4\n"
           "b4:\n"
           "    %7 = addr @main.5\n"
           "    call void @anti_rt_check_failed(%7, 27, 0, 1, sub i64(size_of main.H, 4))\n"
           "    jump b5\n"
           "b5:\n"
           "    %8 = mul i64 1, size_of i8\n"
           "    %9 = ptradd %0, %8\n"
           "    %10 = load i8 %9\n"
           "    %11 = zext i64 %10\n"
           "    %12 = addov i64 sub i64(size_of main.H, 4), %11\n"
           "    branchov %12, b6, b7\n"
           "b6:\n"
           "    %13 = addr @main.6\n"
           "    call void @anti_rt_check_failed(%13, 21, 1, sub i64(size_of main.H, 4), %11)\n"
           "    jump b7\n"
           "b7:\n"
           "    ret i64 %12\n"
           "}\n");
    lowers("fn scale(x: int) -> int {\n"
           "    let k = 2 + 4;\n"
           "    return x * k;\n"
           "}\n"
           "fn main() -> int {\n"
           "    return scale(7);\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 32 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
           "fn main.scale(%0: i64) -> i64 {\n"
           "b0:\n"
           "    %1 = addov i64 2, 4\n"
           "    branchov %1, b1, b2\n"
           "b1:\n"
           "    %2 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%2, 21, 1, 2, 4)\n"
           "    jump b2\n"
           "b2:\n"
           "    %3 = copy i64 %1\n"
           "    %4 = mulov i64 %0, %3\n"
           "    branchov %4, b3, b4\n"
           "b3:\n"
           "    %5 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%5, 21, 1, %0, %3)\n"
           "    jump b4\n"
           "b4:\n"
           "    ret i64 %4\n"
           "}\n"
           "fn main.main() -> i64 {\n"
           "b0:\n"
           "    %0 = call i64 @main.scale(7)\n"
           "    ret i64 %0\n"
           "}\n");

    /* A local whose address is taken lives in a stack slot. */
    lowers("fn f() -> int {\n"
           "    let x = 5;\n"
           "    let p = &x;\n"
           "    *p = *p + 1;\n"
           "    return x;\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.f() -> i64 {\n"
           "b0:\n"
           "    %0 = slot i64\n"
           "    store i64 5, %0\n"
           "    %1 = copy ptr %0\n"
           "    %2 = load i64 %1\n"
           "    %3 = addov i64 %2, 1\n"
           "    branchov %3, b1, b2\n"
           "b1:\n"
           "    %4 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%4, 21, 1, %2, 1)\n"
           "    jump b2\n"
           "b2:\n"
           "    store i64 %3, %1\n"
           "    %5 = load i64 %0\n"
           "    ret i64 %5\n"
           "}\n");

    /* A while loop and an if chain whose branches all return. */
    lowers("fn g(n: int) -> int {\n"
           "    let i = 0;\n"
           "    while i < n do {\n"
           "        i += 1;\n"
           "    }\n"
           "    if i > 3 {\n"
           "        return 1;\n"
           "    } else {\n"
           "        return 0;\n"
           "    }\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.g(%0: i64) -> i64 {\n"
           "b0:\n"
           "    %1 = copy i64 0\n"
           "    jump b1\n"
           "b1:\n"
           "    %2 = slt i8 %1, %0\n"
           "    branch %2, b2, b3\n"
           "b2:\n"
           "    %3 = addov i64 %1, 1\n"
           "    branchov %3, b4, b5\n"
           "b3:\n"
           "    %5 = sgt i8 %1, 3\n"
           "    branch %5, b6, b7\n"
           "b4:\n"
           "    %4 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%4, 21, 1, %1, 1)\n"
           "    jump b5\n"
           "b5:\n"
           "    %1 = copy i64 %3\n"
           "    jump b1\n"
           "b6:\n"
           "    ret i64 1\n"
           "b7:\n"
           "    ret i64 0\n"
           "}\n");

    /* && evaluates its right operand only when the left one is true. */
    lowers("fn h(a: u32, b: u32) -> bool {\n"
           "    return a / b > 1 && a != b;\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 30 align 1 bytes 6d 61 69 6e 3a 32 3a 20 64 69 76 69 73 69 6f 6e 20 62 79 20 7a 65 72 6f 20 69 6e 20 2f 00\n"
           "fn main.h(%0: i32, %1: i32) -> i8 {\n"
           "b0:\n"
           "    %2 = ne i8 %1, 0\n"
           "    branch %2, b2, b1\n"
           "b1:\n"
           "    %3 = zext i64 %0\n"
           "    %4 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%4, 29, 5, %3, 0)\n"
           "    jump b2\n"
           "b2:\n"
           "    %5 = udiv i32 %0, %1\n"
           "    %6 = ugt i8 %5, 1\n"
           "    %7 = copy i8 %6\n"
           "    branch %6, b3, b4\n"
           "b3:\n"
           "    %8 = ne i8 %0, %1\n"
           "    %7 = copy i8 %8\n"
           "    jump b4\n"
           "b4:\n"
           "    ret i8 %7\n"
           "}\n");

    /* Conversions between types of one IR type need no instruction. */
    lowers("extern fn putchar(c: i32) -> i32;\n"
           "fn main() -> int {\n"
           "    let c: char = 'A';\n"
           "    putchar(c as u32 as i32);\n"
           "    return 0;\n"
           "}\n",
           "extern fn putchar(i32) -> i32\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 35 align 1 bytes 6d 61 69 6e 3a 34 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 69 33 32 00\n"
           "fn main.main() -> i64 {\n"
           "b0:\n"
           "    %0 = copy i32 65\n"
           "    %1 = sge i8 %0, 0\n"
           "    branch %1, b2, b1\n"
           "b1:\n"
           "    %2 = zext i64 %0\n"
           "    %3 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%3, 34, 3, %2, 0)\n"
           "    jump b2\n"
           "b2:\n"
           "    %4 = call i32 @putchar(%0)\n"
           "    ret i64 0\n"
           "}\n");

    /* alloc and free call the C library, and p[i] is an address. */
    /* `alloc(T, n)` gives `?*T`, so the program checks it. The `else`
       of the `let` is the one branch the nullable rules emit, and it is
       the one the program wrote. */
    lowers("fn k() -> i16 {\n"
           "    let p = alloc(i16, 4) else { return 0; };\n"
           "    p[2] = 7;\n"
           "    let v = p[2];\n"
           "    free(p);\n"
           "    return v + size_of(i16) as i16;\n"
           "}\n",
           "extern fn malloc(i64) -> ptr\n"
           "extern fn free(ptr)\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 35 align 1 bytes 6d 61 69 6e 3a 36 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 69 31 36 00\n"
           "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 36 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.k() -> i16 {\n"
           "b0:\n"
           "    %0 = mul i64 4, size_of i16\n"
           "    %1 = call ptr @malloc(%0)\n"
           "    %2 = copy ptr %1\n"
           "    %3 = eq i8 %2, 0\n"
           "    branch %3, b1, b2\n"
           "b1:\n"
           "    ret i16 0\n"
           "b2:\n"
           "    %4 = mul i64 2, size_of i16\n"
           "    %5 = ptradd %2, %4\n"
           "    store i16 7, %5\n"
           "    %6 = mul i64 2, size_of i16\n"
           "    %7 = ptradd %2, %6\n"
           "    %8 = load i16 %7\n"
           "    %9 = copy i16 %8\n"
           "    call void @free(%2)\n"
           "    %10 = trunc i16 size_of i16\n"
           "    %11 = sext i64 %10\n"
           "    %12 = eq i8 %11, size_of i16\n"
           "    branch %12, b4, b3\n"
           "b3:\n"
           "    %13 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%13, 34, 2, size_of i16, 0)\n"
           "    jump b4\n"
           "b4:\n"
           "    %14 = trunc i16 size_of i16\n"
           "    %15 = addov i16 %9, %14\n"
           "    branchov %15, b5, b6\n"
           "b5:\n"
           "    %16 = sext i64 %9\n"
           "    %17 = sext i64 %14\n"
           "    %18 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%18, 21, 1, %16, %17)\n"
           "    jump b6\n"
           "b6:\n"
           "    ret i16 %15\n"
           "}\n");

    lowers("fn m(x: f32) -> f64 {\n"
           "    return (x * 2.0) as f64 + 0.5;\n"
           "}\n",
           "fn main.m(%0: f32) -> f64 {\n"
           "b0:\n"
           "    %1 = fmul f32 %0, 2\n"
           "    %2 = fext f64 %1\n"
           "    %3 = fadd f64 %2, 0.5\n"
           "    ret f64 %3\n"
           "}\n");

    /* An f16 in memory is sixteen bits. A read widens them to an f32 and
       `as f16` narrows an f32 to them, each one operation of the IR. */
    lowers("fn h(p: *f16, x: f32) -> f32 {\n"
           "    *p = x as f16;\n"
           "    return *p;\n"
           "}\n",
           "fn main.h(%0: ptr, %1: f32) -> f32 {\n"
           "b0:\n"
           "    %2 = htrunc i16 %1\n"
           "    store i16 %2, %0\n"
           "    %3 = load i16 %0\n"
           "    %4 = hext f32 %3\n"
           "    ret f32 %4\n"
           "}\n");

    /* A function without a result returns at its end. */
    lowers("fn z(n: int) {\n"
           "    if n > 0 {\n"
           "        return;\n"
           "    }\n"
           "}\n",
           "fn main.z(%0: i64) {\n"
           "b0:\n"
           "    %1 = sgt i8 %0, 0\n"
           "    branch %1, b1, b2\n"
           "b1:\n"
           "    ret\n"
           "b2:\n"
           "    ret\n"
           "}\n");

    /* continue goes to the condition of a do while loop. */
    lowers("fn count(n: int) -> int {\n"
           "    let i = 0;\n"
           "    do {\n"
           "        i += 1;\n"
           "        if i == 5 {\n"
           "            continue;\n"
           "        }\n"
           "        if i > 8 {\n"
           "            break;\n"
           "        }\n"
           "    } while i < n\n"
           "    return i;\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.count(%0: i64) -> i64 {\n"
           "b0:\n"
           "    %1 = copy i64 0\n"
           "    jump b1\n"
           "b1:\n"
           "    %2 = addov i64 %1, 1\n"
           "    branchov %2, b4, b5\n"
           "b2:\n"
           "    %6 = slt i8 %1, %0\n"
           "    branch %6, b1, b3\n"
           "b3:\n"
           "    ret i64 %1\n"
           "b4:\n"
           "    %3 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%3, 21, 1, %1, 1)\n"
           "    jump b5\n"
           "b5:\n"
           "    %1 = copy i64 %2\n"
           "    %4 = eq i8 %1, 5\n"
           "    branch %4, b6, b7\n"
           "b6:\n"
           "    jump b2\n"
           "b7:\n"
           "    %5 = sgt i8 %1, 8\n"
           "    branch %5, b8, b9\n"
           "b8:\n"
           "    jump b3\n"
           "b9:\n"
           "    jump b2\n"
           "}\n");

    /* A condition branches after each operand of && and ||. */
    lowers("fn pick(a: bool, b: bool, c: int) -> int {\n"
           "    if a && (b || c > 0) {\n"
           "        return 1;\n"
           "    } else if !a {\n"
           "        return 2;\n"
           "    }\n"
           "    return 3;\n"
           "}\n",
           "fn main.pick(%0: i8 zeroext, %1: i8 zeroext, %2: i64) -> i64 {\n"
           "b0:\n"
           "    branch %0, b3, b2\n"
           "b1:\n"
           "    ret i64 1\n"
           "b2:\n"
           "    branch %0, b6, b5\n"
           "b3:\n"
           "    branch %1, b1, b4\n"
           "b4:\n"
           "    %3 = sgt i8 %2, 0\n"
           "    branch %3, b1, b2\n"
           "b5:\n"
           "    ret i64 2\n"
           "b6:\n"
           "    ret i64 3\n"
           "}\n");

    /* An address-taken parameter is copied into a slot on entry. */
    lowers("const BIAS: i8 = -128;\n"
           "fn bump(x: i8) -> i16 {\n"
           "    let p = &x;\n"
           "    *p -= BIAS;\n"
           "    return x as i16 + (x as u8 as i16);\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
           "global main.1 size 36 align 1 bytes 6d 61 69 6e 3a 35 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 62 79 74 65 00\n"
           "global main.2 size 22 align 1 bytes 6d 61 69 6e 3a 35 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.bump(%0: i8 signext) -> i16 {\n"
           "b0:\n"
           "    %1 = slot i8\n"
           "    store i8 %0, %1\n"
           "    %2 = copy ptr %1\n"
           "    %3 = load i8 %2\n"
           "    %4 = subov i8 %3, -128\n"
           "    branchov %4, b1, b2\n"
           "b1:\n"
           "    %5 = sext i64 %3\n"
           "    %6 = sext i64 -128\n"
           "    %7 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%7, 21, 1, %5, %6)\n"
           "    jump b2\n"
           "b2:\n"
           "    store i8 %4, %2\n"
           "    %8 = load i8 %1\n"
           "    %9 = sext i16 %8\n"
           "    %10 = load i8 %1\n"
           "    %11 = sge i8 %10, 0\n"
           "    branch %11, b4, b3\n"
           "b3:\n"
           "    %12 = sext i64 %10\n"
           "    %13 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%13, 35, 2, %12, 0)\n"
           "    jump b4\n"
           "b4:\n"
           "    %14 = zext i16 %10\n"
           "    %15 = addov i16 %9, %14\n"
           "    branchov %15, b5, b6\n"
           "b5:\n"
           "    %16 = sext i64 %9\n"
           "    %17 = sext i64 %14\n"
           "    %18 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%18, 21, 1, %16, %17)\n"
           "    jump b6\n"
           "b6:\n"
           "    ret i16 %15\n"
           "}\n");

    lowers("fn conv(a: u16, f: f64) -> f32 {\n"
           "    return (a as f64 * f) as f32 + (f as i32) as f32;\n"
           "}\n",
           "fn main.conv(%0: i16 zeroext, %1: f64) -> f32 {\n"
           "b0:\n"
           "    %2 = uitof f64 %0\n"
           "    %3 = fmul f64 %2, %1\n"
           "    %4 = ftrunc f32 %3\n"
           "    %5 = ftosi i32 %1\n"
           "    %6 = sitof f32 %5\n"
           "    %7 = fadd f32 %4, %6\n"
           "    ret f32 %7\n"
           "}\n");

    /* A string literal is a global with a NUL after its bytes, and a str
       is the address of the bytes and their length. Literals with equal
       bytes share one global. */
    lowers("fn f() -> int {\n"
           "    let s = \"hi\";\n"
           "    let b = b\"h\\0\";\n"
           "    let t = \"hi\";\n"
           "    return s.len + b.len + t.len;\n"
           "}\n",
           "type str = struct { ptr: ptr, len: i64 }\n"
           "type []byte = struct { ptr: ptr, len: i64 }\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 3 align 1 bytes 68 69 00\n"
           "global main.1 size 3 align 1 bytes 68 00 00\n"
           "global main.2 size 22 align 1 bytes 6d 61 69 6e 3a 35 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.f() -> i64 {\n"
           "b0:\n"
           "    %0 = slot str\n"
           "    %1 = slot []byte\n"
           "    %2 = slot str\n"
           "    %3 = addr @main.0\n"
           "    store ptr %3, %0\n"
           "    %4 = ptradd %0, offset_of str.len\n"
           "    store i64 2, %4\n"
           "    %5 = addr @main.1\n"
           "    store ptr %5, %1\n"
           "    %6 = ptradd %1, offset_of []byte.len\n"
           "    store i64 2, %6\n"
           "    %7 = addr @main.0\n"
           "    store ptr %7, %2\n"
           "    %8 = ptradd %2, offset_of str.len\n"
           "    store i64 2, %8\n"
           "    %9 = ptradd %0, offset_of str.len\n"
           "    %10 = load i64 %9\n"
           "    %11 = ptradd %1, offset_of []byte.len\n"
           "    %12 = load i64 %11\n"
           "    %13 = addov i64 %10, %12\n"
           "    branchov %13, b1, b2\n"
           "b1:\n"
           "    %14 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%14, 21, 1, %10, %12)\n"
           "    jump b2\n"
           "b2:\n"
           "    %15 = ptradd %2, offset_of str.len\n"
           "    %16 = load i64 %15\n"
           "    %17 = addov i64 %13, %16\n"
           "    branchov %17, b3, b4\n"
           "b3:\n"
           "    %18 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%18, 21, 1, %13, %16)\n"
           "    jump b4\n"
           "b4:\n"
           "    ret i64 %17\n"
           "}\n");

    /* An element of a str or a slice lies after the loaded pointer. A
       slice of a str starts at ptr + lo and holds hi - lo bytes. */
    lowers("fn g(s: str, a: []i32, i: int) -> i32 {\n"
           "    let t = s[1..3];\n"
           "    return a[i] + s[i] as i32 + t.len as i32;\n"
           "}\n",
           "type str = struct { ptr: ptr, len: i64 }\n"
           "type []i32 = struct { ptr: ptr, len: i64 }\n"
           "type []byte = struct { ptr: ptr, len: i64 }\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 28 align 1 bytes 6d 61 69 6e 3a 33 3a 20 69 6e 64 65 78 20 6f 75 74 20 6f 66 20 62 6f 75 6e 64 73 00\n"
           "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "global main.2 size 35 align 1 bytes 6d 61 69 6e 3a 33 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 69 33 32 00\n"
           "fn main.g(%0: agg str, %1: agg []i32, %2: i64) -> i32 {\n"
           "b0:\n"
           "    %3 = slot []byte\n"
           "    %4 = ptradd %0, offset_of str.len\n"
           "    %5 = load i64 %4\n"
           "    %6 = load ptr %0\n"
           "    %7 = mul i64 1, size_of i8\n"
           "    %8 = ptradd %6, %7\n"
           "    store ptr %8, %3\n"
           "    %9 = sub i64 3, 1\n"
           "    %10 = ptradd %3, offset_of []byte.len\n"
           "    store i64 %9, %10\n"
           "    %11 = ptradd %1, offset_of []i32.len\n"
           "    %12 = load i64 %11\n"
           "    %13 = load ptr %1\n"
           "    %14 = ult i8 %2, %12\n"
           "    branch %14, b2, b1\n"
           "b1:\n"
           "    %15 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%15, 27, 0, %2, %12)\n"
           "    jump b2\n"
           "b2:\n"
           "    %16 = mul i64 %2, size_of i32\n"
           "    %17 = ptradd %13, %16\n"
           "    %18 = load i32 %17\n"
           "    %19 = ptradd %0, offset_of str.len\n"
           "    %20 = load i64 %19\n"
           "    %21 = load ptr %0\n"
           "    %22 = ult i8 %2, %20\n"
           "    branch %22, b4, b3\n"
           "b3:\n"
           "    %23 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%23, 27, 0, %2, %20)\n"
           "    jump b4\n"
           "b4:\n"
           "    %24 = mul i64 %2, size_of i8\n"
           "    %25 = ptradd %21, %24\n"
           "    %26 = load i8 %25\n"
           "    %27 = zext i32 %26\n"
           "    %28 = addov i32 %18, %27\n"
           "    branchov %28, b5, b6\n"
           "b5:\n"
           "    %29 = sext i64 %18\n"
           "    %30 = sext i64 %27\n"
           "    %31 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%31, 21, 1, %29, %30)\n"
           "    jump b6\n"
           "b6:\n"
           "    %32 = ptradd %3, offset_of []byte.len\n"
           "    %33 = load i64 %32\n"
           "    %34 = trunc i32 %33\n"
           "    %35 = sext i64 %34\n"
           "    %36 = eq i8 %35, %33\n"
           "    branch %36, b8, b7\n"
           "b7:\n"
           "    %37 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%37, 34, 2, %33, 0)\n"
           "    jump b8\n"
           "b8:\n"
           "    %38 = trunc i32 %33\n"
           "    %39 = addov i32 %28, %38\n"
           "    branchov %39, b9, b10\n"
           "b9:\n"
           "    %40 = sext i64 %28\n"
           "    %41 = sext i64 %38\n"
           "    %42 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%42, 21, 1, %40, %41)\n"
           "    jump b10\n"
           "b10:\n"
           "    ret i32 %39\n"
           "}\n");

    /* A slice literal stores its fields, and a slice of an array points
       into the array. */
    lowers("fn h(p: *u8, n: int) -> ?*u8 {\n"
           "    let arr = [5, 6, 7];\n"
           "    let s = []byte { ptr: p, len: n };\n"
           "    let whole = arr[0..3];\n"
           "    return s.ptr;\n"
           "}\n",
           "type [3]int = array 3 of i64\n"
           "type []byte = struct { ptr: ptr, len: i64 }\n"
           "type []int = struct { ptr: ptr, len: i64 }\n"
           "fn main.h(%0: ptr, %1: i64) -> ptr {\n"
           "b0:\n"
           "    %2 = slot [3]int\n"
           "    %3 = slot []byte\n"
           "    %4 = slot []int\n"
           "    store i64 5, %2\n"
           "    %5 = mul i64 1, size_of i64\n"
           "    %6 = ptradd %2, %5\n"
           "    store i64 6, %6\n"
           "    %7 = mul i64 2, size_of i64\n"
           "    %8 = ptradd %2, %7\n"
           "    store i64 7, %8\n"
           "    store ptr %0, %3\n"
           "    %9 = ptradd %3, offset_of []byte.len\n"
           "    store i64 %1, %9\n"
           "    %10 = mul i64 0, size_of i64\n"
           "    %11 = ptradd %2, %10\n"
           "    store ptr %11, %4\n"
           "    %12 = sub i64 3, 0\n"
           "    %13 = ptradd %4, offset_of []int.len\n"
           "    store i64 %12, %13\n"
           "    %14 = load ptr %3\n"
           "    ret ptr %14\n"
           "}\n");

    /* A str constant is data of its own that points at the literal. */
    lowers("const GREETING: str = \"hey\";\n"
           "fn k() -> int {\n"
           "    let g = GREETING;\n"
           "    return g.len;\n"
           "}\n",
           "type str = struct { ptr: ptr, len: i64 }\n"
           "global main.0 size 4 align 1 bytes 68 65 79 00\n"
           "global main.1 str { @main.0, i64 3 }\n"
           "fn main.k() -> i64 {\n"
           "b0:\n"
           "    %0 = slot str\n"
           "    %1 = addr @main.1\n"
           "    memcopy %0, %1, str\n"
           "    %2 = ptradd %0, offset_of str.len\n"
           "    %3 = load i64 %2\n"
           "    ret i64 %3\n"
           "}\n");
    /* An aggregate parameter is a pointer to the value, and a field is a
       ptradd of its offset. */
    lowers("struct P { x: int, y: i32 }\n"
           "fn t(p: P) -> int {\n"
           "    return p.x + p.y as int;\n"
           "}\n"
           "fn u(p: *P) {\n"
           "    p.y = 3;\n"
           "}\n",
           "type main.P = struct { x: i64, y: i32 }\n"
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.P.descriptor anti.rt.Descriptor { @main.1, i64 1, ptr 0, size_of main.P, i64 0, ptr 0, i64 2, @main.P.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 2 align 1 bytes 50 00\n"
           "global main.2 size 2 align 1 bytes 78 00\n"
           "global main.3 size 2 align 1 bytes 79 00\n"
           "global main.P.fields [2]anti.rt.Field { anti.rt.Field { @main.2, i64 1, offset_of main.P.x, i64 6, i64 0, ptr 0 }, anti.rt.Field { @main.3, i64 1, offset_of main.P.y, i64 5, i64 0, ptr 0 } }\n"
           "global main.5 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.t(%0: agg main.P) -> i64 {\n"
           "b0:\n"
           "    %1 = load i64 %0\n"
           "    %2 = ptradd %0, offset_of main.P.y\n"
           "    %3 = load i32 %2\n"
           "    %4 = sext i64 %3\n"
           "    %5 = addov i64 %1, %4\n"
           "    branchov %5, b1, b2\n"
           "b1:\n"
           "    %6 = addr @main.5\n"
           "    call void @anti_rt_check_failed(%6, 21, 1, %1, %4)\n"
           "    jump b2\n"
           "b2:\n"
           "    ret i64 %5\n"
           "}\n"
           "fn main.u(%0: ptr) {\n"
           "b0:\n"
           "    %1 = ptradd %0, offset_of main.P.y\n"
           "    store i32 3, %1\n"
           "    ret\n"
           "}\n");

    /* An aggregate local lives in a slot. A literal fills the slot, a copy
       is a memcopy, and a function returns the address of its value. */
    lowers("struct V { x: f64, y: f64 }\n"
           "fn make(a: f64) -> V {\n"
           "    let v = V { x: a, y: 2.0 };\n"
           "    let w = v;\n"
           "    w.y = v.x;\n"
           "    return w;\n"
           "}\n",
           "type main.V = struct { x: f64, y: f64 }\n"
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, "
           "parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: "
           "i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, "
           "functions: ptr }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: "
           "i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "global main.V.descriptor anti.rt.Descriptor { @main.1, i64 1, ptr "
           "0, size_of main.V, i64 0, ptr 0, i64 2, @main.V.fields, ptr 0, i64 "
           "0, i64 0, ptr 0 }\n"
           "global main.1 size 2 align 1 bytes 56 00\n"
           "global main.2 size 2 align 1 bytes 78 00\n"
           "global main.3 size 2 align 1 bytes 79 00\n"
           "global main.V.fields [2]anti.rt.Field { anti.rt.Field { @main.2, "
           "i64 1, offset_of main.V.x, i64 15, i64 0, ptr 0 }, anti.rt.Field { "
           "@main.3, i64 1, offset_of main.V.y, i64 15, i64 0, ptr 0 } }\n"
           "fn main.make(%0: f64) -> agg main.V {\n"
           "b0:\n"
           "    %1 = slot main.V\n"
           "    %2 = slot main.V\n"
           "    store f64 %0, %1\n"
           "    %3 = ptradd %1, offset_of main.V.y\n"
           "    store f64 2, %3\n"
           "    memcopy %2, %1, main.V\n"
           "    %4 = ptradd %2, offset_of main.V.y\n"
           "    %5 = load f64 %1\n"
           "    store f64 %5, %4\n"
           "    ret ptr %2\n"
           "}\n");

    /* Array elements lie at multiples of the element size. */
    lowers("fn sum3(a: [3]i32) -> i32 {\n"
           "    let b = [a[2], a[1], 7];\n"
           "    return b[0] + b[2];\n"
           "}\n",
           "type [3]i32 = array 3 of i32\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 28 align 1 bytes 6d 61 69 6e 3a 32 3a 20 69 6e 64 65 78 20 6f 75 74 20 6f 66 20 62 6f 75 6e 64 73 00\n"
           "global main.1 size 28 align 1 bytes 6d 61 69 6e 3a 33 3a 20 69 6e 64 65 78 20 6f 75 74 20 6f 66 20 62 6f 75 6e 64 73 00\n"
           "global main.2 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.sum3(%0: agg [3]i32) -> i32 {\n"
           "b0:\n"
           "    %1 = slot [3]i32\n"
           "    %2 = ult i8 2, 3\n"
           "    branch %2, b2, b1\n"
           "b1:\n"
           "    %3 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%3, 27, 0, 2, 3)\n"
           "    jump b2\n"
           "b2:\n"
           "    %4 = mul i64 2, size_of i32\n"
           "    %5 = ptradd %0, %4\n"
           "    %6 = load i32 %5\n"
           "    store i32 %6, %1\n"
           "    %7 = mul i64 1, size_of i32\n"
           "    %8 = ptradd %1, %7\n"
           "    %9 = ult i8 1, 3\n"
           "    branch %9, b4, b3\n"
           "b3:\n"
           "    %10 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%10, 27, 0, 1, 3)\n"
           "    jump b4\n"
           "b4:\n"
           "    %11 = mul i64 1, size_of i32\n"
           "    %12 = ptradd %0, %11\n"
           "    %13 = load i32 %12\n"
           "    store i32 %13, %8\n"
           "    %14 = mul i64 2, size_of i32\n"
           "    %15 = ptradd %1, %14\n"
           "    store i32 7, %15\n"
           "    %16 = ult i8 0, 3\n"
           "    branch %16, b6, b5\n"
           "b5:\n"
           "    %17 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%17, 27, 0, 0, 3)\n"
           "    jump b6\n"
           "b6:\n"
           "    %18 = mul i64 0, size_of i32\n"
           "    %19 = ptradd %1, %18\n"
           "    %20 = load i32 %19\n"
           "    %21 = ult i8 2, 3\n"
           "    branch %21, b8, b7\n"
           "b7:\n"
           "    %22 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%22, 27, 0, 2, 3)\n"
           "    jump b8\n"
           "b8:\n"
           "    %23 = mul i64 2, size_of i32\n"
           "    %24 = ptradd %1, %23\n"
           "    %25 = load i32 %24\n"
           "    %26 = addov i32 %20, %25\n"
           "    branchov %26, b9, b10\n"
           "b9:\n"
           "    %27 = sext i64 %20\n"
           "    %28 = sext i64 %25\n"
           "    %29 = addr @main.2\n"
           "    call void @anti_rt_check_failed(%29, 21, 1, %27, %28)\n"
           "    jump b10\n"
           "b10:\n"
           "    ret i32 %26\n"
           "}\n");

    /* The length of an array is its element count. The base is still
       evaluated, for the call in make().len. */
    lowers("extern fn make() -> [2]f64;\n"
           "fn count(a: [5]int) -> int {\n"
           "    return a.len + make().len;\n"
           "}\n",
           "type [2]float = array 2 of f64\n"
           "type [5]int = array 5 of i64\n"
           "extern fn make() -> agg [2]float\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.count(%0: agg [5]int) -> i64 {\n"
           "b0:\n"
           "    %1 = call agg @make()\n"
           "    %2 = addov i64 5, 2\n"
           "    branchov %2, b1, b2\n"
           "b1:\n"
           "    %3 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%3, 21, 1, 5, 2)\n"
           "    jump b2\n"
           "b2:\n"
           "    ret i64 %2\n"
           "}\n");

    /* DESIGN: an aggregate constant is read-only data of the module. The
       IR carries it as a typed tree, because the bytes of a struct depend
       on the target that lays it out. A use is the address of that data,
       so a constant costs no code and every use reads the same bytes. */
    lowers("struct Gap { a: u8, b: i32 }\n"
           "struct Pair { one: Gap, two: Gap }\n"
           "const GAP: Gap = Gap { a: 1, b: 2 };\n"
           "const PAIR: Pair = Pair { one: GAP, two: Gap { a: 3, b: 4 } };\n"
           "fn f(out: *Pair) {\n"
           "    *out = PAIR;\n"
           "}\n",
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, "
           "parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: "
           "i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, "
           "functions: ptr }\n"
           "type main.Gap = struct { a: i8, b: i32 }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: "
           "i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "type main.Pair = struct { one: main.Gap, two: main.Gap }\n"
           "global main.Gap.descriptor anti.rt.Descriptor { @main.1, i64 3, "
           "ptr 0, size_of main.Gap, i64 0, ptr 0, i64 2, @main.Gap.fields, "
           "ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 4 align 1 bytes 47 61 70 00\n"
           "global main.2 size 2 align 1 bytes 61 00\n"
           "global main.3 size 2 align 1 bytes 62 00\n"
           "global main.Gap.fields [2]anti.rt.Field { anti.rt.Field { @main.2, "
           "i64 1, offset_of main.Gap.a, i64 8, i64 0, ptr 0 }, anti.rt.Field "
           "{ @main.3, i64 1, offset_of main.Gap.b, i64 5, i64 0, ptr 0 } }\n"
           "global main.Pair.descriptor anti.rt.Descriptor { @main.6, i64 4, "
           "ptr 0, size_of main.Pair, i64 0, ptr 0, i64 2, @main.Pair.fields, "
           "ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.6 size 5 align 1 bytes 50 61 69 72 00\n"
           "global main.7 size 4 align 1 bytes 6f 6e 65 00\n"
           "global main.8 size 4 align 1 bytes 74 77 6f 00\n"
           "global main.Pair.fields [2]anti.rt.Field { anti.rt.Field { "
           "@main.7, i64 3, offset_of main.Pair.one, i64 21, i64 0, "
           "@main.Gap.descriptor }, anti.rt.Field { @main.8, i64 3, offset_of "
           "main.Pair.two, i64 21, i64 0, @main.Gap.descriptor } }\n"
           "global main.10 main.Pair { main.Gap { i8 1, i32 2 }, main.Gap { i8 "
           "3, i32 4 } }\n"
           "fn main.f(%0: ptr) {\n"
           "b0:\n"
           "    %1 = addr @main.10\n"
           "    memcopy %0, %1, main.Pair\n"
           "    ret\n"
           "}\n");

    /* A constant is read-only data and a literal argument gets a slot of
       its own. A call with an aggregate result gives the address of the
       result. */
    lowers("struct V { x: f64, y: f64 }\n"
           "const ONE: V = V { x: 1.0, y: 1.0 };\n"
           "extern fn add(a: V, b: V) -> V;\n"
           "fn apply() -> f64 {\n"
           "    let v = add(ONE, V { x: 3.0, y: 4.0 });\n"
           "    return v.y;\n"
           "}\n",
           "type main.V = struct { x: f64, y: f64 }\n"
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, "
           "parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: "
           "i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, "
           "functions: ptr }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: "
           "i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "extern fn add(agg main.V, agg main.V) -> agg main.V\n"
           "global main.V.descriptor anti.rt.Descriptor { @main.1, i64 1, ptr "
           "0, size_of main.V, i64 0, ptr 0, i64 2, @main.V.fields, ptr 0, i64 "
           "0, i64 0, ptr 0 }\n"
           "global main.1 size 2 align 1 bytes 56 00\n"
           "global main.2 size 2 align 1 bytes 78 00\n"
           "global main.3 size 2 align 1 bytes 79 00\n"
           "global main.V.fields [2]anti.rt.Field { anti.rt.Field { @main.2, "
           "i64 1, offset_of main.V.x, i64 15, i64 0, ptr 0 }, anti.rt.Field { "
           "@main.3, i64 1, offset_of main.V.y, i64 15, i64 0, ptr 0 } }\n"
           "global main.5 main.V { f64 1, f64 1 }\n"
           "fn main.apply() -> f64 {\n"
           "b0:\n"
           "    %0 = slot main.V\n"
           "    %2 = slot main.V\n"
           "    %1 = addr @main.5\n"
           "    store f64 3, %2\n"
           "    %3 = ptradd %2, offset_of main.V.y\n"
           "    store f64 4, %3\n"
           "    %4 = call agg @add(%1, %2)\n"
           "    memcopy %0, %4, main.V\n"
           "    %5 = ptradd %0, offset_of main.V.y\n"
           "    %6 = load f64 %5\n"
           "    ret f64 %6\n"
           "}\n");

    /* A function used as a value is its address. A call through a
       function pointer names a signature, a declared function of the
       module that no code defines. */
    lowers("fn twice(x: int) -> int {\n"
           "    return x * 2;\n"
           "}\n"
           "fn apply(f: fn(int) -> int, x: int) -> int {\n"
           "    return f(x);\n"
           "}\n"
           "fn main() -> int {\n"
           "    let g = twice;\n"
           "    return apply(g, 3) + g(1);\n"
           "}\n",
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "extern fn main.fn.0(i64) -> i64\n"
           "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 32 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
           "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 39 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.twice(%0: i64) -> i64 {\n"
           "b0:\n"
           "    %1 = mulov i64 %0, 2\n"
           "    branchov %1, b1, b2\n"
           "b1:\n"
           "    %2 = addr @main.0\n"
           "    call void @anti_rt_check_failed(%2, 21, 1, %0, 2)\n"
           "    jump b2\n"
           "b2:\n"
           "    ret i64 %1\n"
           "}\n"
           "fn main.apply(%0: ptr, %1: i64) -> i64 {\n"
           "b0:\n"
           "    %2 = call i64 %0 via @main.fn.0(%1)\n"
           "    ret i64 %2\n"
           "}\n"
           "fn main.main() -> i64 {\n"
           "b0:\n"
           "    %0 = addr @main.twice\n"
           "    %1 = copy ptr %0\n"
           "    %2 = call i64 @main.apply(%1, 3)\n"
           "    %3 = call i64 %1 via @main.fn.0(1)\n"
           "    %4 = addov i64 %2, %3\n"
           "    branchov %4, b1, b2\n"
           "b1:\n"
           "    %5 = addr @main.1\n"
           "    call void @anti_rt_check_failed(%5, 21, 1, %2, %3)\n"
           "    jump b2\n"
           "b2:\n"
           "    ret i64 %4\n"
           "}\n");

    /* A field of function pointer type is called through its value. Equal
       function types share a signature, and a narrow parameter keeps its
       extension. */
    lowers("extern fn abs(x: i32) -> i32;\n"
           "struct Ops { unary: fn(i32) -> i32, narrow: fn(i8) -> i8 }\n"
           "fn call(o: *Ops, x: i32) -> i32 {\n"
           "    let same: fn(i32) -> i32 = abs;\n"
           "    return o.unary(x) + same(x) + o.narrow(1) as i32;\n"
           "}\n",
           "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr }\n"
           "type main.Ops = struct { unary: ptr, narrow: ptr }\n"
           "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
           "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
           "extern fn abs(i32) -> i32\n"
           "extern fn main.fn.0(i32) -> i32\n"
           "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
           "extern fn main.fn.1(i8 signext) -> i8\n"
           "global main.Ops.descriptor anti.rt.Descriptor { @main.1, i64 3, ptr 0, size_of main.Ops, i64 0, ptr 0, i64 2, @main.Ops.fields, ptr 0, i64 0, i64 0, ptr 0 }\n"
           "global main.1 size 4 align 1 bytes 4f 70 73 00\n"
           "global main.2 size 6 align 1 bytes 75 6e 61 72 79 00\n"
           "global main.3 size 7 align 1 bytes 6e 61 72 72 6f 77 00\n"
           "global main.Ops.fields [2]anti.rt.Field { anti.rt.Field { @main.2, i64 5, offset_of main.Ops.unary, i64 18, i64 0, ptr 0 }, anti.rt.Field { @main.3, i64 6, offset_of main.Ops.narrow, i64 18, i64 0, ptr 0 } }\n"
           "global main.5 size 22 align 1 bytes 6d 61 69 6e 3a 35 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
           "fn main.call(%0: ptr, %1: i32) -> i32 {\n"
           "b0:\n"
           "    %2 = addr @abs\n"
           "    %3 = copy ptr %2\n"
           "    %4 = load ptr %0\n"
           "    %5 = call i32 %4 via @main.fn.0(%1)\n"
           "    %6 = call i32 %3 via @main.fn.0(%1)\n"
           "    %7 = addov i32 %5, %6\n"
           "    branchov %7, b1, b2\n"
           "b1:\n"
           "    %8 = sext i64 %5\n"
           "    %9 = sext i64 %6\n"
           "    %10 = addr @main.5\n"
           "    call void @anti_rt_check_failed(%10, 21, 1, %8, %9)\n"
           "    jump b2\n"
           "b2:\n"
           "    %11 = ptradd %0, offset_of main.Ops.narrow\n"
           "    %12 = load ptr %11\n"
           "    %13 = call i8 %12 via @main.fn.1(1)\n"
           "    %14 = sext i32 %13\n"
           "    %15 = addov i32 %7, %14\n"
           "    branchov %15, b3, b4\n"
           "b3:\n"
           "    %16 = sext i64 %7\n"
           "    %17 = sext i64 %14\n"
           "    %18 = addr @main.5\n"
           "    call void @anti_rt_check_failed(%18, 21, 1, %16, %17)\n"
           "    jump b4\n"
           "b4:\n"
           "    ret i32 %15\n"
           "}\n");
    records_classes();
    records_type_ids();
    records_check_kinds();
    allocates_zeroed_classes();
}
