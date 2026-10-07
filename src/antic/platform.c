/* The platform layer of antic and anti, which docs/c-guidelines.md names
   under rule 22. It is the one file of antic that branches on the host,
   which the test platform_layer holds, and anti calls it too. The calls
   of the C library that the C runtime of Windows deprecates are made
   here, so no build needs _CRT_SECURE_NO_WARNINGS.

   DESIGN: the tools hold every path, argument and variable as UTF-8. On
   Windows each call takes and gives UTF-16, converted here, and no call
   of an ANSI entry point is made, since those read the bytes in the code
   page of the machine. A letter outside that code page would otherwise
   name another file, or none. */

/* readlink, stat, realpath, opendir, posix_spawn, fork, chdir and waitpid
   are POSIX, outside the C11 library, and glibc declares realpath for
   X/Open alone, which takes POSIX with it. */
#define _XOPEN_SOURCE 700

#include "platform.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "target.h"

/* The work and its context, handed to the thread of
   platform_run_on_stack. */
struct stack_call {
    void (*work)(void *context);
    void *context;
};

/* DESIGN: the host is fixed when the tools are compiled, from the
   compiler's predefined macros. The antic_host_target test compares the
   result with the name CMake computes. */
bool target_host(enum target *t)
{
#if defined(__APPLE__) && defined(__aarch64__)
    *t = TARGET_MACOS_ARM64;
#elif defined(__APPLE__) && defined(__x86_64__)
    *t = TARGET_MACOS_X86_64;
#elif defined(__linux__) && defined(__aarch64__)
    *t = TARGET_LINUX_ARM64;
#elif defined(__linux__) && defined(__x86_64__)
    *t = TARGET_LINUX_X86_64;
#elif defined(_WIN32) && defined(_M_ARM64)
    *t = TARGET_WINDOWS_ARM64;
#elif defined(_WIN32) && defined(_M_X64)
    *t = TARGET_WINDOWS_X86_64;
#else
    *t = TARGET_COUNT;
#endif
    return *t != TARGET_COUNT;
}

#if defined(_WIN32)

#include <corecrt_startup.h>
#include <errno.h>
#include <share.h>
#include <windows.h>

wchar_t *platform_widen(const char *text)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                                    NULL, 0);
    wchar_t *wide;

    if (count <= 0) {
        return NULL;
    }
    wide = alloc_zeroed((size_t)count, sizeof *wide);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide,
                            count) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

/* The UTF-8 of wide. A unit that is no valid UTF-16 becomes U+FFFD, or
   gives NULL when strict is set. The caller frees the result with free. */
static char *narrow(const wchar_t *wide, bool strict)
{
    DWORD flags = strict ? WC_ERR_INVALID_CHARS : 0;
    int count = WideCharToMultiByte(CP_UTF8, flags, wide, -1, NULL, 0, NULL,
                                    NULL);
    char *bytes;

    if (count <= 0) {
        return NULL;
    }
    bytes = alloc_zeroed((size_t)count, 1);
    if (WideCharToMultiByte(CP_UTF8, flags, wide, -1, bytes, count, NULL,
                            NULL) <= 0) {
        free(bytes);
        return NULL;
    }
    return bytes;
}

/* Append the UTF-8 of wide to out. */
static bool append_narrow(struct text *out, const wchar_t *wide)
{
    char *bytes = narrow(wide, false);

    if (bytes == NULL) {
        return false;
    }
    text_append(out, bytes);
    free(bytes);
    return true;
}

/* DESIGN: fopen of the Windows C runtime is _fsopen with _SH_DENYNO,
   which shares the file for reading and writing. The C runtime
   deprecates the first and not the second, so the call is the same one
   by the name that is not deprecated, in its form for UTF-16. fopen_s
   would lock the file against every other open. */
/* DESIGN: Windows lets a scanner of the machine hold a program that just
   ran, or was just written, without sharing it for writing, for a moment.
   A write then fails with EACCES. anti build writes the program into
   dist/ at every build, right after a user ran it, and failed three builds
   in ten on the Windows VM. A write waits for the file up to two seconds,
   in steps of 50 ms, as the file copies of CMake retry. A read never
   waits, and a file that stays locked still fails. A program that Smart
   App Control holds mapped fails with EINVAL instead, which this wait
   does not cover. files_copy_program of anti replaces such a file. */
#define OPEN_WRITE_TRIES 40
#define OPEN_WRITE_STEP_MS 50

