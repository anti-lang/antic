/* What anti.reflect reads. A descriptor is read-only data of the program,
   so every function here is a walk of it.

   DESIGN: the runtime holds the walk because a descriptor is C data with
   C layout. Anti reads the results as plain values, and nothing of the
   layout reaches the language. */
#include <stdint.h>
#include <string.h>

#include "object.h"
#include "reflect.h"

struct anti_text anti_rt_reflect_class_name(const struct anti_descriptor *d)
{
    struct anti_text text;

    text.ptr = d == NULL ? (const unsigned char *)"" : d->name;
    text.len = d == NULL ? 0 : d->name_length;
    return text;
}

const struct anti_descriptor *anti_rt_reflect_parent(
    const struct anti_descriptor *d)
{
    return d == NULL ? NULL : d->parent;
}

int64_t anti_rt_reflect_field_count(const struct anti_descriptor *d)
{
    return d == NULL ? 0 : d->field_count;
}

/* The record at index, or NULL past the end. */
static const struct anti_field *field_at(const struct anti_descriptor *d,
                                         int64_t index)
{
    if (d == NULL || d->fields == NULL || index < 0 ||
        index >= d->field_count) {
        return NULL;
    }
    return &d->fields[index];
}

struct anti_text anti_rt_reflect_field_name(const struct anti_descriptor *d,
                                            int64_t index)
{
    const struct anti_field *f = field_at(d, index);
    struct anti_text text;

    text.ptr = f == NULL ? (const unsigned char *)"" : f->name;
    text.len = f == NULL ? 0 : f->name_length;
    return text;
}

int64_t anti_rt_reflect_field_offset(const struct anti_descriptor *d,
                                     int64_t index)
{
    const struct anti_field *f = field_at(d, index);

    return f == NULL ? -1 : f->offset;
}

int64_t anti_rt_reflect_field_type(const struct anti_descriptor *d,
                                   int64_t index)
{
    const struct anti_field *f = field_at(d, index);

    return f == NULL ? ANTI_TYPE_NONE : f->type;
}

int64_t anti_rt_reflect_field_owned(const struct anti_descriptor *d,
                                    int64_t index)
{
    const struct anti_field *f = field_at(d, index);

    return f == NULL ? 0 : f->owned;
}

int64_t anti_rt_reflect_function_count(const struct anti_descriptor *d)
{
    return d == NULL ? 0 : d->function_count;
}

static const struct anti_function *function_at(const struct anti_descriptor *d,
                                               int64_t index)
{
    if (d == NULL || d->functions == NULL || index < 0 ||
        index >= d->function_count) {
        return NULL;
    }
    return &d->functions[index];
}

struct anti_text anti_rt_reflect_function_name(const struct anti_descriptor *d,
                                               int64_t index)
{
    const struct anti_function *f = function_at(d, index);
    struct anti_text text;

    text.ptr = f == NULL ? (const unsigned char *)"" : f->name;
    text.len = f == NULL ? 0 : f->name_length;
    return text;
}

int64_t anti_rt_reflect_function_slot(const struct anti_descriptor *d,
                                      int64_t index)
{
    const struct anti_function *f = function_at(d, index);

    return f == NULL ? -1 : f->slot;
}

int64_t anti_rt_reflect_function_params(const struct anti_descriptor *d,
                                        int64_t index)
{
    const struct anti_function *f = function_at(d, index);

    return f == NULL ? 0 : f->param_count;
}

/* The kind of Value that carries a field of the type id, or none. */
static enum anti_value_kind value_kind(int64_t type)
{
    int64_t t = anti_rt_type_scalar(type);

