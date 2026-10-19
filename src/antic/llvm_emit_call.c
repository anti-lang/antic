/* The C convention of LLVM IR text, which pairs with abi.c: the
   classification of a function, its parameters, its prologue and the
   return of an aggregate, the arguments and the result of a call, the
   atomic calls, and the signature and the linkage of a function. */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "abi.h"
#include "alloc.h"
#include "llvm_emit_parts.h"
#include "rt_abi.h"

void llvm_emit_classify(struct emitter *e, const struct ir_function *f,
                        struct classified *c)
{
    c->params = alloc_zeroed(f->param_count + 1, sizeof *c->params);
    abi_classify(e->o->target, e->l, f, c->params, &c->result);
}

/* The LLVM type of the result r: void for one through the sret pointer,
   and a literal struct for two words. */
static void result_type(struct text *out, const struct abi_param *r)
{
    if (r->kind == ABI_SRET) {
        text_append(out, "void");
    } else if (r->kind == ABI_COERCE && r->word_count == 2) {
        text_appendf(out, "{ %s, %s }", r->types[0], r->types[1]);
    } else {
        text_append(out, r->types[0]);
    }
}

static uint64_t round_to_word(uint64_t size)
{
    return (size + 7) / 8 * 8;
}

static uint64_t word_align(uint64_t align)
{
    return align < 8 ? 8 : align;
}

/* The name %a<n> of a new alloca of size bytes aligned to align in the
   entry block, for a copy that a call or a signature needs. */
static uint32_t entry_slot(struct emitter *e, uint64_t size, uint64_t align)
{
    text_appendf(e->allocas, "  %%a%" PRIu32 " = alloca [%" PRIu64
                             " x i8], align %" PRIu64 "\n",
                 e->slots, size, align);
    return e->slots++;
}

/* DESIGN: a coerced aggregate lives in a slot of its words' size, a
   multiple of 8 bytes aligned to 8 or more, so a word that reaches past
   the end of the aggregate stays inside the slot. Word k of System V
   stands at 8 k bytes, and every other convention has one word. Returns
   the slot. */
static uint32_t word_slot(struct emitter *e, const struct abi_param *p)
{
    return entry_slot(e, round_to_word(p->size), word_align(p->align));
}

/* Append to address the address of word k of slot a. */
static void word_address(struct emitter *e, uint32_t a, size_t k,
                         struct text *address)
{
    if (k == 0) {
        text_appendf(address, "%%a%" PRIu32, a);
        return;
    }
    text_appendf(e->out, "  %%v%" PRIu32 " = getelementptr i8, ptr %%a%"
                         PRIu32 ", i64 %zu\n",
                 e->value, a, 8 * k);
    text_appendf(address, "%%v%" PRIu32, e->value++);
}

/* Load the words of p from slot a, appending each value to values with
   its type, and its type alone to types. */
static void load_words(struct emitter *e, const struct abi_param *p,
                       uint32_t a, uint32_t *words)
{
    size_t k;

    for (k = 0; k < p->word_count; k++) {
        struct text address = {0};
        word_address(e, a, k, &address);
        words[k] = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align 8\n",
                     words[k], p->types[k], text_cstr(&address));
        text_free(&address);
    }
}

/* Append a separator to list unless it is empty. */
static void separate(struct text *list)
{
    if (list->length > 0) {
        text_append(list, ", ");
    }
}

/* The attribute of an i8 or i16 parameter, which extends as the
   signature records. */
static const char *extension(enum ir_type type, enum ir_ext ext)
{
    if (type != IR_I8 && type != IR_I16) {
        return "";
    }
    return ext == IR_EXT_SIGN   ? " signext"
           : ext == IR_EXT_ZERO ? " zeroext"
                                : "";
}

