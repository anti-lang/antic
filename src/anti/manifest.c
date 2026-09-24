/* The tables of `anti.toml` that a build passes to antic.

   DESIGN: the tool reads the manifest with the runtime's TOML reader,
   the one `anti.toml`, the logger and the runtime configuration share.
   The reader gives a flat list of keys, so `[inject]` and its sub-table
   `[inject.test]` arrive as `inject.<path>` and `inject.test.<path>`,
   which is what TOML itself makes of them. */
#include "manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "text.h"
#include "toml.h"

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

/* The interface of a key under `inject.`, or NULL when the key belongs
   to another table. `inject.test.` names the test table. */
static const char *interface_of(const char *key, size_t length, bool tests)
{
    static const char head[] = "inject.";
    static const char test_head[] = "inject.test.";
    size_t head_length = sizeof head - 1;
    size_t test_length = sizeof test_head - 1;
    bool is_test = length > test_length &&
                   memcmp(key, test_head, test_length) == 0;

    if (length <= head_length || memcmp(key, head, head_length) != 0) {
        return NULL;
    }
    if (is_test) {
        return tests ? key + test_length : NULL;
    }
    return key + head_length;
}

/* Put `Interface=Provider` in the table, replacing what an earlier
   entry of the same interface held. */
static void add_entry(struct manifest_inject *table, const char *interface,
                      const char *provider)
{
    size_t length = strlen(interface);
    struct text entry = {0};
    size_t i;

    text_appendf(&entry, "%s=%s", interface, provider);
    for (i = 0; i < table->count; i++) {
        if (strncmp(table->entries[i], interface, length) == 0 &&
            table->entries[i][length] == '=') {
            free((void *)table->entries[i]);
            table->entries[i] = text_cstr(&entry);
            return;
        }
    }
    table->entries[table->count++] = text_cstr(&entry);
}

bool manifest_inject_read(const char *path, bool tests,
                          struct manifest_inject *out)
{
    struct text bytes = {0};
    struct anti_toml *doc;
    int64_t count;
    int64_t i;

    memset(out, 0, sizeof *out);
    if (!files_read(path, &bytes)) {
        text_free(&bytes);
        return true;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        fprintf(stderr, "anti: %s is no TOML that anti reads\n", path);
        return false;
    }
    count = anti_rt_toml_count(doc);
    out->entries = malloc(((size_t)count + 1) * sizeof *out->entries);
    if (out->entries == NULL) {
        die_out_of_memory();
    }
    /* The plain table first, so an entry of `[inject.test]` replaces the
       one `[inject]` holds for that interface. */
    for (i = 0; i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        const char *interface =
            interface_of((const char *)key.ptr, (size_t)key.len, false);
        if (interface != NULL && value.len > 0) {
            add_entry(out, interface, (const char *)value.ptr);
        }
    }
    for (i = 0; tests && i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        const char *interface =
            interface_of((const char *)key.ptr, (size_t)key.len, true);
        if (interface != NULL && value.len > 0) {
            add_entry(out, interface, (const char *)value.ptr);
        }
    }
    anti_rt_toml_free(doc);
    return true;
}

/* The value of one key of the document, or NULL. */
static const char *value_of(const struct anti_toml *doc, const char *key)
{
    int64_t at = anti_rt_toml_find(doc, (const unsigned char *)key,
                                   (int64_t)strlen(key));
    struct anti_text value;

    if (at < 0) {
        return NULL;
    }
    value = anti_rt_toml_value(doc, at);
    return value.len > 0 ? (const char *)value.ptr : NULL;
}

bool manifest_layout_read(const char *path, struct text *src,
                          struct text *test, struct text *package)
{
    struct text bytes = {0};
    struct anti_toml *doc;
    const char *value;

    text_append(src, "src");
    text_append(test, "test");
    if (!files_read(path, &bytes)) {
        text_free(&bytes);
        return true;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        fprintf(stderr, "anti: %s is no TOML that anti reads\n", path);
        return false;
    }
    if ((value = value_of(doc, "layout.src")) != NULL) {
        src->length = 0;
        text_append(src, value);
    }
    if ((value = value_of(doc, "layout.test")) != NULL) {
        test->length = 0;
        text_append(test, value);
    }
    if ((value = value_of(doc, "package.name")) != NULL) {
        text_append(package, value);
    }
    anti_rt_toml_free(doc);
    return true;
}

void manifest_inject_free(struct manifest_inject *table)
{
    size_t i;

    for (i = 0; i < table->count; i++) {
        free((void *)table->entries[i]);
    }
    free((void *)table->entries);
    memset(table, 0, sizeof *table);
}

