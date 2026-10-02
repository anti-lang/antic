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
#include "../../src/rt/digest.h"
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

/* The index that the stub of anti_rt_fs_read gives for index_path, its
   bytes and their count, which may hold a NUL. */
static char index_path[4200];
static const char *index_bytes;
static size_t index_length;

/* A file opened for reading, which discovery digests. A path with a NUL
   is refused, as the runtime refuses it. */
void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing)
{
    char name[8200];

    if (writing != 0 || len < 0 || (size_t)len >= sizeof name ||
        memchr(path, 0, (size_t)len) != NULL) {
        return NULL;
    }
    memcpy(name, path, (size_t)len);
    name[len] = '\0';
    return fopen(name, "rb");
}

int64_t anti_rt_fs_size(void *file)
{
    (void)file;
    return -1;
}

unsigned char *anti_rt_fs_read(const char *path, int64_t *length)
{
    unsigned char *bytes;

    if (index_bytes == NULL || strcmp(path, index_path) != 0) {
        return NULL;
    }
    bytes = malloc(index_length + 1);
    if (bytes == NULL) {
        return NULL;
    }
    memcpy(bytes, index_bytes, index_length);
    *length = (int64_t)index_length;
    return bytes;
}

const void *anti_rt_body_entry(anti_rt_body body)
{
    union {
        const void *entry;
        anti_rt_body body;
    } cast;

    cast.body = body;
    return cast.entry;
}

/* The interface the library provides, as the host declares it. */
static const int64_t chain[1] = {1};
static struct anti_versions versions = {
    chain, 1, (const unsigned char *)"1.0", 3
};
static const struct anti_descriptor service = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 0, NULL, (const unsigned char *)"1.0.0", 5, &versions, 0, NULL
};

/* Bytes without a NUL after them, which a read past a length reaches. */
static const unsigned char unterminated[4] = {'9', '9', '9', '9'};

static struct anti_provided *table;
static struct anti_provides *entry;
static struct anti_descriptor *class_of;
static struct anti_class *classes;
static struct anti_function *functions;
static struct anti_field *fields;
static void (**during_init)(void);

static struct anti_provided table_good;
static struct anti_provides entry_good;
static struct anti_descriptor class_good;
static struct anti_class classes_good;
static struct anti_function functions_good;
static struct anti_field fields_good;

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
    functions = symbol(library, "anti_bad_functions");
    fields = symbol(library, "anti_bad_fields");
    during_init = symbol(library, "anti_bad_during_init");
    if (table == NULL || entry == NULL || class_of == NULL ||
        classes == NULL || functions == NULL || fields == NULL ||
        during_init == NULL) {
        return 0;
    }
    entry->descriptor = &service;
    table_good = *table;
    entry_good = *entry;
    class_good = *class_of;
    classes_good = *classes;
    functions_good = *functions;
    fields_good = *fields;
    return 1;
}

static void restore(void)
{
    *table = table_good;
    *entry = entry_good;
    *class_of = class_good;
    *classes = classes_good;
    *functions = functions_good;
    *fields = fields_good;
    *during_init = NULL;
    versions.floor = (const unsigned char *)"1.0";
    versions.floor_length = 3;
}

static void *load(void)
{
    return anti_rt_plugin_load((const unsigned char *)PLUGIN_BAD,
                               (int64_t)strlen(PLUGIN_BAD));
}

/* The load refuses with a reason that holds want. */
static void refused_with(int line, const char *want)
{
    void *handle = load();
    struct anti_text why = anti_rt_plugin_message();
    char text[600];

    snprintf(text, sizeof text, "%.*s", (int)why.len, why.ptr);
    if (handle != NULL) {
        check_failures++;
        fprintf(stderr, "line %d: a refused table loaded\n", line);
        anti_rt_plugin_unload(handle);
    } else if (strstr(text, want) == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the refusal does not say `%s`: %s\n", line,
                want, text);
    }
    restore();
}

/* The load refuses and names the damage. */
#define REFUSED() refused_with(__LINE__, "damaged table")

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

    CHECK(heap != NULL);
    if (heap == NULL) {
        return;
    }
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

/* The lists of the class lie inside the library's image and hold what
   their counts say. supports and Object.deserialize walk them. */
