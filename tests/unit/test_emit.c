#include "../binary_stdio.h"
#include "check.h"
#include <stdlib.h>
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "emit.h"
#include "notice.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "regalloc.h"
#include "select.h"
#include "sema.h"
#include "types.h"

/* Compile source as module main for target and append the assembly file
   that the emitter writes, or the error. */
static void emits_as(const char *source, enum target target, bool one_module,
                     const char *expected)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;
    struct types types;
    struct ir_module ir;
    struct mach_function **functions = NULL;
    struct text out = {0};
    char error[200] = "";
    bool ok;
    size_t i;

    types_init(&types, &arena);
    ir_module_init(&ir, &arena, "main");
    if (!lex(source, strlen(source), &arena, &diags, &tokens) ||
        !parse(source, &tokens, &arena, &diags, &module) ||
        !sema_check(module, "main", NULL, NULL, 0, &types, &arena, &diags, true) ||
        !lower_module(module, "main", &ir, &diags, 0)) {
        check_failures++;
        fprintf(stderr, "test source does not lower: %s\n%s\n",
                diags.count > 0 ? diags.items[0].message : "", source);
    } else {
        if (one_module) {
            ir_optimize_module(&ir, "main");
        } else {
            ir_optimize(&ir, "main");
        }
        functions = calloc(ir.function_count + 1, sizeof *functions);
        ok = select_module(target, &ir, functions, error, sizeof error);
        for (i = 0; ok && i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                ok = regalloc_function(target, functions[i], error,
                                       sizeof error);
            }
        }
        ok = ok && (one_module ? emit_module(&out, target, &ir, functions, "main",
                                         error, sizeof error)
                           : emit_program(&out, target, &ir, functions, "main",
                                          error, sizeof error));
        if (!ok) {
            text_append(&out, error);
        }
        CHECK_STR(text_cstr(&out), expected);
        for (i = 0; i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                mach_function_free(functions[i]);
                free(functions[i]);
            }
        }
        free(functions);
    }
    text_free(&out);
    ir_module_free(&ir);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
}

static void emits(const char *source, enum target target,
                  const char *expected)
{
    emits_as(source, target, false, expected);
}

static const char scale[] = "fn scale(x: int) -> int {\n"
                            "    let k = 2 + 4;\n"
                            "    return x * k;\n"
                            "}\n"
                            "\n"
                            "fn main() -> int {\n"
                            "    return scale(7);\n"
                            "}\n";

static const char letters[] = "extern fn putchar(c: i32) -> i32;\n"
                              "\n"
                              "fn main() -> int {\n"
                              "    let i = 0;\n"
                              "    while i < 3 do {\n"
                              "        putchar(65 + i as i32);\n"
                              "        i += 1;\n"
                              "    }\n"
                              "    return i;\n"
                              "}\n";

