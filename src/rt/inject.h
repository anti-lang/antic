/* The start of the injections: the interfaces a program injects, the
   lines of [injections] and of --anti.inject that name a library for
   one, and the providers the loader takes from those libraries before
   main. src/rt/conf.c reads the lines and hands each to this file. */
#ifndef ANTI_RT_INJECT_H
#define ANTI_RT_INJECT_H

#include <stddef.h>
#include <stdint.h>

#include "object.h"

/* The slot of one injectable interface, which every site calls through.
   It holds the function that gives the provider, or NULL until the
   runtime fills it. */
struct anti_inject_slot {
    void *provider;
};

/* DESIGN: the link step writes the injectable interfaces of the
   program, one entry per interface. Each names the class and the field
   that need it, and says whether `inject final` keeps the run-time
   configuration from replacing the provider. The table stands in every
   program, empty where nothing injects, because the runtime reads it
   before `main`. `slot` is the slot that holds the provider. */
struct anti_injectable {
    const unsigned char *name;
    const unsigned char *owner;     /* `module.Class` of the field */
    const unsigned char *field;
    int64_t final;
    struct anti_inject_slot *slot;
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

/* Take a line that names library as the provider of the interface of
   the length bytes at name. where names the line in a message. layer is
   the layer of the configuration the line stands in, and a line of a
   higher layer wins. An interface the program does not inject, one that
   is `inject final` and a seventeenth interface end the program with
   status 70. */
void anti_rt_inject_line(const char *name, size_t length, const char *library,
                         const char *where, int layer);

/* Fill every interface whose provider comes from a library: the line of
   the highest layer, else the `plugin:` or `discover` of the build. dirs
   holds the directories of the key `plugins`, which discovery searches.
   A library that cannot give the provider ends the program with status
   70. */
void anti_rt_inject_start(const char *dirs);

/* Print the injectable interfaces and the libraries the lines named, the
   part of --anti.inspect that follows the keys. */
void anti_rt_inject_inspect(void);

#endif
