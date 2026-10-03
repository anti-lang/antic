/* llvm_emit_module of the steps emit-core, emit-arith and emit-memory: the
   temporaries, the blocks, copy, jump, branch and ret, the scalar
   operations with the guards of release mode, the operations on memory,
   the signatures and the calls, the linkage, the attributes of a
   function, the entry of the runtime, the globals, the sections and the
   constructors, the module flags, and the refusal of every other
   operation. The expected texts follow the sections "Types and layout",
   "Calling convention", "Attributes and metadata", "Instruction mapping",
   "Defined results in release mode" and "Globals, sections and
   constructors" of docs/work-order-llvm-back-end.md. */

#include "../binary_stdio.h"
#include "check.h"
#include "arena.h"
#include "ir.h"
#include "layout.h"
#include "llvm_emit.h"

struct fixture {
    struct arena arena;
    struct ir_module m;
    struct text out;
    char error[200];
    const char *constructor;        /* the option of the same name */
};

static void begin(struct fixture *x)
{
    memset(x, 0, sizeof *x);
    ir_module_init(&x->m, &x->arena, "main");
}

static void end(struct fixture *x)
{
    text_free(&x->out);
    ir_module_free(&x->m);
    arena_free(&x->arena);
}

/* Lay the module out for t as the driver does and translate it. */
static bool run_with(struct fixture *x, enum target t, bool one_module,
                     bool exports)
{
    struct llvm_emit_options o;
    struct layouts l;
    bool ok;
    size_t i;

    memset(&o, 0, sizeof o);
    o.target = t;
    o.cpu = cpu_default(t);
    o.module = "main";
    o.one_module = one_module;
    o.exports = exports;
    o.constructor = x->constructor;
    ok = layout_init(&l, t, &x->m, x->error, sizeof x->error) &&
         layout_data(&l, &x->m);
    for (i = 0; ok && i < x->m.function_count; i++) {
        bool resolved = false;
        ok = layout_resolve(&l, x->m.functions[i], &resolved);
    }
    text_free(&x->out);
    ok = ok && llvm_emit_module(&x->out, &o, &x->m, &l, x->error,
                                sizeof x->error);
    layout_free(&l);
    return ok;
}

static bool run(struct fixture *x, enum target t)
{
    return run_with(x, t, false, false);
}

static bool holds(const struct fixture *x, const char *part)
{
    if (strstr(text_cstr(&x->out), part) != NULL) {
        return true;
    }
    fprintf(stderr, "expected\n%s\nin\n%s\n", part, text_cstr(&x->out));
    return false;
}

static bool lacks(const struct fixture *x, const char *part)
{
    if (strstr(text_cstr(&x->out), part) == NULL) {
        return true;
    }
    fprintf(stderr, "did not expect\n%s\nin\n%s\n", part,
            text_cstr(&x->out));
    return false;
}

/* A function main.f() -> type that returns value. */
static void returns(struct fixture *x, const char *name, enum ir_type type,
                    struct ir_operand value)
{
    struct ir_function *f = ir_function_add(&x->m, "main", name, type,
                                            IR_NO_AGG);
    struct ir_block *b = ir_block_add(f);

    ir_ret(f, b, type, value);
}

/* The head of the text, the empty entry block of the IR and the module
   flags of macos-arm64. */
static void module_text(void)
{
    struct fixture x;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "source_filename = \"main\"\n"
                    "target datalayout = \"e-m:o-p270:32:32-p271:32:32-"
                    "p272:64:64-i64:64-i128:128-n32:64-S128-Fn32\"\n"
                    "target triple = \"arm64-apple-macos11.0\"\n\n"));
    CHECK(holds(&x, "define internal void @main.f() #0 {\n"
                    "b0:\n"
                    "  ret void\n"
                    "}\n"));
    CHECK(holds(&x, "attributes #1 = { nounwind }\n"));
    CHECK(holds(&x, "!llvm.module.flags = !{!0, !1}\n"
                    "!llvm.ident = !{!2}\n"
                    "!0 = !{i32 1, !\"wchar_size\", i32 4}\n"
                    "!1 = !{i32 8, !\"PIC Level\", i32 2}\n"
                    "!2 = !{!\"antic "));
    end(&x);
}

/* The constants of each scalar type. An integer is the signed value of
   its bits in the type, and a float the hexadecimal form of its double. */
static void constants(void)
{
    struct fixture x;

    begin(&x);
    returns(&x, "a", IR_I8, ir_int_op(IR_I8, 255));
    returns(&x, "b", IR_I64, ir_int_op(IR_I64, UINT64_MAX - 6));
    returns(&x, "c", IR_F64, ir_float_op(IR_F64, 1.5));
    returns(&x, "d", IR_F32, ir_float_op(IR_F32, -0.25));
    returns(&x, "e", IR_PTR, ir_int_op(IR_PTR, 0));
    returns(&x, "g", IR_I32, ir_int_op(IR_I32, 0x80000000u));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "  ret i8 -1\n"));
    CHECK(holds(&x, "  ret i64 -7\n"));
    CHECK(holds(&x, "  ret double 0x3FF8000000000000\n"));
    CHECK(holds(&x, "  ret float 0xBFD0000000000000\n"));
    CHECK(holds(&x, "  ret ptr null\n"));
    CHECK(holds(&x, "  ret i32 -2147483648\n"));
    end(&x);
}

/* A parameter goes into the alloca of its temporary, and a copy loads
   its operand and stores it in the alloca of its result. An i8 or i16
   parameter extends as the signature records. */
static void temporaries(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t p;
    uint32_t q;
    uint32_t t;

    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_I16, IR_NO_AGG);
    p = ir_param_add(f, IR_I16, IR_NO_AGG);
    f->params[0].ext = IR_EXT_SIGN;
    q = ir_param_add(f, IR_F64, IR_NO_AGG);
    b = ir_block_add(f);
    t = ir_unary(f, b, IR_COPY, IR_F64, ir_temp_op(f, q));
    ir_assign(f, b, t, ir_float_op(IR_F64, 2.0));
    ir_ret(f, b, IR_I16, ir_temp_op(f, p));
    CHECK(run(&x, TARGET_LINUX_ARM64));
    CHECK(holds(&x, "define internal i16 @main.f(i16 signext %p0, "
                    "double %p1) #0 {\n"
                    "b0:\n"
                    "  %t0 = alloca i16, align 2\n"
                    "  %t1 = alloca double, align 8\n"
                    "  %t2 = alloca double, align 8\n"
                    "  store i16 %p0, ptr %t0, align 2\n"
                    "  store double %p1, ptr %t1, align 8\n"
                    "  %v0 = load double, ptr %t1, align 8\n"
                    "  store double %v0, ptr %t2, align 8\n"
                    "  store double 0x4000000000000000, ptr %t2, align 8\n"
                    "  %v1 = load i16, ptr %t0, align 2\n"
                    "  ret i16 %v1\n"
                    "}\n"));
    end(&x);
}

/* A branch into the failure arm of a check or an assertion marks it
   cold with branch weights, on either side. */
static void cold_branches(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b[4];
    uint32_t p;

    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_VOID, IR_NO_AGG);
    p = ir_param_add(f, IR_I8, IR_NO_AGG);
    b[0] = ir_block_add(f);
    b[1] = ir_block_add(f);
    b[2] = ir_block_add(f);
    b[3] = ir_block_add(f);
    b[1]->fail = IR_FAIL_CHECK;
    b[3]->fail = IR_FAIL_ASSERT;
    ir_branch(f, b[0], ir_temp_op(f, p), b[1], b[2]);
    ir_jump(f, b[1], b[2]);
    ir_branch(f, b[2], ir_temp_op(f, p), b[0], b[3]);
    ir_ret(f, b[3], IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "  %v1 = trunc i8 %v0 to i1\n"
                    "  br i1 %v1, label %b1, label %b2, !prof !3\n"
                    "\n"
                    "b1:\n"
                    "  br label %b2\n"));
    CHECK(holds(&x, "  br i1 %v3, label %b0, label %b3, !prof !4\n"));
    CHECK(holds(&x, "!3 = !{!\"branch_weights\", i32 1, i32 2000}\n"
                    "!4 = !{!\"branch_weights\", i32 2000, i32 1}\n"));
    end(&x);

    /* A text without a cold branch writes neither weight. */
    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(lacks(&x, "branch_weights"));
    end(&x);
}

