/* The C side of anti.fs: opening a file, its size, the listing of a
   directory, removing and renaming. Reading, writing and closing are the
   C streams themselves, which the module calls directly.

   DESIGN: every function reports a failure through errno and nothing
   else, so the module builds each error with SystemError.from_errno on
   every system. The calls of the system stand in the platform layer,
   which takes a path in UTF-8 on every system. */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grow.h"
#include "platform.h"
#include "std.h"

/* Free p and keep errno, which a free before POSIX.1-2024 may change. */
static void release(void *p)
{
    int saved = errno;

    free(p);
    errno = saved;
}

/* The path of the len bytes at bytes, ended by a NUL, or NULL with
   errno set.

   DESIGN: a str carries its length, and a slice of one ends where its
   bytes do and not at a NUL. So the bytes are copied. A NUL inside
   them would name another file than the one the program wrote. The copy
   refuses it with EINVAL. */
static char *native_path(const unsigned char *bytes, int64_t len)
{
    char *path;

    if (len < 0 || len > INT_MAX / 2 ||
        (len > 0 && memchr(bytes, 0, (size_t)len) != NULL)) {
        errno = EINVAL;
        return NULL;
    }
    path = malloc((size_t)len + 1);
    if (path == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    if (len > 0) {
        memcpy(path, bytes, (size_t)len);
    }
    path[len] = '\0';
    return path;
}

void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t mode)
{
    char *name = native_path(path, len);
    FILE *file;

    if (name == NULL) {
        return NULL;
    }
    file = anti_rt_file_open(name, mode);
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
    int64_t at = anti_rt_file_tell(f);
    int64_t end;

    if (at < 0 || anti_rt_file_seek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    end = anti_rt_file_tell(f);
    if (anti_rt_file_seek(f, at, SEEK_SET) != 0) {
        return -1;
    }
    return end;
}

unsigned char *anti_rt_fs_read(const char *path, int64_t *length)
{
    FILE *f = anti_rt_fs_open((const unsigned char *)path,
                              (int64_t)strlen(path), ANTI_FILE_READ);
    unsigned char *bytes = NULL;
    int64_t size;

    if (f == NULL) {
        return NULL;
    }
    /* A directory opens as a stream on macOS and Linux, and on Linux its
       end lies at the largest offset, so the size below would ask for
       2^63 bytes. */
    if (anti_rt_file_is_directory(f)) {
        fclose(f);
        errno = EISDIR;
        return NULL;
    }
    size = anti_rt_fs_size(f);
    if (size >= 0 && (uint64_t)size >= SIZE_MAX) {
        errno = ENOMEM;
    } else if (size >= 0) {
        bytes = malloc((size_t)size + 1);
        if (bytes == NULL) {
            errno = ENOMEM;
        }
    }
    if (bytes != NULL && size > 0 &&
        fread(bytes, 1, (size_t)size, f) != (size_t)size) {
        /* A short read is no lack of memory, whatever errno held. */
        if (errno == 0 || errno == ENOMEM) {
            errno = EIO;
        }
        free(bytes);
        bytes = NULL;
    }
    fclose(f);
    if (bytes != NULL) {
        bytes[size] = '\0';
        *length = size;
    }
    return bytes;
}

/* The names of a listing, each ended by a NUL, one after another. */
struct names {
    unsigned char *bytes;
    int64_t length;
    int64_t capacity;
    int64_t count;
};

static int add_name(struct names *n, const unsigned char *name, size_t len)
{
    unsigned char *grown = NULL;

    if (len < (size_t)INT64_MAX) {
        grown = anti_rt_reserve(n->bytes, &n->capacity, n->length,
                                (int64_t)len + 1, 256);
    }
    if (grown == NULL) {
        errno = ENOMEM;
        return -1;
    }
    n->bytes = grown;
    if (len > 0) {
        memcpy(n->bytes + n->length, name, len);
    }
    n->bytes[n->length + (int64_t)len] = '\0';
    n->length += (int64_t)len + 1;
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
    size_t length = (size_t)n->length;
    unsigned char *block = malloc(head + length + 1);
    struct anti_text *texts = (struct anti_text *)(void *)block;
    const unsigned char *at;
    int64_t i;

    if (block == NULL) {
        release(n->bytes);
        errno = ENOMEM;
        return NULL;
    }
    if (length > 0) {
        memcpy(block + head, n->bytes, length);
    }
    block[head + length] = '\0';
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

/* Add each entry but `.` and `..` to the names at context. */
static int add_entry(void *context, const unsigned char *name, size_t len)
{
    return is_dot_entry(name, len) ? 0 : add_name(context, name, len);
}

/* The entries of the directory, without `.` and `..`, in the order the
   system gives them, or NULL with errno set. */
struct anti_text *anti_rt_fs_list(const unsigned char *path, int64_t len,
                                  int64_t *count)
{
    struct names n = {NULL, 0, 0, 0};
    char *name = native_path(path, len);
    int status;
    int saved;

    if (name == NULL) {
        return NULL;
    }
    status = anti_rt_directory_list(name, add_entry, &n);
    saved = errno;
    release(name);
    if (status != 0) {
        free(n.bytes);
        errno = saved;
        return NULL;
    }
    return finish(&n, count);
}

/* Remove the file, or give -1 with errno set. */
int32_t anti_rt_fs_remove(const unsigned char *path, int64_t len)
{
    char *name = native_path(path, len);
    int status;

    if (name == NULL) {
        return -1;
    }
    status = anti_rt_file_remove(name);
    release(name);
    return status;
}

/* Give the file at from the path to, or give -1 with errno set. */
int32_t anti_rt_fs_rename(const unsigned char *from, int64_t from_len,
                          const unsigned char *to, int64_t to_len)
{
    char *old_name = native_path(from, from_len);
    char *new_name = old_name != NULL ? native_path(to, to_len) : NULL;
    int status = -1;

    if (new_name != NULL) {
        status = anti_rt_file_rename(old_name, new_name);
    }
    release(old_name);
    release(new_name);
    return status;
}
