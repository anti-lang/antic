/* abi_classify, one case per rule of the section "Calling convention" of
   docs/work-order-llvm-back-end.md. The expected classifications follow
   the ABI documents: the System V AMD64 psABI, section 3.2.3, the AAPCS64,
   section 6.8, Apple's "Writing ARM64 code for Apple platforms" and
   Microsoft's "x64 calling convention" and "Overview of ARM64 ABI
   conventions". */

#include "../binary_stdio.h"
#include "check.h"
#include "abi.h"
#include "arena.h"
#include "ir.h"
#include "layout.h"

enum { MAX_PARAMS = 12 };

struct fixture {
    struct arena arena;
    struct ir_module m;
    struct ir_function *f;
    struct abi_param params[MAX_PARAMS];
    struct abi_param result;
};

static void begin(struct fixture *x, enum ir_type result, uint32_t result_agg)
{
    memset(x, 0, sizeof *x);
    ir_module_init(&x->m, &x->arena, "main");
    x->f = ir_function_add(&x->m, "main", "f", result, result_agg);
}

/* The same as begin for a variadic C function. */
static void begin_variadic(struct fixture *x, enum ir_type result)
{
    memset(x, 0, sizeof *x);
    ir_module_init(&x->m, &x->arena, "main");
    x->f = ir_extern_add(&x->m, "f", result, true);
}

static void end(struct fixture *x)
{
    ir_module_free(&x->m);
    arena_free(&x->arena);
}

/* A struct of count fields of the given types, packed or with align(N)
   when asked. */
static uint32_t record(struct fixture *x, const char *name,
                       const enum ir_type *types, size_t count, bool packed,
                       uint64_t align)
{
    struct ir_field fields[8];
    size_t i;

    memset(fields, 0, sizeof fields);
    for (i = 0; i < count; i++) {
        fields[i].name = "f";
        fields[i].type = ir_scalar(types[i]);
    }
    return ir_struct_add(&x->m, IR_AGG_STRUCT, name, fields, count, packed,
                         align);
}

/* A struct of count fields of one type. */
static uint32_t same(struct fixture *x, const char *name, enum ir_type type,
                     size_t count)
{
    enum ir_type types[8];
    size_t i;

    for (i = 0; i < count; i++) {
        types[i] = type;
    }
    return record(x, name, types, count, false, 0);
}

/* A simd struct of count lanes of type. */
static uint32_t simd(struct fixture *x, const char *name, enum ir_type type,
                     size_t count)
{
    struct ir_field fields[16];
    size_t i;

    memset(fields, 0, sizeof fields);
    for (i = 0; i < count; i++) {
        fields[i].name = "l";
        fields[i].type = ir_scalar(type);
    }
    return ir_simd_add(&x->m, name, fields, count);
}

static void param(struct fixture *x, enum ir_type type, enum ir_ext ext)
{
    ir_param_add(x->f, type, IR_NO_AGG);
    x->f->params[x->f->param_count - 1].ext = ext;
}

static void param_agg(struct fixture *x, uint32_t agg)
{
    ir_param_add(x->f, IR_AGG, agg);
}

static void run(struct fixture *x, enum target t)
{
    struct layouts layouts;
    char error[200] = "";

    CHECK(x->f->param_count <= MAX_PARAMS);
    CHECK(layout_init(&layouts, t, &x->m, error, sizeof error));
    abi_classify(t, &layouts, x->f, x->params, &x->result);
    layout_free(&layouts);
}

#define EXPECT(p, kind, words, t0, t1, size, align)                       \
    expect(__LINE__, p, kind, words, t0, t1, size, align)

static const char *const kinds[] = {"DIRECT",   "COERCE", "BYVAL",
                                    "INDIRECT", "SRET",   "VECTOR"};

static void expect(int line, const struct abi_param *p, enum abi_kind kind,
                   size_t words, const char *t0, const char *t1,
                   uint64_t size, uint64_t align)
{
    if (p->kind != kind || p->word_count != words ||
        strcmp(p->types[0], t0) != 0 || strcmp(p->types[1], t1) != 0 ||
        p->size != size || p->align != align) {
        check_failures++;
        fprintf(stderr,
                "%s:%d: expected %s %zu [%s] [%s] %llu/%llu, got %s %zu "
                "[%s] [%s] %llu/%llu\n",
                __FILE__, line, kinds[kind], words, t0, t1,
                (unsigned long long)size, (unsigned long long)align,
                kinds[p->kind], p->word_count, p->types[0], p->types[1],
                (unsigned long long)p->size, (unsigned long long)p->align);
    }
}

