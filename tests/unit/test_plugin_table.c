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
#include "../../src/rt/platform.h"
#include "../../src/rt/plugin.h"
#include "../../src/rt/registry.h"
#include "check.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

int check_failures;

/* The stand-ins of what src/rt/plugin.c calls outside itself. */

/* The slots the calls of this program reach through the interface a
   case names, none until one does. */
static unsigned char reached_bits[1];
static struct anti_slots reached = {NULL, 0, reached_bits};
const struct anti_slot_table anti_rt_slots = {1, &reached, 0};

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
    return anti_rt_file_open(name, ANTI_FILE_READ);
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
static struct anti_descriptor *part;
static struct anti_field *part_fields;
static void (**during_init)(void);

static struct anti_provided table_good;
static struct anti_provides entry_good;
static struct anti_descriptor class_good;
static struct anti_class classes_good;
static struct anti_function functions_good;
static struct anti_field fields_good;
static struct anti_descriptor part_good;
static struct anti_field part_fields_good;

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
    part = symbol(library, "anti_bad_part");
    part_fields = symbol(library, "anti_bad_part_fields");
    during_init = symbol(library, "anti_bad_during_init");
    if (table == NULL || entry == NULL || class_of == NULL ||
        classes == NULL || functions == NULL || fields == NULL ||
        part == NULL || part_fields == NULL || during_init == NULL) {
        return 0;
    }
    entry->descriptor = &service;
    table_good = *table;
    entry_good = *entry;
    class_good = *class_of;
    classes_good = *classes;
    functions_good = *functions;
    fields_good = *fields;
    part_good = *part;
    part_fields_good = *part_fields;
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
    *part = part_good;
    *part_fields = part_fields_good;
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

/* The message of the plugin loader holds want. */
static void says(int line, const char *want)
{
    struct anti_text why = anti_rt_plugin_message();
    char text[600];

    snprintf(text, sizeof text, "%.*s", (int)why.len, why.ptr);
    if (strstr(text, want) == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the message does not say `%s`: %s\n", line,
                want, text);
    }
}

/* An interface that the library does not provide. */
static const struct anti_descriptor other = {
    (const unsigned char *)"Other", 5, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 0, NULL, (const unsigned char *)"1.0.0", 5, &versions, 0, NULL
};

/* The table as the library carries it loads, builds and unloads. An
   interface it does not provide, and any interface once it is unloaded,
   give no object and the reason, which `lib.instance` fails with by R134
   of "Answers of 2026-10-18". */
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
    CHECK(anti_rt_plugin_instance(handle, &other) == NULL);
    says(__LINE__, "the library provides no `Other`");
    CHECK(anti_rt_plugin_unload(handle) == 1);
    /* `unload` of anti.plugin leaves no handle behind. */
    CHECK(anti_rt_plugin_instance(NULL, &service) == NULL);
    says(__LINE__, "the library is not open");
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
    /* A count past its list would walk outside the image. */
    table->count = 100000;
    REFUSED();
    table->count = INT64_MAX / 2;
    REFUSED();
    table->class_count = 100000;
    REFUSED();
    table->class_count = INT64_MAX;
    REFUSED();
    entry->chain_length = 100000;
    REFUSED();
    entry->chain_length = INT64_MAX;
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
    functions->signature_length = -1;
    REFUSED();
    functions->signature_length = 3;
    REFUSED();
    fields->name = NULL;
    REFUSED();
    fields->offset = 4096;
    REFUSED();
    fields->offset = -1;
    REFUSED();
}

/* The type id of an array of count elements of the scalar type. */
static int64_t array_of(int64_t element, int64_t count)
{
    return ANTI_TYPE_ARRAY | element << 8 | element << 16 | count << 24;
}

/* The load succeeds, and the table goes back as the library carries
   it. */
static void loaded_with(int line)
{
    void *handle = load();

    if (handle == NULL) {
        struct anti_text why = anti_rt_plugin_message();
        check_failures++;
        fprintf(stderr, "line %d: a sound table was refused: %.*s\n", line,
                (int)why.len, why.ptr);
    } else {
        CHECK(anti_rt_plugin_unload(handle) == 1);
    }
    restore();
}

#define LOADED() loaded_with(__LINE__)

/* Every byte of a field lies inside the object, the bytes of a value it
   holds inline among them, and so does every field of a struct it
   holds. Object.deserialize writes all of them. */
