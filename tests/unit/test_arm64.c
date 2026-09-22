#include "../binary_stdio.h"
#include "check.h"
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
        !lower_module(module, "main", &ir, &diags, 0)) {
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

static void emits(const char *source, enum target target,
                  const char *expected)
{
    struct text out = {0};

    run(source, target, &out);
    CHECK_STR(text_cstr(&out), expected);
    text_free(&out);
}

static const char eleven[] =
    "extern fn take(a: u8, b: i16, c: int, d: int, e: int, f: int, g: int,\n"
    "               h: int, i: i8, j: i32, k: u8) -> int;\n"
    "fn calls(x: u8, y: i16) -> int {\n"
    "    return take(x, y, 3, 4, 5, 6, 7, 8, -1, 9, 200);\n"
    "}\n"
    "fn last(a: u8, b: i16, c: int, d: int, e: int, f: int, g: int, h: int,\n"
    "        i: i8, j: i32, k: u8) -> int {\n"
    "    return c + j as int + k as int;\n"
    "}\n";

static const char variadic[] =
    "extern fn printf(format: ?*byte, ...) -> i32;\n"
    "fn show(p: *byte, v: int) {\n"
    "    printf(p, v, 7);\n"
    "}\n";

/* The address of a function: adrp gives its 4 KB page, and add the low
   12 bits. */
static void page_address(void)
{
    struct arena arena = {0};
    struct ir_module m;
    struct ir_function *helper;
    struct ir_function *f;
    struct mach_function *functions[2] = {NULL, NULL};
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
    CHECK(select_module(TARGET_LINUX_ARM64, cpu_default(TARGET_LINUX_ARM64),
                        &m, functions, error, sizeof error));
    CHECK_STR(error, "");
    for (i = 0; i < 2 && functions[i] != NULL; i++) {
        CHECK(regalloc_function(TARGET_LINUX_ARM64, functions[i], error,
                                sizeof error));
        mach_print(&out, target_desc(TARGET_LINUX_ARM64),
                   cpu_default(TARGET_LINUX_ARM64), &m, functions[i]);
        mach_function_free(functions[i]);
        free(functions[i]);
    }
    CHECK_STR(text_cstr(&out), "main.helper:\n"
                               "b0:\n"
                               "    mov x0, #1\n"
                               "    ret\n"
                               "main.f:\n"
                               "b0:\n"
                               "    adrp x0, main.helper\n"
                               "    add x0, x0, :lo12:main.helper\n"
                               "    ret\n");
    text_free(&out);
    ir_module_free(&m);
    arena_free(&arena);
}

/* Append the instructions of b to out, one per line, and free them. */
static void print_block(struct text *out, const struct target_desc *target,
                        struct mach_block *b)
{
    struct ir_module m;
    size_t i;

    memset(&m, 0, sizeof m);
    for (i = 0; i < b->count; i++) {
        target->print(out, cpu_default(TARGET_LINUX_ARM64), &m, &b->insts[i],
                      NULL);
        text_append(out, "\n");
    }
    free(b->insts);
    memset(b, 0, sizeof *b);
}

/* Frame offsets beyond the immediates. One frame size fits a 12-bit
   immediate shifted by 12, and another goes through x16. Saved registers
   and spill slots lie beyond the scaled offset limit of 32760 bytes. */
static void large_frames(void)
{
    const struct target_desc *target = target_desc(TARGET_LINUX_ARM64);
    struct mach_block b;
    struct frame frame;
    struct text out = {0};

    memset(&b, 0, sizeof b);
    memset(&frame, 0, sizeof frame);
    frame.needed = true;
    frame.size = 8192;
    target->prologue(&b, &frame);
    frame.size = 70000;
    target->prologue(&b, &frame);
    frame.size = 65536;
    frame.saved[0] = 19;
    frame.saved_offset[0] = 65528;
    frame.saved_count = 1;
    target->prologue(&b, &frame);
    target->epilogue(&b, &frame);
    target->load_spill(&b, 16, 32760);
    target->load_spill(&b, 16, 40000);
    target->store_spill(&b, 16, 40000);
    target->store_spill(&b, 17, 40000);
    print_block(&out, target, &b);
    CHECK_STR(text_cstr(&out), "stp x29, x30, [sp, #-16]!\n"
                               "mov x29, sp\n"
                               "sub sp, sp, #2, lsl #12\n"
                               "stp x29, x30, [sp, #-16]!\n"
                               "mov x29, sp\n"
                               "movz x16, #4464\n"
                               "movk x16, #1, lsl #16\n"
                               "sub sp, sp, x16\n"
                               "stp x29, x30, [sp, #-16]!\n"
                               "mov x29, sp\n"
                               "sub sp, sp, #16, lsl #12\n"
                               "mov x16, #65528\n"
                               "str x19, [sp, x16]\n"
                               "mov x19, #65528\n"
                               "ldr x19, [sp, x19]\n"
                               "mov sp, x29\n"
                               "ldp x29, x30, [sp], #16\n"
                               "ldr x16, [sp, #32760]\n"
                               "mov x16, #40000\n"
                               "ldr x16, [sp, x16]\n"
                               "mov x17, #40000\n"
                               "str x16, [sp, x17]\n"
                               "mov x16, #40000\n"
                               "str x17, [sp, x16]\n");
    text_free(&out);
}

/* Windows probes a frame of 4096 bytes or more with __chkstk, which takes
   the size divided by 16 in x15. Each instruction of the prologue and the
   epilogue has one unwind directive. The prologue allocates the save area
   before the saves, which keeps their offsets small, and points x29 at the
   frame record. The rest of the frame follows x29 in the body. */
static void probe(void)
{
    const struct target_desc *target = target_desc(TARGET_WINDOWS_ARM64);
    struct mach_block b;
    struct frame frame;
    struct text out = {0};

    memset(&b, 0, sizeof b);
    memset(&frame, 0, sizeof frame);
    frame.needed = true;
    frame.size = 8192;
    frame.probe = true;
    frame.unwind = true;
    frame.convention = CONVENTION_WINDOWS_ARM64;
    frame.saved[0] = 19;
    frame.saved_offset[0] = 8184;
    frame.saved[1] = 32 + 8;
    frame.saved_offset[1] = 8176;
    frame.saved_count = 2;
    target->prologue(&b, &frame);
    target->epilogue(&b, &frame);
    print_block(&out, target, &b);
    CHECK_STR(text_cstr(&out), "stp x29, x30, [sp, #-16]!\n"
                               ".seh_save_fplr_x 16\n"
                               "sub sp, sp, #16\n"
                               ".seh_stackalloc 16\n"
                               "str x19, [sp, #8]\n"
                               ".seh_save_reg x19, 8\n"
                               "str d8, [sp]\n"
                               ".seh_save_freg d8, 0\n"
                               "add x29, sp, #16\n"
                               ".seh_add_fp 16\n"
                               ".seh_endprologue\n"
                               "mov x15, #511\n"
                               "bl __chkstk\n"
                               "mov x16, #8176\n"
                               "sub sp, sp, x16\n"
                               ".seh_startepilogue\n"
                               "mov x16, #8176\n"
                               ".seh_nop\n"
                               "add sp, sp, x16\n"
                               ".seh_stackalloc 8176\n"
                               "ldr x19, [sp, #8]\n"
                               ".seh_save_reg x19, 8\n"
                               "ldr d8, [sp]\n"
                               ".seh_save_freg d8, 0\n"
                               "add sp, sp, #16\n"
                               ".seh_stackalloc 16\n"
                               "ldp x29, x30, [sp], #16\n"
                               ".seh_save_fplr_x 16\n"
                               ".seh_endepilogue\n");
    text_free(&out);
    /* A frame without saves sets x29 with mov and frees the frame with one
       add. */
    memset(&frame, 0, sizeof frame);
    frame.needed = true;
    frame.size = 48;
    frame.unwind = true;
    frame.convention = CONVENTION_WINDOWS_ARM64;
    target->prologue(&b, &frame);
    target->epilogue(&b, &frame);
    print_block(&out, target, &b);
    CHECK_STR(text_cstr(&out), "stp x29, x30, [sp, #-16]!\n"
                               ".seh_save_fplr_x 16\n"
                               "mov x29, sp\n"
                               ".seh_set_fp\n"
                               ".seh_endprologue\n"
                               "sub sp, sp, #48\n"
                               ".seh_startepilogue\n"
                               "add sp, sp, #48\n"
                               ".seh_stackalloc 48\n"
                               "ldp x29, x30, [sp], #16\n"
                               ".seh_save_fplr_x 16\n"
                               ".seh_endepilogue\n");
    text_free(&out);
}

/* A slot address beyond a 12-bit offset loads the offset into its
   register first. The entry block computes all 520 slot addresses, and
   the addresses that live across the calls spill. */
static void far_slots(void)
{
    struct text source = {0};
    struct text out = {0};
    int i;

    text_append(&source, "extern fn take(p: ?*int);\nfn big() -> int {\n");
    for (i = 0; i < 520; i++) {
        text_appendf(&source, "    let x%d = %d;\n    take(&x%d);\n", i, i, i);
    }
    text_append(&source, "    return x0;\n}\n");
    run(text_cstr(&source), TARGET_LINUX_ARM64, &out);
    CHECK(strstr(text_cstr(&out), "    mov x16, #8320\n"
                                  "    sub sp, sp, x16\n") != NULL);
    CHECK(strstr(text_cstr(&out), "    add x16, sp, #4088\n"
                                  "    str x16, [sp, #8168]\n"
                                  "    add x16, sp, #1, lsl #12\n"
                                  "    str x16, [sp, #8176]\n"
                                  "    mov x16, #4104\n"
                                  "    add x16, sp, x16\n") != NULL);
    text_free(&source);
    text_free(&out);
}

static const char fnptr[] = "extern fn abs(x: i32) -> i32;\n"
                            "fn apply(f: fn(i32) -> i32, x: i32) -> i32 {\n"
                            "    return f(x);\n"
                            "}\n"
                            "fn pick() -> fn(i32) -> i32 {\n"
                            "    return abs;\n"
                            "}\n";

/* The source of the carry and borrow chains, which both back ends
   compile. */
static const char chains[] =
    "fn add(a: u64, b: u64, c: u64, d: u64) -> u64 {\n"
    "    let (low, f) = a + c;\n"
    "    let (high, g) = b + d + f.carry;\n"
    "    return high ^ low ^ (g.carry as u64);\n"
    "}\n"
    "fn sub(a: u64, b: u64, c: u64, d: u64) -> u64 {\n"
    "    let (low, f) = a - c;\n"
    "    let (high, g) = b - d - f.carry;\n"
    "    return high ^ low ^ (g.carry as u64);\n"
    "}\n"
    "fn flags(a: i32, b: i32) -> bool {\n"
    "    let (r, f) = a + b;\n"
    "    return f.overflow || f.zero || f.negative || r == 3;\n"
    "}\n";

/* A carry into `+` is adcs and a borrow into `-` is sbcs. A compare puts
   the bool of the earlier word into the C flag, which subtraction reads
   inverted. Each flag the program reads is one cset after the
   instruction that gave it. */
static void carries(void)
{
    struct text out = {0};
    const char *text;

    run(chains, TARGET_LINUX_ARM64, &out);
    text = text_cstr(&out);
    CHECK(strstr(text, "    adds ") != NULL);
    CHECK(strstr(text, "    adcs ") != NULL);
    CHECK(strstr(text, "    subs ") != NULL);
    CHECK(strstr(text, "    sbcs ") != NULL);
    CHECK(strstr(text, ", hs\n") != NULL);
    CHECK(strstr(text, ", lo\n") != NULL);
    CHECK(strstr(text, ", vs\n") != NULL);
    CHECK(strstr(text, ", eq\n") != NULL);
    CHECK(strstr(text, ", mi\n") != NULL);
    text_free(&out);
}

void test_arm64(void)
{
    carries();
    /* blr calls the address in a register. The address of a C function
       comes from the GOT on Linux and macOS, and from adrp and add on
       Windows. The dumps print the ELF form. */
    emits(fnptr, TARGET_LINUX_ARM64,
          "main.apply:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    mov x9, x0\n"
          "    mov w0, w1\n"
          "    blr x9\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "main.pick:\n"
          "b0:\n"
          "    adrp x0, :got:abs\n"
          "    ldr x0, [x0, :got_lo12:abs]\n"
          "    ret\n");
    emits(fnptr, TARGET_WINDOWS_ARM64,
          "main.apply:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    .seh_save_fplr_x 16\n"
          "    mov x29, sp\n"
          "    .seh_set_fp\n"
          "    .seh_endprologue\n"
          "    mov x9, x0\n"
          "    mov w0, w1\n"
          "    blr x9\n"
          "    .seh_startepilogue\n"
          "    ldp x29, x30, [sp], #16\n"
          "    .seh_save_fplr_x 16\n"
          "    .seh_endepilogue\n"
          "    ret\n"
          "main.pick:\n"
          "b0:\n"
          "    adrp x0, abs\n"
          "    add x0, x0, :lo12:abs\n"
          "    ret\n");
    large_frames();
    probe();
    far_slots();
    page_address();
    /* An 8-bit value extends before a comparison, because the upper bits
       of its w register are unknown. */
    emits("fn small(a: u8, b: u8) -> bool {\n"
          "    return a + b < 10;\n"
          "}\n",
          TARGET_MACOS_ARM64,
          "main.small:\n"
          "b0:\n"
          "    add w9, w0, w1\n"
          "    uxtb w9, w9\n"
          "    cmp w9, #10\n"
          "    cset w0, lo\n"
          "    ret\n");

    /* A register operand extends inside cmp. */
    emits("fn less(a: i16, b: i16) -> bool {\n"
          "    return a < b;\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.less:\n"
          "b0:\n"
          "    sxth w9, w0\n"
          "    cmp w9, w1, sxth\n"
          "    cset w0, lt\n"
          "    ret\n");

    /* A negative constant compares with cmn. */
    emits("fn above(a: i32) -> bool {\n"
          "    return a > -5;\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.above:\n"
          "b0:\n"
          "    cmn w0, #5\n"
          "    cset w0, gt\n"
          "    ret\n");
    /* The lowest 64-bit value has no negation. An add of it stays an add
       and a comparison with it stays a cmp, each over a register. */
    emits("const LOWEST: int = -9223372036854775807 - 1;\n"
          "fn plus(a: uint) -> uint {\n"
          "    return a + 0x8000000000000000;\n"
          "}\n"
          "fn lowest(a: int) -> bool {\n"
          "    return a == LOWEST;\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.plus:\n"
          "b0:\n"
          "    movz x9, #32768, lsl #48\n"
          "    add x0, x0, x9\n"
          "    ret\n"
          "main.lowest:\n"
          "b0:\n"
          "    movz x9, #32768, lsl #48\n"
          "    cmp x0, x9\n"
          "    cset w0, eq\n"
          "    ret\n");
    /* sdiv and udiv leave the quotient, and msub computes the remainder
       a - q * b. */
    emits("fn divs(a: int, b: int) -> int {\n"
          "    return a / b + a % b;\n"
          "}\n",
          TARGET_MACOS_ARM64,
          "main.divs:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    mov x19, x0\n"
          "    mov x20, x1\n"
          "    cmp x20, #0\n"
          "    b.ne b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #29\n"
          "    mov w2, #4\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    sdiv x21, x19, x20\n"
          "    cmp x20, #0\n"
          "    b.ne b4\n"
          "b3:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #29\n"
          "    mov w2, #4\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    sdiv x9, x19, x20\n"
          "    msub x4, x9, x20, x19\n"
          "    adds x19, x21, x4\n"
          "    b.vc b6\n"
          "b5:\n"
          "    adrp x0, main.2\n"
          "    add x0, x0, :lo12:main.2\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x21\n"
          "    bl anti_rt_check_failed\n"
          "b6:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");

    /* 8-bit operands extend to 32 bits, and a constant divisor needs a
       register. */
    emits("fn bytes(a: u8, b: i8) -> i32 {\n"
          "    return (a / 3) as i32 + (b % b) as i32;\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.bytes:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    str x20, [sp]\n"
          "    mov w19, w1\n"
          "    uxtb w9, w0\n"
          "    mov w10, #3\n"
          "    udiv w9, w9, w10\n"
          "    uxtb w20, w9\n"
          "    uxtb w9, w19\n"
          "    cmp w9, #0\n"
          "    b.ne b2\n"
          "b1:\n"
          "    sxtb x3, w19\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #29\n"
          "    mov w2, #4\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    sxtb w9, w19\n"
          "    sxtb w10, w19\n"
          "    sdiv w11, w9, w10\n"
          "    msub w9, w11, w10, w9\n"
          "    sxtb w9, w9\n"
          "    adds w19, w20, w9\n"
          "    b.vc b4\n"
          "b3:\n"
          "    sxtw x3, w20\n"
          "    sxtw x4, w9\n"
          "    adrp x0, main.2\n"
          "    add x0, x0, :lo12:main.2\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov w0, w19\n"
          "    ldr x19, [sp, #8]\n"
          "    ldr x20, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    /* Shifts take the count in a register or as a constant. */
    emits("fn shifts(a: int, n: int) -> int {\n"
          "    return (a << n) + (a >> n) + ((a as u64 >> n as u64) as int);\n"
          "}\n",
          TARGET_MACOS_ARM64,
          "main.shifts:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    str x22, [sp]\n"
          "    mov x19, x0\n"
          "    mov x20, x1\n"
          "    cmp x20, #64\n"
          "    b.lo b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #39\n"
          "    mov w2, #6\n"
          "    mov x3, x20\n"
          "    mov x4, #64\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    lsl x21, x19, x20\n"
          "    cmp x20, #64\n"
          "    b.lo b4\n"
          "b3:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #39\n"
          "    mov w2, #6\n"
          "    mov x3, x20\n"
          "    mov x4, #64\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    asr x4, x19, x20\n"
          "    adds x22, x21, x4\n"
          "    b.vc b6\n"
          "b5:\n"
          "    adrp x0, main.2\n"
          "    add x0, x0, :lo12:main.2\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x21\n"
          "    bl anti_rt_check_failed\n"
          "b6:\n"
          "    cmp x19, #0\n"
          "    b.ge b8\n"
          "b7:\n"
          "    adrp x0, main.3\n"
          "    add x0, x0, :lo12:main.3\n"
          "    mov x1, #34\n"
          "    mov w2, #2\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b8:\n"
          "    cmp x20, #0\n"
          "    b.ge b10\n"
          "b9:\n"
          "    adrp x0, main.3\n"
          "    add x0, x0, :lo12:main.3\n"
          "    mov x1, #34\n"
          "    mov w2, #2\n"
          "    mov x3, x20\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b10:\n"
          "    cmp x20, #64\n"
          "    b.lo b12\n"
          "b11:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #39\n"
          "    mov w2, #6\n"
          "    mov x3, x20\n"
          "    mov x4, #64\n"
          "    bl anti_rt_check_failed\n"
          "b12:\n"
          "    lsr x19, x19, x20\n"
          "    cmp x19, #0\n"
          "    b.ge b14\n"
          "b13:\n"
          "    adrp x0, main.4\n"
          "    add x0, x0, :lo12:main.4\n"
          "    mov x1, #34\n"
          "    mov w2, #3\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b14:\n"
          "    adds x20, x22, x19\n"
          "    b.vc b16\n"
          "b15:\n"
          "    adrp x0, main.2\n"
          "    add x0, x0, :lo12:main.2\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x22\n"
          "    mov x4, x19\n"
          "    bl anti_rt_check_failed\n"
          "b16:\n"
          "    mov x0, x20\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    ldr x22, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");

    /* A right shift of 8 or 16 bits extends first. A constant count of
       the register width or more goes into a register. */
    emits("fn narrow(a: i8, b: u16, c: i32) -> i32 {\n"
          "    return ((a >> 2) as i32) + ((b >> 3) as i32) + (c << 40);\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.narrow:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    str x20, [sp]\n"
          "    mov w19, w2\n"
          "    sxtb w9, w0\n"
          "    asr w9, w9, #2\n"
          "    sxtb w9, w9\n"
          "    uxth w10, w1\n"
          "    lsr w10, w10, #3\n"
          "    uxth w10, w10\n"
          "    adds w20, w9, w10\n"
          "    b.vc b2\n"
          "b1:\n"
          "    sxtw x3, w9\n"
          "    sxtw x4, w10\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    adrp x0, main.2\n"
          "    add x0, x0, :lo12:main.2\n"
          "    mov x1, #39\n"
          "    mov w2, #6\n"
          "    mov x3, #40\n"
          "    mov x4, #32\n"
          "    bl anti_rt_check_failed\n"
          "    mov w9, #40\n"
          "    lsl w9, w19, w9\n"
          "    adds w19, w20, w9\n"
          "    b.vc b4\n"
          "b3:\n"
          "    sxtw x3, w20\n"
          "    sxtw x4, w9\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov w0, w19\n"
          "    ldr x19, [sp, #8]\n"
          "    ldr x20, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    /* Logical immediates, 12-bit immediates shifted by 12, and a constant
       that fits neither. */
    emits("fn imms(a: int) -> int {\n"
          "    let m = (a & 0xff00) | 0x5555555555555555;\n"
          "    let k = (m ^ 0x1234) + 0x3000;\n"
          "    if k < 0x7000 {\n"
          "        return k - 0x2000;\n"
          "    }\n"
          "    return k;\n"
          "}\n",
          TARGET_MACOS_ARM64,
          "main.imms:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    and x9, x0, #65280\n"
          "    orr x9, x9, #6148914691236517205\n"
          "    mov x10, #4660\n"
          "    eor x3, x9, x10\n"
          "    mov x9, #12288\n"
          "    adds x19, x3, x9\n"
          "    b.vc b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x4, #12288\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    mov x9, x19\n"
          "    cmp x19, #7, lsl #12\n"
          "    b.ge b4\n"
          "b3:\n"
          "    mov x10, #8192\n"
          "    subs x19, x9, x10\n"
          "    b.vs b5\n"
          "    b b6\n"
          "b4:\n"
          "    mov x0, x9\n"
          "    ldr x19, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "b5:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x9\n"
          "    mov x4, #8192\n"
          "    bl anti_rt_check_failed\n"
          "b6:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    /* Addressing modes: an index shifted by the scale of the size, and
       offsets that are positive and scaled or small and negative. A store
       of zero with an index reads wzr. */
    emits("fn clear(p: *i16, n: int) {\n"
          "    let i = 0;\n"
          "    while i < n do {\n"
          "        p[i] = 0;\n"
          "        i += 1;\n"
          "    }\n"
          "}\n"
          "fn around(p: *int, q: *i32, i: int) -> int {\n"
          "    return p[1] + p[-1] + q[i] as int;\n"
          "}\n",
          TARGET_MACOS_ARM64,
          "main.clear:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    str x22, [sp]\n"
          "    mov x19, x0\n"
          "    mov x20, x1\n"
          "    mov x21, #0\n"
          "b1:\n"
          "    cmp x21, x20\n"
          "    b.ge b3\n"
          "b2:\n"
          "    strh wzr, [x19, x21, lsl #1]\n"
          "    mov x9, #1\n"
          "    adds x22, x21, x9\n"
          "    b.vs b4\n"
          "    b b5\n"
          "b3:\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    ldr x22, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "b4:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x21\n"
          "    mov x4, #1\n"
          "    bl anti_rt_check_failed\n"
          "b5:\n"
          "    mov x21, x22\n"
          "    b b1\n"
          "main.around:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    str x19, [sp, #24]\n"
          "    str x20, [sp, #16]\n"
          "    str x21, [sp, #8]\n"
          "    mov x19, x1\n"
          "    mov x20, x2\n"
          "    ldr x3, [x0, #8]\n"
          "    ldr x4, [x0, #-8]\n"
          "    adds x21, x3, x4\n"
          "    b.vc b2\n"
          "b1:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    ldr w9, [x19, x20, lsl #2]\n"
          "    sxtw x4, w9\n"
          "    adds x19, x21, x4\n"
          "    b.vc b4\n"
          "b3:\n"
          "    adrp x0, main.1\n"
          "    add x0, x0, :lo12:main.1\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x21\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #24]\n"
          "    ldr x20, [sp, #16]\n"
          "    ldr x21, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    /* Apple: a stack argument takes its own size at its own alignment,
       and the caller extends a register argument of 8 or 16 bits. */
    emits(eleven, TARGET_MACOS_ARM64,
          "main.calls:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    mov w9, #255\n"
          "    strb w9, [sp]\n"
          "    mov w9, #9\n"
          "    str w9, [sp, #4]\n"
          "    mov w9, #200\n"
          "    strb w9, [sp, #8]\n"
          "    uxtb w0, w0\n"
          "    sxth w1, w1\n"
          "    mov x2, #3\n"
          "    mov x3, #4\n"
          "    mov x4, #5\n"
          "    mov x5, #6\n"
          "    mov x6, #7\n"
          "    mov x7, #8\n"
          "    bl take\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "main.last:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    str x20, [sp]\n"
          "    mov x9, x2\n"
          "    ldr w10, [x29, #20]\n"
          "    ldrb w19, [x29, #24]\n"
          "    sxtw x4, w10\n"
          "    adds x20, x9, x4\n"
          "    b.vc b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x9\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    uxtb w4, w19\n"
          "    adds x19, x20, x4\n"
          "    b.vc b4\n"
          "b3:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x20\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #8]\n"
          "    ldr x20, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");

    /* AAPCS64: every stack argument takes 8 bytes, and the callee extends
       narrow arguments itself. */
    emits(eleven, TARGET_LINUX_ARM64,
          "main.calls:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #32\n"
          "    mov w9, #255\n"
          "    strb w9, [sp]\n"
          "    mov w9, #9\n"
          "    str w9, [sp, #8]\n"
          "    mov w9, #200\n"
          "    strb w9, [sp, #16]\n"
          "    mov x2, #3\n"
          "    mov x3, #4\n"
          "    mov x4, #5\n"
          "    mov x5, #6\n"
          "    mov x6, #7\n"
          "    mov x7, #8\n"
          "    bl take\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n"
          "main.last:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    str x20, [sp]\n"
          "    mov x9, x2\n"
          "    ldr w10, [x29, #24]\n"
          "    ldrb w19, [x29, #32]\n"
          "    sxtw x4, w10\n"
          "    adds x20, x9, x4\n"
          "    b.vc b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x9\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    uxtb w4, w19\n"
          "    adds x19, x20, x4\n"
          "    b.vc b4\n"
          "b3:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #21\n"
          "    mov w2, #1\n"
          "    mov x3, x20\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov x0, x19\n"
          "    ldr x19, [sp, #8]\n"
          "    ldr x20, [sp]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");

    /* Apple passes every variadic argument on the stack in 8 bytes. */
    emits(variadic, TARGET_MACOS_ARM64,
          "main.show:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x1, [sp]\n"
          "    mov x9, #7\n"
          "    str x9, [sp, #8]\n"
          "    bl printf\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
    emits(variadic, TARGET_WINDOWS_ARM64,
          "main.show:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    .seh_save_fplr_x 16\n"
          "    mov x29, sp\n"
          "    .seh_set_fp\n"
          "    .seh_endprologue\n"
          "    mov x2, #7\n"
          "    bl printf\n"
          "    .seh_startepilogue\n"
          "    ldp x29, x30, [sp], #16\n"
          "    .seh_save_fplr_x 16\n"
          "    .seh_endepilogue\n"
          "    ret\n");

    /* A zero extension from 32 bits is a move of the w register into
       itself, which clears the upper bits and must stay. */
    emits("fn z(x: int) -> u64 {\n"
          "    return x as u32 as u64;\n"
          "}\n",
          TARGET_LINUX_ARM64,
          "main.z:\n"
          "b0:\n"
          "    stp x29, x30, [sp, #-16]!\n"
          "    mov x29, sp\n"
          "    sub sp, sp, #16\n"
          "    str x19, [sp, #8]\n"
          "    mov x19, x0\n"
          "    cmp x19, #0\n"
          "    b.ge b2\n"
          "b1:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #34\n"
          "    mov w2, #2\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b2:\n"
          "    mov w9, w19\n"
          "    mov w9, w9\n"
          "    cmp x9, x19\n"
          "    b.eq b4\n"
          "b3:\n"
          "    adrp x0, main.0\n"
          "    add x0, x0, :lo12:main.0\n"
          "    mov x1, #34\n"
          "    mov w2, #2\n"
          "    mov x3, x19\n"
          "    mov x4, #0\n"
          "    bl anti_rt_check_failed\n"
          "b4:\n"
          "    mov w0, w19\n"
          "    ldr x19, [sp, #8]\n"
          "    mov sp, x29\n"
          "    ldp x29, x30, [sp], #16\n"
          "    ret\n");
}