/* The function attributes of each kind of target: the frame pointer of
   Apple's arm64, and the unwind tables and the stack probe of Windows. */
static void attributes(void)
{
    struct fixture x;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "attributes #0 = { nounwind \"frame-pointer\"=\"non-leaf\" "
                    "\"target-cpu\"=\"generic\" \"target-features\"=\""));
    CHECK(run(&x, TARGET_MACOS_X86_64));
    CHECK(holds(&x, "attributes #0 = { nounwind \"frame-pointer\"=\"none\" "
                    "\"target-cpu\"=\"x86-64-v3\" \"target-features\"=\""));
    CHECK(run(&x, TARGET_WINDOWS_ARM64));
    CHECK(holds(&x, "attributes #0 = { nounwind uwtable(sync) "
                    "\"frame-pointer\"=\"none\" "
                    "\"stack-probe-size\"=\"4096\" "
                    "\"target-cpu\"=\"generic\" \"target-features\"=\""));
    CHECK(holds(&x, "!llvm.module.flags = !{!0, !1}\n"
                    "!llvm.ident = !{!2}\n"
                    "!0 = !{i32 1, !\"wchar_size\", i32 2}\n"
                    "!1 = !{i32 7, !\"uwtable\", i32 2}\n"));
    end(&x);
}

/* The linkage of a definition follows the object: internal in a whole
   program, hidden and global in an object of one module, global for a
   program that hosts plugins, and default for an export fn. A copy of a
   generic in an object of one module is weak, in a COMDAT on COFF. */
static void linkage(void)
{
    struct fixture x;
    struct ir_function *f;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    returns(&x, "List<int>.push", IR_VOID, ir_int_op(IR_VOID, 0));
    f = ir_function_add(&x.m, "main", "Point.area", IR_VOID, IR_NO_AGG);
    f->exported = true;
    ir_ret(f, ir_block_add(f), IR_VOID, ir_int_op(IR_VOID, 0));

    CHECK(run_with(&x, TARGET_LINUX_X86_64, false, false));
    CHECK(holds(&x, "define internal void @main.f() #0 {\n"));
    CHECK(holds(&x, "define internal void @main.List$3cint$3e.push() #0 {\n"));
    CHECK(holds(&x, "define dso_local void @Point_area() #0 {\n"));

    CHECK(run_with(&x, TARGET_LINUX_X86_64, false, true));
    CHECK(holds(&x, "define void @main.f() #0 {\n"));

    CHECK(run_with(&x, TARGET_LINUX_X86_64, true, false));
    CHECK(holds(&x, "define hidden void @main.f() #0 {\n"));
    CHECK(holds(&x, "define weak_odr hidden void "
                    "@main.List$3cint$3e.push() #0 {\n"));
    CHECK(holds(&x, "define dso_local void @Point_area() #0 {\n"));

    CHECK(run_with(&x, TARGET_LINUX_X86_64, true, true));
    CHECK(holds(&x, "define void @main.f() #0 {\n"));

    CHECK(run_with(&x, TARGET_MACOS_ARM64, true, false));
    CHECK(holds(&x, "define hidden void @main.f() #0 {\n"));
    CHECK(holds(&x, "define dso_local void @Point_area() #0 {\n"));

    CHECK(run_with(&x, TARGET_WINDOWS_X86_64, true, false));
    CHECK(holds(&x, "$_A4main_List$3cint$3e.push = comdat any\n"));
    CHECK(holds(&x, "define void @_A4main_f() #0 {\n"));
    CHECK(holds(&x, "define weak_odr void @_A4main_List$3cint$3e.push() "
                    "#0 comdat {\n"));
    end(&x);
}

/* A function without a body is a declaration: a C function by its C
   name, variadic where it is, and a function of another module. */
static void declarations(void)
{
    struct fixture x;
    struct ir_function *f;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    f = ir_extern_add(&x.m, "printf", IR_I32, true);
    ir_param_add(f, IR_PTR, IR_NO_AGG);
    f = ir_extern_add(&x.m, "anti_rt_byte", IR_VOID, false);
    ir_param_add(f, IR_I8, IR_NO_AGG);
    f->params[0].ext = IR_EXT_ZERO;
    f = ir_declare_add(&x.m, "other", "g", IR_I64, IR_NO_AGG);
    ir_param_add(f, IR_F32, IR_NO_AGG);
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "declare i32 @printf(ptr, ...) #1\n"));
    CHECK(holds(&x, "declare void @anti_rt_byte(i8 zeroext) #1\n"));
    CHECK(holds(&x, "declare i64 @other.g(float) #1\n"));
    CHECK(run(&x, TARGET_WINDOWS_X86_64));
    CHECK(holds(&x, "declare i64 @_A5other_g(float) #1\n"));
    end(&x);
}

/* Every operation the step emit-core does not translate is refused, with
   the name of the operation, the step that adds it and the function. */
static void refusals(void)
{
    static const struct {
        enum ir_op op;
        const char *message;
    } cases[] = {
        {IR_MULH_S, "the LLVM back end does not translate `smulh` before "
                    "the step emit-wide, in main.f"},
        {IR_ADD_SAT_U, "the LLVM back end does not translate `uaddsat` "
                       "before the step emit-wide, in main.f"},
        {IR_ADD_OV, "the LLVM back end does not translate `addov` before "
                    "the step emit-wide, in main.f"},
    };
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t t;
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        begin(&x);
        f = ir_function_add(&x.m, "main", "f", IR_I64, IR_NO_AGG);
        b = ir_block_add(f);
        t = ir_binary(f, b, cases[i].op,
                      IR_I64,
                      ir_int_op(IR_I64, 1), ir_int_op(IR_I64, 2));
        ir_ret(f, b, IR_I64, ir_temp_op(f, t));
        CHECK(!run(&x, TARGET_MACOS_ARM64));
        CHECK_STR(x.error, cases[i].message);
        end(&x);
    }
}

/* A function main.f(a: from, b: from) -> to whose body is op a, b, or op
   a alone when unary is set. The loads of the two parameters are %v0 and
   %v1, and the operation starts at %v2, or at %v1 when it is unary. */
static void operation(struct fixture *x, enum ir_op op, enum ir_type from,
                      enum ir_type to, bool unary)
{
    struct ir_function *f = ir_function_add(&x->m, "main", "f", to,
                                            IR_NO_AGG);
    struct ir_block *b;
    uint32_t p = ir_param_add(f, from, IR_NO_AGG);
    uint32_t q = unary ? 0 : ir_param_add(f, from, IR_NO_AGG);
    uint32_t t;

    b = ir_block_add(f);
    t = unary ? ir_unary(f, b, op, to, ir_temp_op(f, p))
              : ir_binary(f, b, op, to, ir_temp_op(f, p), ir_temp_op(f, q));
    ir_ret(f, b, to, ir_temp_op(f, t));
}

/* Whether the text of op on from to to holds body. */
static bool translates(enum ir_op op, enum ir_type from, enum ir_type to,
                       bool unary, const char *body)
{
    struct fixture x;
    bool ok;

    begin(&x);
    operation(&x, op, from, to, unary);
    ok = run(&x, TARGET_LINUX_X86_64) && holds(&x, body);
    end(&x);
    return ok;
}

/* The arithmetic that wraps carries no nsw and no nuw, and the float
   arithmetic no fast-math flag. */