static void damaged_widths(void)
{
    fields->type = ANTI_TYPE_I64;
    fields->offset = class_of->size - 8;
    LOADED();
    fields->type = ANTI_TYPE_I64;
    fields->offset = class_of->size - 4;
    REFUSED();
    fields->type = ANTI_TYPE_I32;
    fields->offset = class_of->size;
    REFUSED();
    fields->type = ANTI_TYPE_STR;
    fields->offset = class_of->size - 8;
    REFUSED();
    fields->type = array_of(ANTI_TYPE_I32, 4);
    fields->offset = class_of->size - 16;
    LOADED();
    fields->type = array_of(ANTI_TYPE_I32, 4);
    fields->offset = class_of->size - 12;
    REFUSED();
    fields->type = array_of(ANTI_TYPE_I64, (int64_t)1 << 38);
    fields->offset = 0;
    REFUSED();
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = part;
    fields->offset = class_of->size - 8;
    LOADED();
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = part;
    fields->offset = class_of->size - 4;
    REFUSED();
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = part;
    part->size = 64;
    REFUSED();
    /* A field of the struct reaches past the struct. */
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = part;
    part_fields->offset = 4;
    REFUSED();
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = part;
    part->field_count = 100000;
    REFUSED();
    /* A struct reached through an owned pointer is written in memory of
       its own, which its size gives. */
    fields->type = ANTI_TYPE_PTR | ANTI_TYPE_STRUCT << 8;
    fields->owned = 1;
    fields->descriptor = part;
    LOADED();
    fields->type = ANTI_TYPE_PTR | ANTI_TYPE_STRUCT << 8;
    fields->owned = 1;
    fields->descriptor = part;
    part_fields->type = ANTI_TYPE_STR;
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
   the library in it. A Windows path keeps its drive, since discovery
   splits the directories at `;` there. */
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
    memcpy(plugin_dir, dir, (size_t)(slash - dir));
    plugin_dir[slash - dir] = '\0';
    plugin_name = slash + 1;
    snprintf(index_path, sizeof index_path, "%s/anti-plugins.toml",
             plugin_dir);
    f = anti_rt_file_open(PLUGIN_BAD, ANTI_FILE_READ);
    if (f == NULL) {
        return 0;
    }
    ok = anti_rt_sha256_stream(f, plugin_digest);
    fclose(f);
    return ok;
}

/* Discover the provider of host.Service in search_dirs through an index
   of length bytes at bytes. want is NULL when the index leads to
   plugin_bad, and otherwise a part of the refusal. */
static const char *search_dirs = plugin_dir;

static void discovers_with(int line, const char *bytes, size_t length,
                           const char *want)
{
    void *sub;
    struct anti_text why;
    char text[600];

    index_bytes = bytes;
    index_length = length;
    sub = anti_rt_plugin_provider((const unsigned char *)"host.Service", 12,
                                  (const unsigned char *)"", 0, search_dirs);
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

/* Append s to the text in to, a buffer of size bytes, cut to fit. */
static void append(char *to, size_t size, const char *s)
{
    size_t used = strlen(to);

    snprintf(to + used, size - used, "%s", s);
}

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
        append(text, sizeof text, line);
    }
    if (runtime != NULL) {
        snprintf(line, sizeof line, "runtime = '%s'\n", runtime);
        append(text, sizeof text, line);
    }
    if (digest != NULL) {
        snprintf(line, sizeof line, "digest = '%s'\n", digest);
        append(text, sizeof text, line);
    }
    if (interfaces != NULL) {
        snprintf(line, sizeof line, "interfaces = %s\n", interfaces);
        append(text, sizeof text, line);
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
    append(text, sizeof text,
           entry_of_index(plugin_name, "0.0.0", plugin_digest,
                          "['host.Service']"));
    DISCOVERS(text, "both provide `host.Service`");
    /* The interface after 64 others in the list of the entry. */
    snprintf(interfaces, sizeof interfaces, "[");
    for (k = 0; k < 64; k++) {
        char one[32];
        snprintf(one, sizeof one, "'host.Other%d', ", k);
        append(interfaces, sizeof interfaces, one);
    }
    append(interfaces, sizeof interfaces, "'host.Service']");
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest, interfaces),
              NULL);
}

/* Make dir the working directory. Gives 0 when it cannot be. */
static int change_to(const char *dir)
{
#if defined(_WIN32)
    return SetCurrentDirectoryA(dir) != 0;
#else
    return chdir(dir) == 0;
#endif
}

