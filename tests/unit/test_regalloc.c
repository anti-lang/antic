#include "../binary_stdio.h"
#include "check.h"
#include "cpu.h"
#include <stdlib.h>
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "regalloc.h"
#include "select.h"
#include "sema.h"
#include "types.h"

/* Compile source as module main through register allocation for target,
   and append the machine code of every function, or the error. */
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
    bool ok;
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
        ok = select_module(target, cpu_default(target), &ir, functions, error,
                           sizeof error);
        for (i = 0; ok && i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                ok = regalloc_function(target, functions[i], error,
                                       sizeof error);
            }
        }
        for (i = 0; ok && i < ir.function_count; i++) {
            if (functions[i] != NULL) {
                mach_print(out, target_desc(target), cpu_default(target), &ir,
                           functions[i]);
            }
        }
        if (!ok) {
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

static void allocates(const char *source, enum target target,
                      const char *expected)
{
    struct text out = {0};

    run(source, target, &out);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

/* Select and allocate module m for target without the optimizer, and
   append the machine code of every function, or the error. */
static void run_ir(struct ir_module *m, enum target target, struct text *out)
{
    struct mach_function **functions = NULL;
    char error[200] = "";
    bool ok;
    size_t i;

    functions = calloc(m->function_count + 1, sizeof *functions);
    ok = select_module(target, cpu_default(target), m, functions, error,
                       sizeof error);
    for (i = 0; ok && i < m->function_count; i++) {
        if (functions[i] != NULL) {
            ok = regalloc_function(target, functions[i], error,
                                   sizeof error);
        }
    }
    for (i = 0; ok && i < m->function_count; i++) {
        if (functions[i] != NULL) {
            mach_print(out, target_desc(target), cpu_default(target), m,
                       functions[i]);
        }
    }
    if (!ok) {
        text_append(out, error);
    }
    for (i = 0; i < m->function_count; i++) {
        if (functions[i] != NULL) {
            mach_function_free(functions[i]);
            free(functions[i]);
        }
    }
    free(functions);
}

/* Temporary 0 of a function without parameters is register 0. It holds
   a constant, and a jump that defines nothing follows it. The constant
   is written again before its use in the next block. */
static void constant_in_register_0(enum target target)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *f;
    struct ir_block *b0;
    struct ir_block *b1;
    struct text out = {0};
    uint32_t k;

    ir_module_init(&m, &arena, "main");
    f = ir_function_add(&m, "main", "f", IR_I64, IR_NO_AGG);
    b0 = ir_block_add(f);
    b1 = ir_block_add(f);
    k = ir_temp(f, IR_I64);
    ir_assign(f, b0, k, ir_int_op(IR_I64, 12345));
    ir_jump(f, b0, b1);
    ir_ret(f, b1, IR_I64, ir_temp_op(f, k));
    run_ir(&m, target, &out);
    if (strstr(text_cstr(&out), "12345") == NULL) {
        check_failures++;
        fprintf(stderr, "the constant of register 0 is lost:\n%s",
                text_cstr(&out));
    }
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

static const char scale[] = "fn scale(x: int) -> int {\n"
                            "    let k = 2 + 4;\n"
                            "    return x * k;\n"
                            "}\n"
                            "fn main() -> int {\n"
                            "    return scale(7);\n"
                            "}\n";

static const char twice[] = "extern fn putchar(c: i32) -> i32;\n"
                            "fn twice(c: i32) -> i32 {\n"
                            "    putchar(c);\n"
                            "    putchar(c);\n"
                            "    return c;\n"
                            "}\n";

static const char cell[] = "fn cell(a: int, b: int) -> int {\n"
                           "    let x = a;\n"
                           "    let p = &x;\n"
                           "    *p = *p + b;\n"
                           "    return x;\n"
                           "}\n";

void test_regalloc(void)
{
    constant_in_register_0(TARGET_MACOS_ARM64);
    constant_in_register_0(TARGET_LINUX_X86_64);
    /* The instructions of the assembly listing in chapter 1. */
    allocates(scale, TARGET_MACOS_ARM64,
              "main.scale:\n"
              "b0:\n"
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
              "    b.vc b2\n"
              "b1:\n"
              "    adrp x0, main.0\n"
              "    add x0, x0, :lo12:main.0\n"
              "    mov x1, #21\n"
              "    mov w2, #1\n"
              "    mov x3, #2\n"
              "    mov x4, #4\n"
              "    bl anti_rt_check_failed\n"
              "b2:\n"
              "    mov x4, x20\n"
              "    mul x21, x19, x20\n"
              "    smulh x9, x19, x20\n"
              "    asr x10, x21, #63\n"
              "    cmp x9, x10\n"
              "    b.eq b4\n"
              "b3:\n"
              "    adrp x0, main.1\n"
              "    add x0, x0, :lo12:main.1\n"
              "    mov x1, #21\n"
              "    mov w2, #1\n"
              "    mov x3, x19\n"
              "    bl anti_rt_check_failed\n"
              "b4:\n"
              "    mov x0, x21\n"
              "    ldr x19, [sp, #24]\n"
              "    ldr x20, [sp, #16]\n"
              "    ldr x21, [sp, #8]\n"
              "    mov sp, x29\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n"
              "main.main:\n"
              "b0:\n"
              "    stp x29, x30, [sp, #-16]!\n"
              "    mov x29, sp\n"
              "    mov x0, #7\n"
              "    bl main.scale\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n");
    allocates(scale, TARGET_LINUX_X86_64,
              "main.scale:\n"
              "b0:\n"
              "    pushq %rbp\n"
              "    movq %rsp, %rbp\n"
              "    subq $32, %rsp\n"
              "    movq %rbx, 24(%rsp)\n"
              "    movq %r12, 16(%rsp)\n"
              "    movq %r13, 8(%rsp)\n"
              "    movq %rdi, %rbx\n"
              "    movq $2, %r12\n"
              "    addq $4, %r12\n"
              "    jno b2\n"
              "b1:\n"
              "    leaq main.0(%rip), %rdi\n"
              "    movq $21, %rsi\n"
              "    movl $1, %edx\n"
              "    movq $2, %rcx\n"
              "    movq $4, %r8\n"
              "    call anti_rt_check_failed\n"
              "b2:\n"
              "    movq %r12, %r8\n"
              "    movq %rbx, %r13\n"
              "    imulq %r12, %r13\n"
              "    jno b4\n"
              "b3:\n"
              "    leaq main.1(%rip), %rdi\n"
              "    movq $21, %rsi\n"
              "    movl $1, %edx\n"
              "    movq %rbx, %rcx\n"
              "    call anti_rt_check_failed\n"
              "b4:\n"
              "    movq %r13, %rax\n"
              "    movq 24(%rsp), %rbx\n"
              "    movq 16(%rsp), %r12\n"
              "    movq 8(%rsp), %r13\n"
              "    movq %rbp, %rsp\n"
              "    popq %rbp\n"
              "    ret\n"
              "main.main:\n"
              "b0:\n"
              "    pushq %rbp\n"
              "    movq %rsp, %rbp\n"
              "    movq $7, %rdi\n"
              "    call main.scale\n"
              "    popq %rbp\n"
              "    ret\n");
    /* A Windows caller reserves the shadow store for four arguments. */
    allocates(scale, TARGET_WINDOWS_X86_64,
              "main.scale:\n"
              "b0:\n"
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
              "    jno b2\n"
              "b1:\n"
              "    leaq main.0(%rip), %rcx\n"
              "    movq $4, 32(%rsp)\n"
              "    movq $21, %rdx\n"
              "    movl $1, %r8d\n"
              "    movq $2, %r9\n"
              "    call anti_rt_check_failed\n"
              "b2:\n"
              "    movq %rsi, %rax\n"
              "    movq %rbx, %rdi\n"
              "    imulq %rsi, %rdi\n"
              "    jno b4\n"
              "b3:\n"
              "    leaq main.1(%rip), %rcx\n"
              "    movq %rax, 32(%rsp)\n"
              "    movq $21, %rdx\n"
              "    movl $1, %r8d\n"
              "    movq %rbx, %r9\n"
              "    call anti_rt_check_failed\n"
              "b4:\n"
              "    movq %rdi, %rax\n"
              "    movq 56(%rsp), %rbx\n"
              "    movq 48(%rsp), %rsi\n"
              "    movq 40(%rsp), %rdi\n"
              "    addq $64, %rsp\n"
              "    popq %rbp\n"
              "    ret\n"
              "main.main:\n"
              "b0:\n"
              "    pushq %rbp\n"
              "    .seh_pushreg %rbp\n"
              "    movq %rsp, %rbp\n"
              "    subq $32, %rsp\n"
              "    .seh_stackalloc 32\n"
              "    .seh_endprologue\n"
              "    movq $7, %rcx\n"
              "    call main.scale\n"
              "    addq $32, %rsp\n"
              "    popq %rbp\n"
              "    ret\n");

    /* Intervals across a loop. */
    allocates("fn g(n: int) -> int {\n"
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
              TARGET_LINUX_ARM64,
              "main.g:\n"
              "b0:\n"
              "    stp x29, x30, [sp, #-16]!\n"
              "    mov x29, sp\n"
              "    sub sp, sp, #32\n"
              "    str x19, [sp, #24]\n"
              "    str x20, [sp, #16]\n"
              "    str x21, [sp, #8]\n"
              "    mov x19, x0\n"
              "    mov x20, #0\n"
              "b1:\n"
              "    cmp x20, x19\n"
              "    b.ge b3\n"
              "b2:\n"
              "    mov x9, #1\n"
              "    adds x21, x20, x9\n"
              "    b.vs b4\n"
              "    b b5\n"
              "b3:\n"
              "    cmp x20, #3\n"
              "    b.gt b6\n"
              "    b b7\n"
              "b4:\n"
              "    adrp x0, main.0\n"
              "    add x0, x0, :lo12:main.0\n"
              "    mov x1, #21\n"
              "    mov w2, #1\n"
              "    mov x3, x20\n"
              "    mov x4, #1\n"
              "    bl anti_rt_check_failed\n"
              "b5:\n"
              "    mov x20, x21\n"
              "    b b1\n"
              "b6:\n"
              "    mov x0, #1\n"
              "    ldr x19, [sp, #24]\n"
              "    ldr x20, [sp, #16]\n"
              "    ldr x21, [sp, #8]\n"
              "    mov sp, x29\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n"
              "b7:\n"
              "    mov x0, #0\n"
              "    ldr x19, [sp, #24]\n"
              "    ldr x20, [sp, #16]\n"
              "    ldr x21, [sp, #8]\n"
              "    mov sp, x29\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n");

    /* A value live across a call goes to a callee-saved register, which
       the prologue saves. */
    allocates(twice, TARGET_MACOS_ARM64,
              "main.twice:\n"
              "b0:\n"
              "    stp x29, x30, [sp, #-16]!\n"
              "    mov x29, sp\n"
              "    sub sp, sp, #16\n"
              "    str x19, [sp, #8]\n"
              "    mov w19, w0\n"
              "    mov w0, w19\n"
              "    bl putchar\n"
              "    mov w0, w19\n"
              "    bl putchar\n"
              "    mov w0, w19\n"
              "    ldr x19, [sp, #8]\n"
              "    mov sp, x29\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n");
    allocates(twice, TARGET_LINUX_X86_64,
              "main.twice:\n"
              "b0:\n"
              "    pushq %rbp\n"
              "    movq %rsp, %rbp\n"
              "    subq $16, %rsp\n"
              "    movq %rbx, 8(%rsp)\n"
              "    movl %edi, %ebx\n"
              "    movl %ebx, %edi\n"
              "    call putchar\n"
              "    movl %ebx, %edi\n"
              "    call putchar\n"
              "    movl %ebx, %eax\n"
              "    movq 8(%rsp), %rbx\n"
              "    movq %rbp, %rsp\n"
              "    popq %rbp\n"
              "    ret\n");

    /* The address-taken local x lives in a stack slot. */
    allocates(cell, TARGET_MACOS_ARM64,
              "main.cell:\n"
              "b0:\n"
              "    stp x29, x30, [sp, #-16]!\n"
              "    mov x29, sp\n"
              "    sub sp, sp, #32\n"
              "    str x19, [sp, #24]\n"
              "    str x20, [sp, #16]\n"
              "    mov x9, x0\n"
              "    mov x10, x1\n"
              "    add x19, sp, #0\n"
              "    str x9, [x19]\n"
              "    adds x20, x9, x10\n"
              "    b.vc b2\n"
              "b1:\n"
              "    adrp x0, main.0\n"
              "    add x0, x0, :lo12:main.0\n"
              "    mov x1, #21\n"
              "    mov w2, #1\n"
              "    mov x3, x9\n"
              "    mov x4, x10\n"
              "    bl anti_rt_check_failed\n"
              "b2:\n"
              "    str x20, [x19]\n"
              "    mov x0, x20\n"
              "    ldr x19, [sp, #24]\n"
              "    ldr x20, [sp, #16]\n"
              "    mov sp, x29\n"
              "    ldp x29, x30, [sp], #16\n"
              "    ret\n");
    allocates(cell, TARGET_LINUX_X86_64,
              "main.cell:\n"
              "b0:\n"
              "    pushq %rbp\n"
              "    movq %rsp, %rbp\n"
              "    subq $32, %rsp\n"
              "    movq %rbx, 24(%rsp)\n"
              "    movq %r12, 16(%rsp)\n"
              "    movq %rdi, %rax\n"
              "    movq %rsi, %r8\n"
              "    leaq (%rsp), %rbx\n"
              "    movq %rax, (%rbx)\n"
              "    movq %rax, %r12\n"
              "    addq %r8, %r12\n"
              "    jno b2\n"
              "b1:\n"
              "    leaq main.0(%rip), %rdi\n"
              "    movq $21, %rsi\n"
              "    movl $1, %edx\n"
              "    movq %rax, %rcx\n"
              "    call anti_rt_check_failed\n"
              "b2:\n"
              "    movq %r12, (%rbx)\n"
              "    movq %r12, %rax\n"
              "    movq 24(%rsp), %rbx\n"
              "    movq 16(%rsp), %r12\n"
              "    movq %rbp, %rsp\n"
              "    popq %rbp\n"
              "    ret\n");
}
