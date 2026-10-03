#include "abi.h"

#include <stdio.h>
#include <string.h>

/* The registers that pass arguments under System V, six integer and eight
   vector registers. */
enum { SYSV_INT_ARGS = 6, SYSV_FP_ARGS = 8 };

static void set_word(struct abi_param *p, size_t k, const char *type)
{
    snprintf(p->types[k], sizeof p->types[k], "%s", type);
}

static const char *scalar_name(enum ir_type type, uint64_t bytes)
{
    static const char *const integers[] = {"i8", "i16", "i32", "i64"};

    if (type == IR_F32) {
        return "float";
    }
    if (type == IR_F64) {
        return "double";
    }
    if (type == IR_PTR) {
        return "ptr";
    }
    return integers[bytes == 1 ? 0 : bytes == 2 ? 1 : bytes == 4 ? 2 : 3];
}

static bool is_float(enum ir_type type)
{
    return type == IR_F32 || type == IR_F64;
}

/* A scalar in its own type. c_long, c_wchar and the word of a Mutex take
   the width the layouts give them on the target. */
static void scalar(struct layouts *l, enum ir_type type, enum ir_ext ext,
                   struct abi_param *out)
{
    uint64_t bytes = layout_size(l, ir_scalar(type));

    out->kind = ABI_DIRECT;
    out->size = bytes;
    out->align = layout_align(l, ir_scalar(type));
    out->sign_extend = ext == IR_EXT_SIGN;
    out->word_count = 1;
    set_word(out, 0, scalar_name(type, bytes));
}

/* An aggregate that passes as a pointer: byval, a pointer to a copy, or
   the hidden pointer of a result. */
static void pointer(enum abi_kind kind, const struct layout *agg,
                    struct abi_param *out)
{
    out->kind = kind;
    out->word_count = 1;
    set_word(out, 0, "ptr");
    out->size = agg->size;
    out->align = agg->align;
}

/* A simd struct of 16 bytes as the vector of its lanes. */
static void vector(struct layouts *l, const struct layout *agg,
                   struct abi_param *out)
{
    enum ir_type lane = agg->members[0].type;
    uint64_t bytes = layout_size(l, ir_scalar(lane));

    out->kind = ABI_VECTOR;
    out->word_count = 1;
    snprintf(out->types[0], sizeof out->types[0], "<%llu x %s>",
             (unsigned long long)(agg->size / bytes),
             scalar_name(lane, bytes));
    out->size = agg->size;
    out->align = agg->align;
}

static void coerce(const struct layout *agg, struct abi_param *out)
{
    out->kind = ABI_COERCE;
    out->size = agg->size;
    out->align = agg->align;
}

/* The eightbytes of a System V aggregate of at most 16 bytes. One that
   holds an integer is INTEGER and an i64. One of floats alone is SSE: a
   double for an f64, <2 x float> for two f32 and a float for one. One
   without a member is NO_CLASS and takes no word. Counts the words of
   each register class in ints and floats. Returns false for an aggregate
   of class MEMORY: above 16 bytes, empty, or with a field off its
   alignment. */
static bool sysv_words(const struct layout *agg, struct abi_param *out,
                       size_t *ints, size_t *floats)
{
    size_t count = (size_t)(agg->size + 7) / 8;
    size_t k;
    size_t i;

    *ints = 0;
    *floats = 0;
    if (agg->size > 16 || agg->size == 0 || agg->unaligned) {
        return false;
    }
    coerce(agg, out);
    for (k = 0; k < count; k++) {
        bool integer = false;
        bool f64 = false;
        size_t f32 = 0;
        bool used = false;
        for (i = 0; i < agg->member_count; i++) {
            if (agg->members[i].offset / 8 != k) {
                continue;
            }
            used = true;
            integer = integer || !is_float(agg->members[i].type);
            f64 = f64 || agg->members[i].type == IR_F64;
            f32 += agg->members[i].type == IR_F32;
        }
        if (!used) {
            continue;
        }
        set_word(out, out->word_count++,
                 integer ? "i64"
                 : f64   ? "double"
                 : f32 > 1 ? "<2 x float>"
                           : "float");
        *ints += integer;
        *floats += !integer;
    }
    return true;
}

/* DESIGN: System V puts an aggregate in registers only when every
   eightbyte finds one, and on the stack whole otherwise, while later
   arguments still take the registers left. LLVM would split a pair of
   words between the last register and the stack, so the classification
   counts the registers and makes such an aggregate byval. An sret
   pointer takes the first integer register. */
static void sysv(struct layouts *l, const struct ir_function *f,
                 struct abi_param *params, struct abi_param *result)
{
    size_t ints = 0;
    size_t floats = 0;
    size_t need_int;
    size_t need_fp;
    size_t i;

    if (f->result == IR_AGG) {
        const struct layout *agg = layout_agg(l, f->result_agg);
        if (agg->vector) {
            vector(l, agg, result);
        } else if (!sysv_words(agg, result, &need_int, &need_fp)) {
            memset(result, 0, sizeof *result);
            pointer(ABI_SRET, agg, result);
            ints = 1;
        }
    }
    for (i = 0; i < f->param_count; i++) {
        const struct ir_param *p = &f->params[i];
        const struct layout *agg;
        if (p->type != IR_AGG) {
            scalar(l, p->type, p->ext, &params[i]);
            ints += !is_float(p->type);
            floats += is_float(p->type);
            continue;
        }
        agg = layout_agg(l, p->agg);
        if (agg->vector) {
            vector(l, agg, &params[i]);
            floats++;
        } else if (sysv_words(agg, &params[i], &need_int, &need_fp) &&
                   ints + need_int <= SYSV_INT_ARGS &&
                   floats + need_fp <= SYSV_FP_ARGS) {
            ints += need_int;
            floats += need_fp;
        } else {
            memset(&params[i], 0, sizeof params[i]);
            pointer(ABI_BYVAL, agg, &params[i]);
        }
    }
}