/* DESIGN: the facts of a parameter as attributes, as "Parameters and
   results" in docs/work-order-llvm-optimization.md gives them. A scalar
   is noundef, an own pointer noalias, and a `*T` nonnull and
   dereferenceable for the size of T where that size is not zero. A
   coerced aggregate, a vector and a parameter in memory take none of
   them: the words of an aggregate may hold its padding, whose bytes are
   undefined, and clang leaves them out as well. */
static void param_facts(struct emitter *e, const struct ir_param *param,
                        const struct abi_param *p, struct text *head)
{
    uint64_t size = 0;

    if (p->kind != ABI_DIRECT) {
        return;
    }
    text_append(head, param->own ? " noalias" : "");
    text_append(head, " noundef");
    text_append(head, param->nonnull ? " nonnull" : "");
    if (param->deref_size != IR_NO_INDEX &&
        layout_fold(e->l, param->deref_size, &size) && size > 0) {
        text_appendf(head, " dereferenceable(%" PRIu64 ")", size);
    }
}

/* Append to head the parameters of f as c classifies them, with their
   attributes and, when names is set, the names %sret, %p<i> and
   %p<i>.<k>. Append their types alone to types. */
static void parameters(struct emitter *e, const struct ir_function *f,
                       const struct classified *c, bool names,
                       struct text *head, struct text *types)
{
    size_t i;
    size_t k;

    if (c->result.kind == ABI_SRET) {
        text_appendf(head, "ptr sret([%" PRIu64 " x i8]) align %" PRIu64 "%s",
                     c->result.size, c->result.align, names ? " %sret" : "");
        text_append(types, "ptr");
    }
    for (i = 0; i < f->param_count; i++) {
        const struct abi_param *p = &c->params[i];
        for (k = 0; k < p->word_count; k++) {
            separate(head);
            separate(types);
            text_append(types, p->types[k]);
            if (p->kind == ABI_BYVAL) {
                text_appendf(head, "ptr byval([%" PRIu64 " x i8]) align %"
                                   PRIu64,
                             p->size, p->align);
            } else {
                text_appendf(head, "%s%s", p->types[k],
                             p->kind == ABI_DIRECT
                                 ? extension(f->params[i].type,
                                             f->params[i].ext)
                                 : "");
                param_facts(e, &f->params[i], p, head);
            }
            if (names && p->kind == ABI_COERCE) {
                text_appendf(head, " %%p%zu.%zu", i, k);
            } else if (names) {
                text_appendf(head, " %%p%zu", i);
            }
        }
    }
    if (f->variadic) {
        separate(head);
        separate(types);
        text_append(head, "...");
        text_append(types, "...");
    }
}

/* DESIGN: a parameter arrives as abi_classify passes it, and its
   temporary holds what the IR expects: the scalar, or the address of the
   aggregate. A coerced aggregate is rebuilt from its words in a slot, and
   a vector is stored in a slot. A byval or indirect one is already in
   memory that the callee may write, a copy the caller or LLVM made. */
void llvm_emit_prologue(struct emitter *e, const struct ir_function *f,
                        const struct classified *c)
{
    size_t i;
    size_t k;

    for (i = 0; i < f->param_count && !e->failed; i++) {
        const struct abi_param *p = &c->params[i];
        uint32_t temp = f->params[i].temp;
        uint32_t a = 0;
        switch (p->kind) {
        case ABI_DIRECT:
            text_appendf(e->out, "  store %s %%p%zu, ptr %%t%" PRIu32
                                 ", align %" PRIu64 "\n",
                         llvm_emit_value_type(e, f->params[i].type), i, temp,
                         llvm_emit_align_of(e, f->params[i].type));
            continue;
        case ABI_BYVAL:
        case ABI_INDIRECT:
        case ABI_SRET:
            text_appendf(e->out, "  store ptr %%p%zu, ptr %%t%" PRIu32
                                 ", align 8\n",
                         i, temp);
            continue;
        case ABI_VECTOR:
            a = entry_slot(e, p->size, p->align);
            text_appendf(e->out, "  store %s %%p%zu, ptr %%a%" PRIu32
                                 ", align %" PRIu64 "\n",
                         p->types[0], i, a, p->align);
            break;
        case ABI_COERCE:
            a = word_slot(e, p);
            for (k = 0; k < p->word_count; k++) {
                struct text address = {0};
                word_address(e, a, k, &address);
                text_appendf(e->out, "  store %s %%p%zu.%zu, ptr %s, "
                                     "align 8\n",
                             p->types[k], i, k, text_cstr(&address));
                text_free(&address);
            }
            break;
        }
        text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                             ", align 8\n",
                     a, temp);
    }
}

