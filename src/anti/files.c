#include "files.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "text.h"

void files_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

void *files_array(size_t count, size_t size)
{
    void *items;

    if (size != 0 && count > SIZE_MAX / size) {
        files_out_of_memory();
    }
    /* calloc may answer NULL for no bytes, which is no failure, so the
       request is never for none. */
    items = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    if (items == NULL) {
        files_out_of_memory();
    }
    return items;
}

void *files_resize(void *items, size_t count, size_t size)
{
    void *grown;

    if (size != 0 && count > SIZE_MAX / size) {
        files_out_of_memory();
    }
    grown = realloc(items, count * size == 0 ? 1 : count * size);
    if (grown == NULL) {
        files_out_of_memory();
    }
    return grown;
}

void *files_grow(void *items, size_t *room, size_t size)
{
    size_t grown = *room == 0 ? 16 : *room;
    unsigned char *bytes;

    if (*room != 0) {
        if (grown > SIZE_MAX / 2) {
            files_out_of_memory();
        }
        grown *= 2;
    }
    bytes = files_resize(items, grown, size);
    memset(bytes + *room * size, 0, (grown - *room) * size);
    *room = grown;
    return bytes;
}

static void list_add(struct files_list *out, const char *path)
{
    if (out->count == out->capacity) {
        out->items = files_grow(out->items, &out->capacity, sizeof *out->items);
    }
    text_append(&out->items[out->count++], path);
}

/* The callback of platform_list_directory that adds each name to the
   list in context. */
static void add_name(void *context, const char *name)
{
    list_add(context, name);
}

bool files_list_names(const char *dir, struct files_list *out)
{
    return platform_list_directory(dir, add_name, out);
}

bool files_make_dirs(const char *path)
{
    struct text t = {0};
    char *p;
    size_t i;
    bool ok;

    text_append(&t, path);
    p = t.data;
    for (i = 1; i < t.length; i++) {
        if ((p[i] == '/' || p[i] == '\\') && p[i - 1] != ':') {
            char separator = p[i];
            p[i] = '\0';
            platform_make_dir(p);
            p[i] = separator;
        }
    }
    ok = platform_make_dir(p);
    text_free(&t);
    return ok;
}

/* A link is removed and never followed, so the tree it leads to stays. */
bool files_remove_tree(const char *path)
{
    enum platform_kind kind = platform_kind(path, false);
    struct files_list names = {0};
    bool ok;
    size_t i;

    if (kind == PLATFORM_MISSING) {
        return true;
    }
    if (kind == PLATFORM_UNREADABLE) {
        return false;
    }
    if (kind != PLATFORM_DIRECTORY) {
        return platform_remove_entry(path);
    }
    ok = files_list_names(path, &names);
    for (i = 0; i < names.count; i++) {
        struct text child = {0};
        text_appendf(&child, "%s/%s", path, text_cstr(&names.items[i]));
        ok = files_remove_tree(text_cstr(&child)) && ok;
        text_free(&child);
    }
    files_list_free(&names);
    return platform_remove_entry(path) && ok;
}

bool files_copy(const char *from, const char *to)
{
    FILE *in = platform_open(from, false);
    FILE *out;
    char buffer[65536];
    size_t n;
    bool ok = true;

    if (in == NULL) {
        return false;
    }
    out = platform_open(to, true);
    if (out == NULL) {
        fclose(in);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    ok = ok && !ferror(in);
    fclose(in);
    return fclose(out) == 0 && ok;
}

static void cannot_read(const char *path)
{
    fprintf(stderr, "anti: cannot read %s\n", path);
}

/* Append what is left of f to out and close it, or leave out as it was
   when a read fails part of the way. */
static bool read_stream(FILE *f, struct text *out)
{
    char buffer[65536];
    size_t start = out->length;
    size_t n;
    bool ok;

    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    /* fread gives 0 at the end of the file and on a read error alike. */
    ok = !ferror(f);
    fclose(f);
    if (!ok) {
        out->length = start;
        if (out->data != NULL) {
            out->data[start] = '\0';
        }
    }
    return ok;
}

bool files_read(const char *path, struct text *out)
{
    FILE *f = platform_open(path, false);

    return f != NULL && read_stream(f, out);
}

bool files_read_file(const char *path, struct text *out)
{
    FILE *f = platform_open_file(path);

    return f != NULL && read_stream(f, out);
}

bool files_read_reported(const char *path, struct text *out)
{
    if (!files_read(path, out)) {
        cannot_read(path);
        return false;
    }
    return true;
}

bool files_write(const char *path, const struct text *bytes)
{
    FILE *f = platform_open(path, true);
    bool ok;

    if (f == NULL) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    ok = bytes->length == 0 ||
         fwrite(bytes->data, 1, bytes->length, f) == bytes->length;
    if (fclose(f) != 0 || !ok) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    return true;
}

/* DESIGN: the bytes go to a file beside path, which takes the
   permissions of path and then its name. A write that fails partway
   fills the new file alone, which is removed. */
bool files_replace(const char *path, const struct text *bytes)
{
    struct text temporary = {0};
    bool ok;

    text_appendf(&temporary, "%s%s", path, FILES_NEW_SUFFIX);
    ok = files_write(text_cstr(&temporary), bytes);
    if (ok && (!platform_copy_permissions(path, text_cstr(&temporary)) ||
               !platform_replace(text_cstr(&temporary), path))) {
        fprintf(stderr, "anti: cannot replace %s\n", path);
        ok = false;
    }
    if (!ok) {
        platform_remove(text_cstr(&temporary));
    }
    text_free(&temporary);
    return ok;
}

/* DESIGN: Smart App Control of Windows 11 checks a new program when it
   first runs. Its AppLocker filter and the Application Identity service
   then keep a section of the file mapped for 0.2 to 1.6 s after the
   program ended, measured on the Windows VM. Windows refuses to truncate
   a mapped file with ERROR_USER_MAPPED_FILE, which the C runtime reports
   as EINVAL, so a copy in place failed in `anti build` right after a run,
   and the wait of platform_open, which waits on EACCES alone, never ran.
   The copy goes to a file beside to, and platform_replace_program puts
   it in place under the mapping without a wait.

   DESIGN: the Mac and Linux take the same path, which Eddie decided on
   2026-10-07. Linux refuses to open a program that runs for writing,
   with ETXTBSY, and the rename replaces it: the running process keeps
   the old file until it ends. On every host a copy that fails partway
   leaves the old program whole and removes its own file, where a write
   in place left a broken program in dist/. */
bool files_copy_program(const char *from, const char *to)
{
    struct text temporary = {0};
    bool ok;

    text_appendf(&temporary, "%s%s", to, FILES_NEW_SUFFIX);
    ok = files_copy(from, text_cstr(&temporary)) &&
         platform_copy_permissions(from, text_cstr(&temporary)) &&
         platform_replace_program(text_cstr(&temporary), to);
    if (!ok) {
        platform_remove(text_cstr(&temporary));
    }
    text_free(&temporary);
    return ok;
}

bool files_ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    return n >= m && strcmp(s + n - m, suffix) == 0;
}

