#include "memcheck.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

bool memcheck_available(enum target t)
{
    return t != TARGET_WINDOWS_ARM64;
}

bool memcheck_leaks(enum target t)
{
    return target_info(t)->os != OS_WINDOWS;
}

/* The function that replaces the one of the runtime which marks a block
   as kept until exit. It hands the block to the leak checker. */
static void declare_kept(struct ir_module *m, const char *module)
{
    struct ir_function *ignore = ir_extern_add(m, MEMCHECK_IGNORE, IR_VOID,
                                               false);
    struct ir_function *f;
    struct ir_block *b;
    struct ir_operand block;
    struct ir_operand none = {IR_NONE, IR_VOID, {0}};

    ir_param_add(ignore, IR_PTR, IR_NO_AGG);
    f = ir_function_add(m, module, MEMCHECK_KEPT, IR_VOID, IR_NO_AGG);
    f->exported = true;
    block = ir_temp_op(f, ir_param_add(f, IR_PTR, IR_NO_AGG));
    b = ir_block_add(f);
    ir_call(f, b, IR_VOID, ir_func_op(ignore), &block, 1);
    ir_ret(f, b, IR_VOID, none);
}

void memcheck_declare(struct ir_module *m, const char *module, bool links,
                      enum target t)
{
    static const char *const names[2] = {MEMCHECK_LOAD, MEMCHECK_STORE};
    uint32_t index[2];
    size_t i;

    for (i = 0; i < 2; i++) {
        struct ir_function *f = ir_extern_add(m, names[i], IR_VOID, false);
        ir_param_add(f, IR_PTR, IR_NO_AGG);
        ir_param_add(f, IR_I64, IR_NO_AGG);
        index[i] = f->index;
    }
    m->memory_checks = true;
    m->memcheck_load = index[0];
    m->memcheck_store = index[1];
    /* DESIGN: the leak check of AddressSanitizer is off by default on
       macOS, and a report there ends the program with SIGABRT. The
       program turns the one on and the other off through the hook that
       the runtime reads its options from. The hook is a function of the module that
       links, so a program defines it once. Windows gets neither the hook
       nor the marking, since its runtime has no leak check. */
    if (links && memcheck_leaks(t)) {
        declare_kept(m, module);
        static const char options[] = MEMCHECK_OPTIONS;
        struct ir_global *g =
            ir_global_add(m, module, "memory_check_options",
                          (const uint8_t *)options, sizeof options, 1);
        struct ir_function *f = ir_function_add(m, module,
                                                MEMCHECK_OPTIONS_HOOK, IR_PTR,
                                                IR_NO_AGG);
        f->exported = true;
        struct ir_block *b = ir_block_add(f);
        ir_ret(f, b, IR_PTR, ir_temp_op(f, ir_addr(f, b, ir_global_op(g))));
    }
}

/* Whether each temporary of f holds an address in its frame or in a
   global. Each write of such a temporary is a slot, the address of a
   global, a copy of such an address or an offset from one.
   An access there needs no check. Neither the frame of a function nor a
   global carries poisoned bytes. */
static bool *frame_addresses(const struct ir_function *f)
{
    bool *frame = alloc_zeroed(f->temp_count + 1, sizeof *frame);
    bool changed = true;
    size_t b;
    size_t i;

    for (b = 0; b < f->block_count; b++) {
        for (i = 0; i < f->blocks[b]->count; i++) {
            const struct ir_inst *inst = &f->blocks[b]->insts[i];
            if (inst->result != IR_NO_RESULT) {
                frame[inst->result] = true;
            }
        }
    }
    while (changed) {
        changed = false;
        for (b = 0; b < f->block_count; b++) {
            for (i = 0; i < f->blocks[b]->count; i++) {
                const struct ir_inst *inst = &f->blocks[b]->insts[i];
                bool from_frame =
                    inst->op == IR_SLOT ||
                    (inst->op == IR_ADDR && inst->a.kind == IR_GLOBAL) ||
                    ((inst->op == IR_COPY || inst->op == IR_PTRADD) &&
                     inst->a.kind == IR_TEMP && frame[inst->a.as.temp]);
                if (inst->result != IR_NO_RESULT && frame[inst->result] &&
                    !from_frame) {
                    frame[inst->result] = false;
                    changed = true;
                }
            }
        }
    }
    return frame;
}

/* The checks of one function: where they go and what they know. */
struct checker {
    const struct ir_module *m;
    struct layouts *layouts;
    const bool *frame;
    struct ir_inst *out;
    size_t count;
};