static void arithmetic(void)
{
    CHECK(translates(IR_ADD, IR_I64, IR_I64, false,
                     "  %v2 = add i64 %v0, %v1\n"
                     "  store i64 %v2, ptr %t2, align 8\n"));
    CHECK(translates(IR_SUB, IR_I32, IR_I32, false,
                     "  %v2 = sub i32 %v0, %v1\n"));
    CHECK(translates(IR_MUL, IR_I16, IR_I16, false,
                     "  %v2 = mul i16 %v0, %v1\n"));
    CHECK(translates(IR_AND, IR_I8, IR_I8, false,
                     "  %v2 = and i8 %v0, %v1\n"));
    CHECK(translates(IR_OR, IR_I64, IR_I64, false,
                     "  %v2 = or i64 %v0, %v1\n"));
    CHECK(translates(IR_XOR, IR_I32, IR_I32, false,
                     "  %v2 = xor i32 %v0, %v1\n"));
    CHECK(translates(IR_FADD, IR_F64, IR_F64, false,
                     "  %v2 = fadd double %v0, %v1\n"));
    CHECK(translates(IR_FSUB, IR_F32, IR_F32, false,
                     "  %v2 = fsub float %v0, %v1\n"));
    CHECK(translates(IR_FMUL, IR_F64, IR_F64, false,
                     "  %v2 = fmul double %v0, %v1\n"));
    CHECK(translates(IR_FDIV, IR_F32, IR_F32, false,
                     "  %v2 = fdiv float %v0, %v1\n"));
    CHECK(translates(IR_NEG, IR_I32, IR_I32, true,
                     "  %v1 = sub i32 0, %v0\n"
                     "  store i32 %v1, ptr %t1, align 4\n"));
    CHECK(translates(IR_NOT, IR_I64, IR_I64, true,
                     "  %v1 = xor i64 %v0, -1\n"));
    CHECK(translates(IR_FNEG, IR_F64, IR_F64, true,
                     "  %v1 = fneg double %v0\n"));
}

/* A shift takes its count modulo the width. */
static void shifts(void)
{
    CHECK(translates(IR_SHL, IR_I32, IR_I32, false,
                     "  %v2 = and i32 %v1, 31\n"
                     "  %v3 = shl i32 %v0, %v2\n"
                     "  store i32 %v3, ptr %t2, align 4\n"));
    CHECK(translates(IR_SHR_S, IR_I64, IR_I64, false,
                     "  %v2 = and i64 %v1, 63\n"
                     "  %v3 = ashr i64 %v0, %v2\n"));
    CHECK(translates(IR_SHR_U, IR_I8, IR_I8, false,
                     "  %v2 = and i8 %v1, 7\n"
                     "  %v3 = lshr i8 %v0, %v2\n"));
    CHECK(translates(IR_SHL, IR_I16, IR_I16, false,
                     "  %v2 = and i16 %v1, 15\n"));
}

/* A division by zero gives 0, and the least value divided by minus one
   gives itself, with a remainder of 0. The divisor the instruction sees is
   never 0 and never minus one, so no instruction is undefined. */
static void division(void)
{
    CHECK(translates(IR_SDIV, IR_I64, IR_I64, false,
                     "  %v2 = icmp eq i64 %v1, 0\n"
                     "  %v3 = icmp eq i64 %v1, -1\n"
                     "  %v4 = or i1 %v2, %v3\n"
                     "  %v5 = select i1 %v4, i64 1, i64 %v1\n"
                     "  %v6 = sdiv i64 %v0, %v5\n"
                     "  %v7 = sub i64 0, %v0\n"
                     "  %v8 = select i1 %v3, i64 %v7, i64 %v6\n"
                     "  %v9 = select i1 %v2, i64 0, i64 %v8\n"
                     "  store i64 %v9, ptr %t2, align 8\n"));
    CHECK(translates(IR_SREM, IR_I32, IR_I32, false,
                     "  %v2 = icmp eq i32 %v1, 0\n"
                     "  %v3 = icmp eq i32 %v1, -1\n"
                     "  %v4 = or i1 %v2, %v3\n"
                     "  %v5 = select i1 %v4, i32 1, i32 %v1\n"
                     "  %v6 = srem i32 %v0, %v5\n"
                     "  store i32 %v6, ptr %t2, align 4\n"));
    CHECK(translates(IR_UDIV, IR_I64, IR_I64, false,
                     "  %v2 = icmp eq i64 %v1, 0\n"
                     "  %v3 = select i1 %v2, i64 1, i64 %v1\n"
                     "  %v4 = udiv i64 %v0, %v3\n"
                     "  %v5 = select i1 %v2, i64 0, i64 %v4\n"
                     "  store i64 %v5, ptr %t2, align 8\n"));
    CHECK(translates(IR_UREM, IR_I16, IR_I16, false,
                     "  %v2 = icmp eq i16 %v1, 0\n"
                     "  %v3 = select i1 %v2, i16 1, i16 %v1\n"
                     "  %v4 = urem i16 %v0, %v3\n"
                     "  store i16 %v4, ptr %t2, align 2\n"));
}

/* A comparison gives an i1, which widens to the i8 of a bool. */
static void comparisons(void)
{
    static const struct {
        enum ir_op op;
        enum ir_type type;
        const char *body;
    } cases[] = {
        {IR_EQ, IR_I64, "  %v2 = icmp eq i64 %v0, %v1\n"},
        {IR_NE, IR_I8, "  %v2 = icmp ne i8 %v0, %v1\n"},
        {IR_SLT, IR_I32, "  %v2 = icmp slt i32 %v0, %v1\n"},
        {IR_SLE, IR_I16, "  %v2 = icmp sle i16 %v0, %v1\n"},
        {IR_SGT, IR_I64, "  %v2 = icmp sgt i64 %v0, %v1\n"},
        {IR_SGE, IR_I64, "  %v2 = icmp sge i64 %v0, %v1\n"},
        {IR_ULT, IR_I64, "  %v2 = icmp ult i64 %v0, %v1\n"},
        {IR_ULE, IR_I32, "  %v2 = icmp ule i32 %v0, %v1\n"},
        {IR_UGT, IR_I8, "  %v2 = icmp ugt i8 %v0, %v1\n"},
        {IR_UGE, IR_I16, "  %v2 = icmp uge i16 %v0, %v1\n"},
        {IR_EQ, IR_PTR, "  %v2 = icmp eq ptr %v0, %v1\n"},
        {IR_FEQ, IR_F64, "  %v2 = fcmp oeq double %v0, %v1\n"},
        {IR_FNE, IR_F64, "  %v2 = fcmp une double %v0, %v1\n"},
        {IR_FLT, IR_F32, "  %v2 = fcmp olt float %v0, %v1\n"},
        {IR_FLE, IR_F32, "  %v2 = fcmp ole float %v0, %v1\n"},
        {IR_FGT, IR_F64, "  %v2 = fcmp ogt double %v0, %v1\n"},
        {IR_FGE, IR_F64, "  %v2 = fcmp oge double %v0, %v1\n"},
    };
    char body[200];
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        text_format(body, sizeof body,
                    "%s  %%v3 = zext i1 %%v2 to i8\n"
                    "  store i8 %%v3, ptr %%t2, align 1\n",
                    cases[i].body);
        CHECK(translates(cases[i].op, cases[i].type, IR_I8, false, body));
    }
}

/* The conversions, with the saturating intrinsics for a float to an
   integer and half for an f16. */