/* Every convention passes a scalar in its own type. A pointer is ptr, the
   word of c_long follows the target, and an i8 or i16 records the
   extension its signature gives. A void result is void. */
static void scalars(void)
{
    static const enum target targets[] = {
        TARGET_LINUX_X86_64,   TARGET_MACOS_X86_64,  TARGET_LINUX_ARM64,
        TARGET_MACOS_ARM64,    TARGET_WINDOWS_X86_64, TARGET_WINDOWS_ARM64};
    size_t i;

    for (i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        struct fixture x;
        bool windows = target_info(targets[i])->os == OS_WINDOWS;
        begin(&x, IR_VOID, IR_NO_AGG);
        param(&x, IR_I8, IR_EXT_SIGN);
        param(&x, IR_I16, IR_EXT_ZERO);
        param(&x, IR_I32, IR_EXT_NONE);
        param(&x, IR_I64, IR_EXT_NONE);
        param(&x, IR_F32, IR_EXT_NONE);
        param(&x, IR_F64, IR_EXT_NONE);
        param(&x, IR_PTR, IR_EXT_NONE);
        param(&x, IR_CLONG, IR_EXT_NONE);
        run(&x, targets[i]);
        EXPECT(&x.params[0], ABI_DIRECT, 1, "i8", "", 1, 1);
        CHECK(x.params[0].sign_extend);
        EXPECT(&x.params[1], ABI_DIRECT, 1, "i16", "", 2, 2);
        CHECK(!x.params[1].sign_extend);
        EXPECT(&x.params[2], ABI_DIRECT, 1, "i32", "", 4, 4);
        EXPECT(&x.params[3], ABI_DIRECT, 1, "i64", "", 8, 8);
        EXPECT(&x.params[4], ABI_DIRECT, 1, "float", "", 4, 4);
        EXPECT(&x.params[5], ABI_DIRECT, 1, "double", "", 8, 8);
        EXPECT(&x.params[6], ABI_DIRECT, 1, "ptr", "", 8, 8);
        if (windows) {
            EXPECT(&x.params[7], ABI_DIRECT, 1, "i32", "", 4, 4);
        } else {
            EXPECT(&x.params[7], ABI_DIRECT, 1, "i64", "", 8, 8);
        }
        EXPECT(&x.result, ABI_DIRECT, 0, "void", "", 0, 0);
        end(&x);
    }
}

/* psABI 3.2.3: an aggregate of at most two eightbytes is classified per
   eightbyte. One that holds an integer is INTEGER, i64. One of floats
   alone is SSE: double for an f64, <2 x float> for two f32 and float for
   one. An eightbyte with no member is NO_CLASS and takes no register. */
static void sysv_eightbytes(void)
{
    static const enum ir_type mixed_word[] = {IR_F32, IR_I32};
    static const enum ir_type float_int[] = {IR_F64, IR_I64};
    static const enum ir_type int_byte[] = {IR_I64, IR_I8};
    static const enum target targets[] = {TARGET_LINUX_X86_64,
                                          TARGET_MACOS_X86_64};
    size_t i;

    for (i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        struct fixture x;
        begin(&x, IR_VOID, IR_NO_AGG);
        param_agg(&x, same(&x, "main.I2", IR_I32, 2));
        param_agg(&x, record(&x, "main.IB", int_byte, 2, false, 0));
        param_agg(&x, same(&x, "main.D2", IR_F64, 2));
        param_agg(&x, same(&x, "main.F2", IR_F32, 2));
        param_agg(&x, same(&x, "main.F3", IR_F32, 3));
        param_agg(&x, record(&x, "main.FI", mixed_word, 2, false, 0));
        param_agg(&x, record(&x, "main.DI", float_int, 2, false, 0));
        param_agg(&x, record(&x, "main.A16", int_byte + 1, 1, false, 16));
        run(&x, targets[i]);
        EXPECT(&x.params[0], ABI_COERCE, 1, "i64", "", 8, 4);
        EXPECT(&x.params[1], ABI_COERCE, 2, "i64", "i64", 16, 8);
        EXPECT(&x.params[2], ABI_COERCE, 2, "double", "double", 16, 8);
        EXPECT(&x.params[3], ABI_COERCE, 1, "<2 x float>", "", 8, 4);
        EXPECT(&x.params[4], ABI_COERCE, 2, "<2 x float>", "float", 12, 4);
        EXPECT(&x.params[5], ABI_COERCE, 1, "i64", "", 8, 4);
        EXPECT(&x.params[6], ABI_COERCE, 2, "double", "i64", 16, 8);
        EXPECT(&x.params[7], ABI_COERCE, 1, "i64", "", 16, 16);
        end(&x);
    }
}

