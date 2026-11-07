/* The keys of the files that `anti build` and `anti test` keep in the
   build directory of a mode between runs.

   DESIGN: an output of the cache is reused when the key beside it is the
   key the run would write. The key names everything that decides the
   output: the digest of the input, the version of the compiler, the
   target, the level and the flags, the options of the manifest and the
   digest of every library file the input reaches. A run that writes the
   output again writes its key after it, and drops the old key before, so
   a compile that fails halfway leaves no key the next run trusts. Both
   commands take the keys from this one file, so a file that one writes
   is a file the other can reuse. */

#include "cache.h"

#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "arena.h"
#include "digest.h"
#include "fault.h"
#include "files.h"
#include "platform.h"
#include "say.h"
#include "sha256.h"
#include "userdirs.h"

void cache_key(const char *input, const struct cache_flags *f,
               struct text *out)
{
    struct anti_sha256 digest;
    struct text bytes = {0};
    char hex[65];

    anti_rt_sha256_init(&digest);
    if (files_read(input, &bytes)) {
        anti_rt_sha256_update(&digest, bytes.data, bytes.length);
    }
    anti_rt_sha256_hex(&digest, hex);
    text_appendf(out, "%s %s %s %s %s%s%s%s\n", hex, ANTIC_VERSION,
                 target_name(f->target), cpu_name(f->cpu),
                 f->debug ? "g" : "no-g", f->strict ? " strict" : "",
                 f->checked ? " memory-checks" : "",
                 f->tests ? " tests" : "");
    text_free(&bytes);
}

bool cache_holds(const char *output, const struct text *key)
{
    struct text path = {0};
    struct text stored = {0};
    bool same;

    text_appendf(&path, "%s%s", output, CACHE_KEY_SUFFIX);
    same = files_exists(output) && files_read(text_cstr(&path), &stored) &&
           stored.length == key->length &&
           memcmp(stored.data, key->data, key->length) == 0;
    text_free(&path);
    text_free(&stored);
    return same;
}

void cache_drop(const char *output)
{
    struct text path = {0};

    text_appendf(&path, "%s%s", output, CACHE_KEY_SUFFIX);
    platform_remove(text_cstr(&path));
    text_free(&path);
}

bool cache_store(const char *output, const struct text *key)
{
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s%s", output, CACHE_KEY_SUFFIX);
    ok = files_write(text_cstr(&path), key);
    text_free(&path);
    return ok;
}

bool cache_file(const char *label, const char *path, struct text *out)
{
    char hex[65];

    if (!sha256_file(path, hex)) {
        say_anti("cannot read %s\n", path);
        return false;
    }
    text_appendf(out, "%s %s %s\n", label, path, hex);
    return true;
}

bool cache_key_of(const char *output, struct text *out)
{
    struct text path = {0};
    struct text stored = {0};
    bool ok;

    text_appendf(&path, "%s%s", output, CACHE_KEY_SUFFIX);
    ok = files_read(text_cstr(&path), &stored);
    if (!ok) {
        say_anti("cannot read %s\n", text_cstr(&path));
    } else {
        text_appendf(out, "key %s\n%s", output, text_cstr(&stored));
    }
    text_free(&path);
    text_free(&stored);
    return ok;
}

bool cache_imports(const struct options *search, const char *library,
                   struct text *out)
{
    struct arena arena = {0};
    struct options walk = *search;
    const char **seed = files_array(search->library_count + 1, sizeof *seed);
    const char **closure = NULL;    /* the memory pool holds the list */
    size_t count = 0;
    size_t i;
    bool ok;

    seed[0] = library;
    for (i = 0; i < search->library_count; i++) {
        seed[i + 1] = search->libraries[i];
    }
    walk.input = NULL;
    walk.libraries = seed;
    walk.library_count = search->library_count + 1;
    ok = driver_libraries(&walk, &arena, &closure, &count);
    for (i = 0; ok && i < count; i++) {
        if (strcmp(closure[i], library) != 0) {
            ok = cache_file("import", closure[i], out);
        }
    }
    free((void *)seed);
    arena_free(&arena);
    return ok;
}

