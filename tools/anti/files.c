#include "files.h"

#include <errno.h>
#include <stdio.h>
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

bool path_exists(const char *path)
{
#if defined(_WIN32)
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}