/* The address of a function on Mach-O: a page and a page offset. */
static void page_offsets(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *helper;
    struct ir_function *f;
    struct mach_function *functions[3] = {NULL, NULL, NULL};
    struct text out = {0};
    char error[200] = "";
    uint32_t address;
    size_t i;

    ir_module_init(&m, &arena, "main");
    helper = ir_function_add(&m, "main", "helper", IR_I64, IR_NO_AGG);
    ir_ret(helper, ir_block_add(helper), IR_I64, ir_int_op(IR_I64, 1));
    f = ir_function_add(&m, "main", "f", IR_PTR, IR_NO_AGG);
    address = ir_addr(f, ir_block_add(f), ir_func_op(helper));
    ir_ret(f, f->blocks[0], IR_PTR, ir_temp_op(f, address));
    CHECK(select_module(TARGET_MACOS_ARM64, &m, functions, error,
                        sizeof error));
    for (i = 0; i < 2; i++) {
        CHECK(regalloc_function(TARGET_MACOS_ARM64, functions[i], error,
                                sizeof error));
    }
    CHECK(emit_program(&out, TARGET_MACOS_ARM64, &m, functions, "main", error,
                       sizeof error));
    CHECK_STR(text_cstr(&out), "    .build_version macos, 11, 0\n"
                               "    .text\n"
                               "    .p2align 2\n"
                               "_main.helper:\n"
                               "L_main.helper.b0:\n"
                               "    mov x0, #1\n"
                               "    ret\n"
                               "    .p2align 2\n"
                               "_main.f:\n"
                               "L_main.f.b0:\n"
                               "    adrp x0, _main.helper@PAGE\n"
                               "    add x0, x0, _main.helper@PAGEOFF\n"
                               "    ret\n");
    for (i = 0; i < 2; i++) {
        mach_function_free(functions[i]);
        free(functions[i]);
    }
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Global data follows the functions in the read-only data section of the
   object format, under the mangled name, as bytes. */
static void data_section(enum target target, const char *expected)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_global *text;
    struct ir_function *f;
    struct mach_function *functions[2] = {NULL, NULL};
    struct text out = {0};
    char error[200] = "";
    uint32_t address;
    static const uint8_t hi[] = {'h', 'i', 0};
    static const uint8_t one[] = {1, 0, 0, 0};

    ir_module_init(&m, &arena, "main");
    text = ir_global_add(&m, "main", "0", hi, sizeof hi, 1);
    ir_global_add(&m, "main", "1", one, sizeof one, 4);
    f = ir_function_add(&m, "main", "f", IR_PTR, IR_NO_AGG);
    address = ir_addr(f, ir_block_add(f), ir_global_op(text));
    ir_ret(f, f->blocks[0], IR_PTR, ir_temp_op(f, address));
    CHECK(select_module(target, &m, functions, error, sizeof error));
    CHECK(regalloc_function(target, functions[0], error, sizeof error));
    if (!emit_program(&out, target, &m, functions, "main", error,
                      sizeof error)) {
        text_append(&out, error);
    }
    CHECK_STR(text_cstr(&out), expected);
    mach_function_free(functions[0]);
    free(functions[0]);
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* A str constant puts an address into global data. The emitter writes the
   symbol and leaves the eight bytes to the linker, in the section of the
   format that holds data the loader writes. */
static void data_relocation(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_global *table;
    struct text out = {0};
    char error[200] = "";
    static const uint8_t hi[] = {'h', 'i', 0};
    static const uint8_t zero[16] = {0};
    struct mach_function *functions[1] = {NULL};

    ir_module_init(&m, &arena, "main");
    ir_global_add(&m, "main", "0", hi, sizeof hi, 1);
    table = ir_global_add(&m, "main", "1", zero, sizeof zero, 8);
    ir_global_reloc(&m, table, 0, 0);
    CHECK(emit_program(&out, TARGET_LINUX_ARM64, &m, functions, "main",
                       error, sizeof error));
    CHECK_STR(text_cstr(&out), "    .text\n"
                               "    .section .rodata\n"
                               "main.0:\n"
                               "    .byte 0x68, 0x69, 0x00\n"
                               "    .section .data.rel.ro\n"
                               "    .p2align 3\n"
                               "main.1:\n"
                               "    .quad main.0\n"
                               "    .byte 0x00, 0x00, 0x00, 0x00, 0x00, "
                               "0x00, 0x00, 0x00\n"
                               "    .section .note.GNU-stack,\"\",@progbits\n");
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* An address takes eight bytes, so one that starts too late in the data
   would run past it. */
static void data_relocation_past_end(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_global *table;
    struct text out = {0};
    char error[200] = "";
    static const uint8_t zero[8] = {0};
    struct mach_function *functions[1] = {NULL};

    ir_module_init(&m, &arena, "main");
    table = ir_global_add(&m, "main", "0", zero, sizeof zero, 8);
    ir_global_reloc(&m, table, 4, 0);
    CHECK(!emit_program(&out, TARGET_LINUX_ARM64, &m, functions, "main",
                        error, sizeof error));
    CHECK_STR(error, "the address at 4 of `main.0` ends past its 8 bytes");
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

static const char fnptr[] = "extern fn abs(x: i32) -> i32;\n"
                            "fn apply(f: fn(i32) -> i32, x: i32) -> i32 {\n"
                            "    return f(x);\n"
                            "}\n"
                            "fn pick() -> fn(i32) -> i32 {\n"
                            "    return abs;\n"
                            "}\n";

static void library_data(enum target t, const char *expected)
{
    struct text out = {0};

    emit_constructor(&out, t, "anti_rt_init");
    emit_licenses(&out, t, "lic\n", 4);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

void test_emit(void)
{
    page_offsets();
    data_relocation();
    data_relocation_past_end();

    /* Mach-O names a GOT entry with @GOTPAGE and @GOTPAGEOFF on ARM64 and
       @GOTPCREL on x86_64. */
    emits(fnptr, TARGET_MACOS_ARM64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "    .p2align 2\n"
          "_main.apply:\n"
          "L_main.apply.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x9, x0\n"
          "    mov w0, w1\n"
          "    blr x9\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .p2align 2\n"
          "_main.pick:\n"
          "L_main.pick.b0:\n"
          "    adrp x0, _abs@GOTPAGE\n"
          "    ldr x0, [x0, _abs@GOTPAGEOFF]\n"
          "    ret\n");
    emits(fnptr, TARGET_MACOS_X86_64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "_main.apply:\n"
          "L_main.apply.b0:\n"
          "    pushq %rbp\n"
          "    movq %rsp, %rbp\n"
          "    movq %rdi, %rax\n"
          "    movl %esi, %edi\n"
          "    call *%rax\n"
          "    popq %rbp\n"
          "    ret\n"
          "_main.pick:\n"
          "L_main.pick.b0:\n"
          "    movq _abs@GOTPCREL(%rip), %rax\n"
          "    ret\n");

    data_section(TARGET_LINUX_X86_64,
                 "    .text\n"
                 "main.f:\n"
                 ".Lmain.f.b0:\n"
                 "    leaq main.0(%rip), %rax\n"
                 "    ret\n"
                 "    .section .rodata\n"
                 "main.0:\n"
                 "    .byte 0x68, 0x69, 0x00\n"
                 "    .p2align 2\n"
                 "main.1:\n"
                 "    .byte 0x01, 0x00, 0x00, 0x00\n"
                 "    .section .note.GNU-stack,\"\",@progbits\n");
    data_section(TARGET_MACOS_ARM64,
                 "    .build_version macos, 11, 0\n"
                 "    .text\n"
                 "    .p2align 2\n"
                 "_main.f:\n"
                 "L_main.f.b0:\n"
                 "    adrp x0, _main.0@PAGE\n"
                 "    add x0, x0, _main.0@PAGEOFF\n"
                 "    ret\n"
                 "    .section __TEXT,__const\n"
                 "_main.0:\n"
                 "    .byte 0x68, 0x69, 0x00\n"
                 "    .p2align 2\n"
                 "_main.1:\n"
                 "    .byte 0x01, 0x00, 0x00, 0x00\n");
    data_section(TARGET_WINDOWS_ARM64,
                 "    .text\n"
                 "    .p2align 2\n"
                 "_A4main_f:\n"
                 ".L_A4main_f.b0:\n"
                 "    adrp x0, _A4main_0\n"
                 "    add x0, x0, :lo12:_A4main_0\n"
                 "    ret\n"
                 "    .section .rdata,\"dr\"\n"
                 "_A4main_0:\n"
                 "    .byte 0x68, 0x69, 0x00\n"
                 "    .p2align 2\n"
                 "_A4main_1:\n"
                 "    .byte 0x01, 0x00, 0x00, 0x00\n");

    /* x86_64 subtracts the frame size and addresses slots with 32-bit
       displacements, so a frame of 2 GiB or more is an error. */
    emits("fn main() -> int {\n"
          "    let a = [7 as u8; 3000000000];\n"
          "    return a[2999999999] as int;\n"
          "}\n",
          TARGET_LINUX_X86_64,
          "the stack frame of `main.main` needs 3000000000 bytes, and an "
          "x86_64 frame holds at most 2147483647");

    /* A module compiled on its own keeps every function, global and hidden,
       for the objects of the other modules in the same link. */
    emits_as("fn unused() -> int { return 2; }\n"
             "fn main() -> int { return 0; }\n",
             TARGET_LINUX_X86_64, true,
             "    .text\n"
             "    .globl anti.rt.main\n"
             "    .set anti.rt.main, main.main\n"
             "    .globl main.unused\n"
             "    .hidden main.unused\n"
             "main.unused:\n"
             ".Lmain.unused.b0:\n"
             "    movq $2, %rax\n"
             "    ret\n"
             "    .globl main.main\n"
             "    .hidden main.main\n"
             "main.main:\n"
             ".Lmain.main.b0:\n"
             "    movq $0, %rax\n"
             "    ret\n"
             "    .section .note.GNU-stack,\"\",@progbits\n");
    emits_as("fn main() -> int { return 0; }\n", TARGET_MACOS_ARM64, true,
             "    .build_version macos, 11, 0\n"
             "    .text\n"
             "    .globl _anti.rt.main\n"
             "    .set _anti.rt.main, _main.main\n"
             "    .globl _main.main\n"
             "    .private_extern _main.main\n"
             "    .p2align 2\n"
             "_main.main:\n"
             "L_main.main.b0:\n"
             "    mov x0, #0\n"
             "    ret\n");
    /* A shared library runs anti_rt_init from a constructor, and every
       library and executable that antic links holds anti_licenses. */
    library_data(TARGET_LINUX_X86_64,
                 "    .section .init_array,\"aw\"\n"
                 "    .p2align 3\n"
                 "    .quad anti_rt_init\n"
                 "    .section .rodata\n"
                 "    .globl anti_licenses\n"
                 "anti_licenses:\n"
                 "    .byte 0x6c, 0x69, 0x63, 0x0a, 0x00\n");
    library_data(TARGET_MACOS_ARM64,
                 "    .section __DATA,__mod_init_func,mod_init_funcs\n"
                 "    .p2align 3\n"
                 "    .quad _anti_rt_init\n"
                 "    .section __TEXT,__const\n"
                 "    .globl _anti_licenses\n"
                 "_anti_licenses:\n"
                 "    .byte 0x6c, 0x69, 0x63, 0x0a, 0x00\n");
    library_data(TARGET_WINDOWS_ARM64,
                 "    .section .CRT$XCU,\"dr\"\n"
                 "    .p2align 3\n"
                 "    .quad anti_rt_init\n"
                 "    .section .rdata,\"dr\"\n"
                 "    .globl anti_licenses\n"
                 "anti_licenses:\n"
                 "    .byte 0x6c, 0x69, 0x63, 0x0a, 0x00\n");
    /* The licence notice lists each package once in the order given and
       every distinct licence text once, with the packages it covers. */
    {
        static const char *const lines[] = {"Copyright 2026 Example"};
        struct package rt;
        struct package geo;
        struct package vec;
        const struct package *list[4];
        struct text notice = {0};
        memset(&rt, 0, sizeof rt);
        rt.name = "anti.rt";
        rt.version = "0.1.0";
        rt.license = "0BSD";
        rt.license_text = "Zero clause.\n";
        geo = rt;
        geo.name = "com.example.geo";
        geo.version = "1.2.4";
        geo.license = "MIT";
        geo.license_text = "MIT text.\n";
        geo.attribution = lines;
        geo.attribution_count = 1;
        vec = geo;
        vec.name = "com.example.vec";
        vec.attribution_count = 0;
        list[0] = &rt;
        list[1] = &geo;
        list[2] = &vec;
        list[3] = &geo;
        notice_text(&notice, list, 4);
        CHECK_STR(text_cstr(&notice),
                  "ANTI_LICENSES_BEGIN\n"
                  "package anti.rt 0.1.0 0BSD\n"
                  "package com.example.geo 1.2.4 MIT\n"
                  "attribution Copyright 2026 Example\n"
                  "package com.example.vec 1.2.4 MIT\n"
                  "text for anti.rt\n"
                  "Zero clause.\n"
                  "text for com.example.geo com.example.vec\n"
                  "MIT text.\n"
                  "ANTI_LICENSES_END\n");
        text_free(&notice);
    }
    /* An export fn has the symbol of its name, which is global. */
    emits("export fn twice(x: int) -> int { return 2 * x; }\n"
          "fn main() -> int { return twice(21); }\n",
          TARGET_MACOS_ARM64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "    .globl _anti.rt.main\n"
          "    .set _anti.rt.main, _main.main\n"
          "    .globl _twice\n"
          "    .p2align 2\n"
          "_twice:\n"
          "L_twice.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    mov x9, x0\n"
          "    mov x10, #2\n"
          "    mul x19, x10, x9\n"
          "    mov x10, #2\n"
          "    smulh x10, x10, x9\n"
          "    asr x11, x19, #63\n"
          "    cmp x10, x11\n"
          "    b.eq L_twice.b2\n"
          "L_twice.b1:\n"
          "    adrp x0, _main.0@PAGE\n"
          "    add x0, x0, _main.0@PAGEOFF\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, #2\n"
          "    mov x4, x9\n"
          "    bl _anti_rt_check_failed\n"
          "L_twice.b2:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .p2align 2\n"
          "_main.main:\n"
          "L_main.main.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x0, #21\n"
          "    bl _twice\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .section __TEXT,__const\n"
          "_main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x31, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00\n");
    /* Mach-O: the minimum macOS version, symbols with a leading _ and
       block labels that start with L. */
    emits(scale, TARGET_MACOS_ARM64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "    .globl _anti.rt.main\n"
          "    .set _anti.rt.main, _main.main\n"
          "    .p2align 2\n"
          "_main.scale:\n"
          "L_main.scale.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    mov x19, x0\n"
          "    mov x9, #2\n"
          "    mov x10, #4\n"
          "    adds x20, x9, x10\n"
          "    b.vc L_main.scale.b2\n"
          "L_main.scale.b1:\n"
          "    adrp x0, _main.0@PAGE\n"
          "    add x0, x0, _main.0@PAGEOFF\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, #2\n"
          "    mov x4, #4\n"
          "    bl _anti_rt_check_failed\n"
          "L_main.scale.b2:\n"
          "    mov x4, x20\n"
          "    mul x21, x19, x20\n"
          "    smulh x9, x19, x20\n"
          "    asr x10, x21, #63\n"
          "    cmp x9, x10\n"
          "    b.eq L_main.scale.b4\n"
          "L_main.scale.b3:\n"
          "    adrp x0, _main.1@PAGE\n"
          "    add x0, x0, _main.1@PAGEOFF\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x19\n"
          "    bl _anti_rt_check_failed\n"
          "L_main.scale.b4:\n"
          "    mov x0, x21\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .p2align 2\n"
          "_main.main:\n"
          "L_main.main.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x0, #7\n"
          "    bl _main.scale\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .section __TEXT,__const\n"
          "_main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x32, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "_main.1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x33, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00\n");
    emits(scale, TARGET_MACOS_X86_64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "    .globl _anti.rt.main\n"
          "    .set _anti.rt.main, _main.main\n"
          "_main.scale:\n"
          "L_main.scale.b0:\n"
          "    pushq %rbp\n"
          "    movq %rsp, %rbp\n"
          "    subq $32, %rsp\n"
          "    movq %rbx, 24(%rsp)\n"
          "    movq %r12, 16(%rsp)\n"
          "    movq %r13, 8(%rsp)\n"
          "    movq %rdi, %rbx\n"
          "    movq $2, %r12\n"
          "    addq $4, %r12\n"
          "    jno L_main.scale.b2\n"
          "L_main.scale.b1:\n"
          "    leaq _main.0(%rip), %rdi\n"
          "    movq $21, %rsi\n"
          "    movl $1, %edx\n"
          "    movq $2, %rcx\n"
          "    movq $4, %r8\n"
          "    call _anti_rt_check_failed\n"
          "L_main.scale.b2:\n"
          "    movq %r12, %r8\n"
          "    movq %rbx, %r13\n"
          "    imulq %r12, %r13\n"
          "    jno L_main.scale.b4\n"
          "L_main.scale.b3:\n"
          "    leaq _main.1(%rip), %rdi\n"
          "    movq $21, %rsi\n"
          "    movl $1, %edx\n"
          "    movq %rbx, %rcx\n"
          "    call _anti_rt_check_failed\n"
          "L_main.scale.b4:\n"
          "    movq %r13, %rax\n"
          "    movq 24(%rsp), %rbx\n"
          "    movq 16(%rsp), %r12\n"
          "    movq 8(%rsp), %r13\n"
          "    movq %rbp, %rsp\n"
          "    popq %rbp\n"
          "    ret\n"
          "_main.main:\n"
          "L_main.main.b0:\n"
          "    pushq %rbp\n"
          "    movq %rsp, %rbp\n"
          "    movq $7, %rdi\n"
          "    call _main.scale\n"
          "    popq %rbp\n"
          "    ret\n"
          "    .section __TEXT,__const\n"
          "_main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x32, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "_main.1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x33, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00\n");

    /* ELF: labels that start with .L, and a GNU-stack section that marks
       the stack as not executable. */
    emits(scale, TARGET_LINUX_ARM64,
          "    .text\n"
          "    .globl anti.rt.main\n"
          "    .set anti.rt.main, main.main\n"
          "    .p2align 2\n"
          "main.scale:\n"
          ".Lmain.scale.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    mov x19, x0\n"
          "    mov x9, #2\n"
          "    mov x10, #4\n"
          "    adds x20, x9, x10\n"
          "    b.vc .Lmain.scale.b2\n"
          ".Lmain.scale.b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, #2\n"
          "    mov x4, #4\n"
          "    bl anti_rt_check_failed\n"
          ".Lmain.scale.b2:\n"
          "    mov x4, x20\n"
          "    mul x21, x19, x20\n"
          "    smulh x9, x19, x20\n"
          "    asr x10, x21, #63\n"
          "    cmp x9, x10\n"
          "    b.eq .Lmain.scale.b4\n"
          ".Lmain.scale.b3:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x19\n"
          "    bl anti_rt_check_failed\n"
          ".Lmain.scale.b4:\n"
          "    mov x0, x21\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .p2align 2\n"
          "main.main:\n"
          ".Lmain.main.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x0, #7\n"
          "    bl main.scale\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "    .section .rodata\n"
          "main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x32, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "main.1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x33, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00\n"
          "    .section .note.GNU-stack,\"\",@progbits\n");

    /* COFF: the symbol form of chapter 9. */
    emits(scale, TARGET_WINDOWS_X86_64,
          "    .text\n"
          "    .globl _A4anti2rt_main\n"
          "    .set _A4anti2rt_main, _A4main_main\n"
          "_A4main_scale:\n"
          "    .seh_proc _A4main_scale\n"
          ".L_A4main_scale.b0:\n"
          "    pushq %rbp\n"
          "    .seh_pushreg %rbp\n"
          "    movq %rsp, %rbp\n"
          "    subq $64, %rsp\n"
          "    .seh_stackalloc 64\n"
          "    movq %rbx, 56(%rsp)\n"
          "    .seh_savereg %rbx, 56\n"
          "    movq %rsi, 48(%rsp)\n"
          "    .seh_savereg %rsi, 48\n"
          "    movq %rdi, 40(%rsp)\n"
          "    .seh_savereg %rdi, 40\n"
          "    .seh_endprologue\n"
          "    movq %rcx, %rbx\n"
          "    movq $2, %rsi\n"
          "    addq $4, %rsi\n"
          "    jno .L_A4main_scale.b2\n"
          ".L_A4main_scale.b1:\n"
          "    leaq _A4main_0(%rip), %rcx\n"
          "    movq $4, 32(%rsp)\n"
          "    movq $21, %rdx\n"
          "    movl $1, %r8d\n"
          "    movq $2, %r9\n"
          "    call anti_rt_check_failed\n"
          ".L_A4main_scale.b2:\n"
          "    movq %rsi, %rax\n"
          "    movq %rbx, %rdi\n"
          "    imulq %rsi, %rdi\n"
          "    jno .L_A4main_scale.b4\n"
          ".L_A4main_scale.b3:\n"
          "    leaq _A4main_1(%rip), %rcx\n"
          "    movq %rax, 32(%rsp)\n"
          "    movq $21, %rdx\n"
          "    movl $1, %r8d\n"
          "    movq %rbx, %r9\n"
          "    call anti_rt_check_failed\n"
          ".L_A4main_scale.b4:\n"
          "    movq %rdi, %rax\n"
          "    movq 56(%rsp), %rbx\n"
          "    movq 48(%rsp), %rsi\n"
          "    movq 40(%rsp), %rdi\n"
          "    addq $64, %rsp\n"
          "    popq %rbp\n"
          "    ret\n"
          "    .seh_endproc\n"
          "_A4main_main:\n"
          "    .seh_proc _A4main_main\n"
          ".L_A4main_main.b0:\n"
          "    pushq %rbp\n"
          "    .seh_pushreg %rbp\n"
          "    movq %rsp, %rbp\n"
          "    subq $32, %rsp\n"
          "    .seh_stackalloc 32\n"
          "    .seh_endprologue\n"
          "    movq $7, %rcx\n"
          "    call _A4main_scale\n"
          "    addq $32, %rsp\n"
          "    popq %rbp\n"
          "    ret\n"
          "    .seh_endproc\n"
          "    .section .rdata,\"dr\"\n"
          "_A4main_0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x32, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "_A4main_1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x33, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2a, 0x00\n");

    /* Jumps name block labels, and a C function keeps its C symbol. */
    emits(letters, TARGET_LINUX_X86_64,
          "    .text\n"
          "    .globl anti.rt.main\n"
          "    .set anti.rt.main, main.main\n"
          "main.main:\n"
          ".Lmain.main.b0:\n"
          "    pushq %rbp\n"
          "    movq %rsp, %rbp\n"
          "    subq $16, %rsp\n"
          "    movq %rbx, 8(%rsp)\n"
          "    movq %r12, (%rsp)\n"
          "    movq $0, %rbx\n"
          ".Lmain.main.b1:\n"
          "    cmpq $3, %rbx\n"
          "    jge .Lmain.main.b3\n"
          ".Lmain.main.b2:\n"
          "    movl %ebx, %eax\n"
          "    movslq %eax, %rax\n"
          "    cmpq %rbx, %rax\n"
          "    je .Lmain.main.b5\n"
          "    jmp .Lmain.main.b4\n"
          ".Lmain.main.b3:\n"
          "    movq %rbx, %rax\n"
          "    movq 8(%rsp), %rbx\n"
          "    movq (%rsp), %r12\n"
          "    movq %rbp, %rsp\n"
          "    popq %rbp\n"
          "    ret\n"
          ".Lmain.main.b4:\n"
          "    leaq main.0(%rip), %rdi\n"
          "    movq $34, %rsi\n"
          "    movl $2, %edx\n"
          "    movq %rbx, %rcx\n"
          "    movq $0, %r8\n"
          "    call anti_rt_check_failed\n"
          ".Lmain.main.b5:\n"
          "    movl %ebx, %eax\n"
          "    movl $65, %r12d\n"
          "    addl %eax, %r12d\n"
          "    jno .Lmain.main.b7\n"
          ".Lmain.main.b6:\n"
          "    movslq %eax, %r8\n"
          "    leaq main.1(%rip), %rdi\n"
          "    movq $21, %rsi\n"
          "    movl $1, %edx\n"
          "    movq $65, %rcx\n"
          "    call anti_rt_check_failed\n"
          ".Lmain.main.b7:\n"
          "    movl %r12d, %edi\n"
          "    call putchar\n"
          "    movq %rbx, %r12\n"
          "    addq $1, %r12\n"
          "    jno .Lmain.main.b9\n"
          ".Lmain.main.b8:\n"
          "    leaq main.2(%rip), %rdi\n"
          "    movq $21, %rsi\n"
          "    movl $1, %edx\n"
          "    movq %rbx, %rcx\n"
          "    movq $1, %r8\n"
          "    call anti_rt_check_failed\n"
          ".Lmain.main.b9:\n"
          "    movq %r12, %rbx\n"
          "    jmp .Lmain.main.b1\n"
          "    .section .rodata\n"
          "main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x36, 0x3a, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x20, 0x6f, 0x75\n"
          "    .byte 0x74, 0x20, 0x6f, 0x66, 0x20, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x69\n"
          "    .byte 0x33, 0x32, 0x00\n"
          "main.1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x36, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "main.2:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x37, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "    .section .note.GNU-stack,\"\",@progbits\n");
    emits(letters, TARGET_MACOS_ARM64,
          "    .build_version macos, 11, 0\n"
          "    .text\n"
          "    .globl _anti.rt.main\n"
          "    .set _anti.rt.main, _main.main\n"
          "    .p2align 2\n"
          "_main.main:\n"
          "L_main.main.b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    str x20, [sp]\n"
          "    mov x19, #0\n"
          "L_main.main.b1:\n"
          "    cmp x19, #3\n"
          "    b.ge L_main.main.b3\n"
          "L_main.main.b2:\n"
          "    mov w9, w19\n"
          "    sxtw x9, w9\n"
          "    cmp x9, x19\n"
          "    b.eq L_main.main.b5\n"
          "    b L_main.main.b4\n"
          "L_main.main.b3:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #8]\n"
          "    ldr x20, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "L_main.main.b4:\n"
          "    adrp x0, _main.0@PAGE\n"
          "    add x0, x0, _main.0@PAGEOFF\n"
          "    mov x1, #34\n"
          "    mov w2, #2\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl _anti_rt_check_failed\n"
          "L_main.main.b5:\n"
          "    mov w9, w19\n"
          "    mov w10, #65\n"
          "    adds w20, w10, w9\n"
          "    b.vc L_main.main.b7\n"
          "L_main.main.b6:\n"
          "    sxtw x4, w9\n"
          "    adrp x0, _main.1@PAGE\n"
          "    add x0, x0, _main.1@PAGEOFF\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, #65\n"
          "    bl _anti_rt_check_failed\n"
          "L_main.main.b7:\n"
          "    mov w0, w20\n"
          "    bl _putchar\n"
          "    mov x9, #1\n"
          "    adds x20, x19, x9\n"
          "    b.vc L_main.main.b9\n"
          "L_main.main.b8:\n"
          "    adrp x0, _main.2@PAGE\n"
          "    add x0, x0, _main.2@PAGEOFF\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x19\n"
          "    mov x4, #1\n"
          "    bl _anti_rt_check_failed\n"
          "L_main.main.b9:\n"
          "    mov x19, x20\n"
          "    b L_main.main.b1\n"
          "    .section __TEXT,__const\n"
          "_main.0:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x36, 0x3a, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x20, 0x6f, 0x75\n"
          "    .byte 0x74, 0x20, 0x6f, 0x66, 0x20, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x69\n"
          "    .byte 0x33, 0x32, 0x00\n"
          "_main.1:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x36, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n"
          "_main.2:\n"
          "    .byte 0x6d, 0x61, 0x69, 0x6e, 0x3a, 0x37, 0x3a, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x66, 0x6c, 0x6f, 0x77\n"
          "    .byte 0x20, 0x69, 0x6e, 0x20, 0x2b, 0x00\n");
}
