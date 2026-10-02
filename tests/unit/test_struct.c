#include "../binary_stdio.h"
#include "check.h"
#include "pipeline.h"
#include "cpu.h"
#include <stdlib.h>
#include <string.h>
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
    machine_of(source, target, cpu_default(target), true, out);
}

static void emits(const char *source, enum target target,
                  const char *expected)
{
    struct text out = {0};

    run(source, target, &out);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

static const char copy20[] = "struct Five { a: f32, b: f32, c: f32, d: f32, e: f32 }\n"
                             "fn copy(p: *Five, q: *Five) {\n"
                             "    *p = *q;\n"
                             "}\n";

static const char copy100[] = "fn copy(p: *[100]byte, q: *[100]byte) {\n"
                              "    *p = *q;\n"
                              "}\n";

static const char stack_hfa[] =
    "struct V2 { x: f32, y: f32 }\n"
    "fn last(a: f64, b: f64, c: f64, d: f64, e: f64, f: f64, g: f64, h: f64,\n"
    "        k: f32, v: V2) -> f32 {\n"
    "    return v.y;\n"
    "}\n";

void test_struct(void)
{
    /* A float aggregate on the stack starts at 8 bytes after the f32 k
       under AAPCS64, and at 4 bytes, the alignment of f32, under Apple. */
    emits(stack_hfa, TARGET_LINUX_ARM64,
          "main.last:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    add x9, x29, #24\n"
          "    ldr s0, [x9, #4]\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    emits(stack_hfa, TARGET_MACOS_ARM64,
          "main.last:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    add x9, x29, #20\n"
          "    ldr s0, [x9, #4]\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");

    /* A copy of up to 64 bytes moves 8, 4, 2 and 1 bytes at a time. */
    emits(copy20, TARGET_LINUX_X86_64,
          "main.copy:\n"
          "b0:\n"
          "    movq (%rsi), %rax\n"
          "    movq %rax, (%rdi)\n"
          "    movq 8(%rsi), %rax\n"
          "    movq %rax, 8(%rdi)\n"
          "    movl 16(%rsi), %eax\n"
          "    movl %eax, 16(%rdi)\n"
          "    ret\n");
    emits(copy20, TARGET_MACOS_ARM64,
          "main.copy:\n"
          "b0:\n"
          "    ldr x9, [x1]\n"
          "    str x9, [x0]\n"
          "    ldr x9, [x1, #8]\n"
          "    str x9, [x0, #8]\n"
          "    ldr w9, [x1, #16]\n"
          "    str w9, [x0, #16]\n"
          "    ret\n");
    /* A larger copy calls memcpy. */
    emits(copy100, TARGET_LINUX_ARM64,
          "main.copy:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x2, #100\n"
          "    bl memcpy\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    emits(copy100, TARGET_WINDOWS_X86_64,
          "main.copy:\n"
          "b0:\n"
          "    pushq %rbp\n"
          "    .seh_pushreg %rbp\n"
          "    movq %rsp, %rbp\n"
          "    subq $32, %rsp\n"
          "    .seh_stackalloc 32\n"
          "    .seh_endprologue\n"
          "    movq $100, %r8\n"
          "    call memcpy\n"
          "    addq $32, %rsp\n"
          "    popq %rbp\n"
          "    ret\n");
}