static void conversions(void)
{
    struct fixture x;

    CHECK(translates(IR_TRUNC, IR_I64, IR_I8, true,
                     "  %v1 = trunc i64 %v0 to i8\n"
                     "  store i8 %v1, ptr %t1, align 1\n"));
    CHECK(translates(IR_SEXT, IR_I8, IR_I64, true,
                     "  %v1 = sext i8 %v0 to i64\n"));
    CHECK(translates(IR_ZEXT, IR_I16, IR_I32, true,
                     "  %v1 = zext i16 %v0 to i32\n"));
    CHECK(translates(IR_SITOF, IR_I32, IR_F64, true,
                     "  %v1 = sitofp i32 %v0 to double\n"));
    CHECK(translates(IR_UITOF, IR_I64, IR_F32, true,
                     "  %v1 = uitofp i64 %v0 to float\n"));
    CHECK(translates(IR_FEXT, IR_F32, IR_F64, true,
                     "  %v1 = fpext float %v0 to double\n"));
    CHECK(translates(IR_FTRUNC, IR_F64, IR_F32, true,
                     "  %v1 = fptrunc double %v0 to float\n"));
    CHECK(translates(IR_HEXT, IR_I16, IR_F32, true,
                     "  %v1 = bitcast i16 %v0 to half\n"
                     "  %v2 = fpext half %v1 to float\n"
                     "  store float %v2, ptr %t1, align 4\n"));
    CHECK(translates(IR_HTRUNC, IR_F32, IR_I16, true,
                     "  %v1 = fptrunc float %v0 to half\n"
                     "  %v2 = bitcast half %v1 to i16\n"
                     "  store i16 %v2, ptr %t1, align 2\n"));

    /* Each intrinsic is declared once, after the definitions. */
    begin(&x);
    operation(&x, IR_FTOSI, IR_F64, IR_I32, true);
    operation(&x, IR_FTOSI, IR_F64, IR_I32, true);
    x.m.functions[1]->name = "g";
    operation(&x, IR_FTOUI, IR_F32, IR_I64, true);
    x.m.functions[2]->name = "h";
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "  %v1 = call i32 @llvm.fptosi.sat.i32.f64(double %v0)\n"
                    "  store i32 %v1, ptr %t1, align 4\n"));
    CHECK(holds(&x, "  %v1 = call i64 @llvm.fptoui.sat.i64.f32(float %v0)\n"));
    CHECK(holds(&x, "}\n\n"
                    "declare i32 @llvm.fptosi.sat.i32.f64(double)\n"
                    "declare i64 @llvm.fptoui.sat.i64.f32(float)\n\n"
                    "attributes #0"));
    end(&x);
}

/* A constant operand stands in the guard as it stands in the operation. */
static void constant_operands(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t t;

    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_I64, IR_NO_AGG);
    b = ir_block_add(f);
    t = ir_binary(f, b, IR_UDIV, IR_I64, ir_int_op(IR_I64, 7),
                  ir_int_op(IR_I64, 0));
    ir_ret(f, b, IR_I64, ir_temp_op(f, t));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "  %v0 = icmp eq i64 0, 0\n"
                    "  %v1 = select i1 %v0, i64 1, i64 0\n"
                    "  %v2 = udiv i64 7, %v1\n"));
    end(&x);
}

/* The runtime calls main through its entry, an alias of the main of the
   module that links. */
static void entry(void)
{
    struct fixture x;

    begin(&x);
    returns(&x, "main", IR_I64, ir_int_op(IR_I64, 3));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "define internal i64 @main.main() #0 {\n"));
    CHECK(holds(&x, "}\n\n@anti.rt.main = alias i64 (), ptr @main.main\n"));
    CHECK(run_with(&x, TARGET_WINDOWS_X86_64, true, false));
    CHECK(holds(&x, "@_A4anti2rt_main = alias i64 (), ptr @_A4main_main\n"));
    end(&x);

    /* A module without main has no entry. */
    begin(&x);
    returns(&x, "f", IR_I64, ir_int_op(IR_I64, 3));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(lacks(&x, "alias"));
    end(&x);
}

/* A global is a packed struct of its bytes, constant unless the program
   writes it, with the linkage of a function. A global of zeros is
   zeroinitializer. */
static void globals(void)
{
    static const uint8_t bytes[3] = {1, 0x22, 0xff};
    static const uint8_t zeros[24] = {0};
    struct fixture x;
    struct ir_global *g;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    ir_global_add(&x.m, "main", "bytes", bytes, sizeof bytes, 1);
    g = ir_global_add(&x.m, "main", "count", zeros, 8, 8);
    g->mutable = true;
    g = ir_global_add(&x.m, NULL, "anti_rt_slots", zeros, sizeof zeros, 8);
    g->exported = true;
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "@main.bytes = internal constant <{ [3 x i8] }> "
                    "<{ [3 x i8] c\"\\01\\22\\FF\" }>, align 1\n"
                    "@main.count = internal global <{ [8 x i8] }> "
                    "zeroinitializer, align 8\n"
                    "@anti_rt_slots = dso_local constant <{ [24 x i8] }> "
                    "zeroinitializer, align 8\n\n"));
    CHECK(run_with(&x, TARGET_LINUX_X86_64, true, false));
    CHECK(holds(&x, "@main.bytes = hidden constant <{ [3 x i8] }> "));
    end(&x);
}

/* A slot is an alloca of its bytes in the entry block, wherever the IR
   puts it, and its temporary holds the address. A load and a store take
   the natural alignment of their type, an offset is a getelementptr of
   bytes, a copy calls llvm.memcpy with the folded size, and the address
   of a global is its symbol. */
static void memory(void)
{
    static const uint8_t zeros[8] = {0};
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b[2];
    struct ir_global *g;
    struct ir_vtype pair;
    uint32_t p;
    uint32_t s;
    uint32_t q;
    uint32_t t;
    uint32_t a;

    begin(&x);
    g = ir_global_add(&x.m, "main", "word", zeros, 8, 8);
    pair = ir_aggregate(ir_array_of(&x.m, "i64", ir_scalar(IR_I64), 2));
    f = ir_function_add(&x.m, "main", "f", IR_I32, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b[0] = ir_block_add(f);
    b[1] = ir_block_add(f);
    ir_jump(f, b[0], b[1]);
    s = ir_slot(f, b[1], pair);
    ir_memcopy(f, b[1], ir_temp_op(f, s), ir_temp_op(f, p), pair);
    q = ir_ptradd(f, b[1], ir_temp_op(f, s), ir_int_op(IR_I64, 4));
    ir_store(f, b[1], IR_I32, ir_int_op(IR_I32, 7), ir_temp_op(f, q));
    t = ir_load(f, b[1], IR_I32, ir_temp_op(f, q));
    a = ir_addr(f, b[1], ir_global_op(g));
    ir_store(f, b[1], IR_I64, ir_int_op(IR_I64, 1), ir_temp_op(f, a));
    ir_ret(f, b[1], IR_I32, ir_temp_op(f, t));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "define internal i32 @main.f(ptr %p0) #0 {\n"
                    "b0:\n"
                    "  %t0 = alloca ptr, align 8\n"
                    "  %t1 = alloca ptr, align 8\n"
                    "  %t2 = alloca ptr, align 8\n"
                    "  %t3 = alloca i32, align 4\n"
                    "  %t4 = alloca ptr, align 8\n"
                    "  %s1 = alloca [16 x i8], align 8\n"
                    "  store ptr %p0, ptr %t0, align 8\n"
                    "  br label %b1\n"
                    "\n"
                    "b1:\n"
                    "  store ptr %s1, ptr %t1, align 8\n"
                    "  %v0 = load ptr, ptr %t1, align 8\n"
                    "  %v1 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %v0, ptr %v1, "
                    "i64 16, i1 false)\n"
                    "  %v2 = load ptr, ptr %t1, align 8\n"
                    "  %v3 = getelementptr i8, ptr %v2, i64 4\n"
                    "  store ptr %v3, ptr %t2, align 8\n"
                    "  %v4 = load ptr, ptr %t2, align 8\n"
                    "  store i32 7, ptr %v4, align 4\n"
                    "  %v5 = load ptr, ptr %t2, align 8\n"
                    "  %v6 = load i32, ptr %v5, align 4\n"
                    "  store i32 %v6, ptr %t3, align 4\n"
                    "  store ptr @main.word, ptr %t4, align 8\n"
                    "  %v7 = load ptr, ptr %t4, align 8\n"
                    "  store i64 1, ptr %v7, align 8\n"
                    "  %v8 = load i32, ptr %t3, align 4\n"
                    "  ret i32 %v8\n"
                    "}\n"));
    CHECK(holds(&x, "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, "
                    "i1)\n"));
    end(&x);

    /* The address of a function is its symbol, and an offset of another
       width than 64 bits is extended with its sign first. */
    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_PTR, IR_NO_AGG);
    p = ir_param_add(f, IR_I32, IR_NO_AGG);
    b[0] = ir_block_add(f);
    a = ir_addr(f, b[0], ir_func_op(f));
    q = ir_ptradd(f, b[0], ir_temp_op(f, a), ir_temp_op(f, p));
    ir_ret(f, b[0], IR_PTR, ir_temp_op(f, q));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "  store ptr @main.f, ptr %t1, align 8\n"
                    "  %v0 = load ptr, ptr %t1, align 8\n"
                    "  %v1 = load i32, ptr %t0, align 4\n"
                    "  %v2 = sext i32 %v1 to i64\n"
                    "  %v3 = getelementptr i8, ptr %v0, i64 %v2\n"));
    end(&x);
}

