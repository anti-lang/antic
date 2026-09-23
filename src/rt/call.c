/* reflect.call, which reads the trampolines of a program. */
#include <string.h>

#include "reflect.h"

/* DESIGN: the reasons come in the order of the checks. The index and
   the entry come first, then a signature with a type that no Value
   carries. The count of the arguments against the parameters after self
   comes next. The trampoline checks the kind of each argument last, and
   a refusal there is a wrong kind. */
int64_t anti_rt_reflect_call(void *object, int64_t index, const void *args,
                             int64_t count, void *result)
{
    struct anti_object *o = anti_rt_object_of(object);
    const struct anti_descriptor *d = anti_rt_descriptor(o);
    const struct anti_function *f;
    const void *entry;
    int64_t i;

    if (d == NULL || d->functions == NULL || index < 0 ||
        index >= d->function_count) {
        return ANTI_REFLECT_BAD_INDEX;
    }
    f = &d->functions[index];
    entry = o->table[f->slot];
    if (entry == NULL) {
        return ANTI_REFLECT_BAD_INDEX;
    }
    if (f->signature == NULL) {
        return ANTI_REFLECT_NOT_CARRIED;
    }
    if (count != f->param_count - 1) {
        return ANTI_REFLECT_WRONG_COUNT;
    }
    for (i = 0; i < anti_rt_trampolines.count; i++) {
        const struct anti_trampoline *t = &anti_rt_trampolines.items[i];
        if (strcmp((const char *)t->signature,
                   (const char *)f->signature) == 0) {
            return t->call(entry, o, args, count, result)
                       ? ANTI_REFLECT_DONE
                       : ANTI_REFLECT_WRONG_KIND;
        }
    }
    return ANTI_REFLECT_NOT_CARRIED;
}