/* The value of one key as text, or an empty text when the document has no
   such key. */
static void text_of_key(const struct anti_toml *doc, const char *key,
                        struct text *out)
{
    const char *value = value_of(doc, key);

    if (value != NULL) {
        out->length = 0;
        text_append(out, value);
    }
}

/* The elements of the array at key, as the keys `<key>.0` upwards. */
static void array_of_key(const struct anti_toml *doc, const char *key,
                         struct text **items, size_t *count)
{
    size_t capacity = 0;

    *items = NULL;
    *count = 0;
    for (;;) {
        struct text path = {0};
        const char *value;
        text_appendf(&path, "%s.%zu", key, *count);
        value = value_of(doc, text_cstr(&path));
        text_free(&path);
        if (value == NULL) {
            return;
        }
        if (*count == capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            *items = realloc(*items, capacity * sizeof **items);
            if (*items == NULL) {
                die_out_of_memory();
            }
        }
        memset(&(*items)[*count], 0, sizeof **items);
        text_append(&(*items)[*count], value);
        (*count)++;
    }
}

/* The key of the document at index, as a NUL-terminated string. The
   reader keeps every key that way, so the text is the string. */
static const char *key_at(const struct anti_toml *doc, int64_t index)
{
    struct anti_text key = anti_rt_toml_key(doc, index);

    return (const char *)key.ptr;
}

/* The last segment of a dotted key, which names the field of an entry of
   `[dependencies]`. The segments before it are the package name, because
   a quoted key carries its dots into the path. */
static const char *last_segment(const char *key)
{
    const char *dot = strrchr(key, '.');

    return dot == NULL ? key : dot + 1;
}

/* The entry of name in the dependency list, appended when it is new. */
static struct manifest_dependency *dependency_of(struct manifest *m,
                                                 const char *name,
                                                 size_t name_length)
{
    size_t i;

    for (i = 0; i < m->dependency_count; i++) {
        if (m->dependencies[i].name.length == name_length &&
            memcmp(text_cstr(&m->dependencies[i].name), name, name_length) == 0) {
            return &m->dependencies[i];
        }
    }
    m->dependencies = realloc(m->dependencies,
                              (m->dependency_count + 1) *
                                  sizeof *m->dependencies);
    if (m->dependencies == NULL) {
        die_out_of_memory();
    }
    memset(&m->dependencies[m->dependency_count], 0, sizeof *m->dependencies);
    text_append_bytes(&m->dependencies[m->dependency_count].name, name,
                      name_length);
    return &m->dependencies[m->dependency_count++];
}

/* Read `[repositories]` and `[dependencies]`, whose keys the reader wrote
   with the dots of a quoted key in them. */
static bool read_tables(const char *path, const struct anti_toml *doc,
                        struct manifest *m)
{
    static const char repositories[] = "repositories.";
    static const char dependencies[] = "dependencies.";
    int64_t count = anti_rt_toml_count(doc);
    int64_t i;

