#include "../binary_stdio.h"
#include "check.h"
#include <inttypes.h>
#include <stdlib.h>
#include "arena.h"
#include "cpu.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "select.h"
#include "sema.h"
#include "types.h"

/* Compile source as module main, run the optimizer passes on it and
   select code for target. Returns the printed machine code of all
   functions, or the error. */
static void run(const char *source, enum target target, struct text *out)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;
    struct types types;
    struct ir_module ir;
    struct mach_function **functions = NULL;
    char error[200] = "";
    size_t i;

    types_init(&types, &arena);
    ir_module_init(&ir, &arena, "main");
    if (!lex(source, strlen(source), &arena, &diags, &tokens) ||
        !parse(source, &tokens, &arena, &diags, &module) ||
        !sema_check(module, "main", NULL, NULL, 0, &types, &arena, &diags, true) ||
        !lower_module(module, "main", &ir, &diags, 0, NULL, 0, PACKAGE_VERSION_DEFAULT)) {
        check_failures++;
        fprintf(stderr, "test source does not lower: %s\n%s\n",
                diags.count > 0 ? diags.items[0].message : "", source);
    } else {
        ir_optimize(&ir, "main");
        functions = calloc(ir.function_count + 1, sizeof *functions);
        if (select_module(target, cpu_default(target), &ir, functions, error,
                          sizeof error)) {
            for (i = 0; i < ir.function_count; i++) {
                if (functions[i] != NULL) {
                    mach_print(out, target_desc(target), cpu_default(target), &ir,
                               functions[i]);
                }
            }
        } else {
            text_append(out, error);
        }
        for (i = 0; i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                mach_function_free(functions[i]);
                free(functions[i]);
            }
        }
        free(functions);
    }
    ir_module_free(&ir);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
}

static void selects(const char *source, enum target target,
                    const char *expected)
{
    struct text out = {0};

    run(source, target, &out);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

/* Compile source for target and print the data of every global, which the
   back end lays out. */
static void data(const char *source, enum target target, struct text *out)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *module = NULL;
    struct types types;
    struct ir_module ir;
    struct mach_function **functions = NULL;
    char error[200] = "";
    size_t i;
    uint64_t k;

    types_init(&types, &arena);
    ir_module_init(&ir, &arena, "main");
    if (!lex(source, strlen(source), &arena, &diags, &tokens) ||
        !parse(source, &tokens, &arena, &diags, &module) ||
        !sema_check(module, "main", NULL, NULL, 0, &types, &arena, &diags, true) ||
        !lower_module(module, "main", &ir, &diags, 0, NULL, 0, PACKAGE_VERSION_DEFAULT)) {
        check_failures++;
        fprintf(stderr, "test source does not lower: %s\n%s\n",
                diags.count > 0 ? diags.items[0].message : "", source);
    } else {
        functions = calloc(ir.function_count + 1, sizeof *functions);
        if (!select_module(target, cpu_default(target), &ir, functions, error,
                           sizeof error)) {
            text_append(out, error);
        }
        for (i = 0; i < ir.global_count; i++) {
            const struct ir_global *g = ir.globals[i];
            text_appendf(out, "%s.%s size %" PRIu64 " align %" PRIu64,
                         g->module, g->name, g->size, g->align);
            for (k = 0; k < g->size; k++) {
                text_appendf(out, " %02x", g->bytes[k]);
            }
            text_append(out, "\n");
        }
        for (i = 0; i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                mach_function_free(functions[i]);
                free(functions[i]);
            }
        }
        free(functions);
    }
    ir_module_free(&ir);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
}