FILE *platform_open(const char *path, bool writing)
{
    wchar_t *wide = platform_widen(path);
    const wchar_t *mode = writing ? L"wb" : L"rb";
    FILE *f;
    int tries = 1;

    if (wide == NULL) {
        errno = EINVAL;
        return NULL;
    }
    f = _wfsopen(wide, mode, _SH_DENYNO);
    while (f == NULL && writing && errno == EACCES &&
           tries < OPEN_WRITE_TRIES) {
        Sleep(OPEN_WRITE_STEP_MS);
        f = _wfsopen(wide, mode, _SH_DENYNO);
        tries++;
    }
    free(wide);
    return f;
}

bool platform_remove(const char *path)
{
    wchar_t *wide = platform_widen(path);
    bool removed = wide != NULL && _wremove(wide) == 0;

    free(wide);
    return removed;
}

/* DESIGN: getenv gives storage that the program never frees. _wdupenv_s,
   the call the C runtime names instead, gives a copy the caller frees.
   Its UTF-8 is kept here until the program ends. antic and anti read a
   handful of variables, each once. */
const char *platform_getenv(const char *name)
{
    wchar_t *wide_name = platform_widen(name);
    wchar_t *value = NULL;
    size_t size = 0;
    char *bytes;

    if (wide_name == NULL) {
        return NULL;
    }
    if (_wdupenv_s(&value, &size, wide_name) != 0 || value == NULL) {
        free(wide_name);
        return NULL;
    }
    bytes = narrow(value, true);
    free(value);
    free(wide_name);
    return bytes;
}

/* DESIGN: the C runtime splits the command line into the arguments of
   main in the code page of the machine. _configure_wide_argv asks it to
   split the same line into UTF-16 by the same rules, as it does for a
   program whose entry is wmain, so the count is the one main was given.
   The UTF-8 list is kept until the program ends. */
char **platform_arguments(char **argv)
{
    wchar_t **wide;
    char **list;
    size_t count = 0;
    size_t i;

    if (_configure_wide_argv(_crt_argv_unexpanded_arguments) != 0 ||
        (wide = __wargv) == NULL) {
        return argv;
    }
    while (wide[count] != NULL) {
        count++;
    }
    list = alloc_zeroed(alloc_sum(count, 1), sizeof *list);
    for (i = 0; i < count; i++) {
        list[i] = narrow(wide[i], false);
        if (list[i] == NULL) {
            list[i] = alloc_zeroed(1, 1);
        }
    }
    return list;
}

bool path_is_absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' ||
           ((path[0] | 32) >= 'a' && (path[0] | 32) <= 'z' && path[1] == ':');
}

const char *platform_last_separator(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    return back != NULL && (slash == NULL || back > slash) ? back : slash;
}

char platform_separator(void)
{
    return '\\';
}