    for (i = 0; i < count; i++) {
        const char *key = key_at(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        if (strncmp(key, repositories, sizeof repositories - 1) == 0) {
            const char *alias = key + sizeof repositories - 1;
            m->repositories = realloc(m->repositories,
                                      (m->repository_count + 1) *
                                          sizeof *m->repositories);
            if (m->repositories == NULL) {
                die_out_of_memory();
            }
            memset(&m->repositories[m->repository_count], 0,
                   sizeof *m->repositories);
            text_append(&m->repositories[m->repository_count].alias, alias);
            text_append(&m->repositories[m->repository_count].url,
                        (const char *)value.ptr);
            m->repository_count++;
        } else if (strncmp(key, dependencies, sizeof dependencies - 1) == 0) {
            const char *entry = key + sizeof dependencies - 1;
            const char *field = last_segment(entry);
            struct manifest_dependency *d;
            if (field == entry) {
                fprintf(stderr, "anti: %s: the dependency %s needs a table, "
                                "as { version = \"1.0.0\" }\n", path, entry);
                return false;
            }
            d = dependency_of(m, entry, (size_t)(field - entry - 1));
            if (strcmp(field, "version") == 0) {
                text_append(&d->version, (const char *)value.ptr);
            } else if (strcmp(field, "repo") == 0) {
                text_append(&d->repo, (const char *)value.ptr);
            } else if (strcmp(field, "path") == 0) {
                text_append(&d->path, (const char *)value.ptr);
            } else {
                fprintf(stderr, "anti: %s: a dependency has no %s field\n",
                        path, field);
                return false;
            }
        }
    }
    return true;
}

bool manifest_read(const char *path, bool tests, struct manifest *out)
{
    struct text bytes = {0};
    struct anti_toml *doc;
    size_t i;

    memset(out, 0, sizeof *out);
    text_append(&out->src, "src");
    text_append(&out->test, "test");
    text_append(&out->build, "build");
    text_append(&out->dist, "dist");
    if (!files_read(path, &bytes)) {
        text_free(&bytes);
        fprintf(stderr, "anti: %s: no manifest here, so there is no project. "
                        "`anti new <name>` writes one\n", path);
        manifest_free(out);
        return false;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        fprintf(stderr, "anti: %s is no TOML that anti reads\n", path);
        manifest_free(out);
        return false;
    }
    text_of_key(doc, "package.name", &out->name);
    text_of_key(doc, "package.version", &out->version);
    text_of_key(doc, "package.antic", &out->antic);
    text_of_key(doc, "package.license", &out->license);
    text_of_key(doc, "package.license_text", &out->license_text);
    array_of_key(doc, "package.attribution", &out->attribution,
                 &out->attribution_count);
    text_of_key(doc, "layout.src", &out->src);
    text_of_key(doc, "layout.test", &out->test);
    text_of_key(doc, "layout.build", &out->build);
    text_of_key(doc, "layout.dist", &out->dist);
    array_of_key(doc, "targets.default", &out->default_targets,
                 &out->default_target_count);
    array_of_key(doc, "targets.all", &out->all_targets,
                 &out->all_target_count);
    if (!read_tables(path, doc, out)) {
        anti_rt_toml_free(doc);
        manifest_free(out);
        return false;
    }
    anti_rt_toml_free(doc);
    if (out->name.length == 0) {
        fprintf(stderr, "anti: %s: [package] names no `name`, the root module "
                        "path of the package\n", path);
        manifest_free(out);
        return false;
    }
    /* Every dependency names one source. A table with both a repository
       and a path says two things about where its modules come from. */
    for (i = 0; i < out->dependency_count; i++) {
        const struct manifest_dependency *d = &out->dependencies[i];
        if (d->path.length > 0 && d->repo.length > 0) {
            fprintf(stderr, "anti: %s: the dependency %s names a repository "
                            "and a path\n", path, text_cstr(&d->name));
            manifest_free(out);
            return false;
        }
        if (d->path.length == 0 && d->version.length == 0) {
            fprintf(stderr, "anti: %s: the dependency %s names no version\n",
                    path, text_cstr(&d->name));
            manifest_free(out);
            return false;
        }
        if (d->repo.length > 0 &&
            manifest_repository_url(out, text_cstr(&d->repo)) == NULL) {
            fprintf(stderr, "anti: %s: the dependency %s names the repository "
                            "%s, which [repositories] does not\n", path,
                    text_cstr(&d->name), text_cstr(&d->repo));
            manifest_free(out);
            return false;
        }
    }
    if (!manifest_inject_read(path, tests, &out->inject)) {
        manifest_free(out);
        return false;
    }
    return true;
}

const char *manifest_repository_url(const struct manifest *m,
                                    const char *alias)
{
    size_t i;

    for (i = 0; i < m->repository_count; i++) {
        if (strcmp(text_cstr(&m->repositories[i].alias), alias) == 0) {
            return text_cstr(&m->repositories[i].url);
        }
    }
    return NULL;
}

void manifest_free(struct manifest *m)
{
    size_t i;

    for (i = 0; i < m->attribution_count; i++) {
        text_free(&m->attribution[i]);
    }
    free(m->attribution);
    for (i = 0; i < m->default_target_count; i++) {
        text_free(&m->default_targets[i]);
    }
    free(m->default_targets);
    for (i = 0; i < m->all_target_count; i++) {
        text_free(&m->all_targets[i]);
    }
    free(m->all_targets);
    for (i = 0; i < m->repository_count; i++) {
        text_free(&m->repositories[i].alias);
        text_free(&m->repositories[i].url);
    }
    free(m->repositories);
    for (i = 0; i < m->dependency_count; i++) {
        text_free(&m->dependencies[i].name);
        text_free(&m->dependencies[i].version);
        text_free(&m->dependencies[i].repo);
        text_free(&m->dependencies[i].path);
    }
    free(m->dependencies);
    text_free(&m->name);
    text_free(&m->version);
    text_free(&m->antic);
    text_free(&m->license);
    text_free(&m->license_text);
    text_free(&m->src);
    text_free(&m->test);
    text_free(&m->build);
    text_free(&m->dist);
    manifest_inject_free(&m->inject);
    memset(m, 0, sizeof *m);
}