/* The file of the cache of the user that keeps the digest of each
   archive the key read, beside the identity of the file it read. */
#define CACHE_DIGESTS_FILE "archive-digests.txt"

/* One archive of the file of digests. */
struct known_digest {
    struct text path;
    struct platform_identity identity;
    char hex[65];
};

struct known_digests {
    struct text file;               /* empty where the user has no cache */
    struct known_digest *items;
    size_t count;
    size_t room;
    bool changed;
};

/* The entry of path, or a new empty one at the end. */
static struct known_digest *known_entry(struct known_digests *k,
                                        const char *path)
{
    struct known_digest *d;
    size_t i;

    for (i = 0; i < k->count; i++) {
        if (strcmp(text_cstr(&k->items[i].path), path) == 0) {
            return &k->items[i];
        }
    }
    if (k->count == k->room) {
        k->items = files_grow(k->items, &k->room, sizeof *k->items);
    }
    d = &k->items[k->count++];
    memset(d, 0, sizeof *d);
    text_append(&d->path, path);
    return d;
}

/* Read the file of digests, one archive a line: its size, its time of
   change, its device, its file, its digest and its path. A line of
   another form is passed over, so the archive it named is hashed again. */
static void known_read(struct known_digests *k)
{
    struct text bytes = {0};
    const char *line;

    if (!user_dir(&k->file, USER_DIR_CACHE, USER_DIR_APP)) {
        k->file.length = 0;
        return;
    }
    text_append(&k->file, "/" CACHE_DIGESTS_FILE);
    if (!files_read(text_cstr(&k->file), &bytes)) {
        return;
    }
    for (line = text_cstr(&bytes); *line != '\0';) {
        const char *end = strchr(line, '\n');
        struct platform_identity id;
        char hex[65];
        char *at;
        size_t length;
        id.size = strtoull(line, &at, 10);
        id.changed = strtoll(at, &at, 10);
        id.device = strtoull(at, &at, 10);
        id.file = strtoull(at, &at, 10);
        length = end != NULL ? (size_t)(end - at) : strlen(at);
        if (length > 66 && at[0] == ' ' && at[65] == ' ' &&
            strspn(at + 1, "0123456789abcdef") == 64) {
            struct text path = {0};
            struct known_digest *d;
            memcpy(hex, at + 1, 64);
            hex[64] = '\0';
            text_append_bytes(&path, at + 66, length - 66);
            d = known_entry(k, text_cstr(&path));
            d->identity = id;
            memcpy(d->hex, hex, sizeof hex);
            text_free(&path);
        }
        line = end != NULL ? end + 1 : line + strlen(line);
    }
    text_free(&bytes);
}

/* Write bytes to path, and say nothing when it fails. */
static bool write_quietly(const char *path, const struct text *bytes)
{
    FILE *f = platform_open(path, true);
    bool ok;

    if (f == NULL) {
        return false;
    }
    ok = bytes->length == 0 ||
         ANTI_FWRITE(bytes->data, 1, bytes->length, f) == bytes->length;
    return ANTI_FCLOSE(f) == 0 && ok;
}

/* Write the file of digests again when a digest changed. It is a cache:
   a file that cannot be written leaves the next run to hash again, and
   the write goes beside it first, so a reader never sees half of it. */
static void known_write(struct known_digests *k)
{
    struct text bytes = {0};
    struct text temporary = {0};
    struct text dir = {0};
    size_t i;

    if (!k->changed || k->file.length == 0) {
        return;
    }
    for (i = 0; i < k->count; i++) {
        const struct known_digest *d = &k->items[i];
        if (d->hex[0] != '\0') {
            text_appendf(&bytes, "%llu %lld %llu %llu %s %s\n",
                         (unsigned long long)d->identity.size,
                         (long long)d->identity.changed,
                         (unsigned long long)d->identity.device,
                         (unsigned long long)d->identity.file, d->hex,
                         text_cstr(&d->path));
        }
    }
    text_append_bytes(&dir, k->file.data,
                      k->file.length - (sizeof CACHE_DIGESTS_FILE));
    text_appendf(&temporary, "%s%s", text_cstr(&k->file), FILES_NEW_SUFFIX);
    if (!files_make_dirs(text_cstr(&dir)) ||
        !write_quietly(text_cstr(&temporary), &bytes) ||
        !platform_replace(text_cstr(&temporary), text_cstr(&k->file))) {
        platform_remove(text_cstr(&temporary));
    }
    text_free(&bytes);
    text_free(&temporary);
    text_free(&dir);
}