/* The struct of bitfields a: u32 : 3, b: i32 : 5 and c: u32 : 10, whose
   integers are the i8 at byte 0 for a and b and the i16 at byte 1 for c. */
static uint32_t bit_record(struct fixture *x)
{
    struct ir_field fields[3];

    memset(fields, 0, sizeof fields);
    fields[0].name = "a";
    fields[0].type = ir_scalar(IR_I32);
    fields[0].bits = 3;
    fields[0].ext = IR_EXT_ZERO;
    fields[1].name = "b";
    fields[1].type = ir_scalar(IR_I32);
    fields[1].bits = 5;
    fields[1].ext = IR_EXT_SIGN;
    fields[2].name = "c";
    fields[2].type = ir_scalar(IR_I32);
    fields[2].bits = 10;
    fields[2].ext = IR_EXT_ZERO;
    return ir_struct_add(&x->m, IR_AGG_STRUCT, "main.Bits", fields, 3, false,
                         0);
}

/* A bitfield load reads its integer, shifts the field down and masks it,
   or shifts it to the top and back with its sign, then widens it. A store
   clears the field in the integer and puts the new bits in. An integer
   off its alignment takes the alignment its offset allows. */
static void bitfields(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t agg;
    uint32_t p;
    uint32_t t;
    uint32_t u;

    begin(&x);
    agg = bit_record(&x);
    f = ir_function_add(&x.m, "main", "f", IR_I32, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    t = ir_bitload(f, b, IR_I32, ir_temp_op(f, p), agg, 1);
    u = ir_bitload(f, b, IR_I32, ir_temp_op(f, p), agg, 2);
    ir_bitstore(f, b, IR_I32, ir_temp_op(f, u), ir_temp_op(f, p), agg, 0);
    ir_ret(f, b, IR_I32, ir_temp_op(f, t));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  %v1 = load i8, ptr %v0, align 1\n"
                    "  %v2 = ashr i8 %v1, 3\n"
                    "  %v3 = sext i8 %v2 to i32\n"
                    "  store i32 %v3, ptr %t1, align 4\n"
                    "  %v4 = load ptr, ptr %t0, align 8\n"
                    "  %v5 = getelementptr i8, ptr %v4, i64 1\n"
                    "  %v6 = load i16, ptr %v5, align 1\n"
                    "  %v7 = and i16 %v6, 1023\n"
                    "  %v8 = zext i16 %v7 to i32\n"
                    "  store i32 %v8, ptr %t2, align 4\n"
                    "  %v9 = load i32, ptr %t2, align 4\n"
                    "  %v10 = load ptr, ptr %t0, align 8\n"
                    "  %v11 = load i8, ptr %v10, align 1\n"
                    "  %v12 = trunc i32 %v9 to i8\n"
                    "  %v13 = and i8 %v12, 7\n"
                    "  %v14 = and i8 %v11, -8\n"
                    "  %v15 = or i8 %v14, %v13\n"
                    "  store i8 %v15, ptr %v10, align 1\n"));
    end(&x);
}

/* A global that holds addresses is a packed struct of its bytes and of a
   ptr for each address, a function or another global. A run of zeros is
   zeroinitializer. A global that another object defines is declared
   with the size the IR gives it. */
static void addresses(void)
{
    static const uint8_t bytes[24] = {1};
    static const uint8_t zeros[16] = {0};
    struct fixture x;
    struct ir_global *g;
    struct ir_global *h;

    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    g = ir_global_add(&x.m, "main", "bytes", bytes, 3, 1);
    h = ir_global_add(&x.m, "other", "root", NULL, 0, 1);
    h->is_extern = true;
    h = ir_global_add(&x.m, "main", "table", bytes, 24, 8);
    ir_global_reloc_fn(&x.m, h, 8, 0);
    ir_global_reloc(&x.m, h, 16, g->index);
    h = ir_global_add(&x.m, "main", "slot", zeros, 16, 8);
    ir_global_reloc(&x.m, h, 8, 1);
    h->mutable = true;
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "@main.table = internal constant "
                    "<{ [8 x i8], ptr, ptr }> <{ [8 x i8] "
                    "c\"\\01\\00\\00\\00\\00\\00\\00\\00\", ptr @main.f, "
                    "ptr @main.bytes }>, align 8\n"));
    CHECK(holds(&x, "@main.slot = internal global <{ [8 x i8], ptr }> "
                    "<{ [8 x i8] zeroinitializer, ptr @other.root }>, "
                    "align 8\n"));
    CHECK(holds(&x, "@other.root = external global [0 x i8], align 1\n"));
    end(&x);
}

/* The function of a module that compiles its patterns, and the
   constructor of a shared library, run before main from
   llvm.global_ctors. */
static void constructors(void)
{
    struct fixture x;

    begin(&x);
    returns(&x, IR_PATTERNS_START, IR_VOID, ir_int_op(IR_VOID, 0));
    x.constructor = "anti_rt_init";
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "@llvm.global_ctors = appending global "
                    "[2 x { i32, ptr, ptr }] "
                    "[{ i32, ptr, ptr } { i32 65535, "
                    "ptr @main.patterns.start, ptr null }, "
                    "{ i32, ptr, ptr } { i32 65535, ptr @anti_rt_init, "
                    "ptr null }]\n"));
    CHECK(holds(&x, "declare void @anti_rt_init() #1\n"));
    x.constructor = NULL;
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "@llvm.global_ctors = appending global "
                    "[1 x { i32, ptr, ptr }] "
                    "[{ i32, ptr, ptr } { i32 65535, "
                    "ptr @main.patterns.start, ptr null }]\n"));
    end(&x);

    /* A module without either has no list. */
    begin(&x);
    returns(&x, "f", IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(lacks(&x, "llvm.global_ctors"));
    end(&x);
}

/* The copy of the package header is an object of one private constant in
   the section of emit.c, which llvm.used keeps. The notice is the global
   anti_licenses in the read-only section, ended by a NUL. */
