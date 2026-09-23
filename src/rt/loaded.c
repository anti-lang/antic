/* The libraries a program holds open, and the count of the objects of
   each.

   DESIGN: the state stands apart from the loader, because every hook
   site of every program reaches it. A program that loads nothing pays
   one load and one compare per object. It links no loader, no digest
   and no reader of an index. */
/* dladdr sits behind a feature macro, which the two systems spell
   differently. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include "plugin.h"

#include "atomic.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static struct anti_plugin loaded[ANTI_PLUGIN_MAX];
/* The open libraries. Every hook site reads it, so it is the first
   thing a count asks and the only cost of a program with none. */
static int64_t open_count;

struct anti_plugin *anti_rt_plugin_at(int64_t index)
{
    return index < 0 || index >= ANTI_PLUGIN_MAX ? NULL : &loaded[index];
}

void anti_rt_plugin_opened(void)
{
    anti_rt_atomic_add(&open_count, (int64_t)sizeof open_count, 1);
}

void anti_rt_plugin_closed(void)
{
    anti_rt_atomic_sub(&open_count, (int64_t)sizeof open_count, 1);
}

const struct anti_registry *anti_rt_plugin_registry(int64_t index)
{
    if (index < 0 || index >= ANTI_PLUGIN_MAX || loaded[index].used == 0 ||
        loaded[index].classes.count == 0) {
        return NULL;
    }
    return &loaded[index].classes;
}

/* DESIGN: the image an address lies in tells the host which library an
   object came from. A table of a class of a loaded library lies in that
   library, and one of a class of the program lies in the program. The
   count of live objects rests on nothing else. */
const void *anti_rt_plugin_image(const void *address)
{
#if defined(_WIN32)
    HMODULE module = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)address, &module)) {
        return NULL;
    }
    return module;
#else
    Dl_info info;
    if (dladdr(address, &info) == 0) {
        return NULL;
    }
    return info.dli_fbase;
#endif
}

/* The library the table of a class belongs to, or NULL for one of the
   program. */
static struct anti_plugin *owner(const void *table)
{
    const void *base;
    int64_t i;

    if (table == NULL ||
        anti_rt_atomic_load(&open_count, (int64_t)sizeof open_count) == 0) {
        return NULL;
    }
    base = anti_rt_plugin_image(table);
    for (i = 0; i < ANTI_PLUGIN_MAX; i++) {
        if (loaded[i].used != 0 && loaded[i].base == base) {
            return &loaded[i];
        }
    }
    return NULL;
}

void anti_rt_plugin_created(const void *table)
{
    struct anti_plugin *p = owner(table);

    if (p != NULL) {
        anti_rt_atomic_add(&p->live, (int64_t)sizeof p->live, 1);
    }
}

void anti_rt_plugin_destroyed(const void *table)
{
    struct anti_plugin *p = owner(table);

    if (p != NULL) {
        anti_rt_atomic_sub(&p->live, (int64_t)sizeof p->live, 1);
    }
}