/* Return the aggregate at the address in operand a as c classifies the
   result: a copy through the sret pointer, the vector, or the words
   loaded from a slot of their size. */
void llvm_emit_return_aggregate(struct emitter *e, const struct ir_operand *a)
{
    const struct abi_param *r = &e->signature.result;
    struct text pointer = {0};
    uint32_t words[2];
    uint32_t slot;

    llvm_emit_operand(e, a, IR_PTR, &pointer);
    if (e->failed) {
        text_free(&pointer);
        return;
    }
    if (r->kind == ABI_SRET) {
        llvm_emit_copy_bytes(e, "%sret", text_cstr(&pointer), r->size);
        text_append(e->out, "  ret void\n");
    } else if (r->kind == ABI_VECTOR) {
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     e->value, r->types[0], text_cstr(&pointer), r->align);
        text_appendf(e->out, "  ret %s %%v%" PRIu32 "\n", r->types[0],
                     e->value++);
    } else {
        char name[24];
        slot = word_slot(e, r);
        text_format_name(name, sizeof name, "%%a%" PRIu32, slot);
        llvm_emit_copy_bytes(e, name, text_cstr(&pointer), r->size);
        load_words(e, r, slot, words);
        if (r->word_count == 1) {
            text_appendf(e->out, "  ret %s %%v%" PRIu32 "\n", r->types[0],
                         words[0]);
        } else {
            uint32_t first = llvm_emit_fresh(e);
            uint32_t both = llvm_emit_fresh(e);
            text_appendf(e->out, "  %%v%" PRIu32 " = insertvalue { %s, %s } "
                                 "poison, %s %%v%" PRIu32 ", 0\n",
                         first, r->types[0], r->types[1], r->types[0],
                         words[0]);
            text_appendf(e->out, "  %%v%" PRIu32 " = insertvalue { %s, %s } "
                                 "%%v%" PRIu32 ", %s %%v%" PRIu32 ", 1\n",
                         both, r->types[0], r->types[1], first, r->types[1],
                         words[1]);
            text_appendf(e->out, "  ret { %s, %s } %%v%" PRIu32 "\n",
                         r->types[0], r->types[1], both);
        }
    }
    text_free(&pointer);
}

/* Append to args the argument o for parameter p, after the instructions
   that make it, and its type to types. An aggregate argument is the
   address of the value. */
static void argument(struct emitter *e, const struct abi_param *p,
                     const struct ir_param *param, const struct ir_operand *o,
                     struct text *args, struct text *types)
{
    struct text value = {0};
    uint32_t words[2];
    uint32_t slot;
    size_t k;