static void sections(void)
{
    struct text out = {0};

    llvm_emit_package(&out, TARGET_LINUX_X86_64, "a\"b", 3);
    CHECK(strstr(text_cstr(&out), "target triple = \"x86_64-unknown-linux-"
                                  "gnu\"\n") != NULL);
    CHECK(strstr(text_cstr(&out),
                 "@anti.package = private constant [3 x i8] c\"a\\22b\", "
                 "section \".anti_package\"\n"
                 "@llvm.used = appending global [1 x ptr] "
                 "[ptr @anti.package], section \"llvm.metadata\"\n") != NULL);
    text_free(&out);
    llvm_emit_package(&out, TARGET_MACOS_ARM64, "x", 1);
    CHECK(strstr(text_cstr(&out), "section \"__DATA,__anti_package\"") !=
          NULL);
    text_free(&out);
    llvm_emit_licenses(&out, TARGET_MACOS_ARM64, "ab", 2);
    CHECK_STR(text_cstr(&out),
              "@anti_licenses = dso_local constant [3 x i8] c\"ab\\00\", "
              "section \"__TEXT,__const\", align 1\n");
    text_free(&out);
    llvm_emit_licenses(&out, TARGET_WINDOWS_X86_64, "", 0);
    CHECK_STR(text_cstr(&out),
              "@anti_licenses = dso_local constant [1 x i8] c\"\\00\", "
              "section \".rdata\", align 1\n");
    text_free(&out);
}

/* A struct of count fields of the given types. */
static uint32_t record(struct fixture *x, const char *name,
                       const enum ir_type *types, size_t count)
{
    struct ir_field fields[8];
    size_t i;

    memset(fields, 0, sizeof fields);
    for (i = 0; i < count; i++) {
        fields[i].name = "f";
        fields[i].type = ir_scalar(types[i]);
    }
    return ir_struct_add(&x->m, IR_AGG_STRUCT, name, fields, count, false, 0);
}

/* A call names the function type, so a variadic callee and an indirect
   one read the same. An i8 or i16 argument extends as the parameter
   records. A call through a table calls the pointer it loaded. A call
   whose result nothing reads still returns the type of its signature. */
static void calls(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_function *g;
    struct ir_function *printf_fn;
    struct ir_function *sig;
    struct ir_block *b;
    struct ir_global *fmt;
    struct ir_operand args[2];
    uint32_t p;
    uint32_t t;

    begin(&x);
    fmt = ir_global_add(&x.m, "main", "fmt", (const uint8_t *)"%g\n", 4, 1);
    g = ir_function_add(&x.m, "main", "g", IR_I64, IR_NO_AGG);
    ir_param_add(g, IR_I32, IR_NO_AGG);
    ir_param_add(g, IR_I8, IR_NO_AGG);
    g->params[1].ext = IR_EXT_ZERO;
    ir_ret(g, ir_block_add(g), IR_I64, ir_int_op(IR_I64, 0));
    printf_fn = ir_extern_add(&x.m, "printf", IR_I32, true);
    ir_param_add(printf_fn, IR_PTR, IR_NO_AGG);
    sig = ir_declare_add(&x.m, "main", "fn.0", IR_I32, IR_NO_AGG);
    ir_param_add(sig, IR_I64, IR_NO_AGG);
    f = ir_function_add(&x.m, "main", "f", IR_I64, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    args[0] = ir_int_op(IR_I32, 1);
    args[1] = ir_int_op(IR_I8, 2);
    t = ir_call(f, b, IR_I64, ir_func_op(g), args, 2);
    args[0] = ir_global_op(fmt);
    args[1] = ir_float_op(IR_F64, 1.5);
    ir_call(f, b, IR_I32, ir_func_op(printf_fn), args, 2);
    args[0] = ir_int_op(IR_I64, 5);
    ir_call_indirect(f, b, IR_I32, ir_temp_op(f, p), sig, args, 1);
    b->insts[b->count - 1].c = ir_global_op(fmt);
    b->insts[b->count - 1].field = 3;
    ir_call_indirect(f, b, IR_VOID, ir_temp_op(f, p), sig, args, 1);
    ir_ret(f, b, IR_I64, ir_temp_op(f, t));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "  %v0 = call i64 (i32, i8) @main.g(i32 1, i8 zeroext 2)\n"
                    "  store i64 %v0, ptr %t1, align 8\n"
                    "  %v1 = call i32 (ptr, ...) @printf(ptr @main.fmt, "
                    "double 0x3FF8000000000000)\n"
                    "  store i32 %v1, ptr %t2, align 4\n"
                    "  %v2 = load ptr, ptr %t0, align 8\n"
                    "  %v3 = call i32 (i64) %v2(i64 5)\n"
                    "  store i32 %v3, ptr %t3, align 4\n"
                    "  %v4 = load ptr, ptr %t0, align 8\n"
                    "  %v5 = call i32 (i64) %v4(i64 5)\n"
                    "  %v6 = load i64, ptr %t1, align 8\n"));
    end(&x);
}

/* The struct {i64, f64} passes in two words and returns in two words
   under System V. The callee builds the struct in a slot from the words,
   and the caller copies the struct into a slot of the words' size to
   load them. */
static void coerced(void)
{
    static const enum ir_type pair[] = {IR_I64, IR_F64};
    struct fixture x;
    struct ir_function *f;
    struct ir_function *g;
    struct ir_block *b;
    struct ir_operand arg;
    uint32_t agg;
    uint32_t p;
    uint32_t t;

    begin(&x);
    agg = record(&x, "main.Pair", pair, 2);
    g = ir_function_add(&x.m, "main", "g", IR_AGG, agg);
    p = ir_param_add(g, IR_AGG, agg);
    ir_ret(g, ir_block_add(g), IR_AGG, ir_temp_op(g, p));
    f = ir_function_add(&x.m, "main", "f", IR_VOID, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    arg = ir_temp_op(f, p);
    t = ir_call(f, b, IR_AGG, ir_func_op(g), &arg, 1);
    ir_store(f, b, IR_I64, ir_int_op(IR_I64, 0), ir_temp_op(f, t));
    ir_ret(f, b, IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "define internal { i64, double } @main.g(i64 %p0.0, "
                    "double %p0.1) #0 {\n"
                    "b0:\n"
                    "  %t0 = alloca ptr, align 8\n"
                    "  %a0 = alloca [16 x i8], align 8\n"
                    "  %a1 = alloca [16 x i8], align 8\n"
                    "  store i64 %p0.0, ptr %a0, align 8\n"
                    "  %v0 = getelementptr i8, ptr %a0, i64 8\n"
                    "  store double %p0.1, ptr %v0, align 8\n"
                    "  store ptr %a0, ptr %t0, align 8\n"
                    "  %v1 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v1, "
                    "i64 16, i1 false)\n"
                    "  %v2 = load i64, ptr %a1, align 8\n"
                    "  %v3 = getelementptr i8, ptr %a1, i64 8\n"
                    "  %v4 = load double, ptr %v3, align 8\n"
                    "  %v5 = insertvalue { i64, double } poison, i64 %v2, 0\n"
                    "  %v6 = insertvalue { i64, double } %v5, double %v4, 1\n"
                    "  ret { i64, double } %v6\n"
                    "}\n"));
    CHECK(holds(&x, "  %a0 = alloca [16 x i8], align 8\n"
                    "  %a1 = alloca [16 x i8], align 8\n"
                    "  store ptr %p0, ptr %t0, align 8\n"
                    "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %a0, ptr %v0, "
                    "i64 16, i1 false)\n"
                    "  %v1 = load i64, ptr %a0, align 8\n"
                    "  %v2 = getelementptr i8, ptr %a0, i64 8\n"
                    "  %v3 = load double, ptr %v2, align 8\n"
                    "  %v4 = call { i64, double } (i64, double) "
                    "@main.g(i64 %v1, double %v3)\n"
                    "  %v5 = extractvalue { i64, double } %v4, 0\n"
                    "  store i64 %v5, ptr %a1, align 8\n"
                    "  %v6 = extractvalue { i64, double } %v4, 1\n"
                    "  %v7 = getelementptr i8, ptr %a1, i64 8\n"
                    "  store double %v6, ptr %v7, align 8\n"
                    "  store ptr %a1, ptr %t1, align 8\n"));
    end(&x);
}