    switch (t) {
    case ANTI_TYPE_BOOL: return ANTI_VALUE_BOOL;
    case ANTI_TYPE_CHAR: return ANTI_VALUE_CHAR;
    case ANTI_TYPE_F32:
    case ANTI_TYPE_F64: return ANTI_VALUE_FLOAT;
    case ANTI_TYPE_STR: return ANTI_VALUE_STR;
    case ANTI_TYPE_PTR:
    case ANTI_TYPE_FN: return ANTI_VALUE_PTR;
    case ANTI_TYPE_I8:
    case ANTI_TYPE_I16:
    case ANTI_TYPE_I32:
    case ANTI_TYPE_I64:
    case ANTI_TYPE_CLONG: return ANTI_VALUE_INT;
    case ANTI_TYPE_U8:
    case ANTI_TYPE_U16:
    case ANTI_TYPE_U32:
    case ANTI_TYPE_U64:
    case ANTI_TYPE_CULONG:
    case ANTI_TYPE_CWCHAR: return ANTI_VALUE_UINT;
    default: return ANTI_VALUE_NONE;
    }
}

/* DESIGN: a Value holds an integer at 64 bits and a float as an f64, as
   reflect.call does. get widens a field to that, and set narrows the
   Value to the field's own type, keeping its low bytes. */
void anti_rt_reflect_get(void *object, const struct anti_descriptor *d,
                         int64_t index, struct anti_value *out)
{
    const struct anti_field *f = field_at(d, index);
    enum anti_value_kind kind = f == NULL ? ANTI_VALUE_NONE
                                          : value_kind(f->type);
    unsigned char *at;

    if (kind == ANTI_VALUE_NONE || object == NULL) {
        return;
    }
    at = (unsigned char *)anti_rt_object_of(object) + f->offset;
    memset(&out->data, 0, sizeof out->data);
    out->kind = (uint8_t)kind;
    switch (kind) {
    case ANTI_VALUE_BOOL:
        memcpy(&out->data.b, at, sizeof out->data.b);
        return;
    case ANTI_VALUE_CHAR:
        memcpy(&out->data.c, at, sizeof out->data.c);
        return;
    case ANTI_VALUE_FLOAT:
        if (anti_rt_type_scalar(f->type) == ANTI_TYPE_F32) {
            float narrow;
            memcpy(&narrow, at, sizeof narrow);
            out->data.f = narrow;
        } else {
            memcpy(&out->data.f, at, sizeof out->data.f);
        }
        return;
    case ANTI_VALUE_STR:
        memcpy(&out->data.s, at, sizeof out->data.s);
        return;
    case ANTI_VALUE_PTR:
        memcpy(&out->data.p, at, sizeof out->data.p);
        return;
    default:
        out->data.u = anti_rt_load_integer(at, f->type);
        return;
    }
}

int64_t anti_rt_reflect_set(void *object, const struct anti_descriptor *d,
                            int64_t index, const struct anti_value *value)
{
    const struct anti_field *f = field_at(d, index);
    enum anti_value_kind kind;
    unsigned char *at;

    if (f == NULL || object == NULL) {
        return ANTI_REFLECT_BAD_INDEX;
    }
    kind = value_kind(f->type);
    if (kind == ANTI_VALUE_NONE) {
        return ANTI_REFLECT_NOT_CARRIED;
    }
    if (value->kind != kind) {
        return ANTI_REFLECT_WRONG_KIND;
    }
    at = (unsigned char *)anti_rt_object_of(object) + f->offset;
    switch (kind) {
    case ANTI_VALUE_BOOL:
        memcpy(at, &value->data.b, sizeof value->data.b);
        break;
    case ANTI_VALUE_CHAR:
        memcpy(at, &value->data.c, sizeof value->data.c);
        break;
    case ANTI_VALUE_FLOAT:
        if (anti_rt_type_scalar(f->type) == ANTI_TYPE_F32) {
            float narrow = (float)value->data.f;
            memcpy(at, &narrow, sizeof narrow);
        } else {
            memcpy(at, &value->data.f, sizeof value->data.f);
        }
        break;
    case ANTI_VALUE_STR:
        memcpy(at, &value->data.s, sizeof value->data.s);
        break;
    case ANTI_VALUE_PTR:
        memcpy(at, &value->data.p, sizeof value->data.p);
        break;
    default:
        anti_rt_store_integer(at, f->type, value->data.u);
        break;
    }
    return ANTI_REFLECT_DONE;
}