/* An empty directory of the search path is the working directory, as
   in PATH. The index and the library are read there, and the library
   the digest covers is the one that loads, never one of the same name
   that the loader of the system finds on its own path. */
static void empty_directory(void)
{
    char dir[4096];
    char dirs[16];

#if defined(_WIN32)
    DWORD written = GetCurrentDirectoryA(sizeof dir, dir);
    CHECK(written > 0 && written < sizeof dir);
#else
    CHECK(getcwd(dir, sizeof dir) != NULL);
#endif
    if (!change_to(plugin_dir)) {
        check_failures++;
        fprintf(stderr, "cannot enter %s\n", plugin_dir);
        return;
    }
    snprintf(index_path, sizeof index_path, "./anti-plugins.toml");
    snprintf(dirs, sizeof dirs, "%cnowhere", anti_rt_path_list_separator());
    search_dirs = dirs;
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Service']"), NULL);
    snprintf(dirs, sizeof dirs, "nowhere%c", anti_rt_path_list_separator());
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Service']"), NULL);
    search_dirs = plugin_dir;
    snprintf(index_path, sizeof index_path, "%s/anti-plugins.toml",
             plugin_dir);
    CHECK(change_to(dir));
}

/* The search path splits at the separator of the system alone. A
   directory that does not exist comes first, and the directory of
   plugin_bad second, with the letter of its drive on Windows. */
static void second_directory(void)
{
    char dirs[4096 + 16];

    snprintf(dirs, sizeof dirs, "nowhere%c%s", anti_rt_path_list_separator(),
             plugin_dir);
    search_dirs = dirs;
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Service']"), NULL);
    search_dirs = plugin_dir;
}

/* A directory whose index does not fit a path holds none, and the search
   goes on to the next directory, the one of plugin_bad. */
static void long_directory(void)
{
    static char dirs[4100 + 4096 + 2];

    memset(dirs, 'x', 4100);
    snprintf(dirs + 4100, sizeof dirs - 4100, "%c%s",
             anti_rt_path_list_separator(), plugin_dir);
    search_dirs = dirs;
    DISCOVERS(entry_of_index(plugin_name, "0.0.0", plugin_digest,
                             "['host.Service']"), NULL);
    search_dirs = plugin_dir;
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

/* A length below zero names no path, as one past the room of a path
   does, and an empty path names no library. */
static void path_lengths(void)
{
    void *handle = anti_rt_plugin_load((const unsigned char *)PLUGIN_BAD, -1);

    CHECK(handle == NULL);
    says(__LINE__, "the path of a library is at most 4095 bytes");
    handle = anti_rt_plugin_load((const unsigned char *)"", 0);
    CHECK(handle == NULL);
    if (handle != NULL) {
        anti_rt_plugin_unload(handle);
    }
}

/* A handle given back is no open library to any call, and supports
   answers for an interface the library provides alone. */
static void closed_handle(void)
{
    void *handle = load();

    CHECK(handle != NULL);
    if (handle == NULL) {
        return;
    }
    CHECK(anti_rt_plugin_supports(handle, &service,
                                  (const unsigned char *)"words", 5) == 1);
    CHECK(anti_rt_plugin_supports(handle, &other,
                                  (const unsigned char *)"words", 5) == 0);
    CHECK(anti_rt_plugin_unload(handle) == 1);
    CHECK(anti_rt_plugin_instance(handle, &service) == NULL);
    says(__LINE__, "the library is not open");
    CHECK(anti_rt_plugin_supports(handle, &service,
                                  (const unsigned char *)"words", 5) == 0);
    CHECK(anti_rt_plugin_live(handle) == 0);
    CHECK(anti_rt_plugin_unload(handle) == 0);
    says(__LINE__, "the library is not open");
}

/* A version compares part by part. A part counts its digits and passes
   over what follows them, a missing part is zero, and two parts of as
   many digits compare by their digits. */
static void versions_by_parts(void)
{
    entry->built = (const unsigned char *)"1.0rc";
    entry->built_length = 5;
    versions.floor = (const unsigned char *)"1.0";
    versions.floor_length = 3;
    LOADED();
    entry->built = (const unsigned char *)"1";
    entry->built_length = 1;
    versions.floor = (const unsigned char *)"1.0";
    versions.floor_length = 3;
    LOADED();
    entry->built = (const unsigned char *)"1.1.0.0.0.0.0.0.1";
    entry->built_length = 17;
    versions.floor = (const unsigned char *)"1.1";
    versions.floor_length = 3;
    LOADED();
    entry->built = (const unsigned char *)"1.2.0";
    entry->built_length = 5;
    versions.floor = (const unsigned char *)"1.3";
    versions.floor_length = 3;
    refused_with(__LINE__, "was built for `host.Service` 1.2.0, and this "
                           "program takes 1.3 and above");
}

/* The interface of the host without a chain, and with an empty one. */
static const int64_t no_chain[1] = {1};
static struct anti_versions empty_versions = {no_chain, 0, NULL, 0};
static const struct anti_descriptor unversioned = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 0, NULL, (const unsigned char *)"1.0.0", 5, NULL, 0, NULL
};
static const struct anti_descriptor chainless = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 0, NULL, (const unsigned char *)"1.0.0", 5, &empty_versions, 0, NULL
};

