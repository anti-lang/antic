/* lstat, rmdir and unlink are POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L

#include "files.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../antic/platform.h"
#include "text.h"

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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
    /* calloc may answer NULL for no bytes, which is no failure. */
    items = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    return items;
}

void *files_resize(void *items, size_t count, size_t size)
{
    void *grown;

    if (size != 0 && count > SIZE_MAX / size) {
        files_out_of_memory();
    }
    grown = realloc(items, count * size == 0 ? 1 : count * size);
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

static bool make_one(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
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
            make_one(p);
            p[i] = separator;
        }
    }
    ok = make_one(p);
    text_free(&t);
    return ok;
}

bool files_remove_tree(const char *path)
{
#if defined(_WIN32)
    DWORD attributes = GetFileAttributesA(path);
    WIN32_FIND_DATAA found;
    HANDLE search;
    struct text pattern = {0};
    bool ok = true;

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        return DeleteFileA(path) != 0;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        return RemoveDirectoryA(path) != 0;
    }
    text_appendf(&pattern, "%s\\*", path);
    search = FindFirstFileA(text_cstr(&pattern), &found);
    text_free(&pattern);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            struct text child = {0};
            if (strcmp(found.cFileName, ".") == 0 ||
                strcmp(found.cFileName, "..") == 0) {
                continue;
            }
            text_appendf(&child, "%s\\%s", path, found.cFileName);
            ok = files_remove_tree(text_cstr(&child)) && ok;
            text_free(&child);
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    return RemoveDirectoryA(path) != 0 && ok;
#else
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    bool ok = true;

    if (lstat(path, &st) != 0) {
        return errno == ENOENT;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path) == 0;
    }
    dir = opendir(path);
    if (dir == NULL) {
        return false;
    }
    while ((entry = readdir(dir)) != NULL) {
        struct text child = {0};
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        text_appendf(&child, "%s/%s", path, entry->d_name);
        ok = files_remove_tree(text_cstr(&child)) && ok;
        text_free(&child);
    }
    closedir(dir);
    return rmdir(path) == 0 && ok;
#endif
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

bool files_read(const char *path, struct text *out)
{
    FILE *f = platform_open(path, false);
    char buffer[65536];
    size_t start = out->length;
    size_t n;
    bool ok;

    if (f == NULL) {
        return false;
    }
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

bool files_copy_program(const char *from, const char *to)
{
#if defined(_WIN32)
    return files_copy(from, to);
#else
    struct stat info;

    if (!files_copy(from, to)) {
        return false;
    }
    if (stat(from, &info) != 0) {
        return false;
    }
    return chmod(to, info.st_mode & 07777) == 0;
#endif
}

static void list_add(struct files_list *out, const char *path)
{
    if (out->count == out->capacity) {
        out->items = files_grow(out->items, &out->capacity, sizeof *out->items);
    }
    text_append(&out->items[out->count++], path);
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static int by_path(const void *a, const void *b)
{
    const struct text *x = a;
    const struct text *y = b;

    return strcmp(text_cstr(x), text_cstr(y));
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
#if defined(_WIN32)
    WIN32_FIND_DATAA found;
    HANDLE search;
    struct text pattern = {0};
    DWORD error;
    bool ok = true;

    text_appendf(&pattern, "%s\\*", dir);
    search = FindFirstFileA(text_cstr(&pattern), &found);
    text_free(&pattern);
    if (search == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        cannot_read(dir);
        return false;
    }
    do {
        struct text child = {0};
        if (found.cFileName[0] == '.') {
            continue;
        }
        text_appendf(&child, "%s/%s", dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            bool link = (found.dwFileAttributes &
                         FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            if (deep && !link && !walk(text_cstr(&child), suffix, deep, out)) {
                ok = false;
            }
        } else if (ends_with(found.cFileName, suffix)) {
            list_add(out, text_cstr(&child));
        }
        text_free(&child);
    } while (FindNextFileA(search, &found));
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        cannot_read(dir);
        ok = false;
    }
    FindClose(search);
    return ok;
#else
    DIR *handle = opendir(dir);
    struct dirent *entry;
    bool ok = true;

    if (handle == NULL) {
        if (errno == ENOENT) {
            return true;
        }
        cannot_read(dir);
        return false;
    }
    for (;;) {
        struct text child = {0};
        struct stat st;
        errno = 0;
        entry = readdir(handle);
        if (entry == NULL) {
            if (errno != 0) {
                cannot_read(dir);
                ok = false;
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        text_appendf(&child, "%s/%s", dir, entry->d_name);
        if (lstat(text_cstr(&child), &st) != 0) {
            cannot_read(text_cstr(&child));
            ok = false;
        } else if (S_ISLNK(st.st_mode)) {
            /* A link that leads nowhere names no file. */
            if (stat(text_cstr(&child), &st) != 0) {
                if (errno != ENOENT) {
                    cannot_read(text_cstr(&child));
                    ok = false;
                }
            } else if (!S_ISDIR(st.st_mode) &&
                       ends_with(entry->d_name, suffix)) {
                list_add(out, text_cstr(&child));
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (deep && !walk(text_cstr(&child), suffix, deep, out)) {
                ok = false;
            }
        } else if (ends_with(entry->d_name, suffix)) {
            list_add(out, text_cstr(&child));
        }
        text_free(&child);
    }
    closedir(handle);
    return ok;
#endif
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
#if defined(_WIN32)
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}