bool directory_exists(const char *path)
{
    wchar_t *wide = platform_widen(path);
    DWORD attributes = wide != NULL ? GetFileAttributesW(wide)
                                    : INVALID_FILE_ATTRIBUTES;

    free(wide);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool platform_list_directory(const char *path,
                             void (*each)(void *context, const char *name),
                             void *context)
{
    struct text pattern = {0};
    WIN32_FIND_DATAW entry;
    wchar_t *wide;
    HANDLE find;
    bool complete;

    text_appendf(&pattern, "%s\\*", path);
    wide = platform_widen(text_cstr(&pattern));
    text_free(&pattern);
    if (wide == NULL) {
        return false;
    }
    find = FindFirstFileW(wide, &entry);
    free(wide);
    if (find == INVALID_HANDLE_VALUE) {
        /* The root of a drive has no `.`, so an empty one matches no
           name. */
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        char *name;
        if (wcscmp(entry.cFileName, L".") == 0 ||
            wcscmp(entry.cFileName, L"..") == 0) {
            continue;
        }
        name = narrow(entry.cFileName, false);
        if (name != NULL) {
            each(context, name);
            free(name);
        }
    } while (FindNextFileW(find, &entry));
    /* Read before FindClose, which may set the error again. */
    complete = GetLastError() == ERROR_NO_MORE_FILES;
    FindClose(find);
    return complete;
}

static bool self_path(struct text *out)
{
    wchar_t wide[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, wide, MAX_PATH);

    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    return append_narrow(out, wide);
}

bool absolute_path(const char *path, struct text *out)
{
    wchar_t *wide = platform_widen(path);
    wchar_t *full = NULL;
    DWORD length;
    size_t start = out->length;
    size_t i;
    bool ok = false;

    if (wide == NULL) {
        return false;
    }
    length = GetFullPathNameW(wide, 0, NULL, NULL);
    if (length > 0) {
        full = alloc_zeroed(length, sizeof *full);
        ok = GetFullPathNameW(wide, length, full, NULL) > 0 &&
             append_narrow(out, full);
    }
    for (i = start; ok && i < out->length; i++) {
        if (out->data[i] == '\\') {
            out->data[i] = '/';
        }
    }
    free(full);
    free(wide);
    return ok;
}

/* DESIGN: Windows takes one command line where POSIX takes a list, and
   the C runtime of the program parses it back. An argument is quoted by
   those rules: a run of backslashes doubles before a quote, and a quote
   of the argument itself is escaped. */
static void quote(const char *arg, struct text *out)
{
    size_t i;

    if (arg[0] != '\0' && strpbrk(arg, " \t\n\v\"") == NULL) {
        text_append(out, arg);
        return;
    }
    text_append(out, "\"");
    for (i = 0; arg[i] != '\0';) {
        size_t slashes = 0;
        while (arg[i] == '\\') {
            slashes++;
            i++;
        }
        if (arg[i] == '\0' || arg[i] == '"') {
            slashes *= 2;
        }
        while (slashes-- > 0) {
            text_append(out, "\\");
        }
        if (arg[i] == '\0') {
            break;
        }
        if (arg[i] == '"') {
            text_append(out, "\\");
        }
        text_append_bytes(out, arg + i, 1);
        i++;
    }
    text_append(out, "\"");
}

static int wait_for(HANDLE process, const char *name)
{
    DWORD code = 0;

    if (WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(process, &code)) {
        fprintf(stderr, "antic: waiting for %s: error %lu\n", name,
                (unsigned long)GetLastError());
        return -1;
    }
    return (int)code;
}

/* The program that a relative path with a separator names from
   directory, with .exe when its name has no suffix. CreateProcessW would
   look for it from the directory of antic. */
static wchar_t *program_in(const char *directory, const char *program)
{
    struct text path = {0};
    const char *cut = platform_last_separator(program);
    const char *name = cut != NULL ? cut + 1 : program;
    wchar_t *wide;

    if (directory == NULL || cut == NULL || path_is_absolute(program)) {
        return NULL;
    }
    text_appendf(&path, "%s\\%s%s", directory, program,
                 strchr(name, '.') == NULL ? ".exe" : "");
    wide = platform_widen(text_cstr(&path));
    text_free(&path);
    return wide;
}

/* Start argv[0] with the command line that argv spells, in directory or
   in the current one. Without an application name CreateProcessW searches
   PATH and appends .exe, as a shell does. */
static int start(const char *directory, const char *const argv[],
                 HANDLE output, PROCESS_INFORMATION *info)
{
    struct text line = {0};
    STARTUPINFOW startup;
    wchar_t *wide;
    wchar_t *wide_directory = NULL;
    wchar_t *application;
    size_t i;
    BOOL started;

    for (i = 0; argv[i] != NULL; i++) {
        if (i > 0) {
            text_append(&line, " ");
        }
        quote(argv[i], &line);
    }
    wide = platform_widen(text_cstr(&line));
    text_free(&line);
    if (directory != NULL) {
        wide_directory = platform_widen(directory);
    }
    if (wide == NULL || (directory != NULL && wide_directory == NULL)) {
        fprintf(stderr, "antic: cannot run %s: a path is not UTF-8\n",
                argv[0]);
        free(wide);
        return -1;
    }
    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    if (output != NULL) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = output;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    application = program_in(directory, argv[0]);
    started = CreateProcessW(application, wide, NULL, NULL, output != NULL, 0,
                             NULL, wide_directory, &startup, info);
    free(wide);
    free(wide_directory);
    free(application);
    if (!started) {
        fprintf(stderr, "antic: cannot run %s: error %lu\n", argv[0],
                (unsigned long)GetLastError());
        return -1;
    }
    return 0;
}

int process_run_in(const char *directory, const char *const argv[])
{
    PROCESS_INFORMATION info;
    int status;

    if (start(directory, argv, NULL, &info) != 0) {
        return -1;
    }
    status = wait_for(info.hProcess, argv[0]);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return status;
}

int process_run(const char *const argv[])
{
    return process_run_in(NULL, argv);
}

int process_capture(const char *const argv[], struct text *out)
{
    SECURITY_ATTRIBUTES attributes;
    PROCESS_INFORMATION info;
    HANDLE reader;
    HANDLE writer;
    char buffer[4096];
    DWORD read_bytes;
    int status;

    memset(&attributes, 0, sizeof attributes);
    attributes.nLength = sizeof attributes;
    attributes.bInheritHandle = TRUE;
    if (!CreatePipe(&reader, &writer, &attributes, 0)) {
        fprintf(stderr, "antic: pipe: error %lu\n",
                (unsigned long)GetLastError());
        return -1;
    }
    /* The child writes into the pipe and never reads from it. */
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    if (start(NULL, argv, writer, &info) != 0) {
        CloseHandle(reader);
        CloseHandle(writer);
        return -1;
    }
    CloseHandle(writer);
    while (ReadFile(reader, buffer, sizeof buffer - 1, &read_bytes, NULL) &&
           read_bytes > 0) {
        buffer[read_bytes] = '\0';
        text_append(out, buffer);
    }
    CloseHandle(reader);
    status = wait_for(info.hProcess, argv[0]);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    return status;
}

static DWORD WINAPI stack_start(LPVOID context)
{
    struct stack_call *call = context;

    call->work(call->context);
    return 0;
}

bool platform_run_on_stack(void (*work)(void *context), void *context)
{
    struct stack_call call;
    HANDLE thread;

    call.work = work;
    call.context = context;
    /* The size reserves the stack, and Windows commits its pages as the
       thread reaches them. */
    thread = CreateThread(NULL, PLATFORM_WORK_STACK, stack_start, &call,
                          STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (thread == NULL) {
        fprintf(stderr, "antic: cannot start the thread of the work: "
                        "error %lu\n",
                (unsigned long)GetLastError());
        return false;
    }
    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
        fprintf(stderr, "antic: cannot wait for the thread of the work: "
                        "error %lu\n",
                (unsigned long)GetLastError());
        CloseHandle(thread);
        return false;
    }
    CloseHandle(thread);
    return true;
}

#else

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

extern char **environ;

FILE *platform_open(const char *path, bool writing)
{
    return fopen(path, writing ? "wb" : "rb");
}

bool platform_remove(const char *path)
{
    return remove(path) == 0;
}

const char *platform_getenv(const char *name)
{
    return getenv(name);
}

/* A POSIX system hands main the bytes of each argument as they were
   given, which the tools read as UTF-8. */
char **platform_arguments(char **argv)
{
    return argv;
}

bool path_is_absolute(const char *path)
{
    return path[0] == '/';
}

const char *platform_last_separator(const char *path)
{
    return strrchr(path, '/');
}

char platform_separator(void)
{
    return '/';
}

bool directory_exists(const char *path)
{
    struct stat info;

    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool platform_list_directory(const char *path,
                             void (*each)(void *context, const char *name),
                             void *context)
{
    DIR *dir = opendir(path);
    struct dirent *entry;
    bool complete;

    if (dir == NULL) {
        return false;
    }
    /* readdir gives NULL at the end and on an error alike, and sets errno
       for the error alone. each may set errno, so it is cleared before
       every call. */
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            complete = errno == 0;
            break;
        }
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            each(context, entry->d_name);
        }
    }
    closedir(dir);
    return complete;
}

