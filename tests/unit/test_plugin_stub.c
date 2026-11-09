/* The stub of the loader of plugins, which fills a slot of an interface
   that a library lacks and that only `reflect.call` of the host may
   reach, by Q2 of "Answers of 2026-11-09, third set" in
   docs/decisions.md. No program reaches the slot, since `reflect.call`
   calls through the table of the object and not through the one of the
   sub-object, so this program calls the slot itself. It loads the
   library plugin_bad, whose class holds the interface as a sub-object
   at offset 16, against an interface of one function more than the
   library was built with, in a host that calls through reflection.

   The argument names one case. Each ends the process through the stub,
   and tests/runners/run_plugin_stub.cmake checks the stop and its
   message. A case that comes back prints why and ends with status 1. */

#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/plugin.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* The stand-ins of what src/rt/plugin.c calls outside itself. The host
   calls through reflection and through no interface. */

const struct anti_slot_table anti_rt_slots = {0, NULL, 1};

struct anti_text anti_rt_runtime_version(void)
{
    struct anti_text text = {(const unsigned char *)"0.0.0", 5};

    return text;
}

void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing)
{
    (void)path;
    (void)len;
    (void)writing;
    return NULL;
}

int64_t anti_rt_fs_size(void *file)
{
    (void)file;
    return -1;
}

unsigned char *anti_rt_fs_read(const char *path, int64_t *length)
{
    (void)path;
    (void)length;
    return NULL;
}

/* The interface as the host declares it: the chain of the library's
   version and one function past it, which the library does not carry. */
static const int64_t chain[1] = {1};
static const struct anti_versions versions = {chain, 1, NULL, 0};
static const struct anti_function asked = {
    (const unsigned char *)"asked", 5, 1, 0, NULL, 0
};
static const struct anti_descriptor service = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 1, &asked, (const unsigned char *)"1.0.0", 5, &versions, 0, NULL
};

/* The table of the sub-object as the library writes it, with the entry
   of the interface's version alone. */
static const struct anti_descriptor *const library_table[1] = {&service};

static int64_t sub_offset;

/* The init of the class, which writes the table of its sub-object. */
static void init(void *object)
{
    const struct anti_descriptor *const *table = library_table;

    memcpy((unsigned char *)object + sub_offset, &table, sizeof table);
}

static void *symbol(void *library, const char *name)
{
#if defined(_WIN32)
    union {
        FARPROC from;
        void *to;
    } cast;
    cast.from = GetProcAddress((HMODULE)library, name);
    return cast.to;
#else
    return dlsym(library, name);
#endif
}

/* The library, bound to the interface of the host as the loader of a
   program binds it, and the sub-object of a new object of its class.
   NULL with the reason on standard error. */
static struct anti_object *sub_object(void)
{
#if defined(_WIN32)
    void *library = LoadLibraryA(PLUGIN_BAD);
#else
    void *library = dlopen(PLUGIN_BAD, RTLD_NOW | RTLD_LOCAL);
#endif
    struct anti_provides *entry;
    struct anti_object *sub;
    void *handle;

    entry = library != NULL ? symbol(library, "anti_bad_entry") : NULL;
    if (entry == NULL) {
        fprintf(stderr, "cannot open %s\n", PLUGIN_BAD);
        return NULL;
    }
    entry->descriptor = &service;
    entry->init = init;
    sub_offset = entry->offset;
    handle = anti_rt_plugin_load((const unsigned char *)PLUGIN_BAD,
                                 (int64_t)strlen(PLUGIN_BAD));
    if (handle == NULL) {
        struct anti_text why = anti_rt_plugin_message();
        fprintf(stderr, "the load refused: %.*s\n", (int)why.len, why.ptr);
        return NULL;
    }
    sub = anti_rt_plugin_instance(handle, &service);
    if (sub == NULL) {
        fprintf(stderr, "the library built no object\n");
        return NULL;
    }
    if ((const void *)sub->table == (const void *)library_table ||
        sub->table[0] != &service) {
        fprintf(stderr, "the loader wrote no table of stubs\n");
        return NULL;
    }
    return sub;
}

/* A function of the table of an object. */
typedef void (*slot_body)(struct anti_object *self);

/* The entry at slot of the table of o as the function it holds. */
static slot_body entry_at(const struct anti_object *o, int slot)
{
    union {
        const void *entry;
        slot_body body;
    } cast;

    cast.entry = (const void *)o->table[slot];
    return cast.body;
}

int main(int argc, char **argv)
{
    struct anti_object *sub;
    struct anti_object bare = {NULL};
    /* A table with a header of zeros where the stub looks for its own. */
    static const void *foreign[4] = {NULL, NULL, NULL, &service};
    struct anti_object other;
    const char *name;

    if (argc != 2) {
        fprintf(stderr, "usage: rt_plugin_stub_tests <case>\n");
        return 2;
    }
    name = argv[1];
    sub = sub_object();
    if (sub == NULL) {
        return 1;
    }
    if (strcmp(name, "named") == 0) {
        /* The slot of `asked` through the sub-object names the class, its
           version and the interface. */
        entry_at(sub, 1)(sub);
    } else if (strcmp(name, "no_object") == 0) {
        entry_at(sub, 1)(NULL);
    } else if (strcmp(name, "no_table") == 0) {
        entry_at(sub, 1)(&bare);
    } else if (strcmp(name, "other_table") == 0) {
        other.table = (const struct anti_descriptor *const *)(const void *)
                          &foreign[3];
        entry_at(sub, 1)(&other);
    } else {
        fprintf(stderr, "no case %s\n", name);
        return 2;
    }
    fprintf(stderr, "the stub came back\n");
    return 1;
}