    if (p->kind == ABI_DIRECT) {
        llvm_emit_operand(e, o, param->type, &value);
        separate(args);
        separate(types);
        text_appendf(args, "%s%s %s", p->types[0],
                     extension(param->type, param->ext), text_cstr(&value));
        text_append(types, p->types[0]);
        text_free(&value);
        return;
    }
    llvm_emit_operand(e, o, IR_PTR, &value);
    if (e->failed) {
        text_free(&value);
        return;
    }
    switch (p->kind) {
    case ABI_BYVAL:
        separate(args);
        text_appendf(args, "ptr byval([%" PRIu64 " x i8]) align %" PRIu64
                           " %s",
                     p->size, p->align, text_cstr(&value));
        break;
    case ABI_INDIRECT: {
        char name[24];
        slot = entry_slot(e, p->size, p->align);
        text_format_name(name, sizeof name, "%%a%" PRIu32, slot);
        llvm_emit_copy_bytes(e, name, text_cstr(&value), p->size);
        separate(args);
        text_appendf(args, "ptr %s", name);
        break;
    }
    case ABI_VECTOR:
        text_appendf(e->out, "  %%v%" PRIu32 " = load %s, ptr %s, align %"
                             PRIu64 "\n",
                     e->value, p->types[0], text_cstr(&value), p->align);
        separate(args);
        text_appendf(args, "%s %%v%" PRIu32, p->types[0], e->value++);
        break;
    default: {
        char name[24];
        slot = word_slot(e, p);
        text_format_name(name, sizeof name, "%%a%" PRIu32, slot);
        llvm_emit_copy_bytes(e, name, text_cstr(&value), p->size);
        load_words(e, p, slot, words);
        for (k = 0; k < p->word_count; k++) {
            separate(args);
            text_appendf(args, "%s %%v%" PRIu32, p->types[k], words[k]);
        }
        break;
    }
    }
    for (k = 0; k < p->word_count; k++) {
        separate(types);
        text_append(types, p->types[k]);
    }
    text_free(&value);
}

/* Keep the aggregate result of a call, the value call of c, in a slot
   whose address the result temporary of inst holds: the words of a
   coerced result or the vector. */
static void keep_result(struct emitter *e, const struct ir_inst *inst,
                        const struct abi_param *r, uint32_t call)
{
    uint32_t slot;
    size_t k;

    if (r->kind == ABI_VECTOR) {
        slot = entry_slot(e, r->size, r->align);
        text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %%a%" PRIu32
                             ", align %" PRIu64 "\n",
                     r->types[0], call, slot, r->align);
    } else {
        slot = word_slot(e, r);
        for (k = 0; k < r->word_count; k++) {
            struct text address = {0};
            uint32_t word = call;
            if (r->word_count == 2) {
                word = llvm_emit_fresh(e);
                text_appendf(e->out, "  %%v%" PRIu32 " = extractvalue "
                                     "{ %s, %s } %%v%" PRIu32 ", %zu\n",
                             word, r->types[0], r->types[1], call, k);
            }
            word_address(e, slot, k, &address);
            text_appendf(e->out, "  store %s %%v%" PRIu32 ", ptr %s, "
                                 "align 8\n",
                         r->types[k], word, text_cstr(&address));
            text_free(&address);
        }
    }
    text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                         ", align 8\n",
                 slot, inst->result);
}

/* DESIGN: the result of a call takes the type of the instruction, which
   may differ from the one the callee declares, as a pointer that a
   function of the runtime returns as an i64. Returns the value of type to, which is v when
   the two LLVM types agree. */
static uint32_t reinterpret(struct emitter *e, uint32_t v, enum ir_type from,
                            enum ir_type to)
{
    const char *a = llvm_emit_value_type(e, from);
    const char *b = llvm_emit_value_type(e, to);
    bool float_a = from == IR_F32 || from == IR_F64;
    bool float_b = to == IR_F32 || to == IR_F64;
    const char *op;
    uint32_t n;

    if (a == NULL || b == NULL || strcmp(a, b) == 0) {
        return v;
    }
    if (from == IR_PTR) {
        op = "ptrtoint";
    } else if (to == IR_PTR) {
        op = "inttoptr";
    } else if (float_a || float_b ||
               layout_size(e->l, ir_scalar(from)) ==
                   layout_size(e->l, ir_scalar(to))) {
        op = "bitcast";
    } else if (layout_size(e->l, ir_scalar(from)) >
               layout_size(e->l, ir_scalar(to))) {
        op = "trunc";
    } else {
        op = "zext";
    }
    n = llvm_emit_fresh(e);
    text_appendf(e->out, "  %%v%" PRIu32 " = %s %s %%v%" PRIu32 " to %s\n", n,
                 op, a, v, b);
    return n;
}

