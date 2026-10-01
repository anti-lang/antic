/* `anti symbols inventory`, `check` and `resolve`.

   DESIGN: a binary is known by the build id in its licence notice. An
   archive is known by the id of the debug twin it holds, which the
   notice of that twin carries as well. Nothing else ties the two
   together, so a renamed binary still finds its symbols and a rebuilt
   one never finds the symbols of its predecessor. The readers of
   src/rt/symbols.c answer for the twin, as they do for
   `anti.lang.StackTrace.symbolize`, and the map answers where the twin
   does not. */
#include "syms.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "platform.h"
#include "conf_include.h"
#include "license.h"
#include "symbols.h"
#include "symmap.h"
#include "text.h"
#include "toml.h"
#include "zip.h"

/* A build names the archive beside a binary after its stem with this
   suffix. The index of a deployment archive has the second name. */
#define ARCHIVE_SUFFIX "-symbols.zip"
#define INDEX_NAME "index.toml"
#define PLUGIN_INDEX "anti-plugins.toml"

/* The variable AddressSanitizer reads its options from, and the option
   that leaves the frames of a report to Anti's symbolizer. */
#define SANITIZER_OPTIONS "ASAN_OPTIONS"
#define NO_SYMBOLIZE "symbolize=0"

static bool ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s);
    size_t b = strlen(suffix);

    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* A list of texts. */
struct texts {
    struct text *items;
    size_t count;
    size_t capacity;
};

static struct text *texts_add(struct texts *list, const void *bytes,
                              size_t length)
{
    struct text *t;

    if (list->count == list->capacity) {
        list->items = files_grow(list->items, &list->capacity,
                                 sizeof *list->items);
    }
    t = &list->items[list->count++];
    memset(t, 0, sizeof *t);
    text_append_bytes(t, bytes, length);
    return t;
}

