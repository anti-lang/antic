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

/* Append `archive <path> <digest>` for every file of the list that the
   link takes, with the path below the runtime archive at runtime. */
static bool archive_lines(const char *runtime, const struct files_list *list,
                          struct text *out)
{
    size_t skip = strlen(runtime);
    size_t i;
    char hex[65];

    for (i = 0; i < list->count; i++) {
        const char *path = text_cstr(&list->items[i]);
        if (!link_is_input(path)) {
            continue;
        }
        if (!sha256_file(path, hex)) {
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
    bool ok;

    if (runtime == NULL) {
        text_append(out, "runtime none\n");
        return true;
    }
    ANTI_FAULT_HOLD(true);
    ok = target_archives(runtime, t, cpu, false, &list) &&
         (target_info(t)->os != OS_LINUX ||
          target_archives(runtime, t, cpu, true, &list)) &&
         archive_lines(runtime, &list, out);
    files_list_free(&list);
    ANTI_FAULT_HOLD(false);
    return ok;
}
