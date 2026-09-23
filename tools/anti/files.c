/* lstat, rmdir and unlink are POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L

#include "files.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text.h"

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool make_one(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

bool make_dirs(const char *path)
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

bool remove_tree(const char *path)
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
            ok = remove_tree(text_cstr(&child)) && ok;
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
        ok = remove_tree(text_cstr(&child)) && ok;
        text_free(&child);
    }
    closedir(dir);
    return rmdir(path) == 0 && ok;
#endif
}

bool copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out;
    char buffer[65536];
    size_t n;
    bool ok = true;

    if (in == NULL) {
        return false;
    }
    out = fopen(to, "wb");
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

static void list_add(struct file_list *out, const char *path)
{
    if (out->count == out->capacity) {
        size_t capacity = out->capacity == 0 ? 32 : out->capacity * 2;
        struct text *items = realloc(out->items, capacity * sizeof *items);
        if (items == NULL) {
            fputs("anti: out of memory\n", stderr);
            exit(70);
        }
        memset(items + out->count, 0, (capacity - out->count) * sizeof *items);
        out->items = items;
        out->capacity = capacity;
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
   so a checkout's own directories are no part of a project. */
static bool walk(const char *dir, const char *suffix, struct file_list *out)
{
#if defined(_WIN32)
    WIN32_FIND_DATAA found;
    HANDLE search;
    struct text pattern = {0};
    bool ok = true;

    text_appendf(&pattern, "%s\\*", dir);
    search = FindFirstFileA(text_cstr(&pattern), &found);
    text_free(&pattern);
    if (search == INVALID_HANDLE_VALUE) {
        return true;
    }
    do {
        struct text child = {0};
        if (found.cFileName[0] == '.') {
            continue;
        }
        text_appendf(&child, "%s/%s", dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ok = walk(text_cstr(&child), suffix, out) && ok;
        } else if (ends_with(found.cFileName, suffix)) {
            list_add(out, text_cstr(&child));
        }
        text_free(&child);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return ok;
#else
    DIR *handle = opendir(dir);
    struct dirent *entry;
    bool ok = true;

    if (handle == NULL) {
        return true;
    }
    while ((entry = readdir(handle)) != NULL) {
        struct text child = {0};
        struct stat st;
        if (entry->d_name[0] == '.') {
            continue;
        }
        text_appendf(&child, "%s/%s", dir, entry->d_name);
        if (stat(text_cstr(&child), &st) != 0) {
            ok = false;
        } else if (S_ISDIR(st.st_mode)) {
            ok = walk(text_cstr(&child), suffix, out) && ok;
        } else if (ends_with(entry->d_name, suffix)) {
            list_add(out, text_cstr(&child));
        }
        text_free(&child);
    }
    closedir(handle);
    return ok;
#endif
}

bool list_tree(const char *dir, const char *suffix, struct file_list *out)
{
    size_t from = out->count;
    bool ok = walk(dir, suffix, out);

    if (out->count > from) {
        qsort(out->items + from, out->count - from, sizeof *out->items,
              by_path);
    }
    return ok;
}

void file_list_free(struct file_list *list)
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

bool path_exists(const char *path)
{
#if defined(_WIN32)
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}