static void texts_free(struct texts *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        text_free(&list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

/* The directory of a path, or `.` for a bare name. */
static void directory_of(const char *path, struct text *out)
{
    const char *name = files_base_name(path);

    if (name == path) {
        text_append(out, ".");
    } else {
        text_append_bytes(out, path, (size_t)(name - path - 1));
        if (out->length == 0) {
            text_append(out, "/");
        }
    }
}

/* path, resolved against the directory dir unless it is absolute. */
static void resolved(const char *dir, const char *path, struct text *out)
{
    if (path_is_absolute(path)) {
        text_append(out, path);
    } else {
        text_appendf(out, "%s/%s", dir, path);
    }
}

/* The stem of a binary: its name without the suffix of an executable or
   a shared library of any target. The archive of `libfancy.dylib` is
   `libfancy-symbols.zip`, as the one of `app.exe` is `app-symbols.zip`. */
static void stem_of(const char *name, struct text *out)
{
    static const char *const suffixes[] = {".exe", ".dylib", ".so", ".dll"};
    size_t i;

    text_append(out, files_base_name(name));
    for (i = 0; i < sizeof suffixes / sizeof suffixes[0]; i++) {
        size_t length = strlen(suffixes[i]);
        if (ends_with(text_cstr(out), suffixes[i]) && out->length > length) {
            out->length -= length;
            out->data[out->length] = '\0';
            break;
        }
    }
}

/* Whether the bytes of a file named name are a program: a name without
   a suffix or with `.exe`, and the header of an executable. A Mach-O
   file says so in its header. ELF writes a static program of musl as a
   shared object, so its name tells it from a library. */
static bool is_program(const char *name, const struct text *bytes)
{
    const unsigned char *p = (const unsigned char *)bytes->data;

    if (strchr(name, '.') != NULL && !ends_with(name, ".exe")) {
        return false;
    }
    if (bytes->length >= 16 && p[0] == 0xcf && p[1] == 0xfa &&
        p[2] == 0xed && p[3] == 0xfe) {
        return p[12] == 2;
    }
    return (bytes->length >= 4 && memcmp(p, "\177ELF", 4) == 0) ||
           (bytes->length >= 2 && p[0] == 'M' && p[1] == 'Z');
}

/* One binary of a deployment. */
struct binary {
    struct text path;
    struct text id;
    struct text version;
};

struct binaries {
    struct binary *items;
    size_t count;
    size_t capacity;
    size_t problems;
};

static void binaries_free(struct binaries *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        text_free(&list->items[i].path);
        text_free(&list->items[i].id);
        text_free(&list->items[i].version);
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

/* Add the binary at path, once. A file that is not there or carries no
   build id is reported and counted. */
static void add_binary(struct binaries *list, const char *path)
{
    struct text bytes = {0};
    struct binary *b;
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (strcmp(text_cstr(&list->items[i].path), path) == 0) {
            return;
        }
    }
    if (!files_read(path, &bytes)) {
        printf("missing %s: no such file\n", path);
        list->problems++;
        return;
    }
    if (list->count == list->capacity) {
        list->items = files_grow(list->items, &list->capacity,
                                 sizeof *list->items);
    }
    b = &list->items[list->count];
    memset(b, 0, sizeof *b);
    if (!symmap_notice(&bytes, &b->id, &b->version)) {
        printf("missing %s: it carries no build id of Anti\n", path);
        list->problems++;
        text_free(&b->id);
        text_free(&b->version);
    } else {
        text_append(&b->path, path);
        list->count++;
    }
    text_free(&bytes);
}

/* What the runtime configuration names: the directories of `plugins`
   and the libraries of `[injections]`, each by interface. */
struct configuration {
    struct text dir;
    struct texts plugins;
    struct texts interfaces;
    struct texts libraries;
};

static void configuration_free(struct configuration *c)
{
    text_free(&c->dir);
    texts_free(&c->plugins);
    texts_free(&c->interfaces);
    texts_free(&c->libraries);
}

/* The file at path and the files that include it. */
struct including {
    const char *path;
    const struct including *from;
};

/* Read one file of the configuration. Its includes come first, each
   relative to the file that names it, so the including file wins per
   key, as it does for the runtime. */
static bool read_configuration(struct configuration *c, const char *path,
                               const struct including *from, int depth)
{
    const struct including *on;
    struct including here;
    struct text bytes = {0};
    struct text dir = {0};
    struct anti_toml *doc;
    int64_t count;
    int64_t i;
    bool ok = true;

    /* depth counts the files above this one. The bound and the message
       are the runtime's, from src/rt/conf_include.h. */
    for (on = from; on != NULL; on = on->from) {
        if (strcmp(on->path, path) == 0) {
            break;
        }
    }
    if (on != NULL || depth > ANTI_CONF_INCLUDE_DEPTH) {
        fprintf(stderr, "anti: " ANTI_CONF_INCLUDE_CYCLE "\n", path);
        return false;
    }
    if (!files_read(path, &bytes)) {
        fprintf(stderr, "anti: cannot read %s\n", path);
        return false;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        fprintf(stderr, "anti: %s is no runtime configuration\n", path);
        return false;
    }
    here.path = path;
    here.from = from;
    directory_of(path, &dir);
    count = anti_rt_toml_count(doc);
    for (i = 0; ok && i < count; i++) {
        const char *key = (const char *)anti_rt_toml_key(doc, i).ptr;
        const char *value = (const char *)anti_rt_toml_value(doc, i).ptr;
        if (strcmp(key, "include") == 0 || strncmp(key, "include.", 8) == 0) {
            struct text include = {0};
            resolved(text_cstr(&dir), value, &include);
            ok = read_configuration(c, text_cstr(&include), &here, depth + 1);
            text_free(&include);
        }
    }
    for (i = 0; ok && i < count; i++) {
        const char *key = (const char *)anti_rt_toml_key(doc, i).ptr;
        const char *value = (const char *)anti_rt_toml_value(doc, i).ptr;
        size_t k;
        /* The array of `plugins` stands as plugins.0 and further, and a
           file that sets it replaces what an include gave. A text holds
           the directories as `--anti.plugins` writes them. */
        if (strcmp(key, "runtime.plugins") == 0 ||
            strcmp(key, "runtime.plugins.0") == 0) {
            texts_free(&c->plugins);
        }
        if (strcmp(key, "runtime.plugins") == 0) {
            const char *at = value;
            while (*at != '\0') {
                const char *stop = strchr(at, ':');
                size_t length = stop != NULL ? (size_t)(stop - at)
                                             : strlen(at);
                if (length > 0) {
                    texts_add(&c->plugins, at, length);
                }
                at += length + (stop != NULL ? 1 : 0);
            }
        } else if (strncmp(key, "runtime.plugins.", 16) == 0) {
            texts_add(&c->plugins, value, strlen(value));
        } else if (strncmp(key, "injections.", 11) == 0) {
            const char *name = key + 11;
            for (k = 0; k < c->interfaces.count; k++) {
                if (strcmp(text_cstr(&c->interfaces.items[k]), name) == 0) {
                    break;
                }
            }
            if (k == c->interfaces.count) {
                texts_add(&c->interfaces, name, strlen(name));
                texts_add(&c->libraries, "", 0);
            }
            c->libraries.items[k].length = 0;
            text_append(&c->libraries.items[k], value);
        }
    }
    anti_rt_toml_free(doc);
    text_free(&dir);
    return ok;
}

/* DESIGN: the configuration names no program. It stands beside the
   program it configures, so the program is every executable of Anti in
   the directory of the file. A relative path of `plugins` and of
   `[injections]` is read against that directory, where a deployment
   runs its program from. */
static bool find_binaries(const char *conf, struct binaries *out)
{
    struct configuration c;
    struct files_list files = {0};
    size_t programs;
    size_t i;

    memset(&c, 0, sizeof c);
    directory_of(conf, &c.dir);
    if (!read_configuration(&c, conf, NULL, 0)) {
        configuration_free(&c);
        return false;
    }
    files_list_dir(text_cstr(&c.dir), &files);
    for (i = 0; i < files.count; i++) {
        struct text bytes = {0};
        struct text id = {0};
        struct text version = {0};
        const char *path = text_cstr(&files.items[i]);
        if (files_read(path, &bytes) && is_program(files_base_name(path), &bytes) &&
            symmap_notice(&bytes, &id, &version)) {
            add_binary(out, path);
        }
        text_free(&bytes);
        text_free(&id);
        text_free(&version);
    }
    programs = out->count;
    if (programs == 0) {
        printf("missing the program: %s holds no program of Anti\n",
               text_cstr(&c.dir));
        out->problems++;
    }
    files_list_free(&files);
    for (i = 0; i < c.plugins.count; i++) {
        struct text dir = {0};
        struct text index = {0};
        struct text bytes = {0};
        struct anti_toml *doc = NULL;
        resolved(text_cstr(&c.dir), text_cstr(&c.plugins.items[i]), &dir);
        text_appendf(&index, "%s/%s", text_cstr(&dir), PLUGIN_INDEX);
        if (files_read(text_cstr(&index), &bytes)) {
            doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                                    (int64_t)bytes.length);
        }
        if (doc == NULL) {
            printf("missing %s: no index of plugins\n", text_cstr(&index));
            out->problems++;
        } else {
            int64_t n;
            for (n = 0; n < anti_rt_toml_count(doc); n++) {
                const char *key = (const char *)anti_rt_toml_key(doc, n).ptr;
                if (strncmp(key, "library.", 8) == 0 &&
                    ends_with(key, ".path")) {
                    struct text library = {0};
                    resolved(text_cstr(&dir),
                             (const char *)anti_rt_toml_value(doc, n).ptr,
                             &library);
                    add_binary(out, text_cstr(&library));
                    text_free(&library);
                }
            }
            anti_rt_toml_free(doc);
        }
        text_free(&dir);
        text_free(&index);
        text_free(&bytes);
    }
    /* An empty value is `discover`, whose library one of the `plugins`
       directories holds. */
    for (i = 0; i < c.libraries.count; i++) {
        if (c.libraries.items[i].length > 0) {
            struct text library = {0};
            resolved(text_cstr(&c.dir), text_cstr(&c.libraries.items[i]),
                     &library);
            add_binary(out, text_cstr(&library));
            text_free(&library);
        }
    }
    configuration_free(&c);
    return true;
}

/* The symbols of one build: its id, the module and version they name,
   the archive they came from and the entries that hold them. */
struct unit {
    struct text module;
    struct text id;
    struct text version;
    struct text source;
    struct texts names;
    struct texts bytes;
};

struct units {
    struct unit *items;
    size_t count;
    size_t capacity;
};

static struct unit *units_add(struct units *list)
{
    struct unit *u;

    if (list->count == list->capacity) {
        list->items = files_grow(list->items, &list->capacity,
                                 sizeof *list->items);
    }
    u = &list->items[list->count++];
    memset(u, 0, sizeof *u);
    return u;
}

static void units_free(struct units *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        struct unit *u = &list->items[i];
        text_free(&u->module);
        text_free(&u->id);
        text_free(&u->version);
        text_free(&u->source);
        texts_free(&u->names);
        texts_free(&u->bytes);
    }
    free(list->items);
    memset(list, 0, sizeof *list);
}