static void known_free(struct known_digests *k)
{
    size_t i;

    for (i = 0; i < k->count; i++) {
        text_free(&k->items[i].path);
    }
    free(k->items);
    text_free(&k->file);
}

/* DESIGN: Q4 of "Answers of 2026-11-07, second set" in docs/decisions.md.
   The key reads 40.7 MB of archives for a Linux target, which the release
   build of anti hashed in 0.11 s of a build that changed nothing. The
   digest of an archive is kept beside its size, its time of change and
   the identity of its file, and the archive is hashed again only when one
   of them differs from what the file of digests holds. A write in place
   moves the time of change, and a file put in its place has another
   identity. */
static bool archive_digest(struct known_digests *k, const char *path,
                           char hex[65])
{
    struct platform_identity id;
    struct known_digest *d;

    if (!platform_identity(path, &id)) {
        return sha256_file(path, hex);
    }
    d = known_entry(k, path);
    if (d->hex[0] != '\0' && d->identity.size == id.size &&
        d->identity.changed == id.changed &&
        d->identity.device == id.device && d->identity.file == id.file) {
        memcpy(hex, d->hex, 65);
        return true;
    }
    if (!sha256_file(path, hex)) {
        return false;
    }
    d->identity = id;
    memcpy(d->hex, hex, 65);
    k->changed = true;
    return true;
}

/* Append `archive <path> <digest>` for every file of the list that the
   link takes, with the path below the runtime archive at runtime. */
static bool archive_lines(const char *runtime, const struct files_list *list,
                          struct known_digests *k, struct text *out)
{
    size_t skip = strlen(runtime);
    size_t i;
    char hex[65];

    for (i = 0; i < list->count; i++) {
        const char *path = text_cstr(&list->items[i]);
        if (!link_is_input(path)) {
            continue;
        }
        if (!archive_digest(k, path, hex)) {
            say_anti("cannot read %s\n", path);
            return false;
        }
        text_appendf(out, "archive %s %s\n",
                     path + skip + (path[skip] == '/'), hex);
    }
    return true;
}

/* The files of the runtime archive that a link of target t at level cpu
   may take, in the mode of glibc as well. */
static bool target_archives(const char *runtime, enum target t,
                            enum cpu_level cpu, bool glibc,
                            struct files_list *list)
{
    struct text dir = {0};
    bool ok;

    text_appendf(&dir, "%s/%s/", runtime, RUNTIME_LIB_DIR);
    link_target_dir(&dir, t, glibc);
    ok = files_list_dir(text_cstr(&dir), list);
    text_appendf(&dir, "/%s", cpu_name(cpu));
    ok = ok && files_list_tree(text_cstr(&dir), "", list);
    dir.length = 0;
    text_appendf(&dir, "%s/%s/", runtime, RUNTIME_SYSROOT_DIR);
    link_target_dir(&dir, t, glibc);
    text_append(&dir, "/");
    link_sysroot_libraries(&dir, t);
    ok = ok && files_list_tree(text_cstr(&dir), "", list);
    text_free(&dir);
    return ok;
}

bool cache_runtime(const char *runtime, enum target t, enum cpu_level cpu,
                   struct text *out)
{
    struct files_list list = {0};
    struct known_digests known;
    bool ok;

    if (runtime == NULL) {
        text_append(out, "runtime none\n");
        return true;
    }
    memset(&known, 0, sizeof known);
    ANTI_FAULT_HOLD(true);
    known_read(&known);
    ok = target_archives(runtime, t, cpu, false, &list) &&
         (target_info(t)->os != OS_LINUX ||
          target_archives(runtime, t, cpu, true, &list)) &&
         archive_lines(runtime, &list, &known, out);
    known_write(&known);
    known_free(&known);
    files_list_free(&list);
    ANTI_FAULT_HOLD(false);
    return ok;
}