/* main.g(a: Big) -> Big and a call of it, for a struct of three i64. */
static void big_call(struct fixture *x)
{
    static const enum ir_type big[] = {IR_I64, IR_I64, IR_I64};
    struct ir_function *f;
    struct ir_function *g;
    struct ir_block *b;
    struct ir_operand arg;
    uint32_t agg;
    uint32_t p;

    agg = record(x, "main.Big", big, 3);
    g = ir_function_add(&x->m, "main", "g", IR_AGG, agg);
    p = ir_param_add(g, IR_AGG, agg);
    ir_ret(g, ir_block_add(g), IR_AGG, ir_temp_op(g, p));
    f = ir_function_add(&x->m, "main", "f", IR_VOID, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    arg = ir_temp_op(f, p);
    ir_call(f, b, IR_AGG, ir_func_op(g), &arg, 1);
    ir_ret(f, b, IR_VOID, ir_int_op(IR_VOID, 0));
}

/* A struct above 16 bytes: byval and sret under System V, a pointer to a
   copy the caller makes and sret under AAPCS64 and Microsoft x64. */
static void memory_classes(void)
{
    struct fixture x;

    begin(&x);
    big_call(&x);
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "define internal void @main.g(ptr sret([24 x i8]) "
                    "align 8 %sret, ptr byval([24 x i8]) align 8 %p0) #0 {\n"
                    "b0:\n"
                    "  %t0 = alloca ptr, align 8\n"
                    "  store ptr %p0, ptr %t0, align 8\n"
                    "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %sret, ptr %v0, "
                    "i64 24, i1 false)\n"
                    "  ret void\n"
                    "}\n"));
    CHECK(holds(&x, "  %a0 = alloca [24 x i8], align 8\n"
                    "  store ptr %p0, ptr %t0, align 8\n"
                    "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  call void (ptr, ptr) @main.g(ptr sret([24 x i8]) "
                    "align 8 %a0, ptr byval([24 x i8]) align 8 %v0)\n"
                    "  store ptr %a0, ptr %t1, align 8\n"));
    CHECK(run(&x, TARGET_LINUX_ARM64));
    CHECK(holds(&x, "define internal void @main.g(ptr sret([24 x i8]) "
                    "align 8 %sret, ptr %p0) #0 {\n"));
    CHECK(holds(&x, "  %a0 = alloca [24 x i8], align 8\n"
                    "  %a1 = alloca [24 x i8], align 8\n"
                    "  store ptr %p0, ptr %t0, align 8\n"
                    "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %a1, ptr %v0, "
                    "i64 24, i1 false)\n"
                    "  call void (ptr, ptr) @main.g(ptr sret([24 x i8]) "
                    "align 8 %a0, ptr %a1)\n"));
    CHECK(run(&x, TARGET_WINDOWS_X86_64));
    CHECK(holds(&x, "(ptr sret([24 x i8]) align 8 %sret, ptr %p0) #0 {\n"));
    end(&x);
}

/* A float aggregate of AAPCS64 is an array of its members, a simd struct
   of 16 bytes is its vector in a slot of its alignment, and an aggregate
   of 8 bytes under Microsoft x64 is an integer of its size. */
static void word_classes(void)
{
    static const enum ir_type two_floats[] = {IR_F32, IR_F32};
    struct ir_field lanes[4];
    struct fixture x;
    struct ir_function *g;
    uint32_t agg;
    uint32_t vec;
    uint32_t p;
    size_t i;

    begin(&x);
    agg = record(&x, "main.V2", two_floats, 2);
    memset(lanes, 0, sizeof lanes);
    for (i = 0; i < 4; i++) {
        lanes[i].name = "l";
        lanes[i].type = ir_scalar(IR_F32);
    }
    vec = ir_simd_add(&x.m, "main.F4", lanes, 4);
    g = ir_function_add(&x.m, "main", "g", IR_AGG, agg);
    p = ir_param_add(g, IR_AGG, vec);
    ir_param_add(g, IR_AGG, agg);
    ir_ret(g, ir_block_add(g), IR_AGG, ir_temp_op(g, p));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "define internal [2 x float] @main.g(<4 x float> %p0, "
                    "[2 x float] %p1.0) #0 {\n"
                    "b0:\n"
                    "  %t0 = alloca ptr, align 8\n"
                    "  %t1 = alloca ptr, align 8\n"
                    "  %a0 = alloca [16 x i8], align 16\n"
                    "  %a1 = alloca [8 x i8], align 8\n"
                    "  %a2 = alloca [8 x i8], align 8\n"
                    "  store <4 x float> %p0, ptr %a0, align 16\n"
                    "  store ptr %a0, ptr %t0, align 8\n"
                    "  store [2 x float] %p1.0, ptr %a1, align 8\n"
                    "  store ptr %a1, ptr %t1, align 8\n"
                    "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  call void @llvm.memcpy.p0.p0.i64(ptr %a2, ptr %v0, "
                    "i64 8, i1 false)\n"
                    "  %v1 = load [2 x float], ptr %a2, align 8\n"
                    "  ret [2 x float] %v1\n"));
    CHECK(run(&x, TARGET_WINDOWS_X86_64));
    CHECK(holds(&x, "define internal i64 @_A4main_g(ptr %p0, i64 %p1.0) "
                    "#0 {\n"));
    end(&x);
}

/* A simd struct of 4 lanes of type. */
static uint32_t simd4(struct fixture *x, const char *name, enum ir_type type)
{
    struct ir_field lanes[4];
    size_t i;

    memset(lanes, 0, sizeof lanes);
    for (i = 0; i < 4; i++) {
        lanes[i].name = "l";
        lanes[i].type = ir_scalar(type);
    }
    return ir_simd_add(&x->m, name, lanes, 4);
}

/* vsplat and vbinary, which the simd structs that cross to C need: the
   value in every lane, and the lane operation on whole vectors loaded at
   the alignment of the struct. A comparison stores its mask as i8 lanes,
   and a shift masks its counts to the width of a lane. */
static void vectors(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t f4;
    uint32_t i4;
    uint32_t p;
    uint32_t q;
    uint32_t r;

    begin(&x);
    f4 = simd4(&x, "main.F4", IR_F32);
    i4 = simd4(&x, "main.I4", IR_I32);
    f = ir_function_add(&x.m, "main", "f", IR_VOID, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    q = ir_param_add(f, IR_PTR, IR_NO_AGG);
    r = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    ir_vsplat(f, b, IR_F32, ir_temp_op(f, p), ir_float_op(IR_F32, 2.0), f4);
    ir_vbinary(f, b, IR_FADD, IR_F32, ir_temp_op(f, p), ir_temp_op(f, q),
               ir_temp_op(f, r), f4);
    ir_vbinary(f, b, IR_FLT, IR_F32, ir_temp_op(f, p), ir_temp_op(f, q),
               ir_temp_op(f, r), f4);
    ir_vbinary(f, b, IR_SHL, IR_I32, ir_temp_op(f, p), ir_temp_op(f, q),
               ir_temp_op(f, r), i4);
    ir_ret(f, b, IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "  %v0 = load ptr, ptr %t0, align 8\n"
                    "  %v1 = insertelement <4 x float> poison, float "
                    "0x4000000000000000, i64 0\n"
                    "  %v2 = shufflevector <4 x float> %v1, <4 x float> "
                    "poison, <4 x i32> zeroinitializer\n"
                    "  store <4 x float> %v2, ptr %v0, align 16\n"
                    "  %v3 = load ptr, ptr %t0, align 8\n"
                    "  %v4 = load ptr, ptr %t1, align 8\n"
                    "  %v5 = load ptr, ptr %t2, align 8\n"
                    "  %v6 = load <4 x float>, ptr %v4, align 16\n"
                    "  %v7 = load <4 x float>, ptr %v5, align 16\n"
                    "  %v8 = fadd <4 x float> %v6, %v7\n"
                    "  store <4 x float> %v8, ptr %v3, align 16\n"));
    CHECK(holds(&x, "  %v14 = fcmp olt <4 x float> %v12, %v13\n"
                    "  %v15 = zext <4 x i1> %v14 to <4 x i8>\n"
                    "  store <4 x i8> %v15, ptr %v9, align 1\n"));
    CHECK(holds(&x, "  %v21 = and <4 x i32> %v20, <i32 31, i32 31, i32 31, "
                    "i32 31>\n"
                    "  %v22 = shl <4 x i32> %v19, %v21\n"
                    "  store <4 x i32> %v22, ptr %v16, align 16\n"));
    end(&x);
}