/* DESIGN: an installed antic sits in bin/ of the runtime archive, so the
   directory above it holds lib/, std/ and sysroot/. Asking the system
   where the executable is lets a program compile without --runtime, and
   each system answers a different way. */
#if defined(__APPLE__)

static bool self_path(struct text *out)
{
    uint32_t size = 0;
    char *path;

    _NSGetExecutablePath(NULL, &size);
    if (size == 0) {
        return false;
    }
    path = alloc_zeroed(size, 1);
    if (_NSGetExecutablePath(path, &size) != 0) {
        free(path);
        return false;
    }
    text_append(out, path);
    free(path);
    return true;
}

#else

static bool self_path(struct text *out)
{
    char path[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", path, sizeof path - 1);

    if (length <= 0) {
        return false;
    }
    path[length] = '\0';
    text_append(out, path);
    return true;
}

#endif

bool absolute_path(const char *path, struct text *out)
{
    char *real = realpath(path, NULL);
    const char *slash;
    struct text directory = {0};

    if (real != NULL) {
        text_append(out, real);
        free(real);
        return true;
    }
    slash = strrchr(path, '/');
    if (slash == NULL) {
        text_append(&directory, ".");
    } else if (slash == path) {
        text_append(&directory, "/");
    } else {
        text_appendf(&directory, "%.*s", (int)(slash - path), path);
    }
    real = realpath(text_cstr(&directory), NULL);
    text_free(&directory);
    if (real == NULL) {
        return false;
    }
    text_appendf(out, "%s%s%s", real, strcmp(real, "/") == 0 ? "" : "/",
                 slash != NULL ? slash + 1 : path);
    free(real);
    return true;
}

static int wait_for(pid_t pid, const char *name)
{
    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "antic: waiting for %s: %s\n", name,
                    strerror(errno));
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    fprintf(stderr, "antic: %s ended by signal %d\n", name, WTERMSIG(status));
    return -1;
}