static void damaged_lists(void)
{
    class_of->function_count = 100000;
    REFUSED();
    class_of->function_count = INT64_MAX;
    REFUSED();
    class_of->function_count = -1;
    REFUSED();
    class_of->functions = NULL;
    REFUSED();
    class_of->field_count = 100000;
    REFUSED();
    class_of->field_count = INT64_MAX / 2;
    REFUSED();
    class_of->field_count = -1;
    REFUSED();
    class_of->fields = NULL;
    REFUSED();
    class_of->type_arg_count = 3;
    REFUSED();
    functions->name_length = -1;
    REFUSED();
    functions->slot = -1;
    REFUSED();
    fields->name = NULL;
    REFUSED();
    fields->offset = 4096;
    REFUSED();
    fields->offset = -1;
    REFUSED();
}

/* The chain of parents ends after as many steps as the depth says, and
   every descriptor of it is sound. */
static void damaged_parents(void)
{
    void *heap = malloc(sizeof(struct anti_descriptor));

    CHECK(heap != NULL);
    if (heap == NULL) {
        return;
    }
    class_of->parent = class_of;
    REFUSED();
    class_of->depth = -1;
    REFUSED();
    class_of->depth = 1;
    REFUSED();
    class_of->parent = heap;
    class_of->depth = 1;
    REFUSED();
    class_of->parent = &service;
    class_of->depth = 1;
    class_of->ancestors = (const struct anti_descriptor *const *)(void *)heap;
    REFUSED();
    free(heap);
}

/* The two refusals of a sound table that belongs to another program:
   one built for another runtime, and one whose interface has the name of
   the host's and another structure. */
static void other_program(void)
{
    static const int64_t other_chain[1] = {2};

    table->version = (const unsigned char *)"9.9.9";
    table->version_length = 5;
    refused_with(__LINE__, "was built for runtime 9.9.9, and this program "
                           "carries 0.0.0");
    entry->chain = other_chain;
    refused_with(__LINE__, "`host.Service` of " PLUGIN_BAD
                           " is not the interface this program carries");
    /* A library that carries the interface in its own image was not bound
       against the host. */
    entry->descriptor = class_of;
    refused_with(__LINE__, "carries an `host.Service` of its own");
}

/* An unload while an object of the library is being built. A load of the
   open library gives the slot it has. */
static int8_t unloaded_during_build;

static void unload_during_build(void)
{
    void *handle = load();

    unloaded_during_build = handle != NULL ? anti_rt_plugin_unload(handle)
                                           : -2;
}

/* The object the loader builds counts from before the lock is given back
   until its init has run, where the `created` hook of the class counts
   it. An unload in between refuses, and the library stays open. */
