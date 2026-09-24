/* The registry of the classes of a program, which the link step writes,
   and the two functions that read it. */
#ifndef ANTI_RT_REGISTRY_H
#define ANTI_RT_REGISTRY_H

#include <stdint.h>

#include "object.h"
#include "std.h"

/* The flag of a class whose construct takes arguments. */
#define ANTI_CLASS_ARGS 1

/* The flag of a class with an inline class field that no default fills.
   Only a literal that names the field makes one, so a zero table never
   sits in an inline field of an object the registry built. */
#define ANTI_CLASS_REQUIRED 2

/* One class the program may build by name. init sets the tables of an
   object of the class, writes its defaults and runs construct when it
   takes no arguments. The module path names the module that declares the
   class. */
struct anti_class {
    const struct anti_descriptor *descriptor;
    void (*init)(void *object);
    const unsigned char *module;
    int64_t module_length;
    int64_t flags;
};

/* DESIGN: the compiler writes the registry into the program as
   `anti_rt_registry` when the program reaches a function declared here.
   A program that reaches none gets no registry and does not link
   registry.c, so the symbol is never missing. */
struct anti_registry {
    int64_t count;
    const struct anti_class *classes;
};

extern const struct anti_registry anti_rt_registry;

/* The class named `Class` or `module.Class`, or NULL when the registry
   holds none or holds two of the bare name. */
const struct anti_class *anti_rt_registry_find(const unsigned char *name,
                                               int64_t length);

/* A new object of the class named, on the heap of the C library, or
   NULL. The caller frees it with anti_rt_delete, which `delete`
   calls. */
void *anti_rt_reflect_new(const unsigned char *name, int64_t length);

/* A new object from the JSON that the default serialize writes. The
   object, every string and every owned object come from the
   anti.mem.Allocator from. NULL when the text is not such an object of a
   class of the program, and every block it took is back with from. */
void *anti_lang_Object_deserialize(struct anti_text input,
                                   struct anti_object *from);

#endif