/* addfl and subfl give the plain result and keep four i1: the overflow
   of the signed intrinsic, the carry or borrow of the unsigned one, and
   zero and sign by comparison. A carry in runs both intrinsics again on
   the result. The two carries cannot both be set, and the overflow of
   the whole sum is set when exactly one of the two signed steps
   overflowed, as the flag of adc. flag widens the i1 it reads. */
static void flag_operations(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t p;
    uint32_t q;
    uint32_t c;
    uint32_t r;

    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_I8, IR_NO_AGG);
    p = ir_param_add(f, IR_I64, IR_NO_AGG);
    q = ir_param_add(f, IR_I64, IR_NO_AGG);
    c = ir_param_add(f, IR_I8, IR_NO_AGG);
    b = ir_block_add(f);
    r = ir_flag_op(f, b, IR_ADD_FL, IR_I64, ir_temp_op(f, p),
                   ir_temp_op(f, q), ir_temp_op(f, c));
    ir_flag(f, b, IR_FLAG_OVERFLOW, ir_temp_op(f, r));
    ir_flag(f, b, IR_FLAG_CARRY, ir_temp_op(f, r));
    ir_flag(f, b, IR_FLAG_ZERO, ir_temp_op(f, r));
    r = ir_flag_op(f, b, IR_SUB_FL, IR_I64, ir_temp_op(f, p),
                   ir_temp_op(f, q), ir_temp_op(f, c));
    r = ir_flag(f, b, IR_FLAG_NEGATIVE, ir_temp_op(f, r));
    ir_ret(f, b, IR_I8, ir_temp_op(f, r));
    CHECK(run(&x, TARGET_MACOS_ARM64));
    CHECK(holds(&x, "  %v0 = load i64, ptr %t0, align 8\n"
                    "  %v1 = load i64, ptr %t1, align 8\n"
                    "  %v2 = load i8, ptr %t2, align 1\n"
                    "  %v3 = call { i64, i1 } @llvm.sadd.with.overflow.i64("
                    "i64 %v0, i64 %v1)\n"
                    "  %v4 = call { i64, i1 } @llvm.uadd.with.overflow.i64("
                    "i64 %v0, i64 %v1)\n"
                    "  %v5 = extractvalue { i64, i1 } %v3, 0\n"
                    "  %v6 = extractvalue { i64, i1 } %v3, 1\n"
                    "  %v7 = extractvalue { i64, i1 } %v4, 1\n"
                    "  %v8 = icmp ne i8 %v2, 0\n"
                    "  %v9 = zext i1 %v8 to i64\n"
                    "  %v10 = call { i64, i1 } @llvm.sadd.with.overflow.i64("
                    "i64 %v5, i64 %v9)\n"
                    "  %v11 = call { i64, i1 } @llvm.uadd.with.overflow.i64("
                    "i64 %v5, i64 %v9)\n"
                    "  %v12 = extractvalue { i64, i1 } %v10, 0\n"
                    "  %v13 = extractvalue { i64, i1 } %v10, 1\n"
                    "  %v14 = extractvalue { i64, i1 } %v11, 1\n"
                    "  %v15 = xor i1 %v6, %v13\n"
                    "  %v16 = or i1 %v7, %v14\n"
                    "  %v17 = icmp eq i64 %v12, 0\n"
                    "  %v18 = icmp slt i64 %v12, 0\n"
                    "  store i64 %v12, ptr %t3, align 8\n"
                    "  %v19 = zext i1 %v15 to i8\n"
                    "  store i8 %v19, ptr %t4, align 1\n"
                    "  %v20 = zext i1 %v16 to i8\n"
                    "  store i8 %v20, ptr %t5, align 1\n"
                    "  %v21 = zext i1 %v17 to i8\n"
                    "  store i8 %v21, ptr %t6, align 1\n"));
    CHECK(holds(&x, "@llvm.ssub.with.overflow.i64(i64 %v"));
    CHECK(holds(&x, "@llvm.usub.with.overflow.i64(i64 %v"));
    CHECK(holds(&x, "declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, "
                    "i64)\n"));
    end(&x);
}

/* vselect takes the lane of its first operand where the mask holds,
   with the i8 mask cut to i1. vreduce sums from -0.0 in lane order, and
   a less-than keeps the least lane. */
static void vector_folds(void)
{
    struct fixture x;
    struct ir_function *f;
    struct ir_block *b;
    uint32_t f4;
    uint32_t i4;
    uint32_t m4;
    uint32_t p;
    uint32_t q;
    uint32_t s;

    begin(&x);
    f4 = simd4(&x, "main.F4", IR_F32);
    i4 = simd4(&x, "main.I4", IR_I32);
    m4 = simd4(&x, "main.M4", IR_I8);
    f = ir_function_add(&x.m, "main", "f", IR_I32, IR_NO_AGG);
    p = ir_param_add(f, IR_PTR, IR_NO_AGG);
    q = ir_param_add(f, IR_PTR, IR_NO_AGG);
    b = ir_block_add(f);
    ir_vselect(f, b, IR_F32, ir_temp_op(f, p), ir_temp_op(f, q),
               ir_temp_op(f, p), ir_temp_op(f, q), f4);
    ir_vreduce(f, b, IR_FADD, IR_F32, ir_temp_op(f, p), f4);
    s = ir_vreduce(f, b, IR_SLT, IR_I32, ir_temp_op(f, p), i4);
    ir_vreduce(f, b, IR_OR, IR_I8, ir_temp_op(f, q), m4);
    ir_ret(f, b, IR_I32, ir_temp_op(f, s));
    CHECK(run(&x, TARGET_LINUX_X86_64));
    CHECK(holds(&x, "  %v4 = load <4 x i8>, ptr %v1, align 1\n"
                    "  %v5 = trunc <4 x i8> %v4 to <4 x i1>\n"
                    "  %v6 = load <4 x float>, ptr %v2, align 16\n"
                    "  %v7 = load <4 x float>, ptr %v3, align 16\n"
                    "  %v8 = select <4 x i1> %v5, <4 x float> %v6, "
                    "<4 x float> %v7\n"
                    "  store <4 x float> %v8, ptr %v0, align 16\n"));
    CHECK(holds(&x, "  %v10 = load <4 x float>, ptr %v9, align 16\n"
                    "  %v11 = call float @llvm.vector.reduce.fadd.v4f32("
                    "float -0.000000e+00, <4 x float> %v10)\n"
                    "  store float %v11, ptr %t2, align 4\n"));
    CHECK(holds(&x, "  %v14 = call i32 @llvm.vector.reduce.smin.v4i32("
                    "<4 x i32> %v13)\n"));
    CHECK(holds(&x, "  %v17 = call i8 @llvm.vector.reduce.or.v4i8("
                    "<4 x i8> %v16)\n"));
    end(&x);
}

void test_llvm_emit(void)
{
    module_text();
    constants();
    temporaries();
    cold_branches();
    attributes();
    linkage();
    declarations();
    arithmetic();
    shifts();
    division();
    comparisons();
    conversions();
    constant_operands();
    entry();
    globals();
    memory();
    bitfields();
    addresses();
    constructors();
    sections();
    calls();
    coerced();
    memory_classes();
    word_classes();
    vectors();
    flag_operations();
    vector_folds();
    refusals();
}