static void unload_waits_for_build(void)
{
    const unsigned char path[] = "host.Service";
    void *handle = load();
    void *sub;

    CHECK(handle != NULL);
    if (handle == NULL) {
        return;
    }
    unloaded_during_build = -1;
    *during_init = unload_during_build;
    sub = anti_rt_plugin_instance(handle, &service);
    CHECK(unloaded_during_build == 0);
    CHECK(sub != NULL);
    if (sub != NULL) {
        free((unsigned char *)sub - entry->offset);
    }
    CHECK(anti_rt_plugin_live(handle) == 0);
    CHECK(anti_rt_plugin_unload(handle) == 1);

    /* The provider of an interface builds the same way. */
    unloaded_during_build = -1;
    sub = anti_rt_plugin_provider(path, (int64_t)sizeof path - 1,
                                  (const unsigned char *)PLUGIN_BAD,
                                  (int64_t)strlen(PLUGIN_BAD), NULL);
    CHECK(unloaded_during_build == 0);
    CHECK(sub != NULL);
    if (sub != NULL) {
        free((unsigned char *)sub - entry->offset);
    }
    restore();
    handle = load();
    CHECK(handle != NULL);
    if (handle != NULL) {
        CHECK(anti_rt_plugin_live(handle) == 0);
        CHECK(anti_rt_plugin_unload(handle) == 1);
    }
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

/* The directory of plugin_bad, which discovery searches, and the name of
   the library in it. A Windows path loses its drive, because discovery
   splits the directories at `:`. The path then names the drive of the
   test's working directory, which is the drive of the build. */
static char plugin_dir[4096];
static const char *plugin_name;
static char plugin_digest[65];

static int find_plugin(void)
{
    const char *slash = strrchr(PLUGIN_BAD, '/');
    const char *dir = PLUGIN_BAD;
    FILE *f;
    int ok;

    if (slash == NULL || (size_t)(slash - dir) >= sizeof plugin_dir) {
        return 0;
    }
    if (dir[0] != '\0' && dir[1] == ':') {
        dir += 2;
    }
    memcpy(plugin_dir, dir, (size_t)(slash - dir));
    plugin_dir[slash - dir] = '\0';
    plugin_name = slash + 1;
    snprintf(index_path, sizeof index_path, "%s/anti-plugins.toml",
             plugin_dir);
    f = fopen(PLUGIN_BAD, "rb");
    if (f == NULL) {
        return 0;
    }
    ok = anti_rt_sha256_stream(f, plugin_digest);
    fclose(f);
    return ok;
}

/* Discover the provider of host.Service through an index of length bytes
   at bytes. want is NULL when the index leads to plugin_bad, and
   otherwise a part of the refusal. */
static void discovers_with(int line, const char *bytes, size_t length,
                           const char *want)
{
    void *sub;
    struct anti_text why;
    char text[600];

    index_bytes = bytes;
    index_length = length;
    sub = anti_rt_plugin_provider((const unsigned char *)"host.Service", 12,
                                  (const unsigned char *)"", 0, plugin_dir);
    index_bytes = NULL;
    why = anti_rt_plugin_message();
    snprintf(text, sizeof text, "%.*s", (int)why.len, why.ptr);
    if (sub != NULL) {
        void *handle = load();
        free((unsigned char *)sub - entry->offset);
        CHECK(handle != NULL && anti_rt_plugin_unload(handle) == 1);
        if (want != NULL) {
            check_failures++;
            fprintf(stderr, "line %d: a damaged index found a library\n", line);
        }
    } else if (want == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the index found no library: %s\n", line,
                text);
    } else if (strstr(text, want) == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the refusal does not say `%s`: %s\n", line,
                want, text);
    }
}

#define DISCOVERS(text, want) \
    discovers_with(__LINE__, text, strlen(text), want)

/* An index of one entry with the fields given, each a whole line or
   empty. The entry names plugin_bad unless path says otherwise. */
static const char *entry_of_index(const char *path, const char *runtime,
                                  const char *digest, const char *interfaces)
{
    static char text[8192];
    char line[4300];

    snprintf(text, sizeof text, "[[library]]\n");
    if (path != NULL) {
        snprintf(line, sizeof line, "path = '%s'\n", path);
        strncat(text, line, sizeof text - strlen(text) - 1);
    }
    if (runtime != NULL) {
        snprintf(line, sizeof line, "runtime = '%s'\n", runtime);
        strncat(text, line, sizeof text - strlen(text) - 1);
    }
    if (digest != NULL) {
        snprintf(line, sizeof line, "digest = '%s'\n", digest);
        strncat(text, line, sizeof text - strlen(text) - 1);
    }
    if (interfaces != NULL) {
        snprintf(line, sizeof line, "interfaces = %s\n", interfaces);
        strncat(text, line, sizeof text - strlen(text) - 1);
    }
    return text;
}

#define NONE "no library of the search directories provides `host.Service`"
#define DIGEST "does not match the digest"

/* Discovery reads the index beside the libraries, which a user or a copy
   can damage. Each damage is refused or passed over, and only an entry
   that lists the interface and matches its digest is loaded. */
