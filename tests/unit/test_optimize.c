#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "sema.h"
#include "types.h"

/* Lower source as module main, run the optimizer on it and compare the
   printed IR. The verifier runs before and after the optimizer. */
static void optimizes(const char *source, const char *expected)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;
    struct types types;
    struct ir_module ir;
    struct text out = {0};
    struct text errors = {0};

    types_init(&types, &arena);
    ir_module_init(&ir, &arena, "main");
    if (!lex(source, strlen(source), &arena, &diags, &tokens) ||
        !parse(source, &tokens, &arena, &diags, &module) ||
        !sema_check(module, "main", NULL, NULL, 0, &types, &arena, &diags, true) ||
        !lower_module(module, "main", &ir, &diags, 0, NULL, 0, PACKAGE_VERSION_DEFAULT)) {
        check_failures++;
        fprintf(stderr, "test source does not lower: %s\n%s\n",
                diags.count > 0 ? diags.items[0].message : "", source);
    } else if (!ir_verify(&ir, &errors)) {
        check_failures++;
        fprintf(stderr, "lowered IR fails verification:\n%s",
                text_cstr(&errors));
    } else {
        ir_optimize(&ir, "main");
        ir_print(&out, &ir);
        CHECK_STR(text_cstr(&out), expected);
        if (!ir_verify(&ir, &errors)) {
            check_failures++;
            fprintf(stderr, "optimized IR fails verification:\n%s",
                    text_cstr(&errors));
        }
    }
    text_free(&out);
    text_free(&errors);
    ir_module_free(&ir);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
}

/* A module compiled on its own keeps each of its functions, and the bodies
   of other modules become declarations. */
