/* The platform layer of anti, which docs/c-guidelines.md names under rule
   22. With src/antic/platform.c it is the one file of anti that branches
   on the host, which the test platform_layer holds.

   DESIGN: as in the layer of antic, every path is UTF-8, and on Windows
   each call takes UTF-16, converted by platform_widen. No call of an ANSI
   entry point is made, since those read the bytes in the code page of
   the machine. */

/* lstat, chmod, mkdir, rmdir and unlink are POSIX, outside the C11
   library. */
#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#include <direct.h>
#include <windows.h>

static bool not_found(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

/* The attributes of what wide leads to, through every link, or
   INVALID_FILE_ATTRIBUTES with the error of the system. */
static DWORD followed_attributes(const wchar_t *wide)
{
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE handle;
    BOOL known;

    /* No access asked for reads the attributes alone, and
       FILE_FLAG_BACKUP_SEMANTICS opens a directory. */
    handle = CreateFileW(wide, 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return INVALID_FILE_ATTRIBUTES;
    }
    known = GetFileInformationByHandle(handle, &info);
    CloseHandle(handle);
    return known ? info.dwFileAttributes : INVALID_FILE_ATTRIBUTES;
}

enum platform_kind platform_kind(const char *path, bool follow)
{
    wchar_t *wide = platform_widen(path);
    DWORD attributes;
    DWORD error;

    if (wide == NULL) {
        return PLATFORM_MISSING;
    }
    attributes = GetFileAttributesW(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        if (!follow) {
            free(wide);
            return PLATFORM_LINK;
        }
        attributes = followed_attributes(wide);
    }
    error = GetLastError();
    free(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return not_found(error) ? PLATFORM_MISSING : PLATFORM_UNREADABLE;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? PLATFORM_DIRECTORY
                                                        : PLATFORM_FILE;
}

bool platform_make_dir(const char *path)
{
    wchar_t *wide = platform_widen(path);
    bool made = wide != NULL && (_wmkdir(wide) == 0 || errno == EEXIST);

    free(wide);
    return made;
}

bool platform_remove_entry(const char *path)
{
    wchar_t *wide = platform_widen(path);
    DWORD attributes;
    bool removed = false;

    if (wide == NULL) {
        return false;
    }
    attributes = GetFileAttributesW(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            /* A link to a directory is removed as a directory, which
               leaves what it leads to. */
            removed = RemoveDirectoryW(wide) != 0;
        } else {
            SetFileAttributesW(wide, FILE_ATTRIBUTE_NORMAL);
            removed = DeleteFileW(wide) != 0;
        }
    }
    free(wide);
    return removed;
}

bool platform_rename(const char *from, const char *to)
{
    wchar_t *wide_from = platform_widen(from);
    wchar_t *wide_to = platform_widen(to);
    /* The flags of rename of the C runtime, which moves a file to
       another volume as a copy and refuses a to that exists. */
    bool renamed = wide_from != NULL && wide_to != NULL &&
                   MoveFileExW(wide_from, wide_to, MOVEFILE_COPY_ALLOWED) != 0;

    free(wide_from);
    free(wide_to);
    return renamed;
}

/* Windows keeps no permission bits, and the two names are the POSIX
   interface. */
bool platform_copy_permissions(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return true;
}

/* The variable and the lines are the POSIX interface, which the DESIGN
   of platform.h explains. */
int platform_run_symbolized(const char *const argv[], const char *name,
                            const char *value,
                            void (*each)(void *context, const char *line,
                                         size_t length),
                            void *context)
{
    (void)name;
    (void)value;
    (void)each;
    (void)context;
    return process_run(argv);
}

#else

#include <sys/stat.h>
#include <unistd.h>

enum platform_kind platform_kind(const char *path, bool follow)
{
    struct stat st;

    if ((follow ? stat(path, &st) : lstat(path, &st)) != 0) {
        return errno == ENOENT ? PLATFORM_MISSING : PLATFORM_UNREADABLE;
    }
    if (S_ISLNK(st.st_mode)) {
        return PLATFORM_LINK;
    }
    return S_ISDIR(st.st_mode) ? PLATFORM_DIRECTORY : PLATFORM_FILE;
}

bool platform_make_dir(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool platform_remove_entry(const char *path)
{
    struct stat st;

    if (lstat(path, &st) != 0) {
        return false;
    }
    return (S_ISDIR(st.st_mode) ? rmdir(path) : unlink(path)) == 0;
}

bool platform_rename(const char *from, const char *to)
{
    return rename(from, to) == 0;
}

bool platform_copy_permissions(const char *from, const char *to)
{
    struct stat st;

    return stat(from, &st) == 0 && chmod(to, st.st_mode & 07777) == 0;
}

int platform_run_symbolized(const char *const argv[], const char *name,
                            const char *value,
                            void (*each)(void *context, const char *line,
                                         size_t length),
                            void *context)
{
    return process_run_lines(argv, name, value, each, context);
}

#endif
