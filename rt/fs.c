/* The C side of anti.fs: opening a file, its size, the listing of a
   directory, removing and renaming. Reading, writing and closing are the
   C streams themselves, which the module calls directly.

   DESIGN: every function reports a failure through errno and nothing
   else, so the module builds each error with SystemError.from_errno on
   every system. Windows takes its paths as UTF-16. The functions there
   are the wide ones of the C runtime, which set errno as well. The Win32
   calls set an error of their own and are not used. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <share.h>
#include <wchar.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif

#include "std.h"
#if defined(_WIN32)
#include "utf.h"
#endif

#if defined(_WIN32)
typedef wchar_t path_char;
#else
typedef char path_char;
#endif

/* Free p and keep errno, which a free before POSIX.1-2024 may change. */
static void release(void *p)
{
    int saved = errno;

    free(p);
    errno = saved;
}

/* The path of the len bytes at bytes, ended by a NUL and with room for
   extra more characters, or NULL with errno set.

   DESIGN: a str carries its length, and a slice of one ends where its
   bytes do and not at a NUL. So the bytes are copied. A NUL inside
   them would name another file than the one the program wrote. The copy
   refuses it with EINVAL. */
static path_char *native_path(const unsigned char *bytes, int64_t len,
                              size_t extra)
{
    path_char *path;
    size_t units = (size_t)len;

    if (len < 0 || len > INT_MAX / 2 ||
        (len > 0 && memchr(bytes, 0, (size_t)len) != NULL)) {
        errno = EINVAL;
        return NULL;
    }
#if defined(_WIN32)
    if (len > 0) {
        int counted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          (const char *)bytes, (int)len,
                                          NULL, 0);
        if (counted <= 0) {
            errno = EINVAL;
            return NULL;
        }
        units = (size_t)counted;
    }
#endif
    path = malloc((units + extra + 1) * sizeof *path);
    if (path == NULL) {
        errno = ENOMEM;
        return NULL;
    }
#if defined(_WIN32)
    if (len > 0) {
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)bytes,
                            (int)len, path, (int)units);
    }
#else
    if (len > 0) {
        memcpy(path, bytes, units);
    }
#endif
    path[units] = 0;
    return path;
}

/* Open the file for reading, or for writing when writing is not 0. Both
   are binary, so no byte is translated on Windows. Writing creates the
   file or empties the one that is there. */
void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing)
{
    path_char *name = native_path(path, len, 0);
    FILE *file;

    if (name == NULL) {
        return NULL;
    }
#if defined(_WIN32)
    /* _wfopen is _wfsopen without a lock on the file, which is what fopen
       gives on the other systems. The C runtime deprecates the first and
       not the second. */
    file = _wfsopen(name, writing != 0 ? L"wb" : L"rb", _SH_DENYNO);
#else
    file = fopen(name, writing != 0 ? "wb" : "rb");
#endif
    release(name);
    return file;
}

/* The size of the file in bytes, or -1 with errno set.

   DESIGN: the size is the offset of the end of the stream. A seek writes
   what is buffered first, so a file being written counts every byte
   written so far. fflush cannot do that for both modes, since the C
   library of macOS refuses it on a stream opened for reading. The
   position is put back afterwards. */
int64_t anti_rt_fs_size(void *file)
{
    FILE *f = file;
    int64_t at;
    int64_t end;

#if defined(_WIN32)
    at = _ftelli64(f);
    if (at < 0 || _fseeki64(f, 0, SEEK_END) != 0) {
        return -1;
    }
    end = _ftelli64(f);
    if (_fseeki64(f, at, SEEK_SET) != 0) {
        return -1;
    }
#else
    at = (int64_t)ftello(f);
    if (at < 0 || fseeko(f, 0, SEEK_END) != 0) {
        return -1;
    }
    end = (int64_t)ftello(f);
    if (fseeko(f, (off_t)at, SEEK_SET) != 0) {
        return -1;
    }
#endif
    return end;
}

/* The names of a listing, each ended by a NUL, one after another. */
struct names {
    unsigned char *bytes;
    size_t length;
    size_t capacity;
    int64_t count;
};

static int add_name(struct names *n, const unsigned char *name, size_t len)
{
    if (n->capacity - n->length < len + 1) {
        size_t want = n->capacity == 0 ? 256 : n->capacity;
        unsigned char *grown;
        while (want - n->length < len + 1) {
            want *= 2;
        }
        grown = realloc(n->bytes, want);
        if (grown == NULL) {
            errno = ENOMEM;
            return -1;
        }
        n->bytes = grown;
        n->capacity = want;
    }
    if (len > 0) {
        memcpy(n->bytes + n->length, name, len);
    }
    n->bytes[n->length + len] = '\0';
    n->length += len + 1;
    n->count++;
    return 0;
}