/* range(T low, high) and a space where the result r of f carries a range
   and passes as its own type. */
static void result_range(struct emitter *e, struct text *out,
                         const struct ir_function *f,
                         const struct abi_param *r)
{
    const char *type;

    if (!f->result_range || r->kind != ABI_DIRECT || r->word_count != 1) {
        return;
    }
    type = llvm_emit_value_type(e, f->result);
    if (type == NULL || strcmp(r->types[0], type) != 0) {
        return;
    }
    text_appendf(out, "range(%s ", type);
    llvm_emit_int_constant(out, f->result, f->result_low);
    text_append(out, ", ");
    llvm_emit_int_constant(out, f->result, f->result_high);
    text_append(out, ") ");
}

/* The kind of LLVM instruction that writes an atomic operation of the
   runtime. */
enum atomic_form { ATOMIC_LOAD_FORM, ATOMIC_STORE_FORM, ATOMIC_RMW_FORM,
                   ATOMIC_CAS_FORM };

/* The atomic operations of the runtime and the instruction of each, with
   the operation of an atomicrmw. */
static const struct {
    enum rt_function function;
    enum atomic_form form;
    const char *rmw;
} atomic_calls[] = {
    {RT_FN_ATOMIC_LOAD, ATOMIC_LOAD_FORM, NULL},
    {RT_FN_ATOMIC_STORE, ATOMIC_STORE_FORM, NULL},
    {RT_FN_ATOMIC_SWAP, ATOMIC_RMW_FORM, "xchg"},
    {RT_FN_ATOMIC_ADD, ATOMIC_RMW_FORM, "add"},
    {RT_FN_ATOMIC_SUB, ATOMIC_RMW_FORM, "sub"},
    {RT_FN_ATOMIC_AND, ATOMIC_RMW_FORM, "and"},
    {RT_FN_ATOMIC_OR, ATOMIC_RMW_FORM, "or"},
    {RT_FN_ATOMIC_COMPARE_SWAP, ATOMIC_CAS_FORM, NULL},
};

/* The value of operand o of an atomic call as an integer of bits bits:
   the i64 the runtime takes, truncated. */
static void atomic_value(struct emitter *e, const struct ir_operand *o,
                         unsigned bits, struct text *out)
{
    struct text wide = {0};
    uint32_t narrow;

    llvm_emit_operand(e, o, IR_I64, &wide);
    if (bits == 64 || e->failed) {
        text_append(out, text_cstr(&wide));
    } else {
        narrow = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = trunc i64 %s to i%u\n",
                     narrow, text_cstr(&wide), bits);
        text_appendf(out, "%%v%" PRIu32, narrow);
    }
    text_free(&wide);
}

/* DESIGN: an atomic operation is a call of the runtime in the IR, which
   keeps the IR and the library file as they were, and the text writes it
   as the LLVM instruction that the runtime's body holds: load atomic,
   store atomic, atomicrmw or cmpxchg, sequentially consistent as every
   operation of src/rt/atomic.c, at the natural alignment of its width.
   The level of the function then decides the instructions, as it decided
   those of the runtime of the level: `ldaddal` and `casal` with LSE, a
   load-store-exclusive loop on armv8.0, whose features name no outline
   atomics, and `lock xadd` and `lock cmpxchg` on x86_64. A result is the
   value sign-extended to an i64, as the runtime gives it, and a compare
   and swap gives the success flag as a byte. A width that is no constant
   of 1, 2, 4 or 8 bytes stays a call, which no lowering writes. Returns
   whether it wrote the call. */