/* psABI 3.2.3: an aggregate above 16 bytes, or with a field off its
   alignment, is MEMORY and goes on the stack, byval. A MEMORY result goes
   through a hidden pointer, sret, and one of two eightbytes returns in
   registers. */
static void sysv_memory(void)
{
    static const enum ir_type loose[] = {IR_I8, IR_I32};
    struct fixture x;
    uint32_t big;

    begin(&x, IR_VOID, IR_NO_AGG);
    big = same(&x, "main.I3", IR_I64, 3);
    param_agg(&x, big);
    param_agg(&x, record(&x, "main.P", loose, 2, true, 0));
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.params[0], ABI_BYVAL, 1, "ptr", "", 24, 8);
    EXPECT(&x.params[1], ABI_BYVAL, 1, "ptr", "", 5, 1);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.I3", IR_I64, 3);
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.result, ABI_SRET, 1, "ptr", "", 24, 8);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.D2", IR_F64, 2);
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.result, ABI_COERCE, 2, "double", "double", 16, 8);
    end(&x);
}

/* psABI 3.2.3: when the registers left cannot take every eightbyte of an
   aggregate, the whole of it goes on the stack, and a later argument that
   fits still takes a register. Six integer registers and eight vector
   registers pass arguments, and an sret pointer takes the first integer
   register. */
static void sysv_registers(void)
{
    struct fixture x;
    size_t i;

    begin(&x, IR_VOID, IR_NO_AGG);
    for (i = 0; i < 5; i++) {
        param(&x, IR_I64, IR_EXT_NONE);
    }
    param_agg(&x, same(&x, "main.L2", IR_I64, 2));
    param_agg(&x, same(&x, "main.I2", IR_I32, 2));
    param_agg(&x, same(&x, "main.I2", IR_I32, 2));
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.params[5], ABI_BYVAL, 1, "ptr", "", 16, 8);
    EXPECT(&x.params[6], ABI_COERCE, 1, "i64", "", 8, 4);
    EXPECT(&x.params[7], ABI_BYVAL, 1, "ptr", "", 8, 4);
    end(&x);

    begin(&x, IR_VOID, IR_NO_AGG);
    for (i = 0; i < 7; i++) {
        param(&x, IR_F64, IR_EXT_NONE);
    }
    param_agg(&x, same(&x, "main.D2", IR_F64, 2));
    param_agg(&x, same(&x, "main.F2", IR_F32, 2));
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.params[7], ABI_BYVAL, 1, "ptr", "", 16, 8);
    EXPECT(&x.params[8], ABI_COERCE, 1, "<2 x float>", "", 8, 4);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.I3", IR_I64, 3);
    for (i = 0; i < 5; i++) {
        param(&x, IR_I64, IR_EXT_NONE);
    }
    param_agg(&x, same(&x, "main.I2", IR_I32, 2));
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.params[5], ABI_BYVAL, 1, "ptr", "", 8, 4);
    end(&x);
}

/* A simd struct of 16 bytes is __m128 to C: one vector register on System
   V and on the three AAPCS64 conventions, in and out. Windows x64 returns
   one in xmm0 and passes one as a pointer to a copy. A simd struct of
   another size passes as the struct of its lanes. */
static void vectors(void)
{
    static const enum target registers[] = {
        TARGET_LINUX_X86_64, TARGET_MACOS_X86_64, TARGET_LINUX_ARM64,
        TARGET_MACOS_ARM64, TARGET_WINDOWS_ARM64};
    struct fixture x;
    size_t i;

    for (i = 0; i < sizeof registers / sizeof registers[0]; i++) {
        begin(&x, IR_AGG, IR_NO_AGG);
        x.f->result_agg = simd(&x, "main.F4", IR_F32, 4);
        param_agg(&x, x.f->result_agg);
        param_agg(&x, simd(&x, "main.I4", IR_I32, 4));
        run(&x, registers[i]);
        EXPECT(&x.params[0], ABI_VECTOR, 1, "<4 x float>", "", 16, 16);
        EXPECT(&x.params[1], ABI_VECTOR, 1, "<4 x i32>", "", 16, 16);
        EXPECT(&x.result, ABI_VECTOR, 1, "<4 x float>", "", 16, 16);
        end(&x);
    }

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = simd(&x, "main.F4", IR_F32, 4);
    param_agg(&x, x.f->result_agg);
    run(&x, TARGET_WINDOWS_X86_64);
    EXPECT(&x.params[0], ABI_INDIRECT, 1, "ptr", "", 16, 16);
    EXPECT(&x.result, ABI_VECTOR, 1, "<4 x float>", "", 16, 16);
    end(&x);

    begin(&x, IR_VOID, IR_NO_AGG);
    param_agg(&x, simd(&x, "main.F2", IR_F32, 2));
    run(&x, TARGET_LINUX_X86_64);
    EXPECT(&x.params[0], ABI_COERCE, 1, "<2 x float>", "", 8, 8);
    end(&x);
}

