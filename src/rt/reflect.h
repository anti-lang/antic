#ifndef ANTI_RT_REFLECT_H
#define ANTI_RT_REFLECT_H

#include <stdint.h>

#include "object.h"
#include "std.h"

/* The kinds of anti.reflect.ValueKind, in its order. */
enum anti_value_kind {
    ANTI_VALUE_NONE,
    ANTI_VALUE_INT,
    ANTI_VALUE_UINT,
    ANTI_VALUE_FLOAT,
    ANTI_VALUE_BOOL,
    ANTI_VALUE_CHAR,
    ANTI_VALUE_STR,
    ANTI_VALUE_PTR
};

/* The layout of anti.reflect.Value: a ValueKind, then the payload that
   the kind names. */
struct anti_value {
    uint8_t kind;
    union {
        int64_t i;
        uint64_t u;
        double f;
        int8_t b;
        uint32_t c;
        struct anti_text s;
        void *p;
    } data;
};

/* The reasons that set and call give, which anti.reflect turns into the
   codes of its errors. 0 is success. */
enum anti_reflect_reason {
    ANTI_REFLECT_DONE,
    ANTI_REFLECT_BAD_INDEX,
    ANTI_REFLECT_NOT_CARRIED,
    ANTI_REFLECT_WRONG_KIND,
    ANTI_REFLECT_WRONG_COUNT
};

struct anti_text anti_rt_reflect_class_name(const struct anti_descriptor *d);
const struct anti_descriptor *anti_rt_reflect_parent(
    const struct anti_descriptor *d);
int64_t anti_rt_reflect_field_count(const struct anti_descriptor *d);
struct anti_text anti_rt_reflect_field_name(const struct anti_descriptor *d,
                                            int64_t index);
int64_t anti_rt_reflect_field_offset(const struct anti_descriptor *d,
                                     int64_t index);
int64_t anti_rt_reflect_field_type(const struct anti_descriptor *d,
                                   int64_t index);
int64_t anti_rt_reflect_field_owned(const struct anti_descriptor *d,
                                    int64_t index);
int64_t anti_rt_reflect_function_count(const struct anti_descriptor *d);
struct anti_text anti_rt_reflect_function_name(const struct anti_descriptor *d,
                                               int64_t index);
int64_t anti_rt_reflect_function_slot(const struct anti_descriptor *d,
                                      int64_t index);
int64_t anti_rt_reflect_function_params(const struct anti_descriptor *d,
                                        int64_t index);
/* The field at index of the object as a Value in out. A field of a type
   that no Value carries, and an index out of range, leave out alone. */
void anti_rt_reflect_get(void *object, const struct anti_descriptor *d,
                         int64_t index, struct anti_value *out);
/* Write value into the field at index. Gives a reason other than done,
   and writes nothing, when it cannot. */
int64_t anti_rt_reflect_set(void *object, const struct anti_descriptor *d,
                            int64_t index, const struct anti_value *value);

/* One trampoline of the program. It checks that count and the kinds of
   the reflect.Values at args fit the signature. It then calls entry with
   object and the unpacked values, and writes the result as a Value. It
   gives 0 when the values do not fit. */
struct anti_trampoline {
    const unsigned char *signature;
    int8_t (*call)(const void *entry, void *object, const void *args,
                   int64_t count, void *result);
};

/* DESIGN: the compiler writes the table of every trampoline of the
   program as `anti_rt_trampolines` when the program reaches
   anti_rt_reflect_call. call.c alone reads it, so a program that never
   calls through reflection links no table and misses no symbol. */
struct anti_trampolines {
    int64_t count;
    const struct anti_trampoline *items;
};

extern const struct anti_trampolines anti_rt_trampolines;

/* Call the function at index of the list of the object's class through
   its table, with the reflect.Values at args. The result goes to result.
   Gives the reason, and leaves result alone, when the call cannot be
   made. */
int64_t anti_rt_reflect_call(void *object, int64_t index, const void *args,
                             int64_t count, void *result);

#endif