/* Start argv[0] with the given file actions. posix_spawnp searches PATH
   the way a shell does. */
static int spawn(const char *const argv[], posix_spawn_file_actions_t *actions,
                 pid_t *pid)
{
    int err = posix_spawnp(pid, argv[0], actions, NULL,
                           (char *const *)argv, environ);
    if (err != 0) {
        fprintf(stderr, "antic: cannot run %s: %s\n", argv[0], strerror(err));
        return -1;
    }
    return 0;
}

int process_run(const char *const argv[])
{
    pid_t pid;

    if (spawn(argv, NULL, &pid) != 0) {
        return -1;
    }
    return wait_for(pid, argv[0]);
}

/* DESIGN: posix_spawn sets no working directory before POSIX.1-2024, so
   the child changes to it between fork and exec. The child calls only
   chdir, execvp and _exit there, and a message on failure. */
int process_run_in(const char *directory, const char *const argv[])
{
    pid_t pid;

    if (directory == NULL) {
        return process_run(argv);
    }
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "antic: cannot run %s: %s\n", argv[0], strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (chdir(directory) != 0) {
            fprintf(stderr, "antic: cannot enter %s: %s\n", directory,
                    strerror(errno));
        } else {
            execvp(argv[0], (char *const *)argv);
            fprintf(stderr, "antic: cannot run %s: %s\n", argv[0],
                    strerror(errno));
        }
        _exit(127);
    }
    return wait_for(pid, argv[0]);
}

int process_capture(const char *const argv[], struct text *out)
{
    posix_spawn_file_actions_t actions;
    int fds[2];
    pid_t pid;
    char buffer[4096];
    ssize_t n;
    int started;

    if (pipe(fds) != 0) {
        fprintf(stderr, "antic: pipe: %s\n", strerror(errno));
        return -1;
    }
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    started = spawn(argv, &actions, &pid);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (started != 0) {
        close(fds[0]);
        return -1;
    }
    while ((n = read(fds[0], buffer, sizeof buffer - 1)) > 0) {
        buffer[n] = '\0';
        text_append(out, buffer);
    }
    close(fds[0]);
    return wait_for(pid, argv[0]);
}

/* The environment of the tool with name set to value: a copy of environ
   whose entry of name gives way to the new one. The strings stay those of
   environ, and the caller frees the list and entry. */
static char **environment_with(const char *name, const char *value,
                               struct text *entry)
{
    size_t length = strlen(name);
    size_t count = 0;
    size_t kept = 0;
    char **list;
    size_t i;

    while (environ != NULL && environ[count] != NULL) {
        count++;
    }
    list = alloc_zeroed(alloc_sum(count, 2), sizeof *list);
    for (i = 0; i < count; i++) {
        if (strncmp(environ[i], name, length) != 0 ||
            environ[i][length] != '=') {
            list[kept++] = environ[i];
        }
    }
    text_appendf(entry, "%s=%s", name, value);
    list[kept++] = entry->data;
    list[kept] = NULL;
    return list;
}

int process_run_lines(const char *const argv[], const char *name,
                      const char *value,
                      void (*each)(void *context, const char *line,
                                   size_t length),
                      void *context)
{
    posix_spawn_file_actions_t actions;
    struct text entry = {0};
    struct text pending = {0};
    char **env;
    int fds[2];
    pid_t pid;
    char buffer[4096];
    ssize_t n;
    int err;

    env = environment_with(name, value, &entry);
    if (pipe(fds) != 0) {
        fprintf(stderr, "antic: pipe: %s\n", strerror(errno));
        free(env);
        text_free(&entry);
        return -1;
    }
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    err = posix_spawnp(&pid, argv[0], &actions, NULL, (char *const *)argv,
                       env);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    free(env);
    text_free(&entry);
    if (err != 0) {
        fprintf(stderr, "antic: cannot run %s: %s\n", argv[0], strerror(err));
        close(fds[0]);
        return -1;
    }
    while ((n = read(fds[0], buffer, sizeof buffer)) > 0) {
        size_t start = 0;
        size_t i;
        for (i = 0; i < (size_t)n; i++) {
            if (buffer[i] != '\n') {
                continue;
            }
            text_append_bytes(&pending, buffer + start, i + 1 - start);
            each(context, pending.data, pending.length);
            pending.length = 0;
            start = i + 1;
        }
        text_append_bytes(&pending, buffer + start, (size_t)n - start);
    }
    if (pending.length > 0) {
        each(context, pending.data, pending.length);
    }
    close(fds[0]);
    text_free(&pending);
    return wait_for(pid, argv[0]);
}