/* The entry of a unit whose name ends with suffix, or NULL. */
static const struct text *unit_entry(const struct unit *u, const char *suffix)
{
    size_t i;

    for (i = 0; i < u->names.count; i++) {
        if (ends_with(text_cstr(&u->names.items[i]), suffix)) {
            return &u->bytes.items[i];
        }
    }
    return NULL;
}

/* The id a map names on its `# build` line, which follows the first
   line of the map. */
static bool map_id(const struct text *map, struct text *id)
{
    static const char line[] = "\n" SYMMAP_BUILD_LINE;
    const char *at = strstr(text_cstr(map), line);

    if (at == NULL || strlen(at) < sizeof line - 1 + ANTI_BUILD_ID_LENGTH) {
        return false;
    }
    text_append_bytes(id, at + sizeof line - 1, ANTI_BUILD_ID_LENGTH);
    return true;
}

/* A deployment archive: the index names each unit, and the entries of
   a unit stand under its id. */
static bool load_deployment(const struct zip_archive *z, const char *path,
                            const struct text *index, struct units *out)
{
    struct anti_toml *doc = anti_rt_toml_read(
        (const unsigned char *)index->data, (int64_t)index->length);
    size_t first = out->count;
    int64_t n;
    size_t i;

    if (doc == NULL) {
        fprintf(stderr, "anti: %s holds a broken %s\n", path, INDEX_NAME);
        return false;
    }
    for (n = 0; n < anti_rt_toml_count(doc); n++) {
        const char *key = (const char *)anti_rt_toml_key(doc, n).ptr;
        const char *value = (const char *)anti_rt_toml_value(doc, n).ptr;
        const char *field = strrchr(key, '.');
        struct unit *u;
        if (strncmp(key, "module.", 7) != 0 || field == NULL) {
            continue;
        }
        if (ends_with(key, ".module")) {
            u = units_add(out);
            text_append(&u->module, value);
            continue;
        }
        if (out->count == first) {
            continue;
        }
        u = &out->items[out->count - 1];
        if (strcmp(field, ".id") == 0) {
            text_append(&u->id, value);
        } else if (strcmp(field, ".version") == 0) {
            text_append(&u->version, value);
        } else if (strcmp(field, ".source") == 0) {
            text_append(&u->source, value);
        }
    }
    anti_rt_toml_free(doc);
    for (i = 0; i < z->count; i++) {
        const char *name = text_cstr(&z->items[i].name);
        const char *slash = strchr(name, '/');
        size_t k;
        if (slash == NULL) {
            continue;
        }
        for (k = first; k < out->count; k++) {
            struct unit *u = &out->items[k];
            /* A unit the index gives no id names no entry. An entry
               name that starts with `/` would otherwise match it, with
               memcmp over a null pointer. */
            if (u->id.length > 0 && u->id.length == (size_t)(slash - name) &&
                memcmp(u->id.data, name, u->id.length) == 0) {
                struct text *bytes;
                texts_add(&u->names, slash + 1, strlen(slash + 1));
                bytes = texts_add(&u->bytes, "", 0);
                if (!zip_unpack(z, i, bytes)) {
                    return false;
                }
                break;
            }
        }
    }
    return true;
}

