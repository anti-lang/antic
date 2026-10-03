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
    refusals();
}
