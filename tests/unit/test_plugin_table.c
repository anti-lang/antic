/* The loader of the runtime against a damaged table of a plugin. The
   library plugin_bad carries a writable table. The test damages one value
   of it, loads the library and expects a refusal that names the damage,
   and then puts the value back. The sanitizer builds check that no load
   reads or writes outside what the table describes. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/plugin.h"
#include "../../src/rt/registry.h"
#include "check.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int check_failures;

/* The stand-ins of what src/rt/plugin.c calls outside itself. */

const struct anti_slot_table anti_rt_slots = {0, NULL, 0};

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

/* The interface the library provides, as the host declares it. */
static const int64_t chain[1] = {1};
static struct anti_versions versions = {
    chain, 1, (const unsigned char *)"1.0", 3
};
static const struct anti_descriptor service = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 0, NULL, (const unsigned char *)"1.0.0", 5, &versions
};

/* Bytes without a NUL after them, which a read past a length reaches. */
static const unsigned char unterminated[4] = {'9', '9', '9', '9'};

static struct anti_provided *table;
static struct anti_provides *entry;
static struct anti_descriptor *class_of;
static struct anti_class *classes;

static struct anti_provided table_good;
static struct anti_provides entry_good;
static struct anti_descriptor class_good;
static struct anti_class classes_good;

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

/* Open the library for the test itself, so its data stays mapped and
   writable between the loads of the runtime. */
static int open_bad(void)
{
#if defined(_WIN32)
    void *library = LoadLibraryA(PLUGIN_BAD);
#else
    void *library = dlopen(PLUGIN_BAD, RTLD_NOW | RTLD_LOCAL);
#endif

    if (library == NULL) {
        return 0;
    }
    table = symbol(library, "anti_rt_provides");
    entry = symbol(library, "anti_bad_entry");
    class_of = symbol(library, "anti_bad_class");
    classes = symbol(library, "anti_bad_classes");
    if (table == NULL || entry == NULL || class_of == NULL ||
        classes == NULL) {
        return 0;
    }
    entry->descriptor = &service;
    table_good = *table;
    entry_good = *entry;
    class_good = *class_of;
    classes_good = *classes;
    return 1;
}

static void restore(void)
{
    *table = table_good;
    *entry = entry_good;
    *class_of = class_good;
    *classes = classes_good;
    versions.floor = (const unsigned char *)"1.0";
    versions.floor_length = 3;
}

static void *load(void)
{
    return anti_rt_plugin_load((const unsigned char *)PLUGIN_BAD,
                               (int64_t)strlen(PLUGIN_BAD));
}

/* The load refuses and names the damage. */
static void refused(int line)
{
    void *handle = load();
    struct anti_text why = anti_rt_plugin_message();
    char text[600];

    snprintf(text, sizeof text, "%.*s", (int)why.len, why.ptr);
    if (handle != NULL) {
        check_failures++;
        fprintf(stderr, "line %d: a damaged table loaded\n", line);
        anti_rt_plugin_unload(handle);
    } else if (strstr(text, "damaged table") == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the refusal names no damage: %s\n", line,
                text);
    }
    restore();
}

#define REFUSED() refused(__LINE__)

/* The table as the library carries it loads, builds and unloads. */
static void good_table_loads(void)
{
    void *handle = load();
    void *sub;

    CHECK(handle != NULL);
    if (handle == NULL) {
        return;
    }
    sub = anti_rt_plugin_instance(handle, &service);
    CHECK(sub != NULL);
    if (sub != NULL) {
        free((unsigned char *)sub - entry->offset);
    }
    CHECK(anti_rt_plugin_unload(handle) == 1);
}

static void damaged_texts(void)
{
    table->version = unterminated;
    table->version_length = -1;
    REFUSED();
    table->version = NULL;
    REFUSED();
    entry->path_length = -5;
    REFUSED();
    entry->path = NULL;
    REFUSED();
    entry->built_length = -1;
    REFUSED();
    entry->built = NULL;
    REFUSED();
    class_of->name_length = -1;
    REFUSED();
    class_of->version = unterminated;
    class_of->version_length = -1;
    REFUSED();
    classes->module_length = -1;
    REFUSED();
}

static void damaged_counts(void)
{
    table->count = -1;
    REFUSED();
    table->entries = NULL;
    REFUSED();
    table->class_count = -1;
    REFUSED();
    table->classes = NULL;
    REFUSED();
    entry->chain_length = 0;
    REFUSED();
    entry->chain = NULL;
    REFUSED();
}

static void damaged_pointers(void)
{
    void *heap = malloc(64);

    entry->class_of = NULL;
    REFUSED();
    entry->init = NULL;
    REFUSED();
    /* An address inside no image of the process. */
    entry->descriptor = heap;
    REFUSED();
    classes->descriptor = NULL;
    REFUSED();
    classes->init = NULL;
    REFUSED();
    free(heap);
}

/* The sub-object and its table pointer lie inside the object. */
static void damaged_offsets(void)
{
    entry->offset = 4096;
    REFUSED();
    entry->offset = -8;
    REFUSED();
    entry->offset = class_of->size - (int64_t)sizeof(void *) + 1;
    REFUSED();
    class_of->size = 0;
    entry->offset = 0;
    REFUSED();
    class_of->size = -16;
    REFUSED();
}

/* A part of a version longer than an int64_t compares by its digits. */
static void long_versions(void)
{
    void *handle;

    entry->built = (const unsigned char *)"99999999999999999999.0";
    entry->built_length = 22;
    handle = load();
    CHECK(handle != NULL);
    if (handle != NULL) {
        anti_rt_plugin_unload(handle);
    }
    restore();

    entry->built = (const unsigned char *)"1.99999999999999999999";
    entry->built_length = 22;
    versions.floor = (const unsigned char *)"1.100000000000000000000";
    versions.floor_length = 23;
    handle = load();
    CHECK(handle == NULL);
    restore();

    /* Leading zeros do not count: 21 digits are above 20 nines. */
    entry->built = (const unsigned char *)"1.00100000000000000000000";
    entry->built_length = 25;
    versions.floor = (const unsigned char *)"1.99999999999999999999";
    versions.floor_length = 22;
    handle = load();
    CHECK(handle != NULL);
    if (handle != NULL) {
        anti_rt_plugin_unload(handle);
    }
    restore();
}

int main(void)
{
    if (!open_bad()) {
        fprintf(stderr, "cannot open %s\n", PLUGIN_BAD);
        return 1;
    }
    good_table_loads();
    damaged_texts();
    damaged_counts();
    damaged_pointers();
    damaged_offsets();
    long_versions();
    good_table_loads();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