/* Append the check of size bytes at address before an access on line. An
   address in the frame or of a global needs none. */
static void check(struct checker *c, bool store, struct ir_operand address,
                  uint64_t size, uint32_t line)
{
    struct ir_inst *inst;

    if (size == 0 || address.kind == IR_GLOBAL ||
        (address.kind == IR_TEMP && c->frame[address.as.temp])) {
        return;
    }
    inst = &c->out[c->count++];
    memset(inst, 0, sizeof *inst);
    inst->op = IR_CALL;
    inst->type = IR_VOID;
    inst->result = IR_NO_RESULT;
    /* DESIGN: a check takes the line of the access it guards, so the
       frame of a report names the line of the statement. */
    inst->line = line;
    inst->a.kind = IR_FUNC;
    inst->a.type = IR_PTR;
    inst->a.as.index = store ? c->m->memcheck_store : c->m->memcheck_load;
    inst->arg_count = 2;
    inst->args = alloc_zeroed(2, sizeof *inst->args);
    inst->args[0] = address;
    inst->args[1] = ir_int_op(IR_I64, size);
}

/* The bytes of the lanes of a simd operation: a value of the aggregate
   of, or a mask, one byte per lane. */
static uint64_t lanes(struct checker *c, const struct ir_inst *inst, bool mask)
{
    const struct ir_aggtype *agg = c->m->aggs[inst->of.agg];

    return agg->field_count *
           (mask ? 1 : layout_size(c->layouts, ir_scalar(inst->type)));
}

static bool is_comparison(uint32_t op)
{
    return (op >= IR_EQ && op <= IR_UGE) || (op >= IR_FEQ && op <= IR_FGE);
}

/* The checks of inst, a load or a store or a simd operation on memory. A
   simd comparison writes a mask, and a fold with IR_OR or IR_AND reads
   one. */
static void check_inst(struct checker *c, const struct ir_inst *inst)
{
    uint32_t line = inst->line;
    bool cmp = is_comparison(inst->field);

    switch (inst->op) {
    case IR_LOAD:
        check(c, false, inst->a,
              layout_size(c->layouts, ir_scalar(inst->type)), line);
        break;
    case IR_STORE:
        check(c, true, inst->b,
              layout_size(c->layouts, ir_scalar(inst->type)), line);
        break;
    case IR_MEMCOPY:
        check(c, false, inst->b, layout_size(c->layouts, inst->of), line);
        check(c, true, inst->a, layout_size(c->layouts, inst->of), line);
        break;
    case IR_VBINARY:
        check(c, false, inst->b, lanes(c, inst, false), line);
        check(c, false, inst->c, lanes(c, inst, false), line);
        check(c, true, inst->a, lanes(c, inst, cmp), line);
        break;
    case IR_VUNARY:
    case IR_VSHUFFLE:
        check(c, false, inst->b, lanes(c, inst, false), line);
        check(c, true, inst->a, lanes(c, inst, false), line);
        break;
    case IR_VSPLAT:
        check(c, true, inst->a, lanes(c, inst, false), line);
        break;
    case IR_VSELECT:
        check(c, false, inst->b, lanes(c, inst, true), line);
        check(c, false, inst->c, lanes(c, inst, false), line);
        check(c, false, inst->args[0], lanes(c, inst, false), line);
        check(c, true, inst->a, lanes(c, inst, false), line);
        break;
    case IR_VREDUCE:
        check(c, false, inst->a,
              lanes(c, inst, inst->field == IR_OR || inst->field == IR_AND),
              line);
        break;
    default:
        break;
    }
}

void memcheck_function(struct ir_module *m, struct ir_function *f,
                       struct layouts *l)
{
    struct checker c;
    size_t b;
    size_t i;

    c.m = m;
    c.layouts = l;
    c.frame = frame_addresses(f);
    for (b = 0; b < f->block_count; b++) {
        struct ir_block *block = f->blocks[b];
        /* An instruction takes four checks at most, a vselect. */
        size_t capacity = alloc_product(block->count, 5) + 1;
        c.out = alloc_zeroed(capacity, sizeof *c.out);
        c.count = 0;
        for (i = 0; i < block->count; i++) {
            check_inst(&c, &block->insts[i]);
            c.out[c.count++] = block->insts[i];
        }
        free(block->insts);
        block->insts = c.out;
        block->count = c.count;
        block->capacity = capacity;
    }
    free((void *)c.frame);
}
