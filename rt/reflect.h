#ifndef ANTI_RT_REFLECT_H
#define ANTI_RT_REFLECT_H

#include <stdint.h>

#include "object.h"
#include "std.h"

/* The kinds a field record carries, which are the IR types of src/ir.h.
   A unit test pins the two together. */
#define ANTI_KIND_I8 1
#define ANTI_KIND_I16 2
#define ANTI_KIND_I32 3
#define ANTI_KIND_I64 4
#define ANTI_KIND_PTR 7

struct anti_text anti_rt_reflect_class_name(const struct anti_descriptor *d);
const struct anti_descriptor *anti_rt_reflect_parent(
    const struct anti_descriptor *d);
int64_t anti_rt_reflect_field_count(const struct anti_descriptor *d);
struct anti_text anti_rt_reflect_field_name(const struct anti_descriptor *d,
                                            int64_t index);
int64_t anti_rt_reflect_field_offset(const struct anti_descriptor *d,
                                     int64_t index);
int64_t anti_rt_reflect_field_kind(const struct anti_descriptor *d,
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
int64_t anti_rt_reflect_get(void *object, const struct anti_descriptor *d,
                            int64_t index);
void anti_rt_reflect_set(void *object, const struct anti_descriptor *d,
                         int64_t index, int64_t value);

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
   Gives 0 and leaves result alone when the call cannot be made. */
int8_t anti_rt_reflect_call(void *object, int64_t index, const void *args,
                            int64_t count, void *result);

#endif