static void lays_out(const char *source, enum target target,
                     const char *expected)
{
    struct text out = {0};

    data(source, target, &out);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

static const char scale[] = "fn scale(x: int) -> int {\n"
                            "    let k = 2 + 4;\n"
                            "    return x * k;\n"
                            "}\n"
                            "fn main() -> int {\n"
                            "    return scale(7);\n"
                            "}\n";

static const char loop[] = "fn g(n: int) -> int {\n"
                           "    let i = 0;\n"
                           "    while i < n do {\n"
                           "        i += 1;\n"
                           "    }\n"
                           "    if i > 3 {\n"
                           "        return 1;\n"
                           "    } else {\n"
                           "        return 0;\n"
                           "    }\n"
                           "}\n";

static const char ops[] = "extern fn putchar(c: i32) -> i32;\n"
                          "fn arith(x: int, y: int) -> int {\n"
                          "    let big = x + 5000000000;\n"
                          "    let neg = y - -7;\n"
                          "    return -big * ~neg;\n"
                          "}\n"
                          "fn bits(m: i32) -> i32 {\n"
                          "    return (m & -2) | (m ^ 65536);\n"
                          "}\n"
                          "fn flip(n: int) -> int {\n"
                          "    let i = n;\n"
                          "    while i > 0 do {\n"
                          "        i = 10 - i;\n"
                          "    }\n"
                          "    return i;\n"
                          "}\n"
                          "fn calls(flag: bool, n: i32) -> i32 {\n"
                          "    if flag {\n"
                          "        putchar(n);\n"
                          "    }\n"
                          "    return putchar(-1);\n"
                          "}\n";

static const char cell[] = "fn cell(a: int, b: int) -> int {\n"
                           "    let x = a;\n"
                           "    let p = &x;\n"
                           "    *p = *p + b;\n"
                           "    return x;\n"
                           "}\n"
                           "fn bytes(p: *bool) -> *bool {\n"
                           "    if *p {\n"
                           "        *p = false;\n"
                           "    }\n"
                           "    return &p[8];\n"
                           "}\n";

static const char compare[] = "fn cmp(a: i32, b: i32) -> bool {\n"
                              "    return !(a < b) == (a != b);\n"
                              "}\n";

static const char widen[] = "fn widen(x: c_long, w: c_wchar) -> int {\n"
                            "    return x as int + w as int;\n"
                            "}\n";

static const char aligned[] = "packed struct Tight { a: i8, b: i32 }\n"
                              "struct Wide align(16) { a: i8 }\n"
                              "fn tight(t: Tight, b: int) -> int {\n"
                              "    return t.b as int + b;\n"
                              "}\n"
                              "fn wide(a: int, w: Wide, b: int) -> int {\n"
                              "    return a + w.a as int + b;\n"
                              "}\n";

void test_select(void)
{
    /* System V passes a struct with a field off its alignment in memory,
       and the empty second eightbyte of Wide takes no register. */
    selects(aligned, TARGET_LINUX_X86_64,
            "main.tight:\n"
            "b0:\n"
            "    leaq 16(%rbp), %t0\n"
            "    movq %rdi, %t1\n"
            "    movl 1(%t0), %t3\n"
            "    movslq %t3, %t4\n"
            "    movq %t4, %t5\n"
            "    addq %t1, %t5\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.9(%rip), %t6\n"
            "    movq %t6, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t4, %rcx\n"
            "    movq %t1, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t5, %rax\n"
            "    ret\n"
            "main.wide:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    leaq slot0, %t1\n"
            "    movq %rsi, (%t1)\n"
            "    movq %rdx, %t2\n"
            "    movb (%t1), %t3\n"
            "    movsbq %t3, %t4\n"
            "    movq %t0, %t5\n"
            "    addq %t4, %t5\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.10(%rip), %t6\n"
            "    movq %t6, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t0, %rcx\n"
            "    movq %t4, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t5, %t7\n"
            "    addq %t2, %t7\n"
            "    jno b4\n"
            "b3:\n"
            "    leaq main.10(%rip), %t8\n"
            "    movq %t8, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t5, %rcx\n"
            "    movq %t2, %r8\n"
            "    call anti_rt_check_failed\n"
            "b4:\n"
            "    movq %t7, %rax\n"
            "    ret\n");
    /* AAPCS64 starts a 16-aligned aggregate at an even register, and Apple
       does not. */
    selects(aligned, TARGET_LINUX_ARM64,
            "main.tight:\n"
            "b0:\n"
            "    add t0, sp, slot0\n"
            "    str w0, [t0]\n"
            "    lsr t7, x0, #32\n"
            "    strb t7, [t0, #4]\n"
            "    mov t1, x1\n"
            "    ldr t3, [t0, #1]\n"
            "    sxtw t4, t3\n"
            "    adds t5, t4, t1\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t6, main.9\n"
            "    add t6, t6, :lo12:main.9\n"
            "    mov x0, t6\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t4\n"
            "    mov x4, t1\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov x0, t5\n"
            "    ret\n"
            "main.wide:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    add t1, sp, slot0\n"
            "    str x2, [t1]\n"
            "    str x3, [t1, #8]\n"
            "    mov t2, x4\n"
            "    ldrb t3, [t1]\n"
            "    sxtb t4, t3\n"
            "    adds t5, t0, t4\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t6, main.10\n"
            "    add t6, t6, :lo12:main.10\n"
            "    mov x0, t6\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t4\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    adds t7, t5, t2\n"
            "    b.vc b4\n"
            "b3:\n"
            "    adrp t8, main.10\n"
            "    add t8, t8, :lo12:main.10\n"
            "    mov x0, t8\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t5\n"
            "    mov x4, t2\n"
            "    bl anti_rt_check_failed\n"
            "b4:\n"
            "    mov x0, t7\n"
            "    ret\n");
    selects(aligned, TARGET_MACOS_ARM64,
            "main.tight:\n"
            "b0:\n"
            "    add t0, sp, slot0\n"
            "    str w0, [t0]\n"
            "    lsr t7, x0, #32\n"
            "    strb t7, [t0, #4]\n"
            "    mov t1, x1\n"
            "    ldr t3, [t0, #1]\n"
            "    sxtw t4, t3\n"
            "    adds t5, t4, t1\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t6, main.9\n"
            "    add t6, t6, :lo12:main.9\n"
            "    mov x0, t6\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t4\n"
            "    mov x4, t1\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov x0, t5\n"
            "    ret\n"
            "main.wide:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    add t1, sp, slot0\n"
            "    str x1, [t1]\n"
            "    str x2, [t1, #8]\n"
            "    mov t2, x3\n"
            "    ldrb t3, [t1]\n"
            "    sxtb t4, t3\n"
            "    adds t5, t0, t4\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t6, main.10\n"
            "    add t6, t6, :lo12:main.10\n"
            "    mov x0, t6\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t4\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    adds t7, t5, t2\n"
            "    b.vc b4\n"
            "b3:\n"
            "    adrp t8, main.10\n"
            "    add t8, t8, :lo12:main.10\n"
            "    mov x0, t8\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t5\n"
            "    mov x4, t2\n"
            "    bl anti_rt_check_failed\n"
            "b4:\n"
            "    mov x0, t7\n"
            "    ret\n");
    /* c_long is 64 bits and c_wchar 32 bits on Linux, so the conversions
       are a copy and an extension. wchar_t is a signed int on Linux x86_64
       and macOS and an unsigned int on Linux ARM64. On Windows c_long and
       c_wchar are 32 and 16 bits, wchar_t is unsigned, and both extend. */
    selects(widen, TARGET_LINUX_X86_64,
            "main.widen:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movl %esi, %t1\n"
            "    movslq %t1, %t2\n"
            "    movq %t0, %t3\n"
            "    addq %t2, %t3\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t4\n"
            "    movq %t4, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t0, %rcx\n"
            "    movq %t2, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t3, %rax\n"
            "    ret\n");
    selects(widen, TARGET_LINUX_ARM64,
            "main.widen:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, w1\n"
            "    mov t2, t1\n"
            "    adds t3, t0, t2\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t4, main.0\n"
            "    add t4, t4, :lo12:main.0\n"
            "    mov x0, t4\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t2\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov x0, t3\n"
            "    ret\n");
    selects(widen, TARGET_MACOS_ARM64,
            "main.widen:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, w1\n"
            "    sxtw t2, t1\n"
            "    adds t3, t0, t2\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t4, main.0\n"
            "    add t4, t4, :lo12:main.0\n"
            "    mov x0, t4\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t2\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov x0, t3\n"
            "    ret\n");
    selects(widen, TARGET_WINDOWS_X86_64,
            "main.widen:\n"
            "b0:\n"
            "    movl %ecx, %t0\n"
            "    movw %dx, %t1\n"
            "    movslq %t0, %t2\n"
            "    movzwq %t1, %t3\n"
            "    movq %t2, %t4\n"
            "    addq %t3, %t4\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t5\n"
            "    movq %t3, 32(%rsp)\n"
            "    movq %t5, %rcx\n"
            "    movq $21, %rdx\n"
            "    movl $1, %r8d\n"
            "    movq %t2, %r9\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t4, %rax\n"
            "    ret\n");
    selects(widen, TARGET_WINDOWS_ARM64,
            "main.widen:\n"
            "b0:\n"
            "    mov t0, w0\n"
            "    mov t1, w1\n"
            "    sxtw t2, t0\n"
            "    uxth t3, t1\n"
            "    adds t4, t2, t3\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t5, main.0\n"
            "    add t5, t5, :lo12:main.0\n"
            "    mov x0, t5\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t2\n"
            "    mov x4, t3\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov x0, t4\n"
            "    ret\n");
    selects(scale, TARGET_MACOS_ARM64,
            "main.scale:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t6, #2\n"
            "    mov t7, #4\n"
            "    adds t1, t6, t7\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t2, main.0\n"
            "    add t2, t2, :lo12:main.0\n"
            "    mov x0, t2\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, #2\n"
            "    mov x4, #4\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov t3, t1\n"
            "    mul t4, t0, t1\n"
            "    smulh t8, t0, t1\n"
            "    asr t9, t4, #63\n"
            "    cmp t8, t9\n"
            "    b.eq b4\n"
            "b3:\n"
            "    adrp t5, main.1\n"
            "    add t5, t5, :lo12:main.1\n"
            "    mov x0, t5\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t3\n"
            "    bl anti_rt_check_failed\n"
            "b4:\n"
            "    mov x0, t4\n"
            "    ret\n"
            "main.main:\n"
            "b0:\n"
            "    mov x0, #7\n"
            "    bl main.scale\n"
            "    mov t0, x0\n"
            "    mov x0, t0\n"
            "    ret\n");
    selects(scale, TARGET_LINUX_X86_64,
            "main.scale:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq $2, %t1\n"
            "    addq $4, %t1\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t2\n"
            "    movq %t2, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq $2, %rcx\n"
            "    movq $4, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t1, %t3\n"
            "    movq %t0, %t4\n"
            "    imulq %t1, %t4\n"
            "    jno b4\n"
            "b3:\n"
            "    leaq main.1(%rip), %t5\n"
            "    movq %t5, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t0, %rcx\n"
            "    movq %t3, %r8\n"
            "    call anti_rt_check_failed\n"
            "b4:\n"
            "    movq %t4, %rax\n"
            "    ret\n"
            "main.main:\n"
            "b0:\n"
            "    movq $7, %rdi\n"
            "    call main.scale\n"
            "    movq %rax, %t0\n"
            "    movq %t0, %rax\n"
            "    ret\n");
    /* Windows passes the first integer argument in rcx. */
    selects(scale, TARGET_WINDOWS_X86_64,
            "main.scale:\n"
            "b0:\n"
            "    movq %rcx, %t0\n"
            "    movq $2, %t1\n"
            "    addq $4, %t1\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t2\n"
            "    movq $4, 32(%rsp)\n"
            "    movq %t2, %rcx\n"
            "    movq $21, %rdx\n"
            "    movl $1, %r8d\n"
            "    movq $2, %r9\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t1, %t3\n"
            "    movq %t0, %t4\n"
            "    imulq %t1, %t4\n"
            "    jno b4\n"
            "b3:\n"
            "    leaq main.1(%rip), %t5\n"
            "    movq %t3, 32(%rsp)\n"
            "    movq %t5, %rcx\n"
            "    movq $21, %rdx\n"
            "    movl $1, %r8d\n"
            "    movq %t0, %r9\n"
            "    call anti_rt_check_failed\n"
            "b4:\n"
            "    movq %t4, %rax\n"
            "    ret\n"
            "main.main:\n"
            "b0:\n"
            "    movq $7, %rcx\n"
            "    call main.scale\n"
            "    movq %rax, %t0\n"
            "    movq %t0, %rax\n"
            "    ret\n");

    /* A comparison that only a branch uses becomes flags and a jump. */
    selects(loop, TARGET_LINUX_ARM64,
            "main.g:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, #0\n"
            "b1:\n"
            "    cmp t1, t0\n"
            "    b.ge b3\n"
            "b2:\n"
            "    mov t6, #1\n"
            "    adds t3, t1, t6\n"
            "    b.vs b4\n"
            "    b b5\n"
            "b3:\n"
            "    cmp t1, #3\n"
            "    b.gt b6\n"
            "    b b7\n"
            "b4:\n"
            "    adrp t5, main.0\n"
            "    add t5, t5, :lo12:main.0\n"
            "    mov x0, t5\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t1\n"
            "    mov x4, #1\n"
            "    bl anti_rt_check_failed\n"
            "b5:\n"
            "    mov t1, t3\n"
            "    b b1\n"
            "b6:\n"
            "    mov x0, #1\n"
            "    ret\n"
            "b7:\n"
            "    mov x0, #0\n"
            "    ret\n");
    selects(loop, TARGET_MACOS_X86_64,
            "main.g:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq $0, %t1\n"
            "b1:\n"
            "    cmpq %t0, %t1\n"
            "    jge b3\n"
            "b2:\n"
            "    movq %t1, %t3\n"
            "    addq $1, %t3\n"
            "    jo b4\n"
            "    jmp b5\n"
            "b3:\n"
            "    cmpq $3, %t1\n"
            "    jg b6\n"
            "    jmp b7\n"
            "b4:\n"
            "    leaq main.0(%rip), %t5\n"
            "    movq %t5, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t1, %rcx\n"
            "    movq $1, %r8\n"
            "    call anti_rt_check_failed\n"
            "b5:\n"
            "    movq %t3, %t1\n"
            "    jmp b1\n"
            "b6:\n"
            "    movq $1, %rax\n"
            "    ret\n"
            "b7:\n"
            "    movq $0, %rax\n"
            "    ret\n");

    /* A comparison as a value, and bool values of 32 or 8 bits. */
    selects(compare, TARGET_MACOS_ARM64,
            "main.cmp:\n"
            "b0:\n"
            "    mov t0, w0\n"
            "    mov t1, w1\n"
            "    cmp t0, t1\n"
            "    cset t2, lt\n"
            "    eor t3, t2, #1\n"
            "    cmp t0, t1\n"
            "    cset t4, ne\n"
            "    uxtb t6, t3\n"
            "    cmp t6, t4, uxtb\n"
            "    cset t5, eq\n"
            "    mov w0, t5\n"
            "    ret\n");
    selects(compare, TARGET_LINUX_X86_64,
            "main.cmp:\n"
            "b0:\n"
            "    movl %edi, %t0\n"
            "    movl %esi, %t1\n"
            "    cmpl %t1, %t0\n"
            "    setl %t2\n"
            "    movb %t2, %t3\n"
            "    xorb $1, %t3\n"
            "    cmpl %t1, %t0\n"
            "    setne %t4\n"
            "    cmpb %t4, %t3\n"
            "    sete %t5\n"
            "    movb %t5, %al\n"
            "    ret\n");

    /* Immediates beyond the instruction limits, two-operand forms and
       calls of a C function. */
    selects(ops, TARGET_MACOS_ARM64,
            "main.arith:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, x1\n"
            "    movz t11, #61952\n"
            "    movk t11, #10757, lsl #16\n"
            "    movk t11, #1, lsl #32\n"
            "    adds t2, t0, t11\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t3, main.0\n"
            "    add t3, t3, :lo12:main.0\n"
            "    mov x0, t3\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    movz x4, #61952\n"
            "    movk x4, #10757, lsl #16\n"
            "    movk x4, #1, lsl #32\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    mov t4, t2\n"
            "    mov t12, #-7\n"
            "    subs t5, t1, t12\n"
            "    b.vc b4\n"
            "b3:\n"
            "    adrp t6, main.1\n"
            "    add t6, t6, :lo12:main.1\n"
            "    mov x0, t6\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t1\n"
            "    mov x4, #-7\n"
            "    bl anti_rt_check_failed\n"
            "b4:\n"
            "    neg t7, t4\n"
            "    mvn t8, t5\n"
            "    mul t9, t7, t8\n"
            "    smulh t13, t7, t8\n"
            "    asr t14, t9, #63\n"
            "    cmp t13, t14\n"
            "    b.eq b6\n"
            "b5:\n"
            "    adrp t10, main.2\n"
            "    add t10, t10, :lo12:main.2\n"
            "    mov x0, t10\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t7\n"
            "    mov x4, t8\n"
            "    bl anti_rt_check_failed\n"
            "b6:\n"
            "    mov x0, t9\n"
            "    ret\n"
            "main.bits:\n"
            "b0:\n"
            "    mov t0, w0\n"
            "    and t1, t0, #-2\n"
            "    eor t2, t0, #65536\n"
            "    orr t3, t1, t2\n"
            "    mov w0, t3\n"
            "    ret\n"
            "main.flip:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, t0\n"
            "b1:\n"
            "    cmp t1, #0\n"
            "    b.le b3\n"
            "b2:\n"
            "    mov t5, #10\n"
            "    subs t3, t5, t1\n"
            "    b.vs b4\n"
            "    b b5\n"
            "b3:\n"
            "    mov x0, t1\n"
            "    ret\n"
            "b4:\n"
            "    adrp t4, main.3\n"
            "    add t4, t4, :lo12:main.3\n"
            "    mov x0, t4\n"
            "    mov x1, #22\n"
            "    mov w2, #1\n"
            "    mov x3, #10\n"
            "    mov x4, t1\n"
            "    bl anti_rt_check_failed\n"
            "b5:\n"
            "    mov t1, t3\n"
            "    b b1\n"
            "main.calls:\n"
            "b0:\n"
            "    mov t0, w0\n"
            "    mov t1, w1\n"
            "    cbz t0, b2\n"
            "b1:\n"
            "    mov w0, t1\n"
            "    bl putchar\n"
            "    mov t2, w0\n"
            "b2:\n"
            "    mov w0, #-1\n"
            "    bl putchar\n"
            "    mov t3, w0\n"
            "    mov w0, t3\n"
            "    ret\n");
    selects(ops, TARGET_LINUX_X86_64,
            "main.arith:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq %rsi, %t1\n"
            "    movabsq $5000000000, %t11\n"
            "    movq %t0, %t2\n"
            "    addq %t11, %t2\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t3\n"
            "    movq %t3, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t0, %rcx\n"
            "    movabsq $5000000000, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t2, %t4\n"
            "    movq %t1, %t5\n"
            "    subq $-7, %t5\n"
            "    jno b4\n"
            "b3:\n"
            "    leaq main.1(%rip), %t6\n"
            "    movq %t6, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t1, %rcx\n"
            "    movq $-7, %r8\n"
            "    call anti_rt_check_failed\n"
            "b4:\n"
            "    movq %t4, %t7\n"
            "    negq %t7\n"
            "    movq %t5, %t8\n"
            "    notq %t8\n"
            "    movq %t7, %t9\n"
            "    imulq %t8, %t9\n"
            "    jno b6\n"
            "b5:\n"
            "    leaq main.2(%rip), %t10\n"
            "    movq %t10, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t7, %rcx\n"
            "    movq %t8, %r8\n"
            "    call anti_rt_check_failed\n"
            "b6:\n"
            "    movq %t9, %rax\n"
            "    ret\n"
            "main.bits:\n"
            "b0:\n"
            "    movl %edi, %t0\n"
            "    movl %t0, %t1\n"
            "    andl $-2, %t1\n"
            "    movl %t0, %t2\n"
            "    xorl $65536, %t2\n"
            "    movl %t1, %t3\n"
            "    orl %t2, %t3\n"
            "    movl %t3, %eax\n"
            "    ret\n"
            "main.flip:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq %t0, %t1\n"
            "b1:\n"
            "    cmpq $0, %t1\n"
            "    jle b3\n"
            "b2:\n"
            "    movq $10, %t3\n"
            "    subq %t1, %t3\n"
            "    jo b4\n"
            "    jmp b5\n"
            "b3:\n"
            "    movq %t1, %rax\n"
            "    ret\n"
            "b4:\n"
            "    leaq main.3(%rip), %t4\n"
            "    movq %t4, %rdi\n"
            "    movq $22, %rsi\n"
            "    movl $1, %edx\n"
            "    movq $10, %rcx\n"
            "    movq %t1, %r8\n"
            "    call anti_rt_check_failed\n"
            "b5:\n"
            "    movq %t3, %t1\n"
            "    jmp b1\n"
            "main.calls:\n"
            "b0:\n"
            "    movb %dil, %t0\n"
            "    movl %esi, %t1\n"
            "    testb %t0, %t0\n"
            "    je b2\n"
            "b1:\n"
            "    movl %t1, %edi\n"
            "    call putchar\n"
            "    movl %eax, %t2\n"
            "b2:\n"
            "    movl $-1, %edi\n"
            "    call putchar\n"
            "    movl %eax, %t3\n"
            "    movl %t3, %eax\n"
            "    ret\n");

    /* An address-taken local lives in a stack slot. */
    selects(cell, TARGET_MACOS_ARM64,
            "main.cell:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t1, x1\n"
            "    add t2, sp, slot0\n"
            "    str t0, [t2]\n"
            "    adds t3, t0, t1\n"
            "    b.vc b2\n"
            "b1:\n"
            "    adrp t4, main.0\n"
            "    add t4, t4, :lo12:main.0\n"
            "    mov x0, t4\n"
            "    mov x1, #21\n"
            "    mov w2, #1\n"
            "    mov x3, t0\n"
            "    mov x4, t1\n"
            "    bl anti_rt_check_failed\n"
            "b2:\n"
            "    str t3, [t2]\n"
            "    mov x0, t3\n"
            "    ret\n"
            "main.bytes:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    ldrb t1, [t0]\n"
            "    cbz t1, b2\n"
            "b1:\n"
            "    strb wzr, [t0]\n"
            "b2:\n"
            "    add t2, t0, #8\n"
            "    mov x0, t2\n"
            "    ret\n");
    selects(cell, TARGET_LINUX_X86_64,
            "main.cell:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq %rsi, %t1\n"
            "    leaq slot0, %t2\n"
            "    movq %t0, (%t2)\n"
            "    movq %t0, %t3\n"
            "    addq %t1, %t3\n"
            "    jno b2\n"
            "b1:\n"
            "    leaq main.0(%rip), %t4\n"
            "    movq %t4, %rdi\n"
            "    movq $21, %rsi\n"
            "    movl $1, %edx\n"
            "    movq %t0, %rcx\n"
            "    movq %t1, %r8\n"
            "    call anti_rt_check_failed\n"
            "b2:\n"
            "    movq %t3, (%t2)\n"
            "    movq %t3, %rax\n"
            "    ret\n"
            "main.bytes:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movb (%t0), %t1\n"
            "    testb %t1, %t1\n"
            "    je b2\n"
            "b1:\n"
            "    movb $0, (%t0)\n"
            "b2:\n"
            "    movq %t0, %t2\n"
            "    addq $8, %t2\n"
            "    movq %t2, %rax\n"
            "    ret\n");

    /* Division by a constant on both processors. */
    selects("fn d(x: int) -> int { return x / 3; }\n", TARGET_MACOS_ARM64,
            "main.d:\n"
            "b0:\n"
            "    mov t0, x0\n"
            "    mov t2, #3\n"
            "    sdiv t1, t0, t2\n"
            "    mov x0, t1\n"
            "    ret\n");
    selects("fn d(x: int) -> int { return x / 3; }\n", TARGET_LINUX_X86_64,
            "main.d:\n"
            "b0:\n"
            "    movq %rdi, %t0\n"
            "    movq $3, %t2\n"
            "    movq %t0, %rax\n"
            "    cqto\n"
            "    idivq %t2\n"
            "    movq %rax, %t1\n"
            "    movq %t1, %rax\n"
            "    ret\n");
    selects("fn f(x: f64) -> f64 { return x + 1.0; }\n", TARGET_MACOS_ARM64,
            "main.f:\n"
            "b0:\n"
            "    fmov t0, d0\n"
            "    movz t3, #16368, lsl #48\n"
            "    fmov t2, t3\n"
            "    fadd t1, t0, t2\n"
            "    fmov d0, t1\n"
            "    ret\n");
    selects("fn five(a: int, b: int, c: int, d: int, e: int) -> int {\n"
            "    return e;\n"
            "}\n",
            TARGET_WINDOWS_X86_64,
            "main.five:\n"
            "b0:\n"
            "    movq 48(%rbp), %t4\n"
            "    movq %t4, %rax\n"
            "    ret\n");
    selects("fn five(a: int, b: int, c: int, d: int, e: int, f: int, g: int,\n"
            "        h: int, i: int) -> int {\n"
            "    return i;\n"
            "}\n",
            TARGET_MACOS_ARM64,
            "main.five:\n"
            "b0:\n"
            "    ldr t8, [x29, #16]\n"
            "    mov x0, t8\n"
            "    ret\n");

    /* DESIGN: the back end writes the bytes of an aggregate constant,
       because it alone knows the layout of the type. Padding is zero, so
       the data of a constant reads the same on every run. */
    lays_out("struct Gap { a: u8, b: i32 }\n"
             "const GAP: Gap = Gap { a: 1, b: 2 };\n"
             "fn f(out: *Gap) {\n"
             "    *out = GAP;\n"
             "}\n",
             TARGET_LINUX_X86_64,
             "main.Gap.descriptor size 120 align 8 00 00 00 00 00 00 00 00 03 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00\n"
             "main.1 size 4 align 1 47 61 70 00\n"
             "main.2 size 2 align 1 61 00\n"
             "main.3 size 2 align 1 62 00\n"
             "main.Gap.fields size 96 align 8 00 00 00 00 00 00 00 00 01 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 08 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 01 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 05 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00\n"
             "main.package.version size 6 align 1 30 2e 30 2e 30 00\nmain.6 size 8 align 4 01 00 00 00 02 00 00 00\n");
    /* A bitfield constant holds the bits of the unit it lies in. */
    lays_out("struct Bits { a: u8, _: u32 : 0, b: u8 : 3, c: u8 : 5 }\n"
             "const BITS: Bits = Bits { a: 1, b: 2, c: 3 };\n"
             "fn f(out: *Bits) {\n"
             "    *out = BITS;\n"
             "}\n",
             TARGET_LINUX_X86_64,
             "main.Bits.descriptor size 120 align 8 00 00 00 00 00 00 00 00 04 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 04 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00\n"
             "main.1 size 5 align 1 42 69 74 73 00\n"
             "main.2 size 2 align 1 61 00\n"
             "main.3 size 2 align 1 5f 00\n"
             "main.4 size 2 align 1 62 00\n"
             "main.5 size 2 align 1 63 00\n"
             "main.Bits.fields size 192 align 8 00 00 00 00 00 00 00 00 01 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 01 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 0a "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 04 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 "
             "00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
             "main.package.version size 6 align 1 30 2e 30 2e 30 00\nmain.8 size 5 align 1 01 00 00 00 1a\n");
}
