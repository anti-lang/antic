/* Loading a shared library that `provides` interfaces: the table the
   library carries and what a host does with it. */
#ifndef ANTI_RT_PLUGIN_H
#define ANTI_RT_PLUGIN_H

#include <stdint.h>

#include "object.h"
#include "registry.h"
#include "std.h"

/* DESIGN: one entry per `provides Interface as Class;` line of a
   library. The interface descriptor is the host's, because the library
   is bound against the host at load, and the class descriptor is the
   library's. `init` prepares an object of the class as a literal of it
   would, and `offset` is where the interface sub-object sits inside
   that object. The loader allocates the size the class descriptor
   gives, calls `init` and moves the pointer by the offset. */
struct anti_provides {
    /* windows.h defines `interface`, so the path of the interface is
       named for what it is. */
    const unsigned char *path;
    int64_t path_length;
    const struct anti_descriptor *descriptor;
    const struct anti_descriptor *class_of;
    void (*init)(void *object);
    int64_t offset;
    int64_t flags;              /* of the class, as the registry counts */
    /* DESIGN: what the interface looked like where the library was
       built, copied into the library's own image. The descriptor above
       is the host's once the library is bound against it. The two
       versions are then these numbers against that descriptor's. */
    const int64_t *chain;       /* one hash per prefix of its table */
    int64_t chain_length;
    int64_t fields;             /* the fields the interface declared */
    int64_t size;               /* the bytes an object of it took */
    const unsigned char *built; /* the version of the interface's package */
    int64_t built_length;
};

/* The slots of one abstract class that the calls of the program reach,
   bit k of byte k / 8 for slot k. */
struct anti_slots {
    const struct anti_descriptor *descriptor;
    int64_t slot_count;         /* the highest slot reached, plus one */
    const unsigned char *bits;
};

/* DESIGN: the pass over the whole program writes anti_rt_slots into a
   program that loads a library or injects an interface. The table is
   empty where its calls reach no slot at all. reflect says that the
   program calls through `reflect.call`, which takes a slot at run time
   and may therefore reach any of them. */
struct anti_slot_table {
    int64_t count;
    const struct anti_slots *interfaces;
    int64_t reflect;
};

extern const struct anti_slot_table anti_rt_slots;

/* The bitmap of the slots the program's calls reach through d, or NULL
   where they reach none. */
const struct anti_slots *anti_rt_plugin_slots(const struct anti_descriptor *d);

/* The table a plugin exports as `anti_rt_provides`, with the version of
   the runtime it was built against and the classes it brings. */
struct anti_provided {
    int64_t count;
    const struct anti_provides *entries;
    const unsigned char *version;
    int64_t version_length;
    int64_t class_count;
    const struct anti_class *classes;
};

/* The libraries a program may hold open at once. */
enum { ANTI_PLUGIN_MAX = 16 };

/* DESIGN: the slots of the open libraries, the count of them and the
   two hooks that count their objects stand in src/rt/loaded.c. The hook
   sites of every program reach them. A program that loads nothing
   links no loader, no digest and no reader of an index. */

/* One open library. `live` counts the objects of its classes that the
   program has not deleted, which `unload` refuses to leave behind. */
struct anti_plugin {
    void *handle;
    const void *base;           /* the image the library was mapped at */
    const struct anti_provided *table;
    struct anti_registry classes;
    /* One table per entry, or NULL. The loader writes one where the
       program may reach a slot through `reflect.call` that the library
       does not carry, and fills that slot with a stub. */
    const void ***stubbed;
    int64_t live;
    int64_t used;
};

/* The slot at index, or NULL past the last. */
struct anti_plugin *anti_rt_plugin_at(int64_t index);

/* The image an address lies in, which tells a library from the
   program. */
const void *anti_rt_plugin_image(const void *address);

/* Count a library that was opened, or one that was closed. */
void anti_rt_plugin_opened(void);
void anti_rt_plugin_closed(void);

/* The classes of the open library in slot index, or NULL for a slot
   that holds none. `reflect.new` walks them after the program's own. */
const struct anti_registry *anti_rt_plugin_registry(int64_t index);

/* Open the library at path, check it against the host and take its
   classes into the registry. Gives the handle of the load, or NULL,
   and a failure leaves its reason in anti_rt_plugin_message. */
void *anti_rt_plugin_load(const unsigned char *path, int64_t length);

/* The reason the last call failed, empty after one that did not. The
   bytes belong to the runtime and stand until the next failure. */
struct anti_text anti_rt_plugin_message(void);

/* An object of the class the library provides for the interface, as a
   pointer to its interface sub-object, or NULL. */
void *anti_rt_plugin_instance(void *handle, const struct anti_descriptor *d);

/* Whether the class the library provides for the interface carries a
   public function of that name. */
int8_t anti_rt_plugin_supports(void *handle, const struct anti_descriptor *d,
                               const unsigned char *name, int64_t length);

/* The objects of the library that are alive, counted through the
   `created` and `destroyed` hooks. */
int64_t anti_rt_plugin_live(void *handle);

/* Close the library. Gives 0 while an object of it is alive, and the
   library stays open. */
int8_t anti_rt_plugin_unload(void *handle);

/* DESIGN: the `created` and `destroyed` hooks count the objects of each
   loaded library, so `unload` refuses while one is alive. The check
   costs one load and one compare while no library is open, because the
   count of them is the first thing it reads. */
void anti_rt_plugin_created(const void *table);
void anti_rt_plugin_destroyed(const void *table);

/* The provider of the interface named by path. It comes from the
   library at library, or from the first library of the search
   directories that provides it when library is empty. NULL when there
   is none, with the reason in anti_rt_plugin_message. dirs is the
   `plugins` key, one directory per `:` separated part. */
void *anti_rt_plugin_provider(const unsigned char *path, int64_t path_length,
                              const unsigned char *library,
                              int64_t library_length, const char *dirs);

#endif
