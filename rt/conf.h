/* The runtime configuration: the keys of [runtime], the layer each value
   came from and the options under --anti. that name them. */
#ifndef ANTI_RT_CONF_H
#define ANTI_RT_CONF_H

#include <stdint.h>

#include "object.h"
#include "std.h"

/* DESIGN: the link step writes the injectable interfaces of the
   program, one entry per interface. Each names the class and the field
   that need it, and says whether `inject final` keeps the run-time
   configuration from replacing the provider. The table stands in every
   program, empty where nothing injects, because the runtime reads it
   before `main`. `slot` is the pointer that holds the provider. */
struct anti_injectable {
    const unsigned char *name;
    const unsigned char *owner;     /* `module.Class` of the field */
    const unsigned char *field;
    int64_t final;
    void **slot;
    /* DESIGN: `plugin:<path>` and `discover` of the manifest name a
       library rather than a function of the program. The slot is then
       empty at the link, and the runtime fills it before `main`. */
    const unsigned char *library;   /* the path, or NULL for discovery */
    int64_t library_length;
    int64_t discover;               /* the provider names a library */
    /* A provider that comes from a library is an object, and a slot
       holds a function. The runtime stores the object in the holder and
       puts the thunk, which gives it, in the slot. */
    void **holder;
    void *thunk;
    /* The descriptor of the interface, which `--anti.inspect` reads for
       its version and the slots the program reaches. */
    const struct anti_descriptor *descriptor;
};

struct anti_injectables {
    int64_t count;
    const struct anti_injectable *interfaces;
};

extern const struct anti_injectables anti_rt_injectable;

/* Take one argument under --anti., its name without the prefix and the
   value after the `=`, which is NULL when the argument carried none.
   A name or a value the runtime does not know ends the program with
   status 70. */
void anti_rt_conf_option(const char *name, int64_t length, const char *value);

/* After the option pass: answer --anti.help, read the file that
   --anti.conf or ANTI_CONF names, and answer --anti.inspect. Each of
   the two options ends the program with status 0. */
void anti_rt_conf_start(void);

/* Read the file the program names, with its includes, unless the
   command line or the environment named one already. It is the
   `rt.configure(path)` of anti.runtime. */
void anti_rt_conf_configure(const unsigned char *path, int64_t length);

/* The effective value of a key of [runtime], empty when no layer set
   one and the program runs as it was built. */
struct anti_text anti_rt_conf_get(const unsigned char *key, int64_t length);

#endif