/* A homogeneous floating-point aggregate: one to four members, all f32 or
   all f64. Returns the number of members, or 0. */
static size_t hfa_members(const struct layout *agg)
{
    size_t i;

    if (agg->member_count == 0 || agg->member_count > 4 ||
        !is_float(agg->members[0].type)) {
        return 0;
    }
    for (i = 1; i < agg->member_count; i++) {
        if (agg->members[i].type != agg->members[0].type) {
            return 0;
        }
    }
    return agg->member_count;
}

/* An aggregate under AAPCS64. A homogeneous float aggregate is an array of
   its members unless hfa is false. Another one of at most 16 bytes is an
   array of i64, or an i128 when it is aligned to 16, which starts at an
   even register where the convention asks for one. A larger one goes as
   a pointer, of kind larger. */
static void aapcs64_aggregate(struct layouts *l, const struct layout *agg,
                              bool hfa, enum abi_kind larger,
                              struct abi_param *out)
{
    size_t members = hfa ? hfa_members(agg) : 0;

    if (agg->vector && hfa) {
        vector(l, agg, out);
        return;
    }
    if (members > 0) {
        coerce(agg, out);
        out->word_count = 1;
        snprintf(out->types[0], sizeof out->types[0], "[%zu x %s]", members,
                 agg->members[0].type == IR_F32 ? "float" : "double");
        return;
    }
    if (agg->size > 16) {
        pointer(larger, agg, out);
        return;
    }
    coerce(agg, out);
    out->word_count = 1;
    if (agg->align == 16) {
        set_word(out, 0, "i128");
        return;
    }
    snprintf(out->types[0], sizeof out->types[0], "[%llu x i64]",
             (unsigned long long)((agg->size + 7) / 8));
}

/* DESIGN: LLVM allocates an array argument of AArch64 as one block of
   registers, all or none, and stops the class after one that went to the
   stack. So the three AAPCS64 conventions classify each argument alone.
   Windows passes every composite of a variadic function as an ordinary
   one, an HFA or a vector included, so their named parameters take no
   float array and no vector. */
static void aapcs64(enum target t, struct layouts *l,
                    const struct ir_function *f, struct abi_param *params,
                    struct abi_param *result)
{
    bool hfa = !(f->variadic && target_info(t)->os == OS_WINDOWS);
    size_t i;

    if (f->result == IR_AGG) {
        aapcs64_aggregate(l, layout_agg(l, f->result_agg), true, ABI_SRET,
                          result);
    }
    for (i = 0; i < f->param_count; i++) {
        const struct ir_param *p = &f->params[i];
        if (p->type == IR_AGG) {
            aapcs64_aggregate(l, layout_agg(l, p->agg), hfa, ABI_INDIRECT,
                              &params[i]);
        } else {
            scalar(l, p->type, p->ext, &params[i]);
        }
    }
}

/* An aggregate under Microsoft x64. One of 1, 2, 4 or 8 bytes goes as an
   integer of its size, and any other one as a pointer, of kind other. A
   vector returns in xmm0 and passes as a pointer to a copy, as __m128
   does. */
static void windows_x64_aggregate(struct layouts *l, const struct layout *agg,
                                  bool is_result, enum abi_kind other,
                                  struct abi_param *out)
{
    if (agg->vector && is_result) {
        vector(l, agg, out);
        return;
    }
    if (agg->size == 1 || agg->size == 2 || agg->size == 4 ||
        agg->size == 8) {
        coerce(agg, out);
        out->word_count = 1;
        snprintf(out->types[0], sizeof out->types[0], "i%llu",
                 (unsigned long long)(agg->size * 8));
        return;
    }
    pointer(other, agg, out);
}

static void windows_x64(struct layouts *l, const struct ir_function *f,
                        struct abi_param *params, struct abi_param *result)
{
    size_t i;

    if (f->result == IR_AGG) {
        windows_x64_aggregate(l, layout_agg(l, f->result_agg), true, ABI_SRET,
                              result);
    }
    for (i = 0; i < f->param_count; i++) {
        const struct ir_param *p = &f->params[i];
        if (p->type == IR_AGG) {
            windows_x64_aggregate(l, layout_agg(l, p->agg), false,
                                  ABI_INDIRECT, &params[i]);
        } else {
            scalar(l, p->type, p->ext, &params[i]);
        }
    }
}

void abi_classify(enum target t, struct layouts *l, const struct ir_function *f,
                  struct abi_param *params, struct abi_param *result)
{
    if (f->param_count > 0) {
        memset(params, 0, f->param_count * sizeof *params);
    }
    memset(result, 0, sizeof *result);
    if (f->result == IR_VOID) {
        set_word(result, 0, "void");
    } else if (f->result != IR_AGG) {
        scalar(l, f->result, IR_EXT_NONE, result);
    }
    switch (target_info(t)->convention) {
    case CONVENTION_SYSV:
        sysv(l, f, params, result);
        break;
    case CONVENTION_WINDOWS_X64:
        windows_x64(l, f, params, result);
        break;
    case CONVENTION_AAPCS64:
    case CONVENTION_APPLE_ARM64:
    case CONVENTION_WINDOWS_ARM64:
        aapcs64(t, l, f, params, result);
        break;
    }
}