static void one_module(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *lib;
    struct ir_function *unused;
    struct ir_function *main_fn;
    struct ir_block *b;
    struct text out = {0};
    struct ir_operand arg;
    uint32_t v;

    ir_module_init(&m, &arena, "main");
    lib = ir_function_add(&m, "com.example.lib", "seven", IR_I64, IR_NO_AGG);
    ir_param_add(lib, IR_I64, IR_NO_AGG);
    b = ir_block_add(lib);
    ir_ret(lib, b, IR_I64, ir_int_op(IR_I64, 7));
    unused = ir_function_add(&m, "main", "unused", IR_I64, IR_NO_AGG);
    b = ir_block_add(unused);
    ir_ret(unused, b, IR_I64, ir_int_op(IR_I64, 1));
    main_fn = ir_function_add(&m, "main", "main", IR_I64, IR_NO_AGG);
    b = ir_block_add(main_fn);
    arg = ir_int_op(IR_I64, 3);
    v = ir_call(main_fn, b, IR_I64, ir_func_op(lib), &arg, 1);
    ir_ret(main_fn, b, IR_I64, ir_temp_op(main_fn, v));
    ir_optimize_module(&m, "main");
    ir_print(&out, &m);
    CHECK_STR(text_cstr(&out),
              "extern fn com.example.lib.seven(i64) -> i64\n"
              "fn main.unused() -> i64 {\n"
              "b0:\n"
              "    ret i64 1\n"
              "}\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %0 = call i64 @com.example.lib.seven(3)\n"
              "    ret i64 %0\n"
              "}\n");
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Three blocks where b1 and b2 jump to each other. A search for the end
   of the jumps that stops after a fixed number of steps would alternate
   between b1 and b2 forever. */
static void jump_cycle(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0, *b1, *b2;
    struct text out = {0};

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "loop", IR_VOID, IR_NO_AGG);
    b0 = ir_block_add(f);
    b1 = ir_block_add(f);
    b2 = ir_block_add(f);
    ir_jump(f, b0, b1);
    ir_jump(f, b1, b2);
    ir_jump(f, b2, b1);
    ir_optimize(&m, "main");
    ir_print(&out, &m);
    CHECK_STR(text_cstr(&out), "fn main.loop() {\n"
                               "b0:\n"
                               "    jump b1\n"
                               "b1:\n"
                               "    jump b1\n"
                               "}\n");
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Store forwarding on IR that is not in SSA form. The function stores or
   loads through an address. It writes the held value, the base of the
   address or its offset again, then loads the address. Case 4 writes the
   base between the ptradd and the store. The load must stay, since the
   held value is no longer what the memory holds. */
static void forward_after_write(int which)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b;
    struct text out = {0};
    uint32_t p;
    uint32_t q;
    uint32_t v;
    uint32_t off;
    uint32_t at;
    uint32_t r;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    q = ir_param_add(f, IR_PTR, IR_NO_AGG);
    v = ir_param_add(f, IR_I64, IR_NO_AGG);
    b = ir_block_add(f);
    off = ir_temp(f, IR_I64);
    ir_assign(f, b, off, ir_int_op(IR_I64, 8));
    at = ir_ptradd(f, b, ir_temp_op(f, p), ir_temp_op(f, off));
    if (which == 4) {
        ir_assign(f, b, p, ir_temp_op(f, q));
    }
    if (which == 3) {
        v = ir_load(f, b, IR_I64, ir_temp_op(f, at));
    } else {
        ir_store(f, b, IR_I64, ir_temp_op(f, v), ir_temp_op(f, at));
    }
    if (which == 0 || which == 3) {
        ir_assign(f, b, v, ir_int_op(IR_I64, 5));
    } else if (which == 1) {
        ir_assign(f, b, p, ir_temp_op(f, q));
    } else if (which == 2) {
        ir_assign(f, b, off, ir_int_op(IR_I64, 16));
    }
    at = ir_ptradd(f, b, ir_temp_op(f, p), ir_temp_op(f, off));
    r = ir_load(f, b, IR_I64, ir_temp_op(f, at));
    ir_ret(f, b, IR_I64, ir_temp_op(f, r));
    ir_optimize(&m, "main");
    ir_print(&out, &m);
    if (strstr(text_cstr(&out), "load i64") == NULL) {
        check_failures++;
        fprintf(stderr, "store forwarding case %d lost the load:\n%s", which,
                text_cstr(&out));
    }
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Two operands of one symbolic offset whose unused bytes of the union
   differ, as C leaves them after `o.as.index = sym`. The optimizer names
   them one offset, so the load reads the 7 of the store. The test runs
   through a slot the pass splits into temporaries and through a parameter. */
static void symbolic_offsets(bool slot)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b;
    struct text out = {0};
    struct ir_field fields[2];
    struct ir_operand dirty;
    struct ir_operand clean;
    uint32_t agg;
    uint32_t sym;
    uint32_t base;
    uint32_t at;
    uint32_t r;

    ir_module_init(&m, &arena, "main");
    memset(fields, 0, sizeof fields);
    fields[0].name = "x";
    fields[0].type.type = IR_I64;
    fields[1].name = "y";
    fields[1].type.type = IR_I64;
    agg = ir_struct_add(&m, IR_AGG_STRUCT, "main.P", fields, 2, false, 0);
    sym = ir_sym_offset_of(&m, agg, 1);
    memset(&dirty, 0xff, sizeof dirty);
    dirty.kind = IR_SYM;
    dirty.type = IR_I64;
    dirty.as.index = sym;
    memset(&clean, 0, sizeof clean);
    clean.kind = IR_SYM;
    clean.type = IR_I64;
    clean.as.index = sym;
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    base = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    if (slot) {
        struct ir_vtype of;
        of.type = IR_AGG;
        of.agg = agg;
        base = ir_slot(f, b, of);
    }
    at = ir_ptradd(f, b, ir_temp_op(f, base), dirty);
    ir_store(f, b, IR_I64, ir_int_op(IR_I64, 7), ir_temp_op(f, at));
    at = ir_ptradd(f, b, ir_temp_op(f, base), clean);
    r = ir_load(f, b, IR_I64, ir_temp_op(f, at));
    ir_ret(f, b, IR_I64, ir_temp_op(f, r));
    ir_optimize(&m, "main");
    ir_print(&out, &m);
    if (strstr(text_cstr(&out), "ret i64 7") == NULL) {
        check_failures++;
        fprintf(stderr, "symbolic offset through a %s:\n%s",
                slot ? "slot" : "parameter", text_cstr(&out));
    }
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

void test_optimize(void)
{
    int which;

    one_module();
    jump_cycle();
    for (which = 0; which < 5; which++) {
        forward_after_write(which);
    }
    symbolic_offsets(false);
    symbolic_offsets(true);
    /* Class records serve the passes before the optimizer. Removing the
       unused globals renumbers the rest, so the records go with them. */
    optimizes("class Box\n"
              "{\n"
              "    n: int = 1,\n"
              "}\n"
              "fn main() -> int\n"
              "{\n"
              "    return 0;\n"
              "}\n",
              "type [17]ptr = array 17 of ptr\n"
              "type anti.rt.Descriptor = struct { name: ptr, "
              "name_length: i64, parent: ptr, size: i64, depth: i64, "
              "ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, "
              "offset: i64, function_count: i64, functions: ptr, version: ptr, version_length: i64, versions: ptr }\n"
              "type anti.lang.Object = struct { table: ptr }\n"
              "type main.Box = struct { super: anti.lang.Object, n: i64 }\n"
              "type [2]ptr = array 2 of ptr\n"
              "type anti.rt.Field = struct { name: ptr, name_length: i64, "
              "offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
              "type [1]anti.rt.Field = array 1 of anti.rt.Field\n"
              "type anti.rt.Function = struct { name: ptr, "
              "name_length: i64, slot: i64, param_count: i64, "
              "signature: ptr }\n"
              "type [16]anti.rt.Function = array 16 of anti.rt.Function\n"
              "type str = struct { ptr: ptr, len: i64 }\n"
              "global (null).anti_lang_Object_descriptor size 0 align 1 "
              "bytes\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    ret i64 0\n"
              "}\n");
    /* The function scale of chapter 1. */
    optimizes("fn scale(x: int) -> int {\n"
              "    let k = 2 + 4;\n"
              "    return x * k;\n"
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
              "    %4 = mulov i64 %0, %1\n"
              "    branchov %4, b3, b4\n"
              "b3:\n"
              "    %5 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%5, 21, 1, %0, %3)\n"
              "    jump b4\n"
              "b4:\n"
              "    ret i64 %4\n"
              "}\n");

    /* Folding wraps and truncates as the target does. */
    /* An operation on a target-sized type wraps at a width that only the
       back end knows, so the optimizer leaves it. */
    optimizes("fn f() -> c_long {\n"
              "    let x: c_long = 65536;\n"
              "    return x * 70000;\n"
              "}\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
              "fn main.f() -> clong {\n"
              "b0:\n"
              "    %0 = mulov clong 65536, 70000\n"
              "    branchov %0, b1, b2\n"
              "b1:\n"
              "    %1 = sext i64 65536\n"
              "    %2 = sext i64 70000\n"
              "    %3 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%3, 21, 1, %1, %2)\n"
              "    jump b2\n"
              "b2:\n"
              "    ret clong %0\n"
              "}\n");
    /* A size is symbolic. The optimizer never folds it, and the operations
       around it stay for the back end. */
    optimizes("struct H { tag: u8, n: i32 }\n"
              "fn f() -> int {\n"
              "    let n = size_of(H);\n"
              "    return n * 1 + 4 - 4;\n"
              "}\n",
              "type anti.rt.Descriptor = struct { name: ptr, name_length: i64, parent: ptr, size: i64, depth: i64, ancestors: ptr, field_count: i64, fields: ptr, destruct: ptr, offset: i64, function_count: i64, functions: ptr, version: ptr, version_length: i64, versions: ptr }\n"
              "type main.H = struct { tag: i8, n: i32 }\n"
              "type anti.rt.Field = struct { name: ptr, name_length: i64, offset: i64, type: i64, owned: i64, descriptor: ptr }\n"
              "type [2]anti.rt.Field = array 2 of anti.rt.Field\n"
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.6 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
              "global main.7 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "global main.8 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
              "fn main.f() -> i64 {\n"
              "b0:\n"
              "    %0 = copy i64 size_of main.H\n"
              "    %1 = mulov i64 size_of main.H, 1\n"
              "    branchov %1, b1, b2\n"
              "b1:\n"
              "    %2 = addr @main.6\n"
              "    call void @anti_rt_check_failed(%2, 21, 1, %0, 1)\n"
              "    jump b2\n"
              "b2:\n"
              "    %3 = addov i64 %1, 4\n"
              "    branchov %3, b3, b4\n"
              "b3:\n"
              "    %4 = addr @main.7\n"
              "    call void @anti_rt_check_failed(%4, 21, 1, %1, 4)\n"
              "    jump b4\n"
              "b4:\n"
              "    %5 = subov i64 %3, 4\n"
              "    branchov %5, b5, b6\n"
              "b5:\n"
              "    %6 = addr @main.8\n"
              "    call void @anti_rt_check_failed(%6, 21, 1, %3, 4)\n"
              "    jump b6\n"
              "b6:\n"
              "    ret i64 %5\n"
              "}\n");
    optimizes("fn f1() -> int { return 7 * 6 - 2; }\n"
              "fn f2() -> u8 { let b: u8 = 250; return b + 10; }\n"
              "fn f3() -> int { return -9 / 2 + -9 % 2; }\n"
              "fn f4() -> i32 { return (-16 >> 2) as i32; }\n"
              "fn f5() -> u32 { let s: u32 = 4000000000; return s >> 4; }\n"
              "fn f6() -> int { return (1.5 * 2.0) as int + (3.9 as int); }\n"
              "fn f7() -> bool { return 3 < 4 && 2.0 != 2.0; }\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 31 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
              "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 31 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
              "global main.4 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "global main.5 size 40 align 1 bytes 6d 61 69 6e 3a 34 3a 20 73 68 69 66 74 20 63 6f 75 6e 74 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 3e 3e 00\n"
              "global main.7 size 40 align 1 bytes 6d 61 69 6e 3a 35 3a 20 73 68 69 66 74 20 63 6f 75 6e 74 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 3e 3e 00\n"
              "global main.8 size 22 align 1 bytes 6d 61 69 6e 3a 36 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "fn main.f1() -> i64 {\n"
              "b0:\n"
              "    %0 = mulov i64 7, 6\n"
              "    branchov %0, b1, b2\n"
              "b1:\n"
              "    %1 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%1, 21, 1, 7, 6)\n"
              "    jump b2\n"
              "b2:\n"
              "    %2 = subov i64 %0, 2\n"
              "    branchov %2, b3, b4\n"
              "b3:\n"
              "    %3 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%3, 21, 1, %0, 2)\n"
              "    jump b4\n"
              "b4:\n"
              "    ret i64 %2\n"
              "}\n"
              "fn main.f2() -> i8 {\n"
              "b0:\n"
              "    ret i8 4\n"
              "}\n"
              "fn main.f3() -> i64 {\n"
              "b0:\n"
              "    %0 = addov i64 -4, -1\n"
              "    branchov %0, b1, b2\n"
              "b1:\n"
              "    %1 = addr @main.4\n"
              "    call void @anti_rt_check_failed(%1, 21, 1, -4, -1)\n"
              "    jump b2\n"
              "b2:\n"
              "    ret i64 %0\n"
              "}\n"
              "fn main.f4() -> i32 {\n"
              "b0:\n"
              "    %0 = shl i64 size_of i64, 3\n"
              "    %1 = ult i8 2, %0\n"
              "    branch %1, b2, b1\n"
              "b1:\n"
              "    %2 = addr @main.5\n"
              "    call void @anti_rt_check_failed(%2, 39, 6, 2, %0)\n"
              "    jump b2\n"
              "b2:\n"
              "    ret i32 -4\n"
              "}\n"
              "fn main.f5() -> i32 {\n"
              "b0:\n"
              "    %0 = shl i64 size_of i32, 3\n"
              "    %1 = ult i8 4, %0\n"
              "    branch %1, b2, b1\n"
              "b1:\n"
              "    %2 = addr @main.7\n"
              "    call void @anti_rt_check_failed(%2, 39, 6, 4, %0)\n"
              "    jump b2\n"
              "b2:\n"
              "    ret i32 250000000\n"
              "}\n"
              "fn main.f6() -> i64 {\n"
              "b0:\n"
              "    %0 = addov i64 3, 3\n"
              "    branchov %0, b1, b2\n"
              "b1:\n"
              "    %1 = addr @main.8\n"
              "    call void @anti_rt_check_failed(%1, 21, 1, 3, 3)\n"
              "    jump b2\n"
              "b2:\n"
              "    ret i64 %0\n"
              "}\n"
              "fn main.f7() -> i8 {\n"
              "b0:\n"
              "    ret i8 0\n"
              "}\n");

    /* Undefined cases stay for the target to execute. */
    optimizes("fn u1() -> int { let z = 0; return 1 / z; }\n"
              "fn u2() -> int { let n = 64; return 1 << n; }\n"
              "fn u3() -> i8 { let m: i8 = -128; return m / -1; }\n"
              "fn u4() -> i32 { let x = 30000000000.0; return x as i32; }\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 30 align 1 bytes 6d 61 69 6e 3a 31 3a 20 64 69 76 69 73 69 6f 6e 20 62 79 20 7a 65 72 6f 20 69 6e 20 2f 00\n"
              "global main.1 size 40 align 1 bytes 6d 61 69 6e 3a 32 3a 20 73 68 69 66 74 20 63 6f 75 6e 74 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 3c 3c 00\n"
              "fn main.u1() -> i64 {\n"
              "b0:\n"
              "    %0 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%0, 29, 4, 1, 0)\n"
              "    %1 = sdiv i64 1, 0\n"
              "    ret i64 %1\n"
              "}\n"
              "fn main.u2() -> i64 {\n"
              "b0:\n"
              "    %0 = shl i64 size_of i64, 3\n"
              "    %1 = ult i8 64, %0\n"
              "    branch %1, b2, b1\n"
              "b1:\n"
              "    %2 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%2, 39, 6, 64, %0)\n"
              "    jump b2\n"
              "b2:\n"
              "    %3 = shl i64 1, 64\n"
              "    ret i64 %3\n"
              "}\n"
              "fn main.u3() -> i8 {\n"
              "b0:\n"
              "    %0 = sdiv i8 -128, -1\n"
              "    ret i8 %0\n"
              "}\n"
              "fn main.u4() -> i32 {\n"
              "b0:\n"
              "    %0 = ftosi i32 30000000000\n"
              "    ret i32 %0\n"
              "}\n");

    /* Identities, strength reduction and constants on the right. */
    optimizes("fn p1(x: int) -> int {\n"
              "    return (x + 0) * 8 + 2 * x - x * 1;\n"
              "}\n"
              "fn p2(x: u16) -> u16 {\n"
              "    return (x & 0) | (x ^ 0) + (x >> 0) / 1;\n"
              "}\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 32 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "global main.1 size 22 align 1 bytes 6d 61 69 6e 3a 32 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
              "global main.2 size 22 align 1 bytes 6d 61 69 6e 3a 32 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
              "global main.3 size 40 align 1 bytes 6d 61 69 6e 3a 35 3a 20 73 68 69 66 74 20 63 6f 75 6e 74 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 3e 3e 00\n"
              "fn main.p1(%0: i64) -> i64 {\n"
              "b0:\n"
              "    %1 = addov i64 %0, 0\n"
              "    branchov %1, b1, b2\n"
              "b1:\n"
              "    %2 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%2, 21, 1, %0, 0)\n"
              "    jump b2\n"
              "b2:\n"
              "    %3 = mulov i64 %1, 8\n"
              "    branchov %3, b3, b4\n"
              "b3:\n"
              "    %4 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%4, 21, 1, %1, 8)\n"
              "    jump b4\n"
              "b4:\n"
              "    %5 = mulov i64 2, %0\n"
              "    branchov %5, b5, b6\n"
              "b5:\n"
              "    %6 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%6, 21, 1, 2, %0)\n"
              "    jump b6\n"
              "b6:\n"
              "    %7 = addov i64 %3, %5\n"
              "    branchov %7, b7, b8\n"
              "b7:\n"
              "    %8 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%8, 21, 1, %3, %5)\n"
              "    jump b8\n"
              "b8:\n"
              "    %9 = mulov i64 %0, 1\n"
              "    branchov %9, b9, b10\n"
              "b9:\n"
              "    %10 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%10, 21, 1, %0, 1)\n"
              "    jump b10\n"
              "b10:\n"
              "    %11 = subov i64 %7, %9\n"
              "    branchov %11, b11, b12\n"
              "b11:\n"
              "    %12 = addr @main.2\n"
              "    call void @anti_rt_check_failed(%12, 21, 1, %7, %9)\n"
              "    jump b12\n"
              "b12:\n"
              "    ret i64 %11\n"
              "}\n"
              "fn main.p2(%0: i16 zeroext) -> i16 {\n"
              "b0:\n"
              "    %1 = shl i64 size_of i16, 3\n"
              "    %2 = ult i8 0, %1\n"
              "    branch %2, b2, b1\n"
              "b1:\n"
              "    %3 = addr @main.3\n"
              "    call void @anti_rt_check_failed(%3, 39, 6, 0, %1)\n"
              "    jump b2\n"
              "b2:\n"
              "    %4 = add i16 %0, %0\n"
              "    ret i16 %4\n"
              "}\n");

    /* A variable assigned more than once stays, and the add moves into
       the assignment. */
    optimizes("fn g(n: int) -> int {\n"
              "    let i = 0;\n"
              "    while i < n do {\n"
              "        i += 1;\n"
              "    }\n"
              "    if i > 3 {\n"
              "        return 1;\n"
              "    } else {\n"
              "        return 0;\n"
              "    }\n"
              "}\n"
              "fn l1(x: int) -> int {\n"
              "    let a = x;\n"
              "    a = a + 1;\n"
              "    let b = a;\n"
              "    return b * 2;\n"
              "}\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "global main.1 size 23 align 1 bytes 6d 61 69 6e 3a 31 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "global main.2 size 23 align 1 bytes 6d 61 69 6e 3a 31 36 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2a 00\n"
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
              "    %4 = sgt i8 %1, 3\n"
              "    branch %4, b6, b7\n"
              "b4:\n"
              "    %5 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%5, 21, 1, %1, 1)\n"
              "    jump b5\n"
              "b5:\n"
              "    %1 = copy i64 %3\n"
              "    jump b1\n"
              "b6:\n"
              "    ret i64 1\n"
              "b7:\n"
              "    ret i64 0\n"
              "}\n"
              "fn main.l1(%0: i64) -> i64 {\n"
              "b0:\n"
              "    %1 = copy i64 %0\n"
              "    %2 = addov i64 %0, 1\n"
              "    branchov %2, b1, b2\n"
              "b1:\n"
              "    %3 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%3, 22, 1, %1, 1)\n"
              "    jump b2\n"
              "b2:\n"
              "    %1 = copy i64 %2\n"
              "    %4 = copy i64 %2\n"
              "    %5 = mulov i64 %2, 2\n"
              "    branchov %5, b3, b4\n"
              "b3:\n"
              "    %6 = addr @main.2\n"
              "    call void @anti_rt_check_failed(%6, 22, 1, %4, 2)\n"
              "    jump b4\n"
              "b4:\n"
              "    ret i64 %5\n"
              "}\n");

    /* Jumps to blocks that only jump go straight to the target. */
    optimizes("fn count(n: int) -> int {\n"
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
              "    %3 = slt i8 %1, %0\n"
              "    branch %3, b1, b3\n"
              "b3:\n"
              "    ret i64 %1\n"
              "b4:\n"
              "    %4 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%4, 21, 1, %1, 1)\n"
              "    jump b5\n"
              "b5:\n"
              "    %1 = copy i64 %2\n"
              "    %5 = eq i8 %2, 5\n"
              "    branch %5, b2, b6\n"
              "b6:\n"
              "    %6 = sgt i8 %1, 8\n"
              "    branch %6, b3, b2\n"
              "}\n");

    /* Blocks that jump to each other end the search for a jump target. */
    optimizes("fn spin() {\n"
              "    while true do {\n"
              "    }\n"
              "}\n",
              "fn main.spin() {\n"
              "b0:\n"
              "    jump b1\n"
              "b1:\n"
              "    jump b1\n"
              "}\n");

    /* An i64 converts to f32 in one rounding step. 2^60 + 2^36 + 1 lies
       1 above the midpoint of two f32 values and rounds up, while
       rounding through f64 first lands on the midpoint and rounds down. */
    optimizes("fn r() -> f32 {\n"
              "    return 1152921573326323713 as f32;\n"
              "}\n",
              "fn main.r() -> f32 {\n"
              "b0:\n"
              "    ret f32 1.1529216420458004e+18\n"
              "}\n");

    /* Memory is never assumed to keep a value between two instructions. */
    optimizes("fn bump(x: i8) -> i16 {\n"
              "    let p = &x;\n"
              "    *p -= -128;\n"
              "    return x as i16 + (x as u8 as i16);\n"
              "}\n",
              "extern fn anti_rt_check_failed(ptr, i64, i32, i64, i64)\n"
              "global main.0 size 22 align 1 bytes 6d 61 69 6e 3a 33 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2d 00\n"
              "global main.1 size 36 align 1 bytes 6d 61 69 6e 3a 34 3a 20 76 61 6c 75 65 20 6f 75 74 20 6f 66 20 72 61 6e 67 65 20 66 6f 72 20 62 79 74 65 00\n"
              "global main.2 size 22 align 1 bytes 6d 61 69 6e 3a 34 3a 20 6f 76 65 72 66 6c 6f 77 20 69 6e 20 2b 00\n"
              "fn main.bump(%0: i8 signext) -> i16 {\n"
              "b0:\n"
              "    %1 = slot i8\n"
              "    store i8 %0, %1\n"
              "    %2 = subov i8 %0, -128\n"
              "    branchov %2, b1, b2\n"
              "b1:\n"
              "    %3 = sext i64 %0\n"
              "    %4 = addr @main.0\n"
              "    call void @anti_rt_check_failed(%4, 21, 1, %3, -128)\n"
              "    jump b2\n"
              "b2:\n"
              "    store i8 %2, %1\n"
              "    %5 = copy i8 %2\n"
              "    %6 = sext i16 %2\n"
              "    %7 = sge i8 %2, 0\n"
              "    branch %7, b4, b3\n"
              "b3:\n"
              "    %8 = sext i64 %5\n"
              "    %9 = addr @main.1\n"
              "    call void @anti_rt_check_failed(%9, 35, 2, %8, 0)\n"
              "    jump b4\n"
              "b4:\n"
              "    %10 = zext i16 %5\n"
              "    %11 = addov i16 %6, %10\n"
              "    branchov %11, b5, b6\n"
              "b5:\n"
              "    %12 = sext i64 %6\n"
              "    %13 = sext i64 %10\n"
              "    %14 = addr @main.2\n"
              "    call void @anti_rt_check_failed(%14, 21, 1, %12, %13)\n"
              "    jump b6\n"
              "b6:\n"
              "    ret i16 %11\n"
              "}\n");

    /* With a main, only what main reaches stays in the program. */
    optimizes("extern fn putchar(c: i32) -> i32;\n"
              "fn helper() -> int { return 1; }\n"
              "fn used() -> int { return 2; }\n"
              "fn main() -> int { return used(); }\n",
              "fn main.used() -> i64 {\n"
              "b0:\n"
              "    ret i64 2\n"
              "}\n"
              "fn main.main() -> i64 {\n"
              "b0:\n"
              "    %0 = call i64 @main.used()\n"
              "    ret i64 %0\n"
              "}\n");
}