/* An interface of the host that carries no version, and one whose size
   is not the one the library was built with. */
static void unversioned_host(void)
{
    entry->descriptor = &unversioned;
    refused_with(__LINE__, "provides `host.Service`, and this program "
                           "carries no version of it");
    entry->descriptor = &chainless;
    refused_with(__LINE__, "provides `host.Service`, and this program "
                           "carries no version of it");
    entry->size = 24;
    refused_with(__LINE__, "a field was added to `host.Service`");
}

/* The interface of the host with one function at slot 1, and a program
   that calls slot 2 alone, which no function of the descriptor names. */
static const struct anti_function grown_functions[1] = {
    {(const unsigned char *)"words", 5, 1, 0, NULL, 0}
};
static const struct anti_descriptor grown = {
    (const unsigned char *)"Service", 7, NULL, 16, 0, NULL, 0, NULL, NULL,
    0, 1, grown_functions, (const unsigned char *)"1.0.0", 5, &versions, 0,
    NULL
};

static void slot_without_name(void)
{
    entry->descriptor = &grown;
    reached.descriptor = &grown;
    reached.slot_count = 3;
    reached_bits[0] = 0x04;
    refused_with(__LINE__, "this program calls `slot 2` of `host.Service`");
    reached.descriptor = NULL;
    reached.slot_count = 0;
    reached_bits[0] = 0;
}

/* The type id of an array of arrays of the element, whose lengths stand
   in the records of its descriptor. */
static int64_t nested_of(int64_t element)
{
    return ANTI_TYPE_ARRAY | ANTI_TYPE_ARRAY << 8 | element << 16;
}

/* The records of the levels of an array of arrays. */
static struct anti_field levels[2];

/* The records of the levels of an array lie in an image, and so does
   the descriptor of its element. The bytes of the array stay below
   INT64_MAX. An array without the descriptor of its element, or without
   records of its levels, takes no bytes, and no walk writes it. */
static void damaged_arrays(void)
{
    void *heap = malloc(64);

    CHECK(heap != NULL);
    if (heap == NULL) {
        return;
    }
    fields->type = array_of(ANTI_TYPE_CLASS, 2);
    LOADED();
    fields->type = nested_of(ANTI_TYPE_I32);
    LOADED();
    fields->type = array_of(ANTI_TYPE_I32, 0);
    LOADED();
    fields->type = nested_of(ANTI_TYPE_I32);
    fields->descriptor = part;
    part->field_count = 0;
    REFUSED();
    fields->type = nested_of(ANTI_TYPE_I32);
    fields->descriptor = part;
    part->field_count = ANTI_ARRAY_LEVELS + 1;
    REFUSED();
    fields->type = nested_of(ANTI_TYPE_I32);
    fields->descriptor = part;
    part->field_count = 2;
    part->fields = heap;
    REFUSED();
    levels[0].owned = 1;
    levels[1].owned = 1;
    levels[1].descriptor = heap;
    fields->type = nested_of(ANTI_TYPE_STRUCT);
    fields->descriptor = part;
    part->field_count = 2;
    part->fields = levels;
    REFUSED();
    levels[0].owned = -1;
    levels[1].descriptor = NULL;
    fields->type = nested_of(ANTI_TYPE_I32);
    fields->descriptor = part;
    part->field_count = 2;
    part->fields = levels;
    REFUSED();
    memset(levels, 0, sizeof levels);
    fields->type = array_of(ANTI_TYPE_STRUCT, 2);
    fields->descriptor = part;
    part->size = -8;
    REFUSED();
    fields->type = array_of(ANTI_TYPE_STRUCT, (int64_t)1 << 30);
    fields->descriptor = part;
    part->size = (int64_t)1 << 40;
    REFUSED();
    free(heap);
}

