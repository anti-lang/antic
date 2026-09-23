/* `anti symbols inventory`, `check` and `resolve`.

   DESIGN: a binary is known by the build id in its licence notice, and
   an archive by the id of the debug twin it holds, which the notice of
   that twin carries as well. Nothing else ties the two together, so a
   renamed binary still finds its symbols and a rebuilt one never finds
   the symbols of its predecessor. The readers of rt/symbols.c answer
   for the twin, as they do for `anti.lang.StackTrace.symbolize`, and the
   map answers where the twin does not. */
#include "syms.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "symbols.h"
#include "text.h"
#include "toml.h"
#include "zip.h"

/* The name of the archive a build writes beside a binary is its stem
   with this suffix, and the index of a deployment archive has this
   name. */
#define ARCHIVE_SUFFIX "-symbols.zip"
#define INDEX_NAME "index.toml"
#define PLUGIN_INDEX "anti-plugins.toml"

/* The runtime refuses an include nested deeper than this, and so does
   the reader here. */
enum { INCLUDE_DEPTH = 32 };

/* The notice of every Anti binary begins with these two lines, the
   second one holding the 64 digits of the id. */
static const char notice_begin[] = "ANTI_LICENSES_BEGIN\nbuild ";
enum { ID_LENGTH = 64 };

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

static bool read_all(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[65536];
    size_t n;

    if (f == NULL) {
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
    return true;
}

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
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items,
                              list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            die_out_of_memory();
        }
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

/* The last part of a path, after either separator. */
static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    if (back != NULL && (slash == NULL || back > slash)) {
        slash = back;
    }
    return slash != NULL ? slash + 1 : path;
}

/* The directory of a path, or `.` for a bare name. */
static void directory_of(const char *path, struct text *out)
{
    const char *name = base_name(path);

    if (name == path) {
        text_append(out, ".");
    } else {
        text_append_bytes(out, path, (size_t)(name - path - 1));
        if (out->length == 0) {
            text_append(out, "/");
        }
    }
}

static bool is_absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' ||
           (path[0] != '\0' && path[1] == ':');
}