static bool atomic_call(struct emitter *e, const struct ir_inst *inst,
                        const struct ir_function *callee)
{
    struct text address = {0};
    struct text value = {0};
    struct text desired = {0};
    const char *rmw = NULL;
    enum atomic_form form = ATOMIC_LOAD_FORM;
    bool found = false;
    unsigned bits;
    uint64_t width;
    uint32_t result = 0;
    size_t i;

    if (!callee->is_extern || callee->module != NULL ||
        inst->arg_count < 2 || inst->args[1].kind != IR_INT) {
        return false;
    }
    for (i = 0; i < sizeof atomic_calls / sizeof atomic_calls[0]; i++) {
        if (strcmp(callee->name, rt_name(atomic_calls[i].function)) == 0) {
            form = atomic_calls[i].form;
            rmw = atomic_calls[i].rmw;
            found = true;
        }
    }
    width = inst->args[1].as.integer;
    if (!found || (width != 1 && width != 2 && width != 4 && width != 8)) {
        return false;
    }
    bits = (unsigned)width * 8;
    llvm_emit_operand(e, &inst->args[0], IR_PTR, &address);
    if (form != ATOMIC_LOAD_FORM) {
        atomic_value(e, &inst->args[2], bits, &value);
    }
    if (form == ATOMIC_CAS_FORM) {
        atomic_value(e, &inst->args[3], bits, &desired);
    }
    if (e->failed) {
        goto done;
    }
    switch (form) {
    case ATOMIC_LOAD_FORM:
        result = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = load atomic i%u, ptr %s "
                             "seq_cst, align %" PRIu64 "\n",
                     result, bits, text_cstr(&address), width);
        break;
    case ATOMIC_STORE_FORM:
        text_appendf(e->out, "  store atomic i%u %s, ptr %s seq_cst, "
                             "align %" PRIu64 "\n",
                     bits, text_cstr(&value), text_cstr(&address), width);
        break;
    case ATOMIC_RMW_FORM:
        result = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = atomicrmw %s ptr %s, i%u %s "
                             "seq_cst, align %" PRIu64 "\n",
                     result, rmw, text_cstr(&address), bits,
                     text_cstr(&value), width);
        break;
    case ATOMIC_CAS_FORM: {
        uint32_t pair = llvm_emit_fresh(e);
        uint32_t flag = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = cmpxchg ptr %s, i%u %s, "
                             "i%u %s seq_cst seq_cst, align %" PRIu64 "\n",
                     pair, text_cstr(&address), bits, text_cstr(&value), bits,
                     text_cstr(&desired), width);
        text_appendf(e->out, "  %%v%" PRIu32 " = extractvalue { i%u, i1 } "
                             "%%v%" PRIu32 ", 1\n",
                     flag, bits, pair);
        result = llvm_emit_fresh(e);
        text_appendf(e->out, "  %%v%" PRIu32 " = zext i1 %%v%" PRIu32
                             " to i8\n",
                     result, flag);
        break;
    }
    }
    if (inst->result != IR_NO_RESULT && form != ATOMIC_STORE_FORM) {
        char name[24];
        if (form != ATOMIC_CAS_FORM) {
            if (bits < 64) {
                uint32_t wide = llvm_emit_fresh(e);
                text_appendf(e->out, "  %%v%" PRIu32 " = sext i%u %%v%" PRIu32
                                     " to i64\n",
                             wide, bits, result);
                result = wide;
            }
            result = reinterpret(e, result, IR_I64, inst->type);
        }
        text_format_name(name, sizeof name, "%%v%" PRIu32, result);
        llvm_emit_store_result(e, inst, name);
    }
done:
    text_free(&address);
    text_free(&value);
    text_free(&desired);
    return true;
}

/* DESIGN: a call names its function type, which abi_classify gives for
   the callee, or for the signature of an indirect call. An argument past
   the parameters of the callee, the variadic part of a C function or the
   context a named function never reads, passes in its own type. A call
   through a table is a call of the pointer the IR loaded from it, whose
   descriptor and slot the instruction carries for the passes before. An
   aggregate result lives in a slot of the call, whose address the result
   temporary holds. */