/* AAPCS64 6.8.2: a homogeneous floating-point aggregate of one to four
   members of one float type goes in float registers, [N x float] or
   [N x double], at any size. A fifth member ends it. Another aggregate of
   at most 16 bytes goes in integer registers as [N x i64], and one aligned
   to 16 as i128, which starts at an even register (C.8). A larger one is
   a pointer to a copy (B.4), and a larger result goes through x8. Apple's
   conventions and Windows on ARM64 take the same classes. */
static void aapcs64(void)
{
    static const enum ir_type mixed[] = {IR_F32, IR_F64};
    static const enum ir_type word[] = {IR_I64};
    static const enum target targets[] = {
        TARGET_LINUX_ARM64, TARGET_MACOS_ARM64, TARGET_WINDOWS_ARM64};
    struct fixture x;
    size_t i;

    for (i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        begin(&x, IR_VOID, IR_NO_AGG);
        param_agg(&x, same(&x, "main.F3", IR_F32, 3));
        param_agg(&x, same(&x, "main.D4", IR_F64, 4));
        param_agg(&x, same(&x, "main.F5", IR_F32, 5));
        param_agg(&x, record(&x, "main.FD", mixed, 2, false, 0));
        param_agg(&x, same(&x, "main.I3", IR_I32, 3));
        param_agg(&x, same(&x, "main.B1", IR_I8, 1));
        param_agg(&x, record(&x, "main.A16", word, 1, false, 16));
        param_agg(&x, same(&x, "main.L3", IR_I64, 3));
        run(&x, targets[i]);
        EXPECT(&x.params[0], ABI_COERCE, 1, "[3 x float]", "", 12, 4);
        EXPECT(&x.params[1], ABI_COERCE, 1, "[4 x double]", "", 32, 8);
        EXPECT(&x.params[2], ABI_INDIRECT, 1, "ptr", "", 20, 4);
        EXPECT(&x.params[3], ABI_COERCE, 1, "[2 x i64]", "", 16, 8);
        EXPECT(&x.params[4], ABI_COERCE, 1, "[2 x i64]", "", 12, 4);
        EXPECT(&x.params[5], ABI_COERCE, 1, "[1 x i64]", "", 1, 1);
        EXPECT(&x.params[6], ABI_COERCE, 1, "i128", "", 16, 16);
        EXPECT(&x.params[7], ABI_INDIRECT, 1, "ptr", "", 24, 8);
        end(&x);

        begin(&x, IR_AGG, IR_NO_AGG);
        x.f->result_agg = same(&x, "main.L3", IR_I64, 3);
        run(&x, targets[i]);
        EXPECT(&x.result, ABI_SRET, 1, "ptr", "", 24, 8);
        end(&x);

        begin(&x, IR_AGG, IR_NO_AGG);
        x.f->result_agg = same(&x, "main.D4", IR_F64, 4);
        run(&x, targets[i]);
        EXPECT(&x.result, ABI_COERCE, 1, "[4 x double]", "", 32, 8);
        end(&x);

        begin(&x, IR_AGG, IR_NO_AGG);
        x.f->result_agg = same(&x, "main.I2", IR_I32, 2);
        run(&x, targets[i]);
        EXPECT(&x.result, ABI_COERCE, 1, "[1 x i64]", "", 8, 4);
        end(&x);
    }
}

/* AAPCS64 C.10 and C.13: an aggregate that does not fit the integer
   registers left goes on the stack whole. LLVM allocates an array
   argument as one block, so the class does not depend on the registers
   that earlier arguments took. */