/* path, resolved against the directory dir unless it is absolute. */
static void resolved(const char *dir, const char *path, struct text *out)
{
    if (is_absolute(path)) {
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

    text_append(out, base_name(name));
    for (i = 0; i < sizeof suffixes / sizeof suffixes[0]; i++) {
        size_t length = strlen(suffixes[i]);
        if (ends_with(text_cstr(out), suffixes[i]) && out->length > length) {
            out->length -= length;
            out->data[out->length] = '\0';
            break;
        }
    }
}

/* The build id and the version of the notice in bytes. The version is
   the one of the last `package` line, which names the package of the
   compiled module. Returns false when the bytes hold no notice. */
static bool notice_of(const struct text *bytes, struct text *id,
                      struct text *version)
{
    size_t marker = sizeof notice_begin - 1;
    size_t i;

    for (i = 0; i + marker + ID_LENGTH < bytes->length; i++) {
        const char *at = bytes->data + i;
        const char *end = bytes->data + bytes->length;
        const char *line;
        size_t j;
        if (memcmp(at, notice_begin, marker) != 0) {
            continue;
        }
        for (j = 0; j < ID_LENGTH; j++) {
            char c = at[marker + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                break;
            }
        }
        if (j < ID_LENGTH || at[marker + ID_LENGTH] != '\n') {
            continue;
        }
        text_append_bytes(id, at + marker, ID_LENGTH);
        line = at + marker + ID_LENGTH + 1;
        while (line < end && end - line > 8 &&
               memcmp(line, "package ", 8) == 0) {
            const char *stop = memchr(line, '\n', (size_t)(end - line));
            const char *word;
            const char *after;
            if (stop == NULL) {
                break;
            }
            word = memchr(line + 8, ' ', (size_t)(stop - line - 8));
            after = word != NULL
                        ? memchr(word + 1, ' ', (size_t)(stop - word - 1))
                        : NULL;
            if (word != NULL) {
                version->length = 0;
                text_append_bytes(version, word + 1,
                                  (size_t)((after != NULL ? after : stop) -
                                           word - 1));
            }
            line = stop + 1;
            while (line < end && end - line > 12 &&
                   memcmp(line, "attribution ", 12) == 0) {
                stop = memchr(line, '\n', (size_t)(end - line));
                line = stop != NULL ? stop + 1 : end;
            }
        }
        return true;
    }
    return false;
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
    if (!read_all(path, &bytes)) {
        printf("missing %s: no such file\n", path);
        list->problems++;
        return;
    }
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items,
                              list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            die_out_of_memory();
        }
    }
    b = &list->items[list->count];
    memset(b, 0, sizeof *b);
    if (!notice_of(&bytes, &b->id, &b->version)) {
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

    for (on = from; on != NULL; on = on->from) {
        if (strcmp(on->path, path) == 0 || depth >= INCLUDE_DEPTH) {
            fprintf(stderr, "anti: %s includes itself\n", path);
            return false;
        }
    }
    if (!read_all(path, &bytes)) {
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
    struct file_list files = {0};
    size_t programs;
    size_t i;

    memset(&c, 0, sizeof c);
    directory_of(conf, &c.dir);
    if (!read_configuration(&c, conf, NULL, 0)) {
        configuration_free(&c);
        return false;
    }
    list_dir(text_cstr(&c.dir), &files);
    for (i = 0; i < files.count; i++) {
        struct text bytes = {0};
        struct text id = {0};
        struct text version = {0};
        const char *path = text_cstr(&files.items[i]);
        if (read_all(path, &bytes) && is_program(base_name(path), &bytes) &&
            notice_of(&bytes, &id, &version)) {
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
    file_list_free(&files);
    for (i = 0; i < c.plugins.count; i++) {
        struct text dir = {0};
        struct text index = {0};
        struct text bytes = {0};
        struct anti_toml *doc = NULL;
        resolved(text_cstr(&c.dir), text_cstr(&c.plugins.items[i]), &dir);
        text_appendf(&index, "%s/%s", text_cstr(&dir), PLUGIN_INDEX);
        if (read_all(text_cstr(&index), &bytes)) {
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
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items,
                              list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            die_out_of_memory();
        }
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

/* The id a map names on its `# build` line. */
static bool map_id(const struct text *map, struct text *id)
{
    static const char line[] = "\n# build ";
    const char *at = strstr(text_cstr(map), line);

    if (at == NULL || strlen(at) < sizeof line - 1 + ID_LENGTH) {
        return false;
    }
    text_append_bytes(id, at + sizeof line - 1, ID_LENGTH);
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
            if (u->id.length == (size_t)(slash - name) &&
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
    if (twin == NULL || !notice_of(twin, &u->id, &u->version)) {
        u->id.length = 0;
        if (map == NULL || !map_id(map, &u->id)) {
            fprintf(stderr, "anti: %s holds no build id\n", path);
            return false;
        }
    }
    /* The module is the name of the archive without its suffix, which
       is the stem of the binary it belongs to. */
    text_append(&u->module, base_name(path));
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
        if (path_exists(text_cstr(&archive)) &&
            load_archive(text_cstr(&archive), &units)) {
            s = state_of(b, &units, &found);
        }
        printf("%s %s %s\n", state_name(s), text_cstr(&b->path),
               text_cstr(&b->id));
        if (s == PRESENT && unit_of(&folded, text_cstr(&b->id)) == NULL) {
            struct unit *u = units_add(&folded);
            text_append(&u->module, base_name(text_cstr(&b->path)));
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
    entries = calloc(total, sizeof *entries);
    if (entries == NULL) {
        die_out_of_memory();
    }
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
            if (path_exists(text_cstr(&archive)) &&
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

/* One entry of a map: the range of a function, its name and where the
   debug information gave them, its file and line. */
struct mapped {
    uint64_t start;
    uint64_t end;
    struct text function;
    struct text where;
};

/* The function of vaddr from the lines of a map, and its file and line
   as `file:line` in where. */
static bool map_lookup(const struct text *map, uint64_t vaddr,
                       struct mapped *out)
{
    const char *line = text_cstr(map);

    while (*line != '\0') {
        const char *stop = strchr(line, '\n');
        size_t length = stop != NULL ? (size_t)(stop - line) : strlen(line);
        unsigned long long start;
        unsigned long long end;
        char name[512];
        char where[1024];
        char text[1600];
        int fields;
        if (length < sizeof text && line[0] != '#') {
            memcpy(text, line, length);
            text[length] = '\0';
            where[0] = '\0';
            fields = sscanf(text, "%llx-%llx %511s %1023s", &start, &end,
                            name, where);
            if (fields >= 3 && vaddr >= start &&
                (vaddr < end || (end == start && vaddr == start))) {
                text_append(&out->function, name);
                if (fields == 4) {
                    text_append(&out->where, where);
                }
                return true;
            }
        }
        line += length + (stop != NULL ? 1 : 0);
    }
    return false;
}

static uint64_t get64(const unsigned char *p)
{
    uint64_t v = 0;
    int i;

    for (i = 7; i >= 0; i--) {
        v = v << 8 | p[i];
    }
    return v;
}

static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

/* The address of the __TEXT segment of a Mach-O file, which is where
   its header lies when it runs. A trace gives an offset from the
   header. */
static bool macho_text(const struct text *file, uint64_t *out)
{
    const unsigned char *p = (const unsigned char *)file->data;
    size_t at = 32;
    uint32_t commands;
    uint32_t i;

    if (file->length < 32 || get32(p) != 0xfeedfacfu) {
        return false;
    }
    commands = get32(p + 16);
    for (i = 0; i < commands && at + 8 <= file->length; i++) {
        uint32_t kind = get32(p + at);
        uint32_t size = get32(p + at + 4);
        if (kind == 0x19 && at + 32 <= file->length &&
            strncmp((const char *)p + at + 8, "__TEXT", 16) == 0) {
            *out = get64(p + at + 24);
            return true;
        }
        if (size == 0) {
            break;
        }
        at += size;
    }
    return false;
}

/* The function and the line of offset into the module the unit holds
   the symbols of. The twin answers first. The map answers for what it
   does not. */
static bool resolve_frame(const struct unit *u, uint64_t offset,
                          struct mapped *out)
{
    const struct text *twin = unit_entry(u, ".debug");
    const struct text *map = unit_entry(u, ".map");
    struct anti_found found;
    struct mapped mapped;
    uint64_t base = 0;
    uint64_t vaddr;
    bool elf;
    bool macho;

    memset(&found, 0, sizeof found);
    memset(&mapped, 0, sizeof mapped);
    if (twin == NULL) {
        return false;
    }
    elf = twin->length >= 4 && memcmp(twin->data, "\177ELF", 4) == 0;
    macho = macho_text(twin, &base);
    if (!elf && !macho) {
        return false;
    }
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    vaddr = base + offset - 1;
    if (elf) {
        const uint8_t *bytes = (const uint8_t *)twin->data;
        if (anti_elf_function(bytes, twin->length, vaddr, &found)) {
            text_append_bytes(&out->function, found.function,
                              found.function_length);
        }
        if (anti_elf_line(bytes, twin->length, vaddr, &found) &&
            found.file != NULL) {
            text_append_bytes(&out->where, found.file, found.file_length);
            text_appendf(&out->where, ":%lld", (long long)found.line);
        }
    } else {
        struct anti_macho_table t;
        const char *object;
        const char *symbol;
        uint64_t start;
        if (anti_macho_table((const uint8_t *)twin->data, twin->length, false,
                             0, &t)) {
            if (anti_macho_function(&t, vaddr, &found)) {
                text_append_bytes(&out->function, found.function,
                                  found.function_length);
            }
            /* The link of Mach-O leaves the line table in the objects
               that its debug map names, which stand where it was
               built. */
            if (anti_macho_debug_map(&t, vaddr, &object, &symbol, &start)) {
                struct text bytes = {0};
                if (read_all(object, &bytes)) {
                    anti_macho_relocate((uint8_t *)bytes.data, bytes.length);
                    if (anti_macho_object_line((const uint8_t *)bytes.data,
                                               bytes.length, symbol,
                                               vaddr - start, &found) &&
                        found.file != NULL) {
                        text_append_bytes(&out->where, found.file,
                                          found.file_length);
                        text_appendf(&out->where, ":%lld",
                                     (long long)found.line);
                    }
                }
                text_free(&bytes);
            }
        }
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

/* DESIGN: every line of the trace comes out as it went in, and a frame
   whose module has symbols gains its function and its line after it. A
   frame no archive answers for stays raw, and so does every line that
   is no frame, such as the message of an error before its trace. */
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
    if (!read_all(trace, &input)) {
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
        char text[1024];
        unsigned long long address;
        unsigned long long offset;
        long long number;
        char id[80];
        int used = 0;
        fwrite(line, 1, shown, stdout);
        if (shown < sizeof text) {
            memcpy(text, line, shown);
            text[shown] = '\0';
            if (sscanf(text, "module %lld %79s %*s %n", &number, id, &used) ==
                    2 &&
                used > 0 && number == (long long)modules.count) {
                texts_add(&modules, id, strlen(id));
            } else if (sscanf(text, "%llx %lld+%llx%n", &address, &number,
                              &offset, &used) == 3 &&
                       (size_t)used == shown && number >= 0 &&
                       (size_t)number < modules.count) {
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