/* Read the archive at path into units. A deployment archive holds an
   index and one directory per id. The archive of one build holds its
   entries at the top, and its id is the one of the twin or the map. */
static bool load_archive(const char *path, struct units *out)
{
    struct zip_archive z;
    struct unit *u;
    struct text index = {0};
    const struct text *twin;
    const struct text *map;
    size_t i;
    bool ok = true;

    if (!zip_read(path, &z)) {
        zip_archive_free(&z);
        return false;
    }
    for (i = 0; i < z.count; i++) {
        if (strcmp(text_cstr(&z.items[i].name), INDEX_NAME) == 0) {
            ok = zip_unpack(&z, i, &index) &&
                 load_deployment(&z, path, &index, out);
            text_free(&index);
            zip_archive_free(&z);
            return ok;
        }
    }
    u = units_add(out);
    text_append(&u->source, path);
    for (i = 0; ok && i < z.count; i++) {
        struct text *bytes;
        texts_add(&u->names, z.items[i].name.data, z.items[i].name.length);
        bytes = texts_add(&u->bytes, "", 0);
        ok = zip_unpack(&z, i, bytes);
    }
    zip_archive_free(&z);
    if (!ok) {
        return false;
    }
    twin = unit_entry(u, ".debug");
    map = unit_entry(u, ".map");
    if (twin == NULL || !symmap_notice(twin, &u->id, &u->version)) {
        u->id.length = 0;
        if (map == NULL || !map_id(map, &u->id)) {
            fprintf(stderr, "anti: %s holds no build id\n", path);
            return false;
        }
    }
    /* The module is the name of the archive without its suffix, which
       is the stem of the binary it belongs to. */
    text_append(&u->module, files_base_name(path));
    if (ends_with(text_cstr(&u->module), ARCHIVE_SUFFIX)) {
        u->module.length -= sizeof ARCHIVE_SUFFIX - 1;
        u->module.data[u->module.length] = '\0';
    }
    return true;
}