static void aapcs64_registers(void)
{
    struct fixture x;
    size_t i;

    begin(&x, IR_VOID, IR_NO_AGG);
    for (i = 0; i < 7; i++) {
        param(&x, IR_I64, IR_EXT_NONE);
    }
    param_agg(&x, same(&x, "main.L2", IR_I64, 2));
    run(&x, TARGET_LINUX_ARM64);
    EXPECT(&x.params[7], ABI_COERCE, 1, "[2 x i64]", "", 16, 8);
    end(&x);
}

/* Windows ARM64, the addendum on variadic functions: every composite of
   a variadic function is an ordinary one, with no rule for HFAs, and no
   float register passes an argument. A float aggregate of at most 16
   bytes goes in integer registers, and a larger one is a pointer to a
   copy. Results keep their classes. */
static void windows_arm64_variadic(void)
{
    struct fixture x;
    uint32_t f3;

    begin_variadic(&x, IR_AGG);
    f3 = same(&x, "main.F3", IR_F32, 3);
    x.f->result_agg = f3;
    param_agg(&x, f3);
    param_agg(&x, same(&x, "main.D3", IR_F64, 3));
    run(&x, TARGET_WINDOWS_ARM64);
    EXPECT(&x.params[0], ABI_COERCE, 1, "[2 x i64]", "", 12, 4);
    EXPECT(&x.params[1], ABI_INDIRECT, 1, "ptr", "", 24, 8);
    EXPECT(&x.result, ABI_COERCE, 1, "[3 x float]", "", 12, 4);
    end(&x);

    begin_variadic(&x, IR_VOID);
    param_agg(&x, same(&x, "main.F3", IR_F32, 3));
    run(&x, TARGET_LINUX_ARM64);
    EXPECT(&x.params[0], ABI_COERCE, 1, "[3 x float]", "", 12, 4);
    end(&x);
}

/* Microsoft x64: an aggregate of 1, 2, 4 or 8 bytes passes as an integer
   of that size, floats included, in the register of its position. Any
   other size is a pointer to a copy the caller makes, and no position
   changes either class. A result of 1, 2, 4 or 8 bytes returns in rax,
   and any other one through a hidden pointer. */
static void windows_x64(void)
{
    struct fixture x;
    size_t i;

    begin(&x, IR_VOID, IR_NO_AGG);
    for (i = 0; i < 4; i++) {
        param(&x, IR_I64, IR_EXT_NONE);
    }
    param_agg(&x, same(&x, "main.B1", IR_I8, 1));
    param_agg(&x, same(&x, "main.B2", IR_I8, 2));
    param_agg(&x, same(&x, "main.F1", IR_F32, 1));
    param_agg(&x, same(&x, "main.D1", IR_F64, 1));
    param_agg(&x, same(&x, "main.B3", IR_I8, 3));
    param_agg(&x, same(&x, "main.I3", IR_I32, 3));
    param_agg(&x, same(&x, "main.D2", IR_F64, 2));
    run(&x, TARGET_WINDOWS_X86_64);
    EXPECT(&x.params[4], ABI_COERCE, 1, "i8", "", 1, 1);
    EXPECT(&x.params[5], ABI_COERCE, 1, "i16", "", 2, 1);
    EXPECT(&x.params[6], ABI_COERCE, 1, "i32", "", 4, 4);
    EXPECT(&x.params[7], ABI_COERCE, 1, "i64", "", 8, 8);
    EXPECT(&x.params[8], ABI_INDIRECT, 1, "ptr", "", 3, 1);
    EXPECT(&x.params[9], ABI_INDIRECT, 1, "ptr", "", 12, 4);
    EXPECT(&x.params[10], ABI_INDIRECT, 1, "ptr", "", 16, 8);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.D1", IR_F64, 1);
    run(&x, TARGET_WINDOWS_X86_64);
    EXPECT(&x.result, ABI_COERCE, 1, "i64", "", 8, 8);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.D2", IR_F64, 2);
    run(&x, TARGET_WINDOWS_X86_64);
    EXPECT(&x.result, ABI_SRET, 1, "ptr", "", 16, 8);
    end(&x);

    begin(&x, IR_AGG, IR_NO_AGG);
    x.f->result_agg = same(&x, "main.B3", IR_I8, 3);
    run(&x, TARGET_WINDOWS_X86_64);
    EXPECT(&x.result, ABI_SRET, 1, "ptr", "", 3, 1);
    end(&x);
}

void test_abi(void)
{
    scalars();
    sysv_eightbytes();
    sysv_memory();
    sysv_registers();
    vectors();
    aapcs64();
    aapcs64_registers();
    windows_arm64_variadic();
    windows_x64();
}
