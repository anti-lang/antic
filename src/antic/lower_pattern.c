/* Pattern literals in lowering. A `re"..."` literal is a global that the
   constructor of its module compiles before `main`. */

#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: a pattern literal is a global of the module that holds its
   Regex, one per distinct pattern and mode. The global starts empty, and the
   function IR_PATTERNS_START of the module, which runs before main, fills it
   through the runtime. A use of the literal reads the global, so no
   pattern is compiled lazily and no flag guards a first use. */
struct ir_operand lower_pattern(struct lowerer *l, const struct expr *e)
{
    static const uint8_t empty[8] = {0};
    const struct ir_global *text = lower_literal_global(l, &e->as.text);
    struct ir_global *slot;
    char name[32];
    size_t i;

    bool bytes = types_is_byte_regex(e->type);

    for (i = 0; i < l->regex_count; i++) {
        if (l->regex_literals[i].text == text->index &&
            l->regex_literals[i].bytes == bytes) {
            slot = l->m->globals[l->regex_literals[i].slot];
            return lower_temp(l, ir_addr(l->f, l->b, ir_global_op(slot)));
        }
    }
    snprintf(name, sizeof name, "pattern.%zu", l->regex_count);
    /* The one field of a Regex is a pointer, 8 bytes on every target. */
    slot = ir_global_add(l->m, l->module_name, name, empty, sizeof empty, 8);
    slot->mutable = true;
    l->regex_literals = alloc_grow(l->regex_literals, &l->regex_capacity,
                                   l->regex_count, sizeof *l->regex_literals);
    l->regex_literals[l->regex_count].text = text->index;
    l->regex_literals[l->regex_count].length = (int64_t)e->as.text.length;
    l->regex_literals[l->regex_count].slot = slot->index;
    l->regex_literals[l->regex_count].bytes = bytes;
    l->regex_count++;
    return lower_temp(l, ir_addr(l->f, l->b, ir_global_op(slot)));
}

/* The function IR_PATTERNS_START of the module, which compiles each pattern
   literal into its global. The back end makes it a constructor of the
   object, so it runs before main in a program and when a library loads. */
void lower_patterns_start(struct lowerer *l)
{
    struct ir_operand args[2];
    struct ir_operand made;
    size_t i;

    if (l->regex_count == 0) {
        return;
    }
    l->f = ir_function_add(l->m, l->module_name, IR_PATTERNS_START, IR_VOID,
                           IR_NO_AGG);
    l->b = ir_block_add(l->f);
    for (i = 0; i < l->regex_count; i++) {
        const struct lower_pattern *p = &l->regex_literals[i];
        args[0] = lower_temp(l, ir_addr(l->f, l->b,
                                        ir_global_op(l->m->globals[p->text])));
        args[1] = ir_int_op(IR_I64, (uint64_t)p->length);
        made = lower_rt_call(l,
                             p->bytes ? RT_FN_REGEX_LITERAL_BYTES
                                      : RT_FN_REGEX_LITERAL,
                             args);
        ir_store(l->f, l->b, IR_PTR, made,
                 lower_temp(l, ir_addr(l->f, l->b,
                                       ir_global_op(l->m->globals[p->slot]))));
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
    l->f = NULL;
    l->b = NULL;
}
