/* The libraries a program holds open, and the count of the objects of
   each.

   DESIGN: the state stands apart from the loader, because every hook
   site of every program reaches it. A program that loads nothing pays
   one load and one compare per object. It links no loader, no digest
   and no reader of an index. */
#include "plugin.h"

#include "atomic.h"
#include "platform.h"
#include "std.h"

/* DESIGN: any thread may load and unload a library. Any thread makes
   objects, and the hooks of each look up its library. One lock guards
   the slots. Load and unload write a slot under it. The hooks and the
   lookup of a class by name read the slots under it. */
static struct anti_plugin loaded[ANTI_PLUGIN_MAX];
/* The open libraries. Every hook site reads it, so it is the first
   thing a count asks and the only cost of a program with none. It is
   atomic and stands outside ANTI_RT_LOCK_PLUGINS, so a program with no
   library open never takes the lock. */
static int64_t open_count;

/* DESIGN: the loader of the platform holds a lock of its own while it
   answers anti_rt_plugin_image, and while it runs the constructors of a
   library. A constructor that makes an object takes this lock inside
   that one. A thread that held this lock and asked for an image would
   wait for the other in the opposite order. The flag says whether the
   thread holds this lock, and anti_rt_plugin_image refuses to run under
   it. */
static _Thread_local int8_t holding;

void anti_rt_plugin_hold(void)
{
    anti_rt_lock_hold(ANTI_RT_LOCK_PLUGINS);
    holding = 1;
}

void anti_rt_plugin_release(void)
{
    holding = 0;
    anti_rt_lock_release(ANTI_RT_LOCK_PLUGINS);
}

int64_t anti_rt_plugin_open(void)
{
    return anti_rt_atomic_load(&open_count, (int64_t)sizeof open_count);
}

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
    if (holding != 0) {
        anti_rt_fail_abort("anti: the image of an address was asked for "
                           "under the lock of the loaded libraries");
    }
    return anti_rt_library_image(address);
}

/* Add delta to the count of live objects of the library the table of a
   class belongs to. A class of the program counts nowhere. The image is
   found before the lock, because the loader of the platform holds a lock
   of its own while it answers. */
static void count(const void *table, int64_t delta)
{
    const void *base;
    int64_t i;

    if (table == NULL || anti_rt_plugin_open() == 0) {
        return;
    }
    base = anti_rt_plugin_image(table);
    anti_rt_plugin_hold();
    for (i = 0; i < ANTI_PLUGIN_MAX; i++) {
        if (loaded[i].used != 0 && loaded[i].base == base) {
            anti_rt_atomic_add(&loaded[i].live,
                               (int64_t)sizeof loaded[i].live, delta);
            break;
        }
    }
    anti_rt_plugin_release();
}

void anti_rt_plugin_created(const void *table)
{
    count(table, 1);
}

void anti_rt_plugin_destroyed(const void *table)
{
    count(table, -1);
}