void files_cut_suffix(struct text *t, const char *suffix)
{
    size_t m = strlen(suffix);

    if (m > 0 && files_ends_with(text_cstr(t), suffix)) {
        t->length -= m;
        t->data[t->length] = '\0';
    }
}

static int by_path(const void *a, const void *b)
{
    const struct text *x = a;
    const struct text *y = b;

    return strcmp(text_cstr(x), text_cstr(y));
}

static bool walk(const char *dir, const char *suffix, bool deep,
                 struct files_list *out);

/* One entry of a directory of the walk, at path, whose name is name. */
static bool walk_entry(const char *path, const char *name, const char *suffix,
                       bool deep, struct files_list *out)
{
    enum platform_kind kind = platform_kind(path, false);

    if (kind == PLATFORM_LINK) {
        /* A link that leads nowhere names no file, and one that leads to
           a directory is not followed. */
        kind = platform_kind(path, true);
        if (kind == PLATFORM_DIRECTORY) {
            return true;
        }
    }
    if (kind == PLATFORM_UNREADABLE) {
        cannot_read(path);
        return false;
    }
    if (kind == PLATFORM_DIRECTORY) {
        return !deep || walk(path, suffix, deep, out);
    }
    if (kind == PLATFORM_FILE && files_ends_with(name, suffix)) {
        list_add(out, path);
    }
    return true;
}

/* One directory of the walk. A name that starts with a dot is left alone,
   so a checkout's own directories are no part of a project.

   DESIGN: a link to a directory is not followed, as files_remove_tree follows
   none. A link to a parent then cannot lead the walk round until it runs
   out of descriptors. A link to a file is listed as the file. A directory
   that does not exist adds nothing. Every other failure to read one
   fails the walk, so no caller takes a tree it did not read for the
   whole. */
static bool walk(const char *dir, const char *suffix, bool deep,
                 struct files_list *out)
{
    struct files_list names = {0};
    bool ok = true;
    size_t i;

    if (platform_kind(dir, true) == PLATFORM_MISSING) {
        return true;
    }
    if (!files_list_names(dir, &names)) {
        cannot_read(dir);
        ok = false;
    }
    for (i = 0; i < names.count; i++) {
        const char *name = text_cstr(&names.items[i]);
        struct text child = {0};

        if (name[0] == '.') {
            continue;
        }
        text_appendf(&child, "%s/%s", dir, name);
        ok = walk_entry(text_cstr(&child), name, suffix, deep, out) && ok;
        text_free(&child);
    }
    files_list_free(&names);
    return ok;
}

bool files_list_tree(const char *dir, const char *suffix,
                     struct files_list *out)
{
    size_t from = out->count;
    bool ok = walk(dir, suffix, true, out);

    if (out->count > from) {
        qsort(out->items + from, out->count - from, sizeof *out->items,
              by_path);
    }
    return ok;
}

bool files_list_dir(const char *dir, struct files_list *out)
{
    size_t from = out->count;
    bool ok = walk(dir, "", false, out);

    if (out->count > from) {
        qsort(out->items + from, out->count - from, sizeof *out->items,
              by_path);
    }
    return ok;
}

void files_list_free(struct files_list *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        text_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

const char *files_base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    if (back != NULL && (slash == NULL || back > slash)) {
        slash = back;
    }
    return slash != NULL ? slash + 1 : path;
}

bool files_exists(const char *path)
{
    enum platform_kind kind = platform_kind(path, true);

    return kind == PLATFORM_FILE || kind == PLATFORM_DIRECTORY;
}
