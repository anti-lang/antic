/* The plugin index, `anti-plugins.toml`, which antic writes beside a
   plugin. */

#include "driver_parts.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "sha256.h"
#include "../rt/toml.h"

/* DESIGN: `anti-plugins.toml` beside a plugin lists every library of
   the directory. Each entry holds the interfaces, the runtime version
   it was built against and the digest of its bytes. Discovery reads it
   and opens no library to find out what is inside one. antic writes the
   file, because `anti build` is not built. It reads the file with
   src/rt/toml.c, the reader the runtime loads it with, so the two agree
   on every line end and every quote. It keeps the entries of the other
   libraries and writes them back in its own form. A file that reader
   refuses is replaced. */

/* Append the length bytes at value as a string that src/rt/toml.c reads
   back: in single quotes, or in double quotes when value holds a single
   quote. That reader knows no escapes, so a value with both quotes or a
   line break has no form, and the result is false. */
static bool index_string(struct text *out, const unsigned char *value,
                         size_t length)
{
    const char *quote;

    if (length == 0) {
        text_append(out, "''");
        return true;
    }
    quote = memchr(value, '\'', length) == NULL ? "'" : "\"";
    if ((quote[0] == '"' && memchr(value, '"', length) != NULL) ||
        memchr(value, '\n', length) != NULL) {
        return false;
    }
    text_append(out, quote);
    text_append_bytes(out, value, length);
    text_append(out, quote);
    return true;
}

/* Append the line `key = value` of an entry. */
static bool index_line(struct text *out, const char *key,
                       const unsigned char *value, size_t length)
{
    bool ok;

    text_appendf(out, "%s = ", key);
    ok = index_string(out, value, length);
    text_append(out, "\n");
    return ok;
}

/* Append the start of the entry of a library, up to its interfaces. */
static bool index_start(struct text *out, const struct anti_text *path,
                        const struct anti_text *runtime,
                        const struct anti_text *digest)
{
    text_append(out, "[[library]]\n");
    return index_line(out, "path", path->ptr, (size_t)path->len) &&
           index_line(out, "runtime", runtime->ptr, (size_t)runtime->len) &&
           index_line(out, "digest", digest->ptr, (size_t)digest->len);
}

/* Append the interface name of the list that index_start opened. */
static bool index_interface(struct text *out, bool first,
                            const unsigned char *name, size_t length)
{
    text_append(out, first ? "interfaces = [" : ", ");
    return index_string(out, name, length);
}

/* Close the list of interfaces, which may be empty. */
static void index_end(struct text *out, bool empty)
{
    text_append(out, empty ? "interfaces = []\n" : "]\n");
}

static struct anti_text index_text(const char *s)
{
    struct anti_text t;

    t.ptr = (const unsigned char *)s;
    t.len = (int64_t)strlen(s);
    return t;
}

/* Append the entry of the library name with the interfaces of provides,
   one per line before a tab. */
static bool index_entry(struct text *out, const char *name, const char *digest,
                        const struct text *provides)
{
    const char *line = text_cstr(provides);
    struct anti_text path = index_text(name);
    struct anti_text runtime = index_text(ANTIC_VERSION);
    struct anti_text sum = index_text(digest);
    bool first = true;
    bool ok = index_start(out, &path, &runtime, &sum);

    while (ok && *line != '\0') {
        size_t n = strcspn(line, "\t\n");
        ok = index_interface(out, first, (const unsigned char *)line, n);
        first = false;
        line += strcspn(line, "\n");
        line += *line == '\n';
    }
    index_end(out, first);
    return ok;
}

/* The value of `library.<n>.<field>` of the index, or false without one. */
static bool index_value(const struct anti_toml *doc, int64_t n,
                        const char *field, struct anti_text *value)
{
    struct text key = {0};
    int64_t at;

    text_appendf(&key, "library.%lld.%s", (long long)n, field);
    at = anti_rt_toml_find(doc, (const unsigned char *)text_cstr(&key),
                           (int64_t)key.length);
    text_free(&key);
    if (at < 0) {
        return false;
    }
    *value = anti_rt_toml_value(doc, at);
    return true;
}

/* Append entry n of the index, unless it names no library or names the
   library `name`. */
static bool index_other(struct text *out, const struct anti_toml *doc,
                        int64_t n, const char *name)
{
    struct anti_text path;
    struct anti_text runtime = index_text("");
    struct anti_text digest = index_text("");
    struct anti_text interface;
    struct text key = {0};
    int64_t k;
    bool ok;

    if (!index_value(doc, n, "path", &path) ||
        ((size_t)path.len == strlen(name) &&
         memcmp(path.ptr, name, (size_t)path.len) == 0)) {
        return true;
    }
    index_value(doc, n, "runtime", &runtime);
    index_value(doc, n, "digest", &digest);
    ok = index_start(out, &path, &runtime, &digest);
    for (k = 0; ok; k++) {
        key.length = 0;
        text_appendf(&key, "interfaces.%lld", (long long)k);
        if (!index_value(doc, n, text_cstr(&key), &interface)) {
            break;
        }
        ok = index_interface(out, k == 0, interface.ptr,
                             (size_t)interface.len);
    }
    index_end(out, k == 0);
    text_free(&key);
    return ok;
}

/* Append the entries of the index at path that name another library.
   An index that is no TOML of src/rt/toml.c keeps nothing. */
static bool index_others(struct text *out, const char *path, const char *name)
{
    struct text file = {0};
    struct anti_toml *doc = NULL;
    FILE *f = platform_open(path, false);
    bool ok = true;
    int64_t count;
    int64_t n;

    if (f == NULL) {
        return true;
    }
    fclose(f);
    if (driver_read_bytes(path, &file)) {
        doc = anti_rt_toml_read((const unsigned char *)file.data,
                                (int64_t)file.length);
    }
    text_free(&file);
    if (doc == NULL) {
        return true;
    }
    /* Every entry holds a key, so the count of keys bounds the entries. */
    count = anti_rt_toml_count(doc);
    for (n = 0; ok && n < count; n++) {
        ok = index_other(out, doc, n, name);
    }
    anti_rt_toml_free(doc);
    return ok;
}

bool driver_write_plugin_index(const char *dir, const char *name,
                               const char *library,
                               const struct text *provides)
{
    struct text path = {0};
    struct text content = {0};
    char digest[65];
    bool ok;

    text_appendf(&path, "%s%s", dir, ANTI_PLUGIN_INDEX);
    ok = sha256_file(library, digest);
    if (!ok) {
        fprintf(stderr, "antic: cannot read %s\n", library);
    } else if (!index_others(&content, text_cstr(&path), name) ||
               !index_entry(&content, name, digest, provides)) {
        fprintf(stderr, "antic: %s cannot hold a name with both quotes or "
                        "a line break: %s\n",
                text_cstr(&path), name);
        ok = false;
    } else {
        ok = driver_write_file(text_cstr(&path), &content);
    }
    text_free(&path);
    text_free(&content);
    return ok;
}
