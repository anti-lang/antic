#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ir.h"

static void printed(const struct ir_module *m, const char *expected)
{
    struct text out = {0};
    ir_print(&out, m);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

static void verified(const struct ir_module *m, const char *expected_errors)
{
    struct text errors = {0};
    bool ok = ir_verify(m, &errors);
    CHECK(ok == (expected_errors[0] == '\0'));
    CHECK_STR(text_cstr(&errors), expected_errors);
    text_free(&errors);
}

/* The function scale of chapter 1 after constant folding. */
static void scale(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    uint32_t x;
    uint32_t product;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "scale", IR_I64, IR_NO_AGG);
    x = ir_param_add(f, IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    product = ir_binary(f, b0, IR_MUL, IR_I64, ir_temp_op(f, x),
                        ir_int_op(IR_I64, 6));
    ir_ret(f, b0, IR_I64, ir_temp_op(f, product));
    printed(&m, "fn main.scale(%0: i64) -> i64 {\n"
                "b0:\n"
                "    %1 = mul i64 %0, 6\n"
                "    ret i64 %1\n"
                "}\n");
    verified(&m, "");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A loop that sums 0 to n - 1, with two temporaries assigned many times. */
static void loop(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *entry, *test, *body, *done;
    uint32_t n, total, i, less;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "sum", IR_I64, IR_NO_AGG);
    n = ir_param_add(f, IR_I64, IR_NO_AGG);
    entry = ir_block_add(f);
    test = ir_block_add(f);
    body = ir_block_add(f);
    done = ir_block_add(f);
    total = ir_unary(f, entry, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    i = ir_unary(f, entry, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    ir_jump(f, entry, test);
    less = ir_binary(f, test, IR_SLT, IR_I8, ir_temp_op(f, i),
                     ir_temp_op(f, n));
    ir_branch(f, test, ir_temp_op(f, less), body, done);
    ir_assign(f, body, total,
              ir_temp_op(f, ir_binary(f, body, IR_ADD, IR_I64,
                                      ir_temp_op(f, total), ir_temp_op(f, i))));
    ir_assign(f, body, i,
              ir_temp_op(f, ir_binary(f, body, IR_ADD, IR_I64,
                                      ir_temp_op(f, i), ir_int_op(IR_I64, 1))));
    ir_jump(f, body, test);
    ir_ret(f, done, IR_I64, ir_temp_op(f, total));
    printed(&m, "fn main.sum(%0: i64) -> i64 {\n"
                "b0:\n"
                "    %1 = copy i64 0\n"
                "    %2 = copy i64 0\n"
                "    jump b1\n"
                "b1:\n"
                "    %3 = slt i8 %2, %0\n"
                "    branch %3, b2, b3\n"
                "b2:\n"
                "    %4 = add i64 %1, %2\n"
                "    %1 = copy i64 %4\n"
                "    %5 = add i64 %2, 1\n"
                "    %2 = copy i64 %5\n"
                "    jump b1\n"
                "b3:\n"
                "    ret i64 %1\n"
                "}\n");
    verified(&m, "");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A str value in a stack slot, passed to an extern C function. The slot
   names its type and the length field its offset, never a number. */
static void memory(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *puts_fn, *f;
    struct ir_global *text;
    struct ir_block *b0;
    struct ir_operand arg;
    struct ir_field fields[2];
    uint32_t str;
    uint32_t slot, bytes, len_field, ptr, status, wide;
    static const uint8_t hi[] = {'h', 'i', 0};

    ir_module_init(&m, &arena, "main");
    memset(fields, 0, sizeof fields);
    fields[0].name = "ptr";
    fields[0].type = ir_scalar(IR_PTR);
    fields[1].name = "len";
    fields[1].type = ir_scalar(IR_I64);
    str = ir_struct_add(&m, IR_AGG_STRUCT, "str", fields, 2, false, 0);
    puts_fn = ir_extern_add(&m, "puts", IR_I32, false);
    ir_param_add(puts_fn, IR_PTR, IR_NO_AGG);
    text = ir_global_add(&m, "main", "str.0", hi, sizeof hi, 1);
    f = ir_function_add(&m, "main", "main", IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    slot = ir_slot(f, b0, ir_aggregate(str));
    bytes = ir_addr(f, b0, ir_global_op(text));
    ir_store(f, b0, IR_PTR, ir_temp_op(f, bytes), ir_temp_op(f, slot));
    len_field = ir_ptradd(f, b0, ir_temp_op(f, slot),
                          ir_sym_operand(&m, ir_sym_offset_of(&m, str, 1)));
    ir_store(f, b0, IR_I64, ir_int_op(IR_I64, 2), ir_temp_op(f, len_field));
    ptr = ir_load(f, b0, IR_PTR, ir_temp_op(f, slot));
    arg = ir_temp_op(f, ptr);
    status = ir_call(f, b0, IR_I32, ir_func_op(puts_fn), &arg, 1);
    wide = ir_unary(f, b0, IR_SEXT, IR_I64, ir_temp_op(f, status));
    ir_ret(f, b0, IR_I64, ir_temp_op(f, wide));
    printed(&m, "type str = struct { ptr: ptr, len: i64 }\n"
                "extern fn puts(ptr) -> i32\n"
                "global main.str.0 size 3 align 1 bytes 68 69 00\n"
                "fn main.main() -> i64 {\n"
                "b0:\n"
                "    %0 = slot str\n"
                "    %1 = addr @main.str.0\n"
                "    store ptr %1, %0\n"
                "    %2 = ptradd %0, offset_of str.len\n"
                "    store i64 2, %2\n"
                "    %3 = load ptr %0\n"
                "    %4 = call i32 @puts(%3)\n"
                "    %5 = sext i64 %4\n"
                "    ret i64 %5\n"
                "}\n");
    verified(&m, "");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A table entry names a function, and the printer takes its name from
   the functions. A lookup among the globals read past them here, with
   two functions and one global. */
static void function_reloc(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *puts_fn, *putchar_fn;
    struct ir_global *table;
    static const uint8_t zero[16] = {0};

    ir_module_init(&m, &arena, "main");
    puts_fn = ir_extern_add(&m, "puts", IR_I32, false);
    ir_param_add(puts_fn, IR_PTR, IR_NO_AGG);
    putchar_fn = ir_extern_add(&m, "putchar", IR_I32, false);
    ir_param_add(putchar_fn, IR_I32, IR_NO_AGG);
    table = ir_global_add(&m, "main", "table", zero, sizeof zero, 8);
    ir_global_reloc_fn(&m, table, 0, 1);
    ir_global_reloc(&m, table, 8, 0);
    printed(&m, "extern fn puts(ptr) -> i32\n"
                "extern fn putchar(i32) -> i32\n"
                "global main.table size 16 align 8 bytes 00 00 00 00 00 00 "
                "00 00 00 00 00 00 00 00 00 00 reloc 0 @putchar "
                "reloc 8 @main.table\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* The file table holds each path once, and every instruction carries the
   line of the statement the function's cursor stands on. */
static void positions(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    uint32_t x;
    uint32_t product;

    ir_module_init(&m, &arena, "main");
    CHECK(ir_file_add(&m, "com/example/scale.anti") == 0);
    CHECK(ir_file_add(&m, "com/example/scale.anti") == 0);
    CHECK(ir_file_add(&m, "main.anti") == 1);
    CHECK(m.file_count == 2);
    f = ir_function_add(&m, "main", "scale", IR_I64, IR_NO_AGG);
    CHECK(f->file == IR_NO_INDEX);
    f->file = 0;
    f->decl_line = 3;
    x = ir_param_add(f, IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    f->at_line = 5;
    product = ir_binary(f, b0, IR_MUL, IR_I64, ir_temp_op(f, x),
                        ir_int_op(IR_I64, 6));
    f->at_line = 6;
    ir_ret(f, b0, IR_I64, ir_temp_op(f, product));
    CHECK(b0->insts[0].line == 5);
    CHECK(b0->insts[1].line == 6);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Symbolic values name sizes and offsets and combine with operations.
   Equal values share one entry, and an entry for a number is an integer
   operand. */
static void symbolic(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    struct ir_field fields[2];
    uint32_t foo, size, length, bytes, x, scaled, sum, copy;

    ir_module_init(&m, &arena, "main");
    memset(fields, 0, sizeof fields);
    fields[0].name = "a";
    fields[0].type = ir_scalar(IR_I8);
    fields[1].name = "b";
    fields[1].type = ir_scalar(IR_I32);
    foo = ir_struct_add(&m, IR_AGG_STRUCT, "main.Foo", fields, 2, false, 0);
    CHECK(ir_struct_add(&m, IR_AGG_STRUCT, "main.Foo", fields, 2, false, 0) ==
          foo);
    size = ir_sym_size_of(&m, ir_aggregate(foo));
    CHECK(ir_sym_size_of(&m, ir_aggregate(foo)) == size);
    length = ir_sym_op(&m, IR_SUB, IR_I64, size, ir_sym_int(&m, IR_I64, 1));
    bytes = ir_array_add(&m, "[size_of(main.Foo) - 1]byte", ir_scalar(IR_I8),
                         length, "size_of(Foo) - 1");
    CHECK(ir_sym_operand(&m, ir_sym_int(&m, IR_I64, 7)).kind == IR_INT);
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    x = ir_param_add(f, IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    copy = ir_slot(f, b0, ir_aggregate(bytes));
    scaled = ir_binary(f, b0, IR_MUL, IR_I64, ir_temp_op(f, x),
                       ir_sym_operand(&m, size));
    sum = ir_binary(f, b0, IR_ADD, IR_I64, ir_temp_op(f, scaled),
                    ir_sym_operand(&m, length));
    ir_memcopy(f, b0, ir_temp_op(f, copy), ir_temp_op(f, copy),
               ir_aggregate(bytes));
    ir_ret(f, b0, IR_I64, ir_temp_op(f, sum));
    printed(&m, "type main.Foo = struct { a: i8, b: i32 }\n"
                "type [size_of(main.Foo) - 1]byte = array "
                "sub i64(size_of main.Foo, 1) of i8\n"
                "fn main.f(%0: i64) -> i64 {\n"
                "b0:\n"
                "    %1 = slot [size_of(main.Foo) - 1]byte\n"
                "    %2 = mul i64 %0, size_of main.Foo\n"
                "    %3 = add i64 %2, sub i64(size_of main.Foo, 1)\n"
                "    memcopy %1, %1, [size_of(main.Foo) - 1]byte\n"
                "    ret i64 %3\n"
                "}\n");
    verified(&m, "");
    ir_module_free(&m);
    arena_free(&arena);
}

static void verifier(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0, *b1;
    uint32_t x, y;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    x = ir_param_add(f, IR_I64, IR_NO_AGG);
    y = ir_param_add(f, IR_I32, IR_NO_AGG);
    b0 = ir_block_add(f);
    b1 = ir_block_add(f);
    ir_binary(f, b0, IR_ADD, IR_I64, ir_temp_op(f, x), ir_temp_op(f, y));
    ir_ret(f, b1, IR_I32, ir_temp_op(f, y));
    verified(&m, "main.f b0: add i64 has an operand of type i32\n"
                 "main.f b0: the block does not end with a terminator\n"
                 "main.f b1: ret i32 in a function that returns i64\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A call of a function that never returns ends its block with
   unreachable, and nothing follows unreachable in a block. */
static void never_returns(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *stop;
    struct ir_function *f;
    struct ir_block *b0, *b1, *b2;

    ir_module_init(&m, &arena, "main");
    stop = ir_extern_add(&m, "stop", IR_VOID, false);
    stop->never_returns = true;
    f = ir_function_add(&m, "main", "f", IR_VOID, IR_NO_AGG);
    b0 = ir_block_add(f);
    b1 = ir_block_add(f);
    b2 = ir_block_add(f);
    ir_call(f, b0, IR_VOID, ir_func_op(stop), NULL, 0);
    ir_jump(f, b0, b1);
    ir_call(f, b1, IR_VOID, ir_func_op(stop), NULL, 0);
    ir_unreachable(f, b1);
    ir_unreachable(f, b2);
    ir_ret(f, b2, IR_VOID, ir_int_op(IR_I64, 0));
    verified(&m, "main.f b0: stop never returns, and the block goes on "
                 "after its call\n"
                 "main.f b2: unreachable is not the last instruction\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A load or a store of a table pointer and a load of an entry reach a
   pointer, and nothing stores an entry. */
static void table_accesses(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    uint32_t p;
    uint32_t table;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        ir_module_init(&m, &arena, "main");
        f = ir_function_add(&m, "main", "f", IR_VOID, IR_NO_AGG);
        p = ir_param_add(f, IR_PTR, IR_NO_AGG);
        b0 = ir_block_add(f);
        table = ir_load_access(f, b0, ir_temp_op(f, p), IR_ACCESS_TABLE);
        ir_load_access(f, b0, ir_temp_op(f, table), IR_ACCESS_ENTRY);
        ir_store_access(f, b0, ir_temp_op(f, table), ir_temp_op(f, p),
                        IR_ACCESS_TABLE);
        if (pass == 1) {
            ir_load(f, b0, IR_I64, ir_temp_op(f, p));
            b0->insts[b0->count - 1].field = IR_ACCESS_TABLE;
            ir_store_access(f, b0, ir_temp_op(f, table), ir_temp_op(f, p),
                            IR_ACCESS_ENTRY);
            ir_store(f, b0, IR_PTR, ir_temp_op(f, table), ir_temp_op(f, p));
            b0->insts[b0->count - 1].field = 3;
        }
        ir_ret(f, b0, IR_VOID, ir_int_op(IR_I64, 0));
        verified(&m, pass == 0
                         ? ""
                         : "main.f b0: a load of a table pointer gives ptr, "
                           "not i64\n"
                           "main.f b0: a store of an entry of a table\n"
                           "main.f b0: an access of the kind 3\n");
        ir_module_free(&m);
    }
    arena_free(&arena);
}

/* The facts of "Parameters and results" in
   docs/work-order-llvm-optimization.md. nonnull, a dereferenceable size and
   own stand on a pointer parameter alone, and a size needs nonnull. An
   extension stands on a result of 8 or 16 bits, and allocates on the
   pointer result of a C function. */
static void param_facts(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *grab;
    struct ir_function *f;
    struct ir_block *b0;
    struct text out = {0};
    int pass;

    for (pass = 0; pass < 2; pass++) {
        ir_module_init(&m, &arena, "main");
        grab = ir_extern_add(&m, "grab", IR_PTR, false);
        grab->allocates = true;
        f = ir_function_add(&m, "main", "f", IR_I8, IR_NO_AGG);
        f->result_ext = IR_EXT_SIGN;
        ir_param_add(f, IR_PTR, IR_NO_AGG);
        f->params[0].nonnull = true;
        f->params[0].deref_size = ir_sym_size_of(&m, ir_scalar(IR_I64));
        f->params[0].own = true;
        ir_param_add(f, IR_I64, IR_NO_AGG);
        b0 = ir_block_add(f);
        ir_ret(f, b0, IR_I8, ir_int_op(IR_I8, 0));
        if (pass == 0) {
            ir_print(&out, &m);
            CHECK(strstr(text_cstr(&out),
                         "extern fn grab() -> ptr allocates\n") != NULL);
            CHECK(strstr(text_cstr(&out),
                         "fn main.f(%0: ptr nonnull deref(size_of i64) own, "
                         "%1: i64) -> i8 signext {") != NULL);
            text_free(&out);
            verified(&m, "");
        } else {
            grab->result_ext = IR_EXT_ZERO;
            f->allocates = true;
            f->params[0].nonnull = false;
            f->params[1].own = true;
            f->params[1].nonnull = true;
            f->params[1].deref_size = (uint32_t)m.sym_count;
            verified(&m, "grab: the result is ptr and has an extension\n"
                         "main.f: the result is i8 and allocates\n"
                         "main.f: a function with a body allocates\n"
                         "main.f: parameter 0 is dereferenceable and may be "
                         "none\n"
                         "main.f: parameter 1 is i64 and has a fact of a "
                         "pointer\n"
                         "main.f: parameter 1 is dereferenceable by a "
                         "symbolic value that does not exist\n");
        }
        ir_module_free(&m);
    }
    arena_free(&arena);
}

/* A call through a pointer passes the arguments of its signature. */
static void indirect_arguments(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *sig;
    struct ir_function *f;
    struct ir_block *b0;
    uint32_t target;
    uint32_t result;

    ir_module_init(&m, &arena, "main");
    sig = ir_declare_add(&m, "main", "fn.0", IR_I64, IR_NO_AGG);
    ir_param_add(sig, IR_I64, IR_NO_AGG);
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    target = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b0 = ir_block_add(f);
    result = ir_call_indirect(f, b0, IR_I64, ir_temp_op(f, target), sig, NULL,
                              0);
    ir_ret(f, b0, IR_I64, ir_temp_op(f, result));
    verified(&m, "main.f b0: call passes 0 arguments to fn.0, which takes 1\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A variadic argument is a scalar, as the checker requires. The back
   ends read the parameter record of an aggregate argument, which a
   variadic argument does not have. */
static void variadic_aggregate(void)
{
    static const char *const names[] = {"x"};
    struct ir_field fields[1];
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *print;
    struct ir_function *f;
    struct ir_block *b0;
    struct ir_operand args[2];
    uint32_t agg;
    uint32_t value;

    ir_module_init(&m, &arena, "main");
    memset(fields, 0, sizeof fields);
    fields[0].name = names[0];
    fields[0].type = ir_scalar(IR_I64);
    agg = ir_struct_add(&m, IR_AGG_STRUCT, "main.Pair", fields, 1, false, 0);
    print = ir_extern_add(&m, "printf", IR_I32, true);
    ir_param_add(print, IR_PTR, IR_NO_AGG);
    f = ir_function_add(&m, "main", "f", IR_VOID, IR_NO_AGG);
    value = ir_param_add(f, IR_AGG, agg);
    b0 = ir_block_add(f);
    args[0] = ir_int_op(IR_PTR, 0);
    args[1] = ir_temp_op(f, value);
    args[1].type = IR_AGG;
    ir_call(f, b0, IR_I32, ir_func_op(print), args, 2);
    ir_ret(f, b0, IR_VOID, ir_int_op(IR_I64, 0));
    verified(&m, "main.f b0: call passes an aggregate as variadic argument 1 "
                 "of printf\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* An integer constant keeps only the bits of its type. */
static void constants(void)
{
    CHECK(ir_int_op(IR_I8, (uint64_t)-1).as.integer == 0xff);
    CHECK(ir_int_op(IR_I16, 0x12345).as.integer == 0x2345);
    CHECK(ir_int_op(IR_I32, (uint64_t)-2).as.integer == 0xfffffffe);
    CHECK(ir_int_op(IR_I64, (uint64_t)-2).as.integer == (uint64_t)-2);
}

/* A temporary is used before any definition on the path through b1. */
static void unassigned(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0, *b1, *b2;
    uint32_t c, x;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    c = ir_param_add(f, IR_I8, IR_NO_AGG);
    b0 = ir_block_add(f);
    b1 = ir_block_add(f);
    b2 = ir_block_add(f);
    ir_branch(f, b0, ir_temp_op(f, c), b1, b2);
    x = ir_unary(f, b1, IR_COPY, IR_I64, ir_int_op(IR_I64, 1));
    ir_jump(f, b1, b2);
    ir_ret(f, b2, IR_I64, ir_temp_op(f, x));
    verified(&m, "main.f b2: ret uses %1 before a definition on some path\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* Every index an instruction or a parameter holds names something the
   module or the function has, and the verifier fails on one that does
   not. */
static void out_of_range(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *g;
    struct ir_function *f;
    struct ir_block *b0;
    struct ir_operand args[1];
    uint32_t x;
    uint32_t loaded;
    uint32_t copied;
    uint32_t result;
    struct ir_inst *inst;

    ir_module_init(&m, &arena, "main");
    g = ir_function_add(&m, "main", "g", IR_I64, IR_NO_AGG);
    ir_param_add(g, IR_I64, IR_NO_AGG);
    ir_ret(g, ir_block_add(g), IR_I64, ir_int_op(IR_I64, 0));
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    x = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b0 = ir_block_add(f);
    loaded = ir_load(f, b0, IR_I64, ir_temp_op(f, x));
    copied = ir_unary(f, b0, IR_COPY, IR_I64, ir_temp_op(f, loaded));
    args[0] = ir_temp_op(f, copied);
    result = ir_call(f, b0, IR_I64, ir_func_op(g), args, 1);
    ir_ret(f, b0, IR_I64, ir_temp_op(f, result));
    verified(&m, "");

    /* A load of global 99, a copy into %77 and a call of function 99
       with %50 as its argument. */
    inst = &b0->insts[0];
    inst->a.kind = IR_GLOBAL;
    inst->a.type = IR_PTR;
    inst->a.as.index = 99;
    b0->insts[1].result = 77;
    inst = &b0->insts[2];
    inst->a.as.index = 99;
    inst->args[0].as.temp = 50;
    verified(&m, "main.f b0: load names global 99, which does not exist\n"
                 "main.f b0: copy writes %77, which does not exist\n"
                 "main.f b0: call names function 99, which does not exist\n"
                 "main.f b0: call uses %50, which does not exist\n");

    /* A parameter whose temporary the function does not have. */
    b0->insts[0].a = ir_temp_op(f, x);
    b0->insts[1].result = copied;
    b0->insts[2].a = ir_func_op(g);
    b0->insts[2].args[0] = ir_temp_op(f, copied);
    verified(&m, "");
    f->params[0].temp = 99;
    verified(&m, "main.f: parameter 0 is %99, which does not exist\n");
    f->params[0].temp = x;

    /* A jump whose target is the integer 65536 and not a block. */
    f = ir_function_add(&m, "main", "h", IR_VOID, IR_NO_AGG);
    b0 = ir_block_add(f);
    ir_jump(f, b0, ir_block_add(f));
    ir_ret(f, f->blocks[1], IR_VOID, ir_int_op(IR_I64, 0));
    f->blocks[1]->insts[0].a.kind = IR_NONE;
    verified(&m, "");
    b0->insts[0].a = ir_int_op(IR_I64, 65536);
    verified(&m, "main.h b0: jump goes to an operand that is not a block\n");
    ir_module_free(&m);
    arena_free(&arena);
}

/* A global of the runtime and a function of a library have no module.
   The printer and the verifier write the name alone, where they once
   passed the NULL module to %s. */
static void no_module(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    static const uint8_t zero[8] = {0};

    ir_module_init(&m, &arena, "main");
    ir_global_add(&m, NULL, "anti_rt_slots", zero, sizeof zero, 8);
    printed(&m, "global anti_rt_slots size 8 align 8 bytes 00 00 00 00 00 "
                "00 00 00\n");
    f = ir_function_add(&m, NULL, "f", IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    ir_ret(f, b0, IR_I32, ir_int_op(IR_I32, 0));
    verified(&m, "f b0: ret i32 in a function that returns i64\n");
    ir_module_free(&m);
    arena_free(&arena);
}

void test_ir(void)
{
    constants();
    out_of_range();
    indirect_arguments();
    variadic_aggregate();
    unassigned();
    scale();
    loop();
    memory();
    function_reloc();
    positions();
    symbolic();
    verifier();
    never_returns();
    table_accesses();
    param_facts();
    no_module();
}