/* The descriptor of a field lies in an image. A pointer to an array
   reaches no descriptor, and a struct without one takes no bytes. */
static void damaged_descriptors(void)
{
    void *heap = malloc(64);

    CHECK(heap != NULL);
    if (heap == NULL) {
        return;
    }
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = heap;
    REFUSED();
    fields->type = ANTI_TYPE_PTR | ANTI_TYPE_ARRAY << 8;
    fields->descriptor = part;
    LOADED();
    fields->type = ANTI_TYPE_STRUCT;
    fields->descriptor = NULL;
    LOADED();
    free(heap);
}

/* Seventeen types, each reached through a pointer of the one before,
   which the list of the types to check holds past its first room. */
static struct anti_descriptor reached_types[17];
static struct anti_field reached_fields[17];

static void many_types(void)
{
    int i;

    for (i = 0; i < 17; i++) {
        reached_types[i] = part_good;
        reached_fields[i] = part_fields_good;
        if (i + 1 < 17) {
            reached_fields[i].type = ANTI_TYPE_PTR | ANTI_TYPE_STRUCT << 8;
            reached_fields[i].descriptor = &reached_types[i + 1];
        }
        reached_types[i].fields = &reached_fields[i];
    }
    fields->type = ANTI_TYPE_PTR | ANTI_TYPE_STRUCT << 8;
    fields->descriptor = &reached_types[0];
    LOADED();
}

/* A type argument of a size below zero, a function of a count of
   parameters below zero, and a signature outside every image. */
static const struct anti_field type_argument = {
    (const unsigned char *)"T", 1, -1, ANTI_TYPE_I64, 0, NULL
};

static void damaged_records(void)
{
    void *heap = malloc(64);

    CHECK(heap != NULL);
    if (heap == NULL) {
        return;
    }
    class_of->type_arg_count = 1;
    class_of->type_args = &type_argument;
    REFUSED();
    functions->param_count = -1;
    REFUSED();
    functions->signature = heap;
    functions->signature_length = 3;
    REFUSED();
    free(heap);
}

/* A depth that leaves no room for the class itself, a list of ancestors
   that does not end in the class, and a parent of no bytes. */
static struct anti_descriptor empty_parent;

static void damaged_chain(void)
{
    static const struct anti_descriptor *const wrong[1] = {&service};

    class_of->depth = INT64_MAX;
    REFUSED();
    class_of->ancestors = wrong;
    REFUSED();
    empty_parent = part_good;
    empty_parent.size = 0;
    empty_parent.field_count = 0;
    class_of->parent = &empty_parent;
    class_of->depth = 1;
    REFUSED();
}

/* The texts of an entry fit an int, and an entry names its interface. */
static void damaged_entry(void)
{
    entry->path_length = (int64_t)INT32_MAX + 1;
    REFUSED();
    entry->descriptor = NULL;
    REFUSED();
}

/* A runtime longer than a path, and a library whose path does not fit
   beside its directory, are passed over. No directories search none. */
static void passed_over(void)
{
    static char runtime[4200];
    static char dir[4100];
    static char name[64];
    void *sub;

    memset(runtime, '9', 4100);
    runtime[4100] = '\0';
    DISCOVERS(entry_of_index(plugin_name, runtime, plugin_digest,
                             "['host.Service']"), NONE);
    memset(dir, 'd', 4070);
    dir[4070] = '\0';
    memset(name, 'n', 40);
    name[40] = '\0';
    search_dirs = dir;
    snprintf(index_path, sizeof index_path, "%s/anti-plugins.toml", dir);
    DISCOVERS(entry_of_index(name, "0.0.0", plugin_digest,
                             "['host.Service']"), NONE);
    search_dirs = plugin_dir;
    snprintf(index_path, sizeof index_path, "%s/anti-plugins.toml",
             plugin_dir);
    sub = anti_rt_plugin_provider((const unsigned char *)"host.Service", 12,
                                  (const unsigned char *)"", 0, NULL);
    CHECK(sub == NULL);
    says(__LINE__, NONE);
}

/* A reason cut to the room of the message ends in `...`. */
static void long_reason(void)
{
    char path[601];
    struct anti_text why;
    void *handle;

    memset(path, 'p', 600);
    path[0] = '/';
    path[600] = '\0';
    handle = anti_rt_plugin_load((const unsigned char *)path, 600);
    why = anti_rt_plugin_message();
    CHECK(handle == NULL);
    CHECK(why.len == 511 && memcmp(why.ptr + 508, "...", 3) == 0);
}

