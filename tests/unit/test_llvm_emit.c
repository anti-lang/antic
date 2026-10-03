/* llvm_emit_module of the step emit-core: the temporaries, the blocks,
   copy, jump, branch and ret, the signatures, the linkage, the attributes
   of a function, the module flags, and the refusal of every other
   operation. The expected texts follow the sections "Types and layout",
   "Attributes and metadata" and "Instruction mapping" of
   docs/work-order-llvm-back-end.md. */

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
        {IR_ADD, "the LLVM back end does not translate `add` before the "
                 "step emit-arith, in main.f"},
        {IR_FEQ, "the LLVM back end does not translate `feq` before the "
                 "step emit-arith, in main.f"},
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
                      cases[i].op == IR_FEQ ? IR_I8 : IR_I64,
                      ir_int_op(IR_I64, 1), ir_int_op(IR_I64, 2));
        ir_ret(f, b, IR_I64, ir_temp_op(f, t));
        CHECK(!run(&x, TARGET_MACOS_ARM64));
        CHECK_STR(x.error, cases[i].message);
        end(&x);
    }

    /* A slot and a call wait for the step emit-memory. */
    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_VOID, IR_NO_AGG);
    b = ir_block_add(f);
    ir_slot(f, b, ir_scalar(IR_I64));
    ir_ret(f, b, IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(!run(&x, TARGET_MACOS_ARM64));
    CHECK_STR(x.error, "the LLVM back end does not translate `slot` before "
                       "the step emit-memory, in main.f");
    end(&x);

    /* So does a parameter of an aggregate. */
    begin(&x);
    f = ir_function_add(&x.m, "main", "f", IR_VOID, IR_NO_AGG);
    ir_param_add(f, IR_AGG, ir_array_of(&x.m, "i64", ir_scalar(IR_I64), 2));
    ir_ret(f, ir_block_add(f), IR_VOID, ir_int_op(IR_VOID, 0));
    CHECK(!run(&x, TARGET_MACOS_ARM64));
    CHECK_STR(x.error, "the LLVM back end does not translate an aggregate "
                       "parameter or result before the step emit-memory, "
                       "in main.f");
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
    refusals();
}
