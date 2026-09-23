#include "mach.h"

#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "select.h"

struct mach_inst *mach_append(struct mach_block *b)
{
    struct mach_inst *inst;

    b->insts = ir_grow(b->insts, &b->capacity, b->count, sizeof *b->insts);
    inst = &b->insts[b->count++];
    memset(inst, 0, sizeof *inst);
    return inst;
}

/* Slots and virtual registers are 32-bit indices in an operand. A
   function that needs more than that many ends the run as an allocation
   failure does. */
uint32_t mach_slot_add(struct mach_function *f, uint64_t size,
                       uint64_t align)
{
    if (f->slot_count >= UINT32_MAX) {
        ir_out_of_memory();
    }
    f->slots = ir_grow(f->slots, &f->slot_capacity, f->slot_count,
                       sizeof *f->slots);
    f->slots[f->slot_count].size = size;
    f->slots[f->slot_count].align = align;
    f->slots[f->slot_count].offset = 0;
    return (uint32_t)f->slot_count++;
}

uint32_t mach_vreg_add(struct mach_function *f, bool fp)
{
    if (f->vreg_count >= UINT32_MAX) {
        ir_out_of_memory();
    }
    f->fp = ir_grow(f->fp, &f->fp_capacity, f->vreg_count, sizeof *f->fp);
    f->fp[f->vreg_count] = fp;
    return f->vreg_count++;
}

void mach_function_free(struct mach_function *f)
{
    size_t i;

    for (i = 0; i < f->block_count; i++) {
        free(f->blocks[i].insts);
    }
    free(f->blocks);
    free(f->slots);
    free(f->fp);
    f->fp = NULL;
    f->blocks = NULL;
    f->block_count = 0;
    f->slots = NULL;
    f->slot_count = 0;
}

void mach_print(struct text *out, const struct target_desc *target,
                enum cpu_level cpu, const struct ir_module *m,
                const struct mach_function *f)
{
    size_t b;
    size_t i;

    text_appendf(out, "%s.%s:\n", f->ir->module, f->ir->name);
    for (b = 0; b < f->block_count; b++) {
        text_appendf(out, "b%zu:\n", b);
        for (i = 0; i < f->blocks[b].count; i++) {
            text_append(out, "    ");
            target->print(out, cpu, m, &f->blocks[b].insts[i], NULL);
            text_append(out, "\n");
        }
    }
}

void mach_symbol(struct text *out, const struct ir_module *m,
                 const struct names *names, const struct mach_operand *o)
{
    const struct ir_function *f;
    const struct ir_global *g;

    switch (o->kind) {
    case MACH_BLOCK:
        if (names == NULL) {
            text_appendf(out, "b%" PRId64, o->value);
        } else {
            block_label(out, names->target, names->function,
                        (size_t)o->value);
        }
        break;
    case MACH_FUNC:
        f = m->functions[o->value];
        if (names == NULL) {
            text_appendf(out, "%s%s%s", f->module != NULL ? f->module : "",
                         f->module != NULL ? "." : "", f->name);
        } else {
            mach_function_symbol(out, names->target, f);
        }
        break;
    case MACH_GLOBAL:
        g = m->globals[o->value];
        if (names == NULL) {
            text_appendf(out, "%s.%s", g->module, g->name);
        } else if (g->exported) {
            c_symbol(out, names->target, g->name);
        } else {
            mangle(out, names->target, g->module, g->name);
        }
        break;
    case MACH_NAME:
        if (names == NULL) {
            text_append(out, o->name);
        } else {
            c_symbol(out, names->target, o->name);
        }
        break;
    default:
        break;
    }
}

void mach_function_symbol(struct text *out, enum target t,
                          const struct ir_function *f)
{
    if (f->module == NULL || f->exported) {
        c_symbol(out, t, f->name);
    } else {
        mangle(out, t, f->module, f->name);
    }
}