static void *stack_start(void *context)
{
    struct stack_call *call = context;

    call->work(call->context);
    return NULL;
}

bool platform_run_on_stack(void (*work)(void *context), void *context)
{
    struct stack_call call;
    pthread_attr_t attributes;
    pthread_t thread;
    int error;

    call.work = work;
    call.context = context;
    error = pthread_attr_init(&attributes);
    if (error == 0) {
        error = pthread_attr_setstacksize(&attributes, PLATFORM_WORK_STACK);
        if (error == 0) {
            error = pthread_create(&thread, &attributes, stack_start, &call);
        }
        pthread_attr_destroy(&attributes);
    }
    if (error != 0) {
        fprintf(stderr, "antic: cannot start the thread of the work: %s\n",
                strerror(error));
        return false;
    }
    error = pthread_join(thread, NULL);
    if (error != 0) {
        fprintf(stderr, "antic: cannot wait for the thread of the work: %s\n",
                strerror(error));
        return false;
    }
    return true;
}

#endif

bool self_directory(struct text *out)
{
    struct text path = {0};
    const char *bytes;
    const char *cut;

    if (!self_path(&path)) {
        text_free(&path);
        return false;
    }
    bytes = text_cstr(&path);
    cut = platform_last_separator(bytes);
    if (cut == NULL || cut == bytes) {
        text_free(&path);
        return false;
    }
    text_append_bytes(out, bytes, (size_t)(cut - bytes));
    text_free(&path);
    return true;
}

/* The length of the root of an absolute path. That is a drive such as
   `C:`, the host and share of a path that starts with `//`, or nothing. */
static size_t root_length(const char *path)
{
    size_t n = 0;
    int parts = 0;

    if (strncmp(path, "//", 2) != 0) {
        return strcspn(path, "/");
    }
    n = 2;
    while (parts < 2 && path[n] != '\0') {
        n += strcspn(path + n, "/");
        parts++;
        if (parts < 2 && path[n] == '/') {
            n++;
        }
    }
    return n;
}

static bool same_root(const char *a, size_t n, const char *b, size_t m)
{
    size_t i;

    if (n != m) {
        return false;
    }
    for (i = 0; i < n; i++) {
        char x = a[i] >= 'A' && a[i] <= 'Z' ? (char)(a[i] - 'A' + 'a') : a[i];
        char y = b[i] >= 'A' && b[i] <= 'Z' ? (char)(b[i] - 'A' + 'a') : b[i];
        if (x != y) {
            return false;
        }
    }
    return true;
}

/* The next part of a path after at, skipping empty parts. Returns its
   length, 0 at the end. */
static size_t part(const char *path, size_t *at)
{
    while (path[*at] == '/') {
        (*at)++;
    }
    return strcspn(path + *at, "/");
}

bool path_relative(struct text *out, const char *path, const char *directory)
{
    size_t root = root_length(path);
    size_t at = root;
    size_t from = root_length(directory);
    size_t start = out->length;
    size_t n;
    size_t m;

    if (!same_root(path, root, directory, from)) {
        return false;
    }
    /* The parts the two share. */
    for (;;) {
        size_t a = at;
        size_t b = from;
        n = part(path, &a);
        m = part(directory, &b);
        if (n == 0 || n != m || strncmp(path + a, directory + b, n) != 0) {
            at = a;
            from = b;
            break;
        }
        at = a + n;
        from = b + m;
    }
    /* A `..` for each part of the directory left, then the rest of the
       path. */
    while ((m = part(directory, &from)) > 0) {
        text_append(out, out->length > start ? "/.." : "..");
        from += m;
    }
    while ((n = part(path, &at)) > 0) {
        text_appendf(out, "%s%.*s", out->length > start ? "/" : "", (int)n,
                     path + at);
        at += n;
    }
    if (out->length == start) {
        text_append(out, ".");
    }
    return true;
}