#if !defined(_WIN32)
/* The reason of the system names the path it could not open. A path
   that holds a form of a missing symbol is read as one, up to the byte
   that ends the symbol, and the loader then reads the file for the list
   of the modules of the library. The file is no library, so its load
   fails. Windows names hold no colon. */
static char reasons_dir[4300];

/* Make the directory of the files of these cases beside plugin_bad. */
static void reasons_ready(void)
{
    snprintf(reasons_dir, sizeof reasons_dir, "%s/plugin-reasons",
             plugin_dir);
    mkdir(reasons_dir, 0755);
}

#define USES "uses `net.example`, which this program does not import"

/* Write list into dir/name, load the file and expect a reason that holds
   want. */
static void reason_is(int line, const char *name, const char *list,
                      const char *want)
{
    char path[4400];
    void *handle;
    FILE *f;

    snprintf(path, sizeof path, "%s/%s", reasons_dir, name);
    f = fopen(path, "wb");
    if (f == NULL || fwrite(list, 1, strlen(list), f) != strlen(list)) {
        check_failures++;
        fprintf(stderr, "line %d: cannot write %s\n", line, path);
        if (f != NULL) {
            fclose(f);
        }
        return;
    }
    fclose(f);
    handle = anti_rt_plugin_load((const unsigned char *)path,
                                 (int64_t)strlen(path));
    CHECK(handle == NULL);
    if (handle != NULL) {
        anti_rt_plugin_unload(handle);
    }
    says(line, want);
    remove(path);
}

/* The list ends at the end of the file, and its first module is the
   start of the symbol without the dot after it. */
static void reasons_with_paths(void)
{
    static const char list[] = "anti-plugin-modules: net.ex net.example";
    char longer[301];
    char path[4400];
    void *handle;

    reasons_ready();
    reason_is(__LINE__, "undefined symbol: net.example.f", list, USES);
    reason_is(__LINE__, "undefined symbol: net.example.f x", list, USES);
    reason_is(__LINE__, "undefined symbol: net.example.f\nx", list, USES);
    reason_is(__LINE__, "symbol not found in flat namespace 'net.example.f'",
              list, USES);
    reason_is(__LINE__, "undefined symbol: ", list, "undefined symbol: : ");
    /* A symbol longer than the loader keeps names no module. */
    memset(longer, 'n', 300);
    memcpy(longer, "undefined symbol: ", 18);
    longer[150] = '/';
    longer[300] = '\0';
    snprintf(path, sizeof path, "%s/%s", reasons_dir, longer);
    handle = anti_rt_plugin_load((const unsigned char *)path,
                                 (int64_t)strlen(path));
    CHECK(handle == NULL);
    says(__LINE__, "nnn: ");
}

/* The loader reads at most 1 GiB of a library for the list. The file
   holds no data past its first bytes, so it takes no room on disk. */
static void huge_library(void)
{
    static const char list[] = "anti-plugin-modules: net.example";
    char path[4400];
    void *handle;
    FILE *f;

    reasons_ready();
    snprintf(path, sizeof path, "%s/undefined symbol: net.example.f",
             reasons_dir);
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) {
        return;
    }
    CHECK(fwrite(list, 1, sizeof list, f) == sizeof list);
    CHECK(fseeko(f, ((off_t)1 << 30) + 4096, SEEK_SET) == 0);
    CHECK(fputc('x', f) == 'x');
    fclose(f);
    handle = anti_rt_plugin_load((const unsigned char *)path,
                                 (int64_t)strlen(path));
    CHECK(handle == NULL);
    says(__LINE__, USES);
    remove(path);
}
#endif

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
    damaged_widths();
    damaged_parents();
    path_lengths();
    closed_handle();
    versions_by_parts();
    unversioned_host();
    slot_without_name();
    damaged_arrays();
    damaged_descriptors();
    many_types();
    damaged_records();
    damaged_chain();
    damaged_entry();
    long_reason();
    other_program();
    unload_waits_for_build();
    long_versions();
    good_table_loads();
    if (!find_plugin()) {
        fprintf(stderr, "cannot digest %s\n", PLUGIN_BAD);
        return 1;
    }
    damaged_indexes();
    empty_directory();
    second_directory();
    long_directory();
    nul_in_path();
    passed_over();
#if !defined(_WIN32)
    reasons_with_paths();
    huge_library();
#endif
    good_table_loads();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