static int is_dot_entry(const unsigned char *name, size_t len)
{
    return (len == 1 && name[0] == '.') ||
           (len == 2 && name[0] == '.' && name[1] == '.');
}

/* DESIGN: the listing is one block, the array of str first and the
   bytes of the names after it. The program frees all of it with one free
   of the slice's pointer, as it frees the result of `parallel`. The
   runtime lays the array out, so the IR carries no size of a str. */
static struct anti_text *finish(struct names *n, int64_t *count)
{
    size_t head = (size_t)n->count * sizeof(struct anti_text);
    unsigned char *block = malloc(head + n->length + 1);
    struct anti_text *texts = (struct anti_text *)(void *)block;
    const unsigned char *at;
    int64_t i;

    if (block == NULL) {
        release(n->bytes);
        errno = ENOMEM;
        return NULL;
    }
    if (n->length > 0) {
        memcpy(block + head, n->bytes, n->length);
    }
    block[head + n->length] = '\0';
    release(n->bytes);
    at = block + head;
    for (i = 0; i < n->count; i++) {
        size_t len = strlen((const char *)at);
        texts[i].ptr = at;
        texts[i].len = (int64_t)len;
        at += len + 1;
    }
    *count = n->count;
    return texts;
}

/* The entries of the directory, without `.` and `..`, in the order the
   system gives them, or NULL with errno set. */
struct anti_text *anti_rt_fs_list(const unsigned char *path, int64_t len,
                                  int64_t *count)
{
    struct names n = {NULL, 0, 0, 0};
    int failed = 0;
    int saved;
#if defined(_WIN32)
    /* The pattern of the search is the directory, a separator and `*`. */
    path_char *pattern = native_path(path, len, 2);
    struct _wfinddata64_t data;
    unsigned char name[3 * (sizeof data.name / sizeof data.name[0])];
    intptr_t search;
    size_t units;

    if (pattern == NULL) {
        return NULL;
    }
    units = wcslen(pattern);
    if (units == 0) {
        release(pattern);
        errno = ENOENT;
        return NULL;
    }
    if (pattern[units - 1] != L'/' && pattern[units - 1] != L'\\') {
        pattern[units++] = L'\\';
    }
    pattern[units++] = L'*';
    pattern[units] = 0;
    search = _wfindfirst64(pattern, &data);
    release(pattern);
    if (search == -1) {
        return NULL;
    }
    do {
        size_t written = anti_utf16_to_utf8((const uint16_t *)data.name,
                                            wcslen(data.name), name);
        if (!is_dot_entry(name, written) &&
            add_name(&n, name, written) != 0) {
            failed = 1;
            break;
        }
    } while (_wfindnext64(search, &data) == 0);
    /* The search ends with ENOENT when no entry is left. */
    if (!failed && errno != ENOENT) {
        failed = 1;
    }
    saved = errno;
    _findclose(search);
#else
    path_char *name = native_path(path, len, 0);
    struct dirent *entry;
    DIR *dir;

    if (name == NULL) {
        return NULL;
    }
    dir = opendir(name);
    release(name);
    if (dir == NULL) {
        return NULL;
    }
    for (;;) {
        size_t length;
        /* readdir gives NULL at the end and on a failure, and only a
           failure sets errno. */
        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            failed = errno != 0;
            break;
        }
        length = strlen(entry->d_name);
        if (!is_dot_entry((const unsigned char *)entry->d_name, length) &&
            add_name(&n, (const unsigned char *)entry->d_name, length) != 0) {
            failed = 1;
            break;
        }
    }
    saved = errno;
    closedir(dir);
#endif
    if (failed) {
        free(n.bytes);
        errno = saved;
        return NULL;
    }
    return finish(&n, count);
}

/* Remove the file, or give -1 with errno set. */
int32_t anti_rt_fs_remove(const unsigned char *path, int64_t len)
{
    path_char *name = native_path(path, len, 0);
    int status;

    if (name == NULL) {
        return -1;
    }
#if defined(_WIN32)
    status = _wremove(name);
#else
    status = remove(name);
#endif
    release(name);
    return status == 0 ? 0 : -1;
}

/* Give the file at from the path to, or give -1 with errno set. */
int32_t anti_rt_fs_rename(const unsigned char *from, int64_t from_len,
                          const unsigned char *to, int64_t to_len)
{
    path_char *old_name = native_path(from, from_len, 0);
    path_char *new_name;
    int status;

    if (old_name == NULL) {
        return -1;
    }
    new_name = native_path(to, to_len, 0);
    if (new_name == NULL) {
        release(old_name);
        return -1;
    }
#if defined(_WIN32)
    status = _wrename(old_name, new_name);
#else
    status = rename(old_name, new_name);
#endif
    release(old_name);
    release(new_name);
    return status == 0 ? 0 : -1;
}
