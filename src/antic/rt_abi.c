#include "rt_abi.h"

#include <stdlib.h>

/* The IR type of one (IR type, C type) pair of a row. */
#define RT_IR(ir, c) IR_##ir

#define RT_SIGNATURE_ROW(id, name, ...)                                    \
    {#name, RT_COUNT(__VA_ARGS__), {RT_EACH(RT_IR, __VA_ARGS__)}},

static const struct rt_signature signatures[RT_FUNCTION_COUNT] = {
    RT_FUNCTIONS(RT_SIGNATURE_ROW)
};

const struct rt_signature *rt_signature(enum rt_function f)
{
    return &signatures[f];
}

const char *rt_name(enum rt_function f)
{
    return signatures[f].name;
}

/* One item of a record: the member of the C struct and its IR type. */
struct rt_item {
    const char *name;
    enum ir_type type;
};

struct rt_record_form {
    const char *name;
    const struct rt_item *items;
    size_t count;
};

#define RT_ITEM_ROW(id, member, type) {#member, IR_##type},
#define RT_ITEMS(id, ir_name, c_struct, list, count)                       \
    static const struct rt_item c_struct##_items[count] = {                \
        list(RT_ITEM_ROW)};
RT_RECORDS(RT_ITEMS)

#define RT_RECORD_ROW(id, ir_name, c_struct, list, count)                  \
    {ir_name, c_struct##_items, count},
static const struct rt_record_form records[RT_RECORD_COUNT] = {
    RT_RECORDS(RT_RECORD_ROW)
};

const char *rt_record_name(enum rt_record r)
{
    return records[r].name;
}

size_t rt_record_items(enum rt_record r)
{
    return records[r].count;
}

uint32_t rt_record_agg(struct ir_module *m, enum rt_record r)
{
    const struct rt_record_form *form = &records[r];
    struct ir_field *fields;
    uint32_t agg = ir_agg_find(m, form->name);
    size_t i;

    if (agg != IR_NO_AGG) {
        return agg;
    }
    fields = ir_alloc(form->count, sizeof *fields);
    for (i = 0; i < form->count; i++) {
        fields[i].name = form->items[i].name;
        fields[i].type = ir_scalar(form->items[i].type);
    }
    agg = ir_struct_add(m, IR_AGG_STRUCT, form->name, fields, form->count,
                        false, 0);
    free(fields);
    return agg;
}
