/* reflect.call, which reads the trampolines of a program. */
#include <string.h>

#include "reflect.h"

int8_t anti_rt_reflect_call(void *object, int64_t index, const void *args,
                            int64_t count, void *result)
{
    struct anti_object *o = anti_rt_object_of(object);
    const struct anti_descriptor *d = anti_rt_descriptor(o);
    const struct anti_function *f;
    const void *entry;
    int64_t i;

    if (d == NULL || d->functions == NULL || index < 0 ||
        index >= d->function_count) {
        return 0;
    }
    f = &d->functions[index];
    entry = o->table[f->slot];
    if (f->signature == NULL || entry == NULL) {
        return 0;
    }
    for (i = 0; i < anti_rt_trampolines.count; i++) {
        const struct anti_trampoline *t = &anti_rt_trampolines.items[i];
        if (strcmp((const char *)t->signature,
                   (const char *)f->signature) == 0) {
            return t->call(entry, o, args, count, result);
        }
    }
    return 0;
}
