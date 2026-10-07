/* The platform layer of anti, which docs/c-guidelines.md names under rule
   22. With src/antic/platform.c it is the one file of anti that branches
   on the host, which the test platform_layer holds.

   DESIGN: as in the layer of antic, every path is UTF-8, and on Windows
   each call takes UTF-16, converted by platform_widen. No call of an ANSI
   entry point is made, since those read the bytes in the code page of
   the machine. */

/* lstat, chmod, mkdir, rmdir, unlink, open, fstat, fcntl and fdopen are
   POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L

#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <windows.h>

#include "text.h"

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

/* Give the file from the name to with the semantics of a POSIX rename.
   Returns ERROR_SUCCESS, or the error of Windows. */
static DWORD rename_posix(const wchar_t *from, const wchar_t *to)
{
    DWORD room = GetFullPathNameW(to, 0, NULL, NULL);
    DWORD length;
    DWORD size;
    FILE_RENAME_INFO *info;
    HANDLE file;
    DWORD error = ERROR_SUCCESS;

    if (room == 0) {
        return GetLastError();
    }
    /* FILE_RENAME_INFO holds the first character of the name. */
    size = (DWORD)sizeof *info + room * (DWORD)sizeof(wchar_t);
    info = calloc(1, size);
    if (info == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    length = GetFullPathNameW(to, room, info->FileName, NULL);
    if (length == 0 || length >= room) {
        free(info);
        return length == 0 ? GetLastError() : ERROR_INVALID_NAME;
    }
    info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS |
                  FILE_RENAME_FLAG_POSIX_SEMANTICS;
    info->FileNameLength = length * (DWORD)sizeof(wchar_t);
    /* A link is renamed itself, as MoveFileExW does. */
    file = CreateFileW(from, DELETE | SYNCHRONIZE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
    } else {
        if (!SetFileInformationByHandle(file, FileRenameInfoEx, info, size)) {
            error = GetLastError();
        }
        CloseHandle(file);
    }
    free(info);
    return error;
}

bool platform_replace(const char *from, const char *to)
{
    wchar_t *wide_from = platform_widen(from);
    wchar_t *wide_to = platform_widen(to);
    bool replaced = wide_from != NULL && wide_to != NULL &&
                    MoveFileExW(wide_from, wide_to,
                                MOVEFILE_REPLACE_EXISTING |
                                    MOVEFILE_WRITE_THROUGH) != 0;

    free(wide_from);
    free(wide_to);
    return replaced;
}

/* Move the file at to aside, to the name aside, give from the name to and
   delete the old file. The old file takes its name back when from cannot
   have it. */
static bool replace_aside(const wchar_t *from, const wchar_t *to,
                          const wchar_t *aside)
{
    if (rename_posix(to, aside) != ERROR_SUCCESS) {
        return false;
    }
    if (rename_posix(from, to) != ERROR_SUCCESS) {
        rename_posix(aside, to);
        return false;
    }
    /* The delete takes the name at once while a holder keeps the file
       mapped as data. One mapped as an image refuses it, and the next
       replace of the same program removes the file. */
    DeleteFileW(aside);
    return true;
}

/* DESIGN: Smart App Control holds a new program for up to 1.6 s after it
   ran, which the DESIGN of files_copy_program measures. It maps the file
   as data at times and as an image at others. MoveFileExW refused to
   replace a file mapped as data with ERROR_ACCESS_DENIED. A rename with
   FILE_RENAME_FLAG_POSIX_SEMANTICS replaces a file mapped as data at
   once: the name goes to the new file, and the holder keeps the old one
   until it lets go. A file mapped as an image refuses that rename as
   well, and still lets itself be renamed, so the replace moves it aside
   to the name with PLATFORM_OLD_SUFFIX and gives from its name. The old
   file is deleted at once when Windows allows it and by the next replace
   otherwise, which first removes what an earlier one left. That name in
   dist/ belongs to anti, so the remove takes no file of the user. NTFS
   takes the flag from Windows 10 1709. A file system without it, or an
   older Windows, refuses the call with ERROR_INVALID_PARAMETER,
   ERROR_NOT_SUPPORTED or ERROR_INVALID_FUNCTION, and the replace is then
   platform_replace. A holder that shares no deletion still makes the
   replace fail, and to stays as it was. */
bool platform_replace_program(const char *from, const char *to)
{
    struct text name = {0};
    wchar_t *wide_from = platform_widen(from);
    wchar_t *wide_to = platform_widen(to);
    wchar_t *aside;
    bool replaced = false;
    DWORD error;

    text_appendf(&name, "%s%s", to, PLATFORM_OLD_SUFFIX);
    aside = platform_widen(text_cstr(&name));
    text_free(&name);
    if (wide_from != NULL && wide_to != NULL && aside != NULL) {
        DeleteFileW(aside);
        error = rename_posix(wide_from, wide_to);
        if (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED ||
            error == ERROR_INVALID_FUNCTION) {
            replaced = platform_replace(from, to);
        } else if (error == ERROR_ACCESS_DENIED) {
            replaced = replace_aside(wide_from, wide_to, aside);
        } else {
            replaced = error == ERROR_SUCCESS;
        }
    }
    free(wide_from);
    free(wide_to);
    free(aside);
    return replaced;
}

FILE *platform_open_file(const char *path)
{
    wchar_t *wide = platform_widen(path);
    HANDLE handle;
    int fd;
    FILE *f;

    if (wide == NULL) {
        return NULL;
    }
    handle = CreateFileW(wide, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    /* A device such as CON or NUL is FILE_TYPE_CHAR and a pipe
       FILE_TYPE_PIPE. */
    if (GetFileType(handle) != FILE_TYPE_DISK) {
        CloseHandle(handle);
        return NULL;
    }
    fd = _open_osfhandle((intptr_t)handle, _O_RDONLY | _O_BINARY);
    if (fd == -1) {
        CloseHandle(handle);
        return NULL;
    }
    f = _fdopen(fd, "rb");
    if (f == NULL) {
        _close(fd);
    }
    return f;
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

#include <fcntl.h>
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

bool platform_replace(const char *from, const char *to)
{
    return rename(from, to) == 0;
}

bool platform_replace_program(const char *from, const char *to)
{
    return rename(from, to) == 0;
}

/* O_NONBLOCK lets the open of a FIFO return at once rather than wait
   for a writer. The read of a regular file ignores it, and the flag is
   taken off before the stream reads all the same. */
FILE *platform_open_file(const char *path)
{
    struct stat st;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    int flags;
    FILE *f;

    if (fd < 0) {
        return NULL;
    }
    flags = fcntl(fd, F_GETFL);
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || flags == -1 ||
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        close(fd);
        return NULL;
    }
    f = fdopen(fd, "rb");
    if (f == NULL) {
        close(fd);
    }
    return f;
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