void llvm_emit_call(struct emitter *e, const struct ir_inst *inst)
{
    const struct ir_function *callee =
        e->m->functions[inst->b.kind == IR_FUNC ? inst->b.as.index
                                                : inst->a.as.index];
    struct classified c;
    struct text target = {0};
    struct text args = {0};
    struct text types = {0};
    struct text result = {0};
    struct text facts = {0};
    uint32_t sret = 0;
    uint32_t value;
    size_t i;

    if (atomic_call(e, inst, callee)) {
        return;
    }
    llvm_emit_classify(e, callee, &c);
    result_type(&result, &c.result);
    if (inst->a.kind == IR_FUNC) {
        llvm_emit_function_name(&target, e->o->target,
                                e->m->functions[inst->a.as.index]);
    } else {
        llvm_emit_operand(e, &inst->a, IR_PTR, &target);
    }
    if (c.result.kind == ABI_SRET) {
        sret = word_slot(e, &c.result);
        text_appendf(&args, "ptr sret([%" PRIu64 " x i8]) align %" PRIu64
                            " %%a%" PRIu32,
                     c.result.size, c.result.align, sret);
        text_append(&types, "ptr");
    }
    for (i = 0; i < inst->arg_count && !e->failed; i++) {
        struct ir_param extra;
        struct text unnamed = {0};
        if (i < callee->param_count) {
            argument(e, &c.params[i], &callee->params[i], &inst->args[i],
                     &args, &types);
            continue;
        }
        memset(&extra, 0, sizeof extra);
        extra.type = inst->args[i].type;
        if (llvm_emit_value_type(e, extra.type) != NULL) {
            struct abi_param p;
            memset(&p, 0, sizeof p);
            p.kind = ABI_DIRECT;
            p.word_count = 1;
            text_format_name(p.types[0], sizeof p.types[0], "%s",
                             llvm_emit_value_type(e, extra.type));
            /* The variadic part of a C function stays out of its type. */
            argument(e, &p, &extra, &inst->args[i], &args,
                     callee->variadic ? &unnamed : &types);
        }
        text_free(&unnamed);
    }
    if (callee->variadic) {
        separate(&types);
        text_append(&types, "...");
    }
    if (!e->failed) {
        bool named = strcmp(text_cstr(&result), "void") != 0;
        value = e->value;
        text_append(e->out, "  ");
        if (named) {
            text_appendf(e->out, "%%v%" PRIu32 " = ", e->value++);
        }
        if (inst->a.kind == IR_FUNC) {
            result_range(e, &facts, callee, &c.result);
        }
        text_appendf(e->out, "call %s%s (%s) %s(%s)\n", text_cstr(&facts),
                     text_cstr(&result), text_cstr(&types),
                     text_cstr(&target), text_cstr(&args));
        if (inst->result == IR_NO_RESULT) {
            /* Nothing reads the result. */
        } else if (c.result.kind == ABI_SRET) {
            text_appendf(e->out, "  store ptr %%a%" PRIu32 ", ptr %%t%" PRIu32
                                 ", align 8\n",
                         sret, inst->result);
        } else if (callee->result == IR_AGG) {
            keep_result(e, inst, &c.result, value);
        } else {
            char name[24];
            value = reinterpret(e, value, callee->result, inst->type);
            text_format_name(name, sizeof name, "%%v%" PRIu32, value);
            llvm_emit_store_result(e, inst, name);
        }
    }
    free(c.params);
    text_free(&target);
    text_free(&args);
    text_free(&types);
    text_free(&result);
    text_free(&facts);
}

/* DESIGN: a result of 8 or 16 bits extends where its callee extends it.
   antic extends it in every function it defines, so a definition and a
   declaration of another module write the extension the signature
   records. A function of C extends it where its convention says so,
   System V x86_64 and Apple arm64, where clang writes it too. AAPCS64 of
   Linux and both Windows conventions leave the upper bits to the caller,
   so a declaration of C there writes none, and the caller extends what it
   reads. */
static bool result_extends(const struct emitter *e,
                           const struct ir_function *f)
{
    enum convention convention = target_info(e->o->target)->convention;

    return !f->is_extern || f->module != NULL ||
           convention == CONVENTION_SYSV ||
           convention == CONVENTION_APPLE_ARM64;
}