static void damaged_indexes(void)
{
    static char text[8192];
    static char interfaces[2048];
    static char long_path[4200];
    char digest[65];
    size_t length;
    int k;

    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Service']"), NULL);
    /* No TOML, an index cut off in a string, and an empty one. */
    DISCOVERS("this is no TOML\n", "is no index of plugins");
    DISCOVERS("[[library]]\npath = 'plugin_b", "is no index of plugins");
    DISCOVERS("[[library]\n", "is no index of plugins");
    DISCOVERS("", NONE);
    /* An entry that does not list the interface. */
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest, NULL), NONE);
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "'host.Service'"), NONE);
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Other']"), NONE);
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest, "[]"),
              NONE);
    /* A library of another runtime, or of none, is passed over. */
    DISCOVERS(entry_of_index(plugin_name, "9.9.9", plugin_digest,
                             "['host.Service']"), NONE);
    DISCOVERS(entry_of_index(plugin_name, NULL, plugin_digest,
                             "['host.Service']"), NONE);
    /* No path, an empty one, one past the room of a path. */
    DISCOVERS(entry_of_index(NULL, "0.0.0", plugin_digest, "['host.Service']"),
              NONE);
    DISCOVERS(entry_of_index("", "0.0.0", plugin_digest, "['host.Service']"),
              NONE);
    memset(long_path, 'x', 4100);
    long_path[4100] = '\0';
    DISCOVERS(entry_of_index(long_path, "0.0.0", plugin_digest,
                             "['host.Service']"), NONE);
    /* A digest that is missing, short, of another file, or a library that
       is missing or a directory. */
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", NULL, "['host.Service']"),
              DIGEST);
    memcpy(digest, plugin_digest, 63);
    digest[63] = '\0';
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", digest, "['host.Service']"),
              DIGEST);
    digest[63] = plugin_digest[63] == '0' ? '1' : '0';
    digest[64] = '\0';
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", digest, "['host.Service']"),
              DIGEST);
    DISCOVERS(entry_of_index("nowhere.so", "0.0.0", plugin_digest,
                             "['host.Service']"), DIGEST);
    DISCOVERS(entry_of_index(".", "0.0.0", plugin_digest, "['host.Service']"),
              DIGEST);
    /* A path that holds a NUL names no file. Read up to the NUL, it would
       name plugin_bad, whose digest the entry gives. */
    snprintf(long_path, sizeof long_path, "%s|x", plugin_name);
    snprintf(text, sizeof text, "%s",
             entry_of_index(long_path, "0.0.0", plugin_digest,
                            "['host.Service']"));
    length = strlen(text);
    *strchr(text, '|') = '\0';
    discovers_with(__LINE__, text, length, NONE);
    /* Two entries that provide the interface. */
    snprintf(text, sizeof text, "%s",
             entry_of_index(plugin_name, "0.0.0", plugin_digest,
                            "['host.Service']"));
    strncat(text, entry_of_index(plugin_name, "0.0.0", plugin_digest,
                                 "['host.Service']"),
            sizeof text - strlen(text) - 1);
    DISCOVERS(text, "both provide `host.Service`");
    /* The interface after 64 others in the list of the entry. */
    snprintf(interfaces, sizeof interfaces, "[");
    for (k = 0; k < 64; k++) {
        char one[32];
        snprintf(one, sizeof one, "'host.Other%d', ", k);
        strncat(interfaces, one, sizeof interfaces - strlen(interfaces) - 1);
    }
    strncat(interfaces, "'host.Service']",
            sizeof interfaces - strlen(interfaces) - 1);
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest, interfaces),
              NULL);
}

/* A path holds no NUL, which would name another file than the one the
   program gave. The bytes up to it name plugin_bad. */
static void nul_in_path(void)
{
    char path[4200];
    size_t length = strlen(PLUGIN_BAD);
    void *handle;
    struct anti_text why;

    snprintf(path, sizeof path, "%s|x", PLUGIN_BAD);
    path[length] = '\0';
    handle = anti_rt_plugin_load((const unsigned char *)path,
                                 (int64_t)length + 2);
    why = anti_rt_plugin_message();
    CHECK(handle == NULL);
    if (handle != NULL) {
        anti_rt_plugin_unload(handle);
    }
    CHECK(why.len > 0 &&
          strstr((const char *)why.ptr, "holds a NUL byte") != NULL);
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
    damaged_lists();
    damaged_parents();
    other_program();
    unload_waits_for_build();
    long_versions();
    good_table_loads();
    if (!find_plugin()) {
        fprintf(stderr, "cannot digest %s\n", PLUGIN_BAD);
        return 1;
    }
    damaged_indexes();
    nul_in_path();
    good_table_loads();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