/* The unit of id, or NULL. */
static const struct unit *unit_of(const struct units *list, const char *id)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (strcmp(text_cstr(&list->items[i].id), id) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

/* Whether a unit belongs to the binary at path by name: its module has
   the stem of the binary. */
static bool same_module(const struct unit *u, const char *path)
{
    struct text a = {0};
    struct text b = {0};
    bool same;

    stem_of(text_cstr(&u->module), &a);
    stem_of(path, &b);
    same = strcmp(text_cstr(&a), text_cstr(&b)) == 0;
    text_free(&a);
    text_free(&b);
    return same;
}

/* The archive of the binary at path, beside it or in from. */
static void archive_of(const char *path, const char *from, struct text *out)
{
    struct text dir = {0};
    struct text stem = {0};

    if (from != NULL) {
        text_append(&dir, from);
    } else {
        directory_of(path, &dir);
    }
    stem_of(path, &stem);
    text_appendf(out, "%s/%s%s", text_cstr(&dir), text_cstr(&stem),
                 ARCHIVE_SUFFIX);
    text_free(&dir);
    text_free(&stem);
}

/* What the symbols of one binary are. */
enum state { PRESENT, STALE, MISSING };

static const char *state_name(enum state s)
{
    return s == PRESENT ? "present" : s == STALE ? "stale" : "missing";
}

/* The state of the binary b against units, and the unit that holds its
   symbols when they are present. */
static enum state state_of(const struct binary *b, const struct units *units,
                           const struct unit **found)
{
    size_t i;

    *found = unit_of(units, text_cstr(&b->id));
    if (*found != NULL) {
        return PRESENT;
    }
    for (i = 0; i < units->count; i++) {
        if (same_module(&units->items[i], text_cstr(&b->path))) {
            return STALE;
        }
    }
    return MISSING;
}

/* Write a literal string of TOML, which takes every byte but the quote
   and the end of a line as it stands. A path of Windows keeps its
   backslashes that way. */
static void toml_literal(struct text *out, const char *key, const char *value)
{
    text_appendf(out, "%s = '", key);
    for (; *value != '\0'; value++) {
        if (*value != '\'' && *value != '\n' && *value != '\r') {
            text_append_bytes(out, value, 1);
        }
    }
    text_append(out, "'\n");
}

int syms_inventory(const char *conf, const char *from, const char *out)
{
    struct binaries binaries = {0};
    struct units folded = {0};
    struct text index = {0};
    struct zip_entry *entries = NULL;
    size_t entry_count = 0;
    size_t total = 1;
    size_t i;
    size_t j;
    bool ok;

    if (!find_binaries(conf, &binaries)) {
        return 1;
    }
    text_append(&index, "# The symbols archives of one deployment, by build "
                        "id. Each entry stands\n# under a directory named "
                        "after its id.\n");
    for (i = 0; i < binaries.count; i++) {
        const struct binary *b = &binaries.items[i];
        struct units units = {0};
        struct text archive = {0};
        const struct unit *found = NULL;
        enum state s = MISSING;
        archive_of(text_cstr(&b->path), from, &archive);
        if (files_exists(text_cstr(&archive)) &&
            load_archive(text_cstr(&archive), &units)) {
            s = state_of(b, &units, &found);
        }
        printf("%s %s %s\n", state_name(s), text_cstr(&b->path),
               text_cstr(&b->id));
        if (s == PRESENT && unit_of(&folded, text_cstr(&b->id)) == NULL) {
            struct unit *u = units_add(&folded);
            text_append(&u->module, files_base_name(text_cstr(&b->path)));
            text_append(&u->id, text_cstr(&b->id));
            text_append(&u->version, text_cstr(&b->version));
            text_append(&u->source, text_cstr(&archive));
            for (j = 0; j < found->names.count; j++) {
                const struct text *name = &found->names.items[j];
                const struct text *bytes = &found->bytes.items[j];
                struct text *keyed = texts_add(&u->names, "", 0);
                text_appendf(keyed, "%s/%s", text_cstr(&b->id),
                             text_cstr(name));
                texts_add(&u->bytes, bytes->data, bytes->length);
            }
            total += found->names.count;
            text_append(&index, "\n[[module]]\n");
            toml_literal(&index, "module", text_cstr(&u->module));
            toml_literal(&index, "id", text_cstr(&u->id));
            toml_literal(&index, "version", text_cstr(&u->version));
            toml_literal(&index, "source", text_cstr(&u->source));
        }
        units_free(&units);
        text_free(&archive);
    }
    entries = files_array(total, sizeof *entries);
    entries[entry_count].name = INDEX_NAME;
    entries[entry_count].bytes = index.data;
    entries[entry_count].size = index.length;
    entry_count++;
    for (i = 0; i < folded.count; i++) {
        const struct unit *u = &folded.items[i];
        for (j = 0; j < u->names.count; j++) {
            entries[entry_count].name = text_cstr(&u->names.items[j]);
            entries[entry_count].bytes = u->bytes.items[j].data;
            entries[entry_count].size = u->bytes.items[j].length;
            entries[entry_count].executable =
                ends_with(text_cstr(&u->names.items[j]), ".debug");
            entry_count++;
        }
    }
    ok = zip_write(out, entries, entry_count);
    if (ok) {
        printf("wrote %s with %zu of %zu modules\n", out, folded.count,
               binaries.count + binaries.problems);
    }
    free(entries);
    units_free(&folded);
    binaries_free(&binaries);
    text_free(&index);
    return ok ? 0 : 1;
}

int syms_check(const char *conf, const char *const *symbols, size_t count)
{
    struct binaries binaries = {0};
    struct units given = {0};
    size_t absent;
    size_t i;

    for (i = 0; i < count; i++) {
        if (!load_archive(symbols[i], &given)) {
            units_free(&given);
            return 1;
        }
    }
    if (!find_binaries(conf, &binaries)) {
        units_free(&given);
        return 1;
    }
    absent = binaries.problems;
    for (i = 0; i < binaries.count; i++) {
        const struct binary *b = &binaries.items[i];
        const struct unit *found = NULL;
        struct units beside = {0};
        struct text archive = {0};
        enum state s = MISSING;
        if (count > 0) {
            s = state_of(b, &given, &found);
        } else {
            archive_of(text_cstr(&b->path), NULL, &archive);
            if (files_exists(text_cstr(&archive)) &&
                load_archive(text_cstr(&archive), &beside)) {
                s = state_of(b, &beside, &found);
            }
        }
        printf("%s %s %s\n", state_name(s), text_cstr(&b->path),
               text_cstr(&b->id));
        absent += s != PRESENT;
        units_free(&beside);
        text_free(&archive);
    }
    binaries_free(&binaries);
    units_free(&given);
    return absent > 0 ? 1 : 0;
}

/* DESIGN: the lines of a map and of a trace come from other machines.
   sscanf leaves a number that does not fit its object undefined, so a
   cursor reads them instead. It takes digits alone and refuses a sign
   and a value above 64 bits. It names each token by its start and its
   length, so no line is copied into a buffer of fixed size. */
struct cursor {
    const char *at;
    const char *end;
};

static bool is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* Pass over blanks. Returns whether there was one. */
static bool cursor_blank(struct cursor *c)
{
    const char *from = c->at;

    while (c->at < c->end && is_blank(*c->at)) {
        c->at++;
    }
    return c->at > from;
}

static int digit_of(char c, unsigned base)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (base == 16 && c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (base == 16 && c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* The digits in base at the cursor as one value. Returns false for no
   digit and for a value above UINT64_MAX. */
static bool cursor_number(struct cursor *c, unsigned base, uint64_t *out)
{
    uint64_t value = 0;
    const char *from = c->at;
    int d;

    while (c->at < c->end && (d = digit_of(*c->at, base)) >= 0) {
        if (value > (UINT64_MAX - (uint64_t)d) / base) {
            return false;
        }
        value = value * base + (uint64_t)d;
        c->at++;
    }
    *out = value;
    return c->at > from;
}

/* A hex number, with or without `0x`. */
static bool cursor_hex(struct cursor *c, uint64_t *out)
{
    if (c->end - c->at > 2 && c->at[0] == '0' &&
        (c->at[1] == 'x' || c->at[1] == 'X')) {
        c->at += 2;
    }
    return cursor_number(c, 16, out);
}

static bool cursor_char(struct cursor *c, char want)
{
    if (c->at < c->end && *c->at == want) {
        c->at++;
        return true;
    }
    return false;
}

/* The bytes up to the next blank or the end. Returns false for none. */
static bool cursor_token(struct cursor *c, const char **token, size_t *length)
{
    const char *from = c->at;

    while (c->at < c->end && !is_blank(*c->at)) {
        c->at++;
    }
    *token = from;
    *length = (size_t)(c->at - from);
    return *length > 0;
}

/* Whether the n bytes at s are a location, `<file>:<line>`. */
static bool is_location(const char *s, size_t n)
{
    size_t digits = 0;

    while (digits < n && s[n - 1 - digits] >= '0' && s[n - 1 - digits] <= '9') {
        digits++;
    }
    return digits > 0 && digits + 1 < n && s[n - 1 - digits] == ':';
}

/* Append the name a person reads for the n bytes of a symbol at s. */
static void append_name(struct text *out, const char *s, size_t n)
{
    char *readable = malloc(n + 1);
    size_t length = readable != NULL
                        ? anti_rt_symbol_unescape(s, n, readable, n)
                        : 0;

    text_append_bytes(out, length > 0 ? readable : s, length > 0 ? length : n);
    free(readable);
}

/* One entry of a map: the range of a function, its name and where the
   debug information gave them, its file and line. */
struct mapped {
    uint64_t start;
    uint64_t end;
    struct text function;
    struct text where;
};

bool syms_map_lookup(const char *map, uint64_t vaddr, struct text *function,
                     struct text *where_out)
{
    const char *line = map;

    while (*line != '\0') {
        const char *stop = strchr(line, '\n');
        size_t length = stop != NULL ? (size_t)(stop - line) : strlen(line);
        struct cursor c;
        uint64_t start;
        uint64_t end;
        const char *name;
        size_t name_length;
        const char *where;
        size_t where_length = 0;
        c.at = line;
        c.end = line + length;
        cursor_blank(&c);
        /* DESIGN: a name may hold a blank, as `Pair<int, str>.swap`
           does, and a location never does, since it ends the line in
           the form `<file>:<line>`. The name is therefore the rest of
           the line, less a last word of that form. */
        if (line[0] != '#' && cursor_hex(&c, &start) &&
            cursor_char(&c, '-') && cursor_hex(&c, &end) &&
            cursor_blank(&c) && c.at < c.end && vaddr >= start &&
            (vaddr < end || (end == start && vaddr == start))) {
            const char *stop_at = c.end;
            name = c.at;
            while (stop_at > name && is_blank(stop_at[-1])) {
                stop_at--;
            }
            where = stop_at;
            while (where > name && !is_blank(where[-1])) {
                where--;
            }
            where_length = (size_t)(stop_at - where);
            if (where > name && is_location(where, where_length)) {
                stop_at = where;
                while (stop_at > name && is_blank(stop_at[-1])) {
                    stop_at--;
                }
            } else {
                where_length = 0;
            }
            name_length = (size_t)(stop_at - name);
            append_name(function, name, name_length);
            if (where_length > 0) {
                text_append_bytes(where_out, where, where_length);
            }
            return true;
        }
        line += length + (stop != NULL ? 1 : 0);
    }
    return false;
}

/* The function of vaddr from the lines of a map, and its file and line
   as `file:line` in where. */
static bool map_lookup(const struct text *map, uint64_t vaddr,
                       struct mapped *out)
{
    return syms_map_lookup(text_cstr(map), vaddr, &out->function,
                           &out->where);
}

/* The function and the line of vaddr in binary, an ELF or a Mach-O file,
   or a debug twin of one. Returns false for a file of neither kind. */
static bool resolve_binary(const struct text *binary, uint64_t vaddr,
                           struct mapped *out)
{
    const uint8_t *bytes = (const uint8_t *)binary->data;
    struct anti_found found;
    struct anti_macho_table t;
    const char *object;
    const char *symbol;
    uint64_t start;
    uint64_t base;

    memset(&found, 0, sizeof found);
    if (binary->length >= 4 && memcmp(bytes, "\177ELF", 4) == 0) {
        if (anti_rt_elf_function(bytes, binary->length, vaddr, &found)) {
            append_name(&out->function, found.function,
                        found.function_length);
        }
        if (anti_rt_elf_line(bytes, binary->length, vaddr, &found) &&
            found.file != NULL) {
            text_append_bytes(&out->where, found.file, found.file_length);
            text_appendf(&out->where, ":%lld", (long long)found.line);
        }
        return true;
    }
    if (!anti_rt_macho_text(bytes, binary->length, &base)) {
        return false;
    }
    if (anti_rt_macho_table(bytes, binary->length, false, 0, &t)) {
        if (anti_rt_macho_function(&t, vaddr, &found)) {
            append_name(&out->function, found.function,
                        found.function_length);
        }
        /* The link of Mach-O leaves the line table in the objects that
           its debug map names, which stand where it was built. */
        if (anti_rt_macho_debug_map(&t, vaddr, &object, &symbol, &start)) {
            struct text file = {0};
            if (files_read(object, &file)) {
                anti_rt_macho_relocate((uint8_t *)file.data, file.length);
                if (anti_rt_macho_object_line((const uint8_t *)file.data,
                                              file.length, symbol,
                                              vaddr - start, &found) &&
                    found.file != NULL) {
                    text_append_bytes(&out->where, found.file,
                                      found.file_length);
                    text_appendf(&out->where, ":%lld",
                                 (long long)found.line);
                }
            }
            text_free(&file);
        }
    }
    return true;
}

/* The function and the line of offset into the module the unit holds
   the symbols of. The twin answers first. The map answers for what it
   does not. */
static bool resolve_frame(const struct unit *u, uint64_t offset,
                          struct mapped *out)
{
    const struct text *twin = unit_entry(u, ".debug");
    const struct text *map = unit_entry(u, ".map");
    struct mapped mapped;
    uint64_t base = 0;
    uint64_t vaddr;

    memset(&mapped, 0, sizeof mapped);
    if (twin == NULL) {
        return false;
    }
    /* The offset of a Mach-O frame counts from its text, and the one of
       an ELF frame from address 0. */
    if (!anti_rt_macho_text((const uint8_t *)twin->data, twin->length,
                            &base)) {
        base = 0;
    }
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    vaddr = base + offset - 1;
    if (!resolve_binary(twin, vaddr, out)) {
        return false;
    }
    if ((out->function.length == 0 || out->where.length == 0) && map != NULL &&
        map_lookup(map, vaddr, &mapped)) {
        if (out->function.length == 0) {
            text_append(&out->function, text_cstr(&mapped.function));
        }
        if (out->where.length == 0) {
            text_append(&out->where, text_cstr(&mapped.where));
        }
    }
    text_free(&mapped.function);
    text_free(&mapped.where);
    return out->function.length > 0;
}

/* DESIGN: every line of the trace comes out as it went in. A frame
   whose module has symbols gains its function and its line after it. A
   frame no archive answers for stays raw. So does every line that is no
   frame, such as the message of an error before its trace. */
int syms_resolve(const char *trace, const char *const *symbols, size_t count)
{
    struct units units = {0};
    struct texts modules = {0};
    struct text input = {0};
    const char *line;
    size_t i;

    for (i = 0; i < count; i++) {
        if (!load_archive(symbols[i], &units)) {
            units_free(&units);
            return 1;
        }
    }
    if (!files_read(trace, &input)) {
        fprintf(stderr, "anti: cannot read %s\n", trace);
        units_free(&units);
        return 1;
    }
    line = text_cstr(&input);
    while (*line != '\0') {
        const char *stop = strchr(line, '\n');
        size_t length = stop != NULL ? (size_t)(stop - line) : strlen(line);
        size_t shown = length > 0 && line[length - 1] == '\r' ? length - 1
                                                              : length;
        struct cursor c;
        uint64_t address;
        uint64_t offset;
        uint64_t number;
        const char *id;
        size_t id_length;
        const char *base;
        size_t base_length;
        fwrite(line, 1, shown, stdout);
        c.at = line;
        c.end = line + shown;
        if (shown > 7 && memcmp(line, "module ", 7) == 0) {
            c.at += 7;
            cursor_blank(&c);
            if (cursor_number(&c, 10, &number) && cursor_blank(&c) &&
                cursor_token(&c, &id, &id_length) && cursor_blank(&c) &&
                cursor_token(&c, &base, &base_length) &&
                number == modules.count) {
                texts_add(&modules, id, id_length);
            }
        } else {
            cursor_blank(&c);
            if (cursor_hex(&c, &address) && cursor_blank(&c) &&
                cursor_number(&c, 10, &number) && cursor_char(&c, '+') &&
                cursor_hex(&c, &offset) && c.at == c.end &&
                number < modules.count) {
                const struct unit *u = unit_of(
                    &units, text_cstr(&modules.items[number]));
                struct mapped m;
                memset(&m, 0, sizeof m);
                if (u != NULL && resolve_frame(u, offset, &m)) {
                    printf(" %s", text_cstr(&m.function));
                    if (m.where.length > 0) {
                        printf(" %s", text_cstr(&m.where));
                    }
                }
                text_free(&m.function);
                text_free(&m.where);
            }
        }
        fputc('\n', stdout);
        line += length + (stop != NULL ? 1 : 0);
    }
    texts_free(&modules);
    text_free(&input);
    units_free(&units);
    return 0;
}

/* The modules a report names, each read once. A module the read failed
   for holds no bytes. */
struct report {
    struct texts paths;
    struct texts files;
};

static uint32_t big_endian(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
           (uint32_t)p[3];
}

/* The processor types of Mach-O that a frame of macOS names after the
   path of its module. */
struct slice_arch {
    const char *name;
    uint32_t cpu;
    uint32_t sub;
};

static const struct slice_arch slice_archs[] = {
    {"arm64", 0x0100000c, 0},
    {"arm64e", 0x0100000c, 2},
    {"x86_64", 0x01000007, 3},
    {"x86_64h", 0x01000007, 8},
};

/* DESIGN: macOS ships its own libraries and the runtime of
   AddressSanitizer as universal files, one Mach-O file per processor
   behind a header of their offsets. The frame names the processor, and
   the reader takes that file alone. A file of one processor stays as it
   is. The header and each entry are big-endian words. Returns false for
   a universal file without the processor. */
static bool thin_slice(struct text *file, const char *arch, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)file->data;
    const struct slice_arch *want = NULL;
    uint32_t count;
    uint32_t i;
    size_t k;

    if (file->length < 8 || big_endian(bytes) != 0xcafebabe) {
        return true;
    }
    for (k = 0; k < sizeof slice_archs / sizeof slice_archs[0]; k++) {
        if (strlen(slice_archs[k].name) == length &&
            memcmp(slice_archs[k].name, arch, length) == 0) {
            want = &slice_archs[k];
        }
    }
    count = big_endian(bytes + 4);
    for (i = 0; want != NULL && i < count && 8 + 20 * (size_t)(i + 1) <=
                                                 file->length;
         i++) {
        const unsigned char *entry = bytes + 8 + 20 * (size_t)i;
        size_t offset = big_endian(entry + 8);
        size_t size = big_endian(entry + 12);
        if (big_endian(entry) != want->cpu ||
            (big_endian(entry + 4) & 0x00ffffff) != want->sub ||
            offset > file->length || size > file->length - offset) {
            continue;
        }
        memmove(file->data, file->data + offset, size);
        file->length = size;
        return true;
    }
    return false;
}

/* The module at the length bytes of path, and in a universal file the
   one of the arch_length bytes of arch. */
static const struct text *report_module(struct report *r, const char *path,
                                        size_t length, const char *arch,
                                        size_t arch_length)
{
    struct text *file;
    struct text *key;
    size_t i;

    for (i = 0; i < r->paths.count; i++) {
        key = &r->paths.items[i];
        if (key->length == length + 1 + arch_length &&
            memcmp(key->data, path, length) == 0 &&
            memcmp(key->data + length + 1, arch, arch_length) == 0) {
            return &r->files.items[i];
        }
    }
    key = texts_add(&r->paths, path, length);
    file = texts_add(&r->files, "", 0);
    if (!files_read(text_cstr(key), file) ||
        !thin_slice(file, arch, arch_length)) {
        file->length = 0;
    }
    text_append(key, ":");
    text_append_bytes(key, arch, arch_length);
    return file;
}

/* DESIGN: `anti run` and `anti test` run a program of --memory-checks
   with symbolize=0, so each frame of its report names its module and the
   offset in it: `#3 0x... (/path/prog:arm64+0x100000a10)` on macOS and
   `(/path/prog+0xa10)` on Linux. A frame whose module Anti's symbolizer
   reads gets `in <function> <file>:<line>` in place of the module, the
   form of the runtime's own symbolizer. Every other line comes out as
   it went in. The runtime already printed the instruction before the
   return address, so the offset is looked up as it stands. */
static void report_line(void *context, const char *line, size_t length)
{
    struct report *r = context;
    size_t shown = length;
    const char *stop;
    const char *plus = NULL;
    const char *colon = NULL;
    const char *p;
    struct cursor c;
    struct mapped m;
    uint64_t number;
    uint64_t offset;
    const struct text *module;

    while (shown > 0 && (line[shown - 1] == '\n' || line[shown - 1] == '\r')) {
        shown--;
    }
    c.at = line;
    c.end = line + shown;
    cursor_blank(&c);
    if (!cursor_char(&c, '#') || !cursor_number(&c, 10, &number) ||
        !cursor_blank(&c) || !cursor_hex(&c, &number)) {
        fwrite(line, 1, length, stderr);
        return;
    }
    stop = c.at;
    cursor_blank(&c);
    if (!cursor_char(&c, '(') || c.end - c.at < 5 || c.end[-1] != ')') {
        fwrite(line, 1, length, stderr);
        return;
    }
    for (p = c.at; p + 3 <= c.end - 1; p++) {
        if (memcmp(p, "+0x", 3) == 0) {
            plus = p;
        }
    }
    for (p = c.at; plus != NULL && p < plus; p++) {
        if (*p == ':') {
            colon = p;
        } else if (*p == '/') {
            colon = NULL;
        }
    }
    memset(&m, 0, sizeof m);
    if (plus != NULL) {
        struct cursor o;
        o.at = plus + 1;
        o.end = c.end - 1;
        module = report_module(
            r, c.at, (size_t)((colon != NULL ? colon : plus) - c.at),
            colon != NULL ? colon + 1 : plus,
            colon != NULL ? (size_t)(plus - colon - 1) : 0);
        if (cursor_hex(&o, &offset) && o.at == o.end && module->length > 0 &&
            resolve_binary(module, offset, &m) && m.function.length > 0) {
            fwrite(line, 1, (size_t)(stop - line), stderr);
            fprintf(stderr, " in %s", text_cstr(&m.function));
            if (m.where.length > 0) {
                fprintf(stderr, " %s\n", text_cstr(&m.where));
            } else {
                fprintf(stderr, " %.*s\n", (int)(c.end - c.at + 1),
                        c.at - 1);
            }
            text_free(&m.function);
            text_free(&m.where);
            return;
        }
    }
    text_free(&m.function);
    text_free(&m.where);
    fwrite(line, 1, length, stderr);
}

/* Windows keeps the symbolizing of AddressSanitizer, as the DESIGN of
   platform_run_symbolized in platform.h says. */
int syms_run_checked(const char *const argv[])
{
    struct report r;
    struct text value = {0};
    const char *before = platform_getenv(SANITIZER_OPTIONS);
    int status;

    memset(&r, 0, sizeof r);
    if (before != NULL && before[0] != '\0') {
        text_appendf(&value, "%s:", before);
    }
    text_append(&value, NO_SYMBOLIZE);
    status = platform_run_symbolized(argv, SANITIZER_OPTIONS,
                                     text_cstr(&value), report_line, &r);
    texts_free(&r.paths);
    texts_free(&r.files);
    text_free(&value);
    return status;
}