/* The attributes of the result r of f, each followed by a space: noalias
   on the result of a function that allocates, noundef on a scalar and
   the extension of a result of 8 or 16 bits. */
static void result_facts(struct emitter *e, const struct ir_function *f,
                         const struct abi_param *r)
{
    if (r->kind != ABI_DIRECT || f->result == IR_VOID) {
        return;
    }
    text_append(e->out, f->allocates ? "noalias " : "");
    text_append(e->out, "noundef ");
    if (result_extends(e, f) &&
        (f->result == IR_I8 || f->result == IR_I16)) {
        text_append(e->out, f->result_ext == IR_EXT_SIGN   ? "signext "
                            : f->result_ext == IR_EXT_ZERO ? "zeroext "
                                                           : "");
    }
    result_range(e, e->out, f, r);
}

/* The result type, the name and the parameters of f in parentheses, as
   abi_classify gives them, with the names of the parameters when names
   is set. The function type goes to type unless it is NULL. Returns
   false after a failure. */
bool llvm_emit_signature(struct emitter *e, const struct ir_function *f,
                         bool names, struct text *type)
{
    struct classified c;
    struct text head = {0};
    struct text types = {0};
    struct text result = {0};

    llvm_emit_classify(e, f, &c);
    result_type(&result, &c.result);
    parameters(e, f, &c, names, &head, &types);
    result_facts(e, f, &c.result);
    text_appendf(e->out, "%s ", text_cstr(&result));
    llvm_emit_function_name(e->out, e->o->target, f);
    text_appendf(e->out, "(%s)", text_cstr(&head));
    if (type != NULL) {
        text_appendf(type, "%s (%s)", text_cstr(&result), text_cstr(&types));
    }
    text_free(&head);
    text_free(&types);
    text_free(&result);
    free(c.params);
    return !e->failed;
}

/* Whether f is a copy of a generic in an object of one module, which
   every module that uses it defines. */
bool llvm_emit_link_once(const struct emitter *e, const struct ir_function *f)
{
    return e->o->one_module && ir_is_copy_name(f->name);
}

/* DESIGN: the linkage of a function. A whole program keeps every
   function internal, unless it is an export fn or the program hosts
   plugins. An object of one module makes every function global and
   hidden for the other objects, and a copy of a generic weak, in a COMDAT
   on COFF, which has no hidden symbols. An export fn is dso_local with
   default visibility. */
/* DESIGN: a program that links statically against musl links the C
   library, mimalloc and the runtime into one image. An export fn there
   named after a function of the C library, as `write`, took the calls of
   musl and mimalloc to that function, so the program behaved otherwise
   than on macOS and Windows. Where the link takes no object of the user,
   nothing calls a function by name, and its symbol is local, with the
   same name, which llvm.used keeps as an export fn is kept. An object of
   the user may call an export, which then stays global. In a dev build
   the module that links holds the program alone, so its own export fns
   follow the rule. */
bool llvm_emit_local_export(const struct emitter *e,
                            const struct ir_function *f)
{
    return f->exported && !f->is_extern && e->o->local_exports &&
           (!e->o->one_module ||
            (f->module != NULL && strcmp(f->module, e->o->module) == 0));
}

void llvm_emit_linkage(struct emitter *e, const struct ir_function *f)
{
    bool coff = target_info(e->o->target)->format == FORMAT_COFF;

    if (llvm_emit_local_export(e, f)) {
        text_append(e->out, "internal ");
        return;
    }
    if (!f->exported && !e->o->one_module && !e->o->exports) {
        text_append(e->out, "internal ");
        return;
    }
    if (llvm_emit_link_once(e, f)) {
        text_append(e->out, "weak_odr ");
    }
    if (f->exported) {
        text_append(e->out, "dso_local ");
    } else if (e->o->one_module && !e->o->exports && !coff) {
        text_append(e->out, "hidden ");
    }
}
