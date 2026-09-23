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

#include "text.h"
#include "toml.h"

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

static bool read_file(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[8192];
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
    if (!read_file(path, &bytes)) {
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
    if (!read_file(path, &bytes)) {
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
