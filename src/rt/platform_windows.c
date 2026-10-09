/* The platform layer on Windows. See platform.h. */
#if defined(_WIN32)

/* rand_s stands behind this macro, which comes before the first header
   that includes stdlib.h. */
#define _CRT_RAND_S

#include <assert.h>
#include <corecrt_startup.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <share.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#include <windows.h>
#include <dbghelp.h>

#include "cpu_level.h"
#include "platform.h"
#include "rt.h"
#include "signals.h"
#include "std.h"
#include "utf.h"

/* See the DESIGN comment of the same function in platform_posix.c. */
int64_t anti_rt_is_windows(void)
{
    return 1;
}

/* The UCRT deprecates strerror, and strerror_s writes into a buffer the
   caller owns. One per thread keeps the result alive as long as the
   caller needs it. */
const unsigned char *anti_rt_errno_text(int32_t code)
{
    static _Thread_local char text[128];

    if (strerror_s(text, sizeof text, (int)code) != 0) {
        text[0] = '\0';
    }
    return (const unsigned char *)text;
}

int32_t anti_rt_last_error(void)
{
    return (int32_t)GetLastError();
}

/* The most UTF-16 units of the message of a Win32 error. One unit gives
   at most 3 bytes of UTF-8, and a surrogate pair of two units gives 4. */
enum { MESSAGE_UNITS = 512 };

/* DESIGN: FormatMessageW writes the message in UTF-16, in the language
   of the user, and the conversion of the rest of this file makes the
   UTF-8 of a str of it. FormatMessageA would write the ANSI code page,
   which is no UTF-8 on a French or a German Windows, M32 of the second
   audit. A message that does not fit, or that is no valid UTF-16, is
   empty, as one of an unknown code is. */
const unsigned char *anti_rt_last_error_text(int32_t code)
{
    static _Thread_local wchar_t wide[MESSAGE_UNITS];
    static _Thread_local char text[MESSAGE_UNITS * 3 + 1];
    DWORD units = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
        (DWORD)code, 0, wide, MESSAGE_UNITS, NULL);
    int bytes = 0;

    while (units > 0 &&
           (wide[units - 1] == L'\n' || wide[units - 1] == L'\r')) {
        units--;
    }
    if (units > 0) {
        bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide,
                                    (int)units, text, (int)sizeof text - 1,
                                    NULL, NULL);
    }
    text[bytes > 0 ? bytes : 0] = '\0';
    return (const unsigned char *)text;
}

struct anti_rt_monitor {
    SRWLOCK lock;
    CONDITION_VARIABLE conditions[2];
};

/* SRWLOCK_INIT and CONDITION_VARIABLE_INIT are all zero, so the storage
   of a static monitor is ready before main without an initializer. */
static struct anti_rt_monitor monitors[ANTI_RT_LOCK_COUNT];

struct anti_rt_monitor *anti_rt_monitor_of(enum anti_rt_lock which)
{
    return &monitors[which];
}

struct anti_rt_monitor *anti_rt_monitor_new(void)
{
    struct anti_rt_monitor *m = malloc(sizeof *m);

    if (m == NULL) {
        return NULL;
    }
    InitializeSRWLock(&m->lock);
    InitializeConditionVariable(&m->conditions[0]);
    InitializeConditionVariable(&m->conditions[1]);
    return m;
}

/* An SRWLOCK and a condition variable hold nothing of the system. */
void anti_rt_monitor_free(struct anti_rt_monitor *m)
{
    free(m);
}

void anti_rt_monitor_hold(struct anti_rt_monitor *m)
{
    AcquireSRWLockExclusive(&m->lock);
}

void anti_rt_monitor_release(struct anti_rt_monitor *m)
{
    ReleaseSRWLockExclusive(&m->lock);
}

void anti_rt_monitor_wait(struct anti_rt_monitor *m, int condition)
{
    SleepConditionVariableSRW(&m->conditions[condition], &m->lock, INFINITE,
                              0);
}

void anti_rt_monitor_wake_one(struct anti_rt_monitor *m, int condition)
{
    WakeConditionVariable(&m->conditions[condition]);
}

void anti_rt_monitor_wake_all(struct anti_rt_monitor *m, int condition)
{
    WakeAllConditionVariable(&m->conditions[condition]);
}

_Static_assert(sizeof(struct anti_rt_word) == sizeof(SRWLOCK) &&
                   _Alignof(struct anti_rt_word) == _Alignof(SRWLOCK),
               "the word of a Mutex is an SRWLOCK");

void anti_rt_word_lock(struct anti_rt_word *w)
{
    AcquireSRWLockExclusive((PSRWLOCK)w);
}

void anti_rt_word_unlock(struct anti_rt_word *w)
{
    ReleaseSRWLockExclusive((PSRWLOCK)w);
}

/* The body of a thread, on the heap until the thread takes it. */
struct start {
    void (*body)(void);
};

static DWORD WINAPI thread_main(LPVOID start)
{
    void (*body)(void) = ((struct start *)start)->body;

    free(start);
    body();
    return 0;
}

int anti_rt_thread_start(void (*body)(void))
{
    struct start *start = malloc(sizeof *start);
    HANDLE thread;

    if (start == NULL) {
        return -1;
    }
    start->body = body;
    thread = CreateThread(NULL, 0, thread_main, start, 0, NULL);
    if (thread == NULL) {
        free(start);
        return -1;
    }
    CloseHandle(thread);
    return 0;
}

/* The thread-local variables of Windows stand in no block of the heap. */
const void *anti_rt_thread_block(void)
{
    return NULL;
}

int64_t anti_rt_processors(void)
{
    DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    return n > 0 ? (int64_t)n : 1;
}

/* Free p and keep errno, which free may change. */
static void release(void *p)
{
    int saved = errno;

    free(p);
    errno = saved;
}

/* The UTF-16 form of the UTF-8 text, with room for extra more units
   after its NUL, in memory the caller frees. NULL with errno set: EINVAL
   when it is no valid UTF-8 or is too long, and ENOMEM when memory runs
   out. */
static wchar_t *wide_of(const char *text, size_t extra)
{
    size_t length = strlen(text);
    int units;
    wchar_t *wide;

    if (length >= INT_MAX) {
        errno = EINVAL;
        return NULL;
    }
    units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
                                (int)length + 1, NULL, 0);
    if (units <= 0) {
        errno = EINVAL;
        return NULL;
    }
    wide = malloc(((size_t)units + extra) * sizeof *wide);
    if (wide == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, (int)length + 1,
                        wide, units);
    return wide;
}

/* The units UTF-16 units at wide as UTF-8 into the room bytes at out,
   ended by a NUL. A unit that pairs with no other gives U+FFFD. Gives
   the bytes before the NUL, and 0 with an empty text when they do not
   fit. */
static size_t utf8_of(const wchar_t *wide, size_t units, char *out,
                      size_t room)
{
    int bytes = 0;

    if (units > 0 && units < INT_MAX / 3 && room > 1 && room <= INT_MAX) {
        bytes = WideCharToMultiByte(CP_UTF8, 0, wide, (int)units, out,
                                    (int)room - 1, NULL, NULL);
    }
    if (room > 0) {
        out[bytes > 0 ? bytes : 0] = '\0';
    }
    return bytes > 0 ? (size_t)bytes : 0;
}

/* DESIGN: GetEnvironmentVariableW reports the length the value needs, and
   another thread may change the value between that call and the read. So
   the read repeats until the value fits the buffer it was sized for. The
   UTF-16 then becomes the UTF-8 the rest of the runtime reads, as
   anti_rt_file_open reads a path. */
int anti_rt_getenv(const char *name, char **value)
{
    wchar_t *wide_name = wide_of(name, 0);
    wchar_t *wide = NULL;
    DWORD size = 0;
    DWORD length;
    int bytes;

    *value = NULL;
    if (wide_name == NULL) {
        return -1;
    }
    for (;;) {
        length = GetEnvironmentVariableW(wide_name, wide, size);
        if (length == 0) {
            free(wide);
            free(wide_name);
            return 0;
        }
        if (length < size) {
            break;
        }
        free(wide);
        size = length;
        wide = malloc((size_t)size * sizeof *wide);
        if (wide == NULL) {
            free(wide_name);
            return -1;
        }
    }
    free(wide_name);
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide,
                                (int)length + 1, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        free(wide);
        return 0;
    }
    *value = malloc((size_t)bytes);
    if (*value == NULL) {
        free(wide);
        return -1;
    }
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)length + 1,
                        *value, bytes, NULL, NULL);
    free(wide);
    return 0;
}

int anti_rt_path_is_absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' ||
           ((path[0] | 32) >= 'a' && (path[0] | 32) <= 'z' && path[1] == ':');
}

const char *anti_rt_path_last_separator(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    return back != NULL && (slash == NULL || back > slash) ? back : slash;
}

/* The table anti_rt_imports of a plugin, which antic writes. */
struct imports {
    int64_t count;
    volatile LONG64 state;      /* IMPORTS_NONE until the places are done */
    void **places[];
};

enum { IMPORTS_NONE, IMPORTS_BUSY, IMPORTS_DONE };

/* DESIGN: a plugin reaches a name of its host through the __imp_ entry of
   the import library, which Windows fills when it loads the library. An
   address in the data of the plugin cannot be such a load. Each one holds
   the address of its __imp_ entry instead, and the loader replaces it
   with the address the entry holds. The first open of a library does it. A
   second open of the same library returns the same image, and the state
   word keeps it from replacing an address twice. A place whose page
   cannot be written fails the open, which then closes the library. */
static bool fill_imports(HMODULE handle)
{
    union {
        FARPROC from;
        struct imports *to;
    } cast;
    struct imports *t;
    bool ok = true;
    int64_t i;

    cast.from = GetProcAddress(handle, "anti_rt_imports");
    t = cast.to;
    if (t == NULL) {
        return true;
    }
    if (InterlockedCompareExchange64(&t->state, IMPORTS_BUSY, IMPORTS_NONE) !=
        IMPORTS_NONE) {
        while (InterlockedCompareExchange64(&t->state, IMPORTS_DONE,
                                            IMPORTS_DONE) != IMPORTS_DONE) {
            SwitchToThread();
        }
        return true;
    }
    for (i = 0; ok && i < t->count; i++) {
        void **place = t->places[i];
        DWORD was;
        ok = VirtualProtect(place, sizeof *place, PAGE_READWRITE, &was) != 0;
        if (ok) {
            *place = *(void **)*place;
            ok = VirtualProtect(place, sizeof *place, was, &was) != 0;
        }
    }
    InterlockedExchange64(&t->state, IMPORTS_DONE);
    return ok;
}

FILE *anti_rt_file_open(const char *path, int mode)
{
    static const wchar_t *const modes[] = {L"rb", L"wb", L"ab"};
    wchar_t *name;
    FILE *file;

    if (mode < ANTI_FILE_READ || mode > ANTI_FILE_APPEND) {
        errno = EINVAL;
        return NULL;
    }
    name = wide_of(path, 0);
    if (name == NULL) {
        return NULL;
    }
    /* _wfopen is _wfsopen without a lock on the file, which is what fopen
       gives on the other systems. The C runtime deprecates the first and
       not the second. */
    file = _wfsopen(name, modes[mode], _SH_DENYNO);
    release(name);
    return file;
}

int64_t anti_rt_file_tell(FILE *file)
{
    return _ftelli64(file);
}

int anti_rt_file_seek(FILE *file, int64_t offset, int whence)
{
    return _fseeki64(file, offset, whence) == 0 ? 0 : -1;
}

int anti_rt_file_is_directory(FILE *file)
{
    struct _stat64 info;

    return _fstat64(_fileno(file), &info) == 0 &&
                   (info.st_mode & _S_IFMT) == _S_IFDIR
               ? 1
               : 0;
}

int anti_rt_file_remove(const char *path)
{
    wchar_t *name = wide_of(path, 0);
    int status;

    if (name == NULL) {
        return -1;
    }
    status = _wremove(name);
    release(name);
    return status == 0 ? 0 : -1;
}

int anti_rt_file_rename(const char *from, const char *to)
{
    wchar_t *old_name = wide_of(from, 0);
    wchar_t *new_name = old_name != NULL ? wide_of(to, 0) : NULL;
    int status = -1;

    if (new_name != NULL) {
        status = _wrename(old_name, new_name);
    }
    release(old_name);
    release(new_name);
    return status == 0 ? 0 : -1;
}

int anti_rt_directory_list(const char *path,
                           int (*each)(void *context,
                                       const unsigned char *name,
                                       size_t length),
                           void *context)
{
    /* The pattern of the search is the directory, a separator and `*`. */
    wchar_t *pattern = wide_of(path, 2);
    struct _wfinddata64_t data;
    char name[3 * (sizeof data.name / sizeof data.name[0]) + 1];
    intptr_t search = -1;
    size_t units;
    int status = 0;
    int saved;

    if (pattern == NULL) {
        return -1;
    }
    units = wcslen(pattern);
    if (units == 0) {
        errno = ENOENT;
    } else {
        if (pattern[units - 1] != L'/' && pattern[units - 1] != L'\\') {
            pattern[units++] = L'\\';
        }
        pattern[units++] = L'*';
        pattern[units] = 0;
        search = _wfindfirst64(pattern, &data);
    }
    release(pattern);
    if (search == -1) {
        return -1;
    }
    do {
        size_t written =
            utf8_of(data.name, wcslen(data.name), name, sizeof name);
        if (each(context, (const unsigned char *)name, written) != 0) {
            status = -1;
            break;
        }
    } while (_wfindnext64(search, &data) == 0);
    /* The search ends with ENOENT when no entry is left. */
    if (status == 0 && errno != ENOENT) {
        status = -1;
    }
    saved = errno;
    _findclose(search);
    errno = saved;
    return status;
}

void *anti_rt_library_open(const char *path)
{
    wchar_t *wide = wide_of(path, 0);
    HMODULE handle;

    if (wide == NULL) {
        SetLastError(ERROR_INVALID_NAME);
        return NULL;
    }
    handle = LoadLibraryW(wide);
    free(wide);
    if (handle != NULL && !fill_imports(handle)) {
        DWORD error = GetLastError();
        FreeLibrary(handle);
        SetLastError(error);
        return NULL;
    }
    return handle;
}

void anti_rt_library_close(void *handle)
{
    FreeLibrary((HMODULE)handle);
}

void *anti_rt_library_symbol(void *handle, const char *name)
{
    union {
        FARPROC from;
        void *to;
    } cast;

    cast.from = GetProcAddress((HMODULE)handle, name);
    return cast.to;
}

const char *anti_rt_library_error(char *text, size_t size)
{
    int written =
        snprintf(text, size, "error %lu", (unsigned long)GetLastError());

    /* A text cut short still names the error, and a failure leaves an
       empty one. */
    if (written < 0 && size > 0) {
        text[0] = '\0';
    }
    return text;
}

/* The module that holds the byte at address, as the loader knows it, or
   NULL outside every module. */
static HMODULE module_holding(const void *address)
{
    HMODULE module = NULL;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)address, &module)) {
        return NULL;
    }
    return module;
}

const void *anti_rt_library_image(const void *address)
{
    return module_holding(address);
}

/* DESIGN: the x64 convention of Windows keeps no chain of frame pointers
   through C code. The walk takes the unwind data instead, which antic
   writes for every function and the C compiler for its own. The names
   come from DbgHelp, which reads the PDB that every Windows link
   writes.

   The walk unwinds one frame at a time with RtlVirtualUnwind.
   RtlCaptureStackBackTrace follows the chain of frame records on ARM64.
   A C function that builds no record, as anti_rt_trace_walk, then hides
   the frame of its caller. A frame without unwind data ends the walk, and
   so does one that does not move up the stack. */
int64_t anti_rt_trace_walk(uint64_t *into, int64_t room, int64_t skip)
{
    CONTEXT context;
    int64_t count = 0;
    int64_t depth;

    if (room > 64) {
        room = 64;
    }
    if (room <= 0 || skip < 0 || skip > 1000) {
        return 0;
    }
    RtlCaptureContext(&context);
    /* The first frame is the one of anti_rt_trace_walk, which the trace
       leaves out. */
    for (depth = 0; count < room; depth++) {
#if defined(_M_ARM64)
        DWORD64 pc = context.Pc;
        DWORD64 sp = context.Sp;
#else
        DWORD64 pc = context.Rip;
        DWORD64 sp = context.Rsp;
#endif
        DWORD64 base = 0;
        PVOID data = NULL;
        DWORD64 establisher = 0;
        PRUNTIME_FUNCTION entry;

        if (pc == 0) {
            break;
        }
        if (depth > skip) {
            into[count++] = (uint64_t)pc;
        }
        entry = RtlLookupFunctionEntry(pc, &base, NULL);
        if (entry == NULL) {
            break;
        }
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, pc, entry, &context, &data,
                         &establisher, NULL);
#if defined(_M_ARM64)
        if (context.Sp <= sp) {
            break;
        }
#else
        if (context.Rsp <= sp) {
            break;
        }
#endif
    }
    return count;
}

/* DESIGN: a static library for C has no notice, so the runtime names
   `anti_licenses` without needing it. COFF names a default that the
   linker takes when nothing defines the notice. */
const char anti_rt_no_licenses[1] = {0};
#pragma comment(linker, "/alternatename:anti_licenses=anti_rt_no_licenses")
extern const char anti_licenses[];

/* The runtime reads its own notice by the name above, and the loader
   gives the notice of every other module by its export. */
bool anti_rt_module_at(uint64_t address, struct anti_rt_module *out)
{
    HMODULE module = module_holding((const void *)(uintptr_t)address);
    wchar_t name[1024];
    DWORD n;

    out->format = ANTI_RT_IMAGE_PE;
    out->base = 0;
    out->headers = NULL;
    out->header_count = 0;
    out->notice_read = false;
    out->notice = NULL;
    out->path[0] = '\0';
    if (module == NULL) {
        return false;
    }
    out->base = (uint64_t)(uintptr_t)module;
    n = GetModuleFileNameW(module, name, 1024);
    if (n > 0 && n < 1024) {
        utf8_of(name, n, out->path, sizeof out->path);
    }
    out->notice_read = true;
    if (module ==
        module_holding((const void *)(uintptr_t)anti_rt_trace_walk)) {
        out->notice = anti_licenses;
    } else {
        out->notice = anti_rt_library_symbol(module, "anti_licenses");
    }
    return true;
}

typedef BOOL(WINAPI *sym_initialize)(HANDLE, PCSTR, BOOL);
typedef DWORD(WINAPI *sym_set_options)(DWORD);
typedef BOOL(WINAPI *sym_from_addr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL(WINAPI *sym_line)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL(WINAPI *sym_search_path)(HANDLE, PWSTR, DWORD);
typedef BOOL(WINAPI *sym_set_search_path)(HANDLE, PCWSTR);
typedef DWORD(WINAPI *sym_inline_count)(HANDLE, DWORD64);
typedef BOOL(WINAPI *sym_inline_trace)(HANDLE, DWORD64, DWORD, DWORD64,
                                       DWORD64, LPDWORD, LPDWORD);
typedef BOOL(WINAPI *sym_inline_line)(HANDLE, DWORD64, ULONG, DWORD64,
                                      PDWORD, PIMAGEHLP_LINE64);

/* DbgHelp serves one thread at a time, so ANTI_RT_LOCK_DEBUG covers
   every call and the state below. */
static bool help_tried;
static sym_from_addr help_from_addr;
static sym_line help_line;
static sym_search_path help_get_path;
static sym_set_search_path help_set_path;
static sym_inline_count help_inline_count;
static sym_inline_trace help_inline_trace;
static sym_inline_line help_inline_line;

/* The modules whose directory the search path of DbgHelp holds. The
   record in an executable names its PDB by file name alone. DbgHelp looks
   for it in the working directory of the process and not beside the
   module. */
#define SEARCHED_MAX 32
static uint64_t searched[SEARCHED_MAX];
static size_t searched_count;

static void open_help(void)
{
    HMODULE help = LoadLibraryW(L"dbghelp.dll");
    sym_initialize initialize;
    sym_set_options options;

    help_tried = true;
    if (help == NULL) {
        return;
    }
    initialize = (sym_initialize)(void (*)(void))GetProcAddress(
        help, "SymInitialize");
    options = (sym_set_options)(void (*)(void))GetProcAddress(
        help, "SymSetOptions");
    if (initialize == NULL || options == NULL) {
        return;
    }
    options(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    if (!initialize(GetCurrentProcess(), NULL, TRUE)) {
        return;
    }
    help_from_addr = (sym_from_addr)(void (*)(void))GetProcAddress(
        help, "SymFromAddr");
    help_line = (sym_line)(void (*)(void))GetProcAddress(
        help, "SymGetLineFromAddr64");
    help_get_path = (sym_search_path)(void (*)(void))GetProcAddress(
        help, "SymGetSearchPathW");
    help_set_path = (sym_set_search_path)(void (*)(void))GetProcAddress(
        help, "SymSetSearchPathW");
    help_inline_count = (sym_inline_count)(void (*)(void))GetProcAddress(
        help, "SymAddrIncludeInlineTrace");
    help_inline_trace = (sym_inline_trace)(void (*)(void))GetProcAddress(
        help, "SymQueryInlineTrace");
    help_inline_line = (sym_inline_line)(void (*)(void))GetProcAddress(
        help, "SymGetLineFromInlineContext");
}

/* DESIGN: the line of an address inside inlined code is the line of the
   innermost inlined callee, as the line table of DWARF gives it on Linux
   and macOS and as the entry on inlined frames in docs/decisions.md asks.
   CodeView keeps the lines of an inlined callee in the inline sites of
   its caller, and SymGetLineFromAddr64 reads the lines of the caller
   alone. The inline context of the address names the innermost callee,
   and SymGetLineFromInlineContext reads its line. An address of no
   inlined code, or a DbgHelp without the three functions, takes
   SymGetLineFromAddr64. */
static bool inline_line(DWORD64 address, IMAGEHLP_LINE64 *line)
{
    DWORD context = 0;
    DWORD index = 0;
    DWORD displacement = 0;

    if (help_inline_count == NULL || help_inline_trace == NULL ||
        help_inline_line == NULL ||
        help_inline_count(GetCurrentProcess(), address) == 0 ||
        !help_inline_trace(GetCurrentProcess(), address, 0, address, address,
                           &context, &index)) {
        return false;
    }
    return help_inline_line(GetCurrentProcess(), address, context, 0,
                            &displacement, line) != FALSE;
}

/* Put the directory of the module at base on the search path, before
   DbgHelp loads the symbols of that module. */
static void search_module(uint64_t base)
{
    wchar_t path[4096];
    wchar_t file[1024];
    size_t length;
    size_t used;
    size_t i;
    DWORD n;

    for (i = 0; i < searched_count; i++) {
        if (searched[i] == base) {
            return;
        }
    }
    if (searched_count == SEARCHED_MAX || help_get_path == NULL ||
        help_set_path == NULL) {
        return;
    }
    searched[searched_count++] = base;
    n = GetModuleFileNameW((HMODULE)(uintptr_t)base, file, 1024);
    if (n == 0 || n >= 1024) {
        return;
    }
    length = n;
    while (length > 0 && file[length - 1] != L'\\' &&
           file[length - 1] != L'/') {
        length--;
    }
    if (length > 0) {
        length--;
    }
    if (length == 0 ||
        !help_get_path(GetCurrentProcess(), path, (DWORD)(sizeof path /
                                                           sizeof path[0]))) {
        return;
    }
    used = wcslen(path);
    if (used + 1 + length + 1 > sizeof path / sizeof path[0]) {
        return;
    }
    if (used > 0) {
        path[used++] = L';';
    }
    memcpy(path + used, file, length * sizeof file[0]);
    path[used + length] = 0;
    help_set_path(GetCurrentProcess(), path);
}

/* A name or a file that does not fit its room is left empty, and so is
   the file of line 0. CodeView, like DWARF, writes line 0 for code of no
   source line. The LLVM back end writes such an entry for every function
   of a build without -g, since LLVM names a function in the PDB only when
   it has a location, see the entry on -g of the LLVM back end in
   docs/decisions.md. */
void anti_rt_debug_lookup(uint64_t address, uint64_t base,
                          struct anti_rt_debug_answer *out)
{
    union {
        SYMBOL_INFO info;
        char room[sizeof(SYMBOL_INFO) + ANTI_RT_SYMBOL_ROOM];
    } symbol;
    IMAGEHLP_LINE64 line;
    DWORD64 displacement = 0;
    DWORD column = 0;

    out->function[0] = '\0';
    out->file[0] = '\0';
    out->line = 0;
    anti_rt_lock_hold(ANTI_RT_LOCK_DEBUG);
    if (!help_tried) {
        open_help();
    }
    if (base != 0) {
        search_module(base);
    }
    memset(&symbol, 0, sizeof symbol);
    symbol.info.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.info.MaxNameLen = ANTI_RT_SYMBOL_ROOM - 1;
    if (help_from_addr != NULL &&
        help_from_addr(GetCurrentProcess(), (DWORD64)address, &displacement,
                       &symbol.info)) {
        size_t n = strnlen(symbol.info.Name, symbol.info.NameLen);
        if (n < sizeof out->function) {
            memcpy(out->function, symbol.info.Name, n);
            out->function[n] = '\0';
        }
    }
    memset(&line, 0, sizeof line);
    line.SizeOfStruct = sizeof line;
    if ((inline_line((DWORD64)address, &line) ||
         (help_line != NULL &&
          help_line(GetCurrentProcess(), (DWORD64)address, &column,
                    &line))) &&
        line.LineNumber != 0 && line.FileName != NULL &&
        strlen(line.FileName) < sizeof out->file) {
        memcpy(out->file, line.FileName, strlen(line.FileName) + 1);
        out->line = (int64_t)line.LineNumber;
    }
    anti_rt_lock_release(ANTI_RT_LOCK_DEBUG);
}

/* The function both handlers below call. It is set once, under
   ANTI_RT_LOCK_SIGNALS and before either handler is installed. */
static int (*deliver_signal)(int64_t sig);

/* The console control handler runs on a thread Windows makes, so it
   calls the function of the program without a pipe. */
static BOOL WINAPI on_console(DWORD event)
{
    int64_t sig = event == CTRL_BREAK_EVENT ? ANTI_SIGBREAK : ANTI_SIGINT;

    return deliver_signal(sig) ? TRUE : FALSE;
}

/* The C runtime of Windows answers raise from a table of its own. Its
   default ends the program with code 3, and the console handler above
   never sees a raised signal. The C runtime resets the handler before it
   calls it, so the handler installs itself again. */
static void __cdecl on_raise(int sig)
{
    signal(sig, on_raise);
    deliver_signal((int64_t)sig);
}

int anti_rt_signal_route(int (*deliver)(int64_t sig))
{
    if (deliver_signal != NULL) {
        return 0;
    }
    deliver_signal = deliver;
    SetConsoleCtrlHandler(on_console, TRUE);
    return 0;
}

void anti_rt_signal_catch(int64_t sig)
{
    signal((int)sig, on_raise);
}

int anti_rt_entropy(void *out, size_t count)
{
    unsigned char *at = out;

    while (count > 0) {
        unsigned int word;
        size_t n = count < sizeof word ? count : sizeof word;
        if (rand_s(&word) != 0) {
            return -1;
        }
        memcpy(at, &word, n);
        at += n;
        count -= n;
    }
    return 0;
}

/* See the DESIGN comment on the clocks in platform_posix.c. */

int64_t anti_rt_monotonic(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart / frequency.QuadPart) * 1000000000 +
           (int64_t)(now.QuadPart % frequency.QuadPart) * 1000000000 /
               (int64_t)frequency.QuadPart;
}

int64_t anti_rt_wall(void)
{
    FILETIME file;
    ULARGE_INTEGER value;

    GetSystemTimeAsFileTime(&file);
    value.LowPart = file.dwLowDateTime;
    value.HighPart = file.dwHighDateTime;
    /* FILETIME counts 100-nanosecond units from 1601, and the epoch of
       the language is 1970. */
    return (int64_t)(value.QuadPart - 116444736000000000ULL) * 100;
}

/* A wait of any length is a loop of Sleep steps, which platform.h sizes
   below the INFINITE of Sleep. */
void anti_rt_sleep(int64_t nanoseconds)
{
    while (nanoseconds > 0) {
        uint32_t step = anti_rt_sleep_step(nanoseconds);

        Sleep((DWORD)step);
        nanoseconds -= (int64_t)step * 1000000;
    }
}

/* The n UTF-16 units at units as UTF-8 ended by a NUL, in a block of its
   own, or NULL when memory runs out. One unit gives at most 3 bytes. */
static char *utf8_block(const uint16_t *units, size_t n)
{
    unsigned char *bytes = malloc(3 * n + 1);
    size_t written;

    if (bytes == NULL) {
        return NULL;
    }
    written = anti_rt_utf16_to_utf8(units, n, bytes);
    bytes[written] = 0;
    return (char *)bytes;
}

/* The n units at s and the 0 unit after them as uint16_t, which the
   wchar_t of Windows holds but may not be read as, in memory the caller
   frees, or NULL. */
static uint16_t *units_of(const wchar_t *s, size_t n)
{
    uint16_t *units = malloc((n + 1) * sizeof *units);
    size_t i;

    if (units == NULL) {
        return NULL;
    }
    for (i = 0; i <= n; i++) {
        units[i] = (uint16_t)s[i];
    }
    return units;
}

/* The count strings at p, each ended by a 0 unit, as a list of UTF-8
   blocks. NULL when memory runs out, with every block of the call given
   back. */
static char **utf8_list(const uint16_t *p, size_t count)
{
    char **out = malloc((count + 1) * sizeof *out);
    size_t i;

    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        size_t n = 0;
        while (p[n] != 0) {
            n++;
        }
        out[i] = utf8_block(p, n);
        if (out[i] == NULL) {
            while (i > 0) {
                free(out[--i]);
            }
            free(out);
            return NULL;
        }
        p += n + 1;
    }
    out[count] = NULL;
    return out;
}

char **anti_rt_process_arguments(int argc, char **argv, size_t *count)
{
    const wchar_t *line = GetCommandLineW();
    size_t n = wcslen(line);
    uint16_t *units = units_of(line, n);
    uint16_t *split = malloc((2 * n + 2) * sizeof *split);
    char **list = NULL;

    /* The argv of the C runtime is in the ANSI code page, which platform.h
       says is not read, and the count is the one of the same split. */
    (void)argc;
    (void)argv;
    *count = 0;
    if (units != NULL && split != NULL) {
        *count = anti_rt_split_command_line(units, split);
        list = utf8_list(split, *count);
    }
    free(units);
    free(split);
    return list;
}

char **anti_rt_process_environment(size_t *count)
{
    wchar_t *block = GetEnvironmentStringsW();
    const wchar_t *p = block;
    uint16_t *units;
    char **list = NULL;
    size_t n = 0;

    *count = 0;
    if (block == NULL) {
        return utf8_list(NULL, 0);
    }
    while (*p != 0) {
        n++;
        p += wcslen(p) + 1;
    }
    units = units_of(block, (size_t)(p - block));
    if (units != NULL) {
        *count = n;
        list = utf8_list(units, n);
        free(units);
    }
    FreeEnvironmentStringsW(block);
    return list;
}

/* DESIGN: the raw-bytes rule. An Anti program writes the bytes it was
   given, and the runtime never translates them. */
void anti_rt_streams_binary(void)
{
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
}

/* DESIGN: the Universal C Runtime has no aligned_alloc, so the memory at
   an alignment comes from _aligned_malloc, whose memory only
   _aligned_free releases. */
void *anti_rt_aligned_alloc(size_t size, size_t align)
{
    return _aligned_malloc(size, align);
}

void anti_rt_aligned_free(void *p)
{
    _aligned_free(p);
}

/* See rt.h. Windows has no leak check in its runtime of
   AddressSanitizer, so no program replaces this definition, and it is
   not weak. */
void anti_rt_memory_kept(const void *p)
{
    /* The block serves the leak checker of a program that replaces this
       definition on the other systems alone. */
    (void)p;
}

#if defined(ANTI_RT_X86_64)

/* DESIGN: the two instructions as inline assembly, the same as in
   platform_posix.c, since the headers of mingw-w64 declare the intrinsics
   of Microsoft's compiler otherwise than clang does. xgetbv is written by
   its bytes, so that the assembler needs no xsave option. */
void anti_rt_cpuid(uint32_t leaf, uint32_t sub, uint32_t out[4])
{
    __asm__ volatile("cpuid"
                     : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                     : "a"(leaf), "c"(sub));
}

uint64_t anti_rt_xcr0(void)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile(".byte 0x0f, 0x01, 0xd0"
                     : "=a"(low), "=d"(high)
                     : "c"(0));
    return (uint64_t)high << 32 | low;
}

#elif defined(ANTI_RT_ARM64)

/* The values of IsProcessorFeaturePresent, which an older SDK may not
   declare. */
#ifndef PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE 34
#endif
#ifndef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE 43
#endif

int32_t anti_rt_arm64_level(void)
{
    /* DESIGN: Windows answers for the ARMv8.1 atomics and the ARMv8.2 dot
       products and has no query above them. Where a feature has no flag
       the level is assumed, because every Windows-on-ARM machine sold is
       armv8.2 or later. A machine with both flags therefore reports
       armv8.5 as well. */
    if (IsProcessorFeaturePresent(
            PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE) &&
        IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) {
        return ANTI_CPU_ARMV8_5;
    }
    return ANTI_CPU_ARMV8_0;
}

#endif

/* The static part of the C runtime, for the runtime archive alone.

   DESIGN: the archive compiles this file for the gnu triple against the
   headers of mingw-w64, which define __MINGW32__. The host build of a
   Windows machine compiles it for its unit tests as well, against the
   headers of Microsoft and with Microsoft's C runtime, which gives this
   part itself and holds no dirent.h, so the part stays out there.

   DESIGN: a Windows program links against ucrtbase.dll, the C library of
   every Windows since 10, through the import library that
   tools/get-sysroot.cmake writes from the .def file of mingw-w64, and
   against no library of Microsoft, which Eddie decided on 2026-09-27
   under "Binary distribution" in docs/decisions.md. The DLL holds the C
   library alone. What the static libraries of Microsoft gave beside it
   stands here: the start of a program, whose entry point stands in
   platform_entry.c, the entry point of a DLL, the tables of the
   initialisers, the directory of thread-local storage, the stack probe,
   the check of the stack cookie and the marks the compiler names. atexit
   and the printf family, which the headers of the UCRT declare and the
   DLL does not export, stand in platform_stdio.c. The names are the ones
   the compiler, the linker and the loader look for, so rule 25 of
   docs/c-guidelines.md stops at them, as the entry on the runtime of a
   Windows target under "Binary distribution" says. The C of the runtime
   is compiled for the gnu triple against the headers of mingw-w64, so it
   compiles as C of mingw-w64 would, and the program is compiled for the
   msvc triple. */

#if defined(__MINGW32__)

#include <dirent.h>

#define ANTI_RT_SECTION(name) __attribute__((section(name), used))

/* The tables of the initialisers: the C initialisers in .CRT$XI*, each
   of which gives 0 or an error, and the constructors in .CRT$XC*, where
   the compiler puts the constructor of a module. The linker sorts the
   sections of a group by name, so the two marks of each table bound what
   the objects of the program put between them. */
ANTI_RT_SECTION(".CRT$XIA") static const _PIFV c_initialisers_start[1] = {0};
ANTI_RT_SECTION(".CRT$XIZ") static const _PIFV c_initialisers_end[1] = {0};
ANTI_RT_SECTION(".CRT$XCA") static const _PVFV constructors_start[1] = {0};
ANTI_RT_SECTION(".CRT$XCZ") static const _PVFV constructors_end[1] = {0};

/* Run the two tables, before main and when a process attaches a DLL. */
static bool run_initialisers(void)
{
    if (_initterm_e((_PIFV *)c_initialisers_start,
                    (_PIFV *)c_initialisers_end) != 0) {
        return false;
    }
    _initterm((_PVFV *)constructors_start, (_PVFV *)constructors_end);
    return true;
}

/* The directory of thread-local storage, which the linker puts into the
   header of the image and the loader reads for every thread. The raw
   data is the .tls section between the two marks, the callbacks are the
   .CRT$XL* group after its first mark, which holds none, and the index
   is the word the loader writes and the code of every thread-local
   variable reads. The struct is IMAGE_TLS_DIRECTORY with pointers for
   its addresses, so that its initialiser is addresses alone. */
struct tls_directory {
    const void *raw_data_start;
    const void *raw_data_end;
    unsigned long *index;
    const void *callbacks;
    uint32_t zero_fill_size;
    uint32_t characteristics;
};

ANTI_RT_SECTION(".tls") static char *tls_start = NULL;
ANTI_RT_SECTION(".tls$ZZZ") static char *tls_end = NULL;
ANTI_RT_SECTION(".CRT$XLA") static const PIMAGE_TLS_CALLBACK tls_callbacks_start = NULL;
ANTI_RT_SECTION(".CRT$XLZ") static const PIMAGE_TLS_CALLBACK tls_callbacks_end = NULL;
/* The loader writes the index of the module once, before any thread
   reads it. The index, the directory, the mark of floating point and the
   cookie below are named by the code the back end writes and by the
   linker, never by the IR, so each is marked used: the LTO of a release
   build would otherwise drop them before the back end asks for them. */
__attribute__((used)) unsigned long _tls_index = 0;
__attribute__((used)) const struct tls_directory _tls_used = {
    &tls_start, &tls_end, &_tls_index, &tls_callbacks_start + 1, 0, 0
};

/* The mark of a module that uses floating point, which the code of
   x86_64 names and the C runtime of Microsoft defined. */
__attribute__((used)) const int _fltused = 0x9875;

/* The cookie of the stack check of Microsoft's compiler, which the
   profile runtime of compiler-rt and a C object of a user built with
   the check carry. It starts as the default value of Microsoft's C
   runtime and takes a random value at the start of a program or a DLL,
   as that runtime gives it one, with the top 16 bits clear so that a
   text that overflows a buffer spells it with difficulty. The check
   keeps every register, as its callers hold their result across it, so
   it is assembly, as the stack probe is. The probe touches every page of
   a frame above a page from the top down, so that the guard page of the
   stack moves. The code of the msvc triple calls __chkstk with the size
   in rax on x86_64 and in x15, in units of 16 bytes, on ARM64. The C of
   the gnu triple calls ___chkstk_ms on x86_64 with the same contract. */
#define DEFAULT_SECURITY_COOKIE 0x2B992DDFA232u
__attribute__((used)) uintptr_t __security_cookie = DEFAULT_SECURITY_COOKIE;

/* A random cookie from the C runtime, or the default when it gives none. */
static void set_security_cookie(void)
{
    unsigned int low;
    unsigned int high;
    uintptr_t value;

    if (rand_s(&low) != 0 || rand_s(&high) != 0) {
        return;
    }
    value = ((uintptr_t)high << 32 | low) & 0x0000FFFFFFFFFFFFu;
    if (value != 0 && value != DEFAULT_SECURITY_COOKIE) {
        __security_cookie = value;
    }
}

#if defined(ANTI_RT_X86_64)
__asm__(".text\n"
        ".globl __chkstk\n"
        ".globl ___chkstk_ms\n"
        ".globl __security_check_cookie\n"
        ".def __chkstk; .scl 2; .type 32; .endef\n"
        ".def ___chkstk_ms; .scl 2; .type 32; .endef\n"
        ".def __security_check_cookie; .scl 2; .type 32; .endef\n"
        ".p2align 4\n"
        "__chkstk:\n"
        "___chkstk_ms:\n"
        "    pushq %rcx\n"
        "    pushq %rax\n"
        "    cmpq $0x1000, %rax\n"
        "    leaq 24(%rsp), %rcx\n"
        "    jb 1f\n"
        "2:\n"
        "    subq $0x1000, %rcx\n"
        "    testq %rcx, (%rcx)\n"
        "    subq $0x1000, %rax\n"
        "    cmpq $0x1000, %rax\n"
        "    ja 2b\n"
        "1:\n"
        "    subq %rax, %rcx\n"
        "    testq %rcx, (%rcx)\n"
        "    popq %rax\n"
        "    popq %rcx\n"
        "    retq\n"
        ".p2align 4\n"
        "__security_check_cookie:\n"
        "    cmpq __security_cookie(%rip), %rcx\n"
        "    jne 3f\n"
        "    retq\n"
        "3:\n"
        "    ud2\n");
#elif defined(ANTI_RT_ARM64)
__asm__(".text\n"
        ".globl __chkstk\n"
        ".globl __security_check_cookie\n"
        ".def __chkstk; .scl 2; .type 32; .endef\n"
        ".def __security_check_cookie; .scl 2; .type 32; .endef\n"
        ".p2align 4\n"
        "__chkstk:\n"
        "    lsl x16, x15, #4\n"
        "    mov x17, sp\n"
        "1:\n"
        "    sub x17, x17, #4096\n"
        "    subs x16, x16, #4096\n"
        "    ldr xzr, [x17]\n"
        "    b.gt 1b\n"
        "    ret\n"
        ".p2align 4\n"
        "__security_check_cookie:\n"
        "    adrp x16, __security_cookie\n"
        "    ldr x16, [x16, :lo12:__security_cookie]\n"
        "    cmp x0, x16\n"
        "    b.ne 2f\n"
        "    ret\n"
        "2:\n"
        "    brk #0xf003\n");
#endif

/* The compiler of the gnu triple calls __main at the start of main,
   where the constructors of a program run under GCC. They have run
   before main here, so it does nothing. */
void __main(void);
void __main(void)
{
}

/* See platform.h. The C runtime starts itself when the loader maps its
   DLL. The program sets its type, which the message of an assertion
   reads, its arguments and its environment, which the UCRT keeps per
   module, runs its initialisers and leaves through exit, which runs the
   atexit functions and flushes the streams. */
_Noreturn void anti_rt_windows_start(int (*program)(int, char **))
{
    set_security_cookie();
    _set_app_type(_crt_console_app);
    if (_initialize_narrow_environment() != 0 ||
        _configure_narrow_argv(_crt_argv_unexpanded_arguments) != 0 ||
        !run_initialisers()) {
        abort();
    }
    exit(program(*__p___argc(), *__p___argv()));
}

/* The entry point of a DLL, under the name lld-link looks for in its
   mingw mode. Its initialisers run when a process attaches it, and
   nothing happens on the other events. */
BOOL WINAPI DllMainCRTStartup(HINSTANCE instance, DWORD reason,
                              LPVOID reserved);
BOOL WINAPI DllMainCRTStartup(HINSTANCE instance, DWORD reason,
                              LPVOID reserved)
{
    /* The loader passes the module and, on a detach, whether the process
       ends, which the start has no use for. */
    (void)instance;
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    set_security_cookie();
    return run_initialisers();
}

/* DESIGN: the headers of mingw-w64 declare a few functions that its own
   library defines and ucrtbase.dll does not export, and the native
   libraries and the C of the tests reach fourteen of them: assert of
   raylib and miniaudio ends in _assert, raylib calls hypotf, raylib walks
   a directory with opendir, readdir and closedir, and the macros of
   math.h that classify a number, fpclassify, isnan and signbit among
   them, call the nine functions below where the compiler does not inline
   the bodies the header offers. They stand here, with the meaning the
   library of mingw-w64 gives them, so that no library of mingw-w64 is
   built. A program of C against the headers of the package that reaches
   another function of that library gets an undefined symbol from the
   link. */

int __fpclassify(double x)
{
    return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL,
                                FP_ZERO, x);
}

int __fpclassifyf(float x)
{
    return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL,
                                FP_ZERO, x);
}

int __fpclassifyl(long double x)
{
    return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL,
                                FP_ZERO, x);
}

int __isnan(double x)
{
    return __builtin_isnan(x);
}

int __isnanf(float x)
{
    return __builtin_isnan(x);
}

int __isnanl(long double x)
{
    return __builtin_isnan(x);
}

int __signbit(double x)
{
    return __builtin_signbit(x) != 0;
}

int __signbitf(float x)
{
    return __builtin_signbit(x) != 0;
}

int __signbitl(long double x)
{
    return __builtin_signbit(x) != 0;
}

void _assert(const char *message, const char *file, unsigned line)
{
    fprintf(stderr, "Assertion failed: %s, file %s, line %u\n", message,
            file, line);
    abort();
}

float hypotf(float x, float y)
{
    return (float)hypot((double)x, (double)y);
}

/* A directory that opendir opened: the entry readdir gives, the handle
   of the find functions, the count of the entries read, -1 past the end,
   and the pattern path\* in UTF-16. The DIR of dirent.h is the type of
   the library of mingw-w64, which no caller looks into, so its pointer
   stands for this struct. The names go through UTF-16 and back, as every
   text of the runtime does, where the library of mingw-w64 takes the
   code page of the system. */
struct open_directory {
    struct dirent entry;
    intptr_t handle;
    int state;
    wchar_t *pattern;
};

DIR *opendir(const char *path)
{
    struct open_directory *directory = malloc(sizeof *directory);
    wchar_t *wide;
    size_t units;
    DWORD attributes;

    if (directory == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    wide = wide_of(path, 2);
    if (wide == NULL) {
        free(directory);
        return NULL;
    }
    units = wcslen(wide);
    attributes = units == 0 ? INVALID_FILE_ATTRIBUTES
                            : GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        errno = attributes == INVALID_FILE_ATTRIBUTES ? ENOENT : ENOTDIR;
        free(wide);
        free(directory);
        return NULL;
    }
    if (wide[units - 1] != L'/' && wide[units - 1] != L'\\' &&
        wide[units - 1] != L':') {
        wide[units++] = L'\\';
    }
    wide[units] = L'*';
    wide[units + 1] = L'\0';
    memset(&directory->entry, 0, sizeof directory->entry);
    directory->handle = -1;
    directory->state = 0;
    directory->pattern = wide;
    return (DIR *)directory;
}

struct dirent *readdir(DIR *opened)
{
    struct open_directory *directory = (struct open_directory *)opened;
    struct _wfinddata64_t found;
    size_t bytes;

    if (directory->state < 0) {
        return NULL;
    }
    if (directory->state == 0) {
        directory->handle = _wfindfirst64(directory->pattern, &found);
        if (directory->handle == -1) {
            directory->state = -1;
            return NULL;
        }
    } else if (_wfindnext64(directory->handle, &found) != 0) {
        /* The end of the directory is no error of the walk. */
        if (GetLastError() == ERROR_NO_MORE_FILES) {
            errno = 0;
        }
        _findclose(directory->handle);
        directory->handle = -1;
        directory->state = -1;
        return NULL;
    }
    directory->state++;
    bytes = utf8_of(found.name, wcslen(found.name), directory->entry.d_name,
                    sizeof directory->entry.d_name);
    if (bytes == 0 && found.name[0] != L'\0') {
        /* A name the entry of dirent.h cannot hold ends the walk, since an
           empty name would stand for a file that is not there. */
        errno = ENAMETOOLONG;
        _findclose(directory->handle);
        directory->handle = -1;
        directory->state = -1;
        return NULL;
    }
    directory->entry.d_namlen = (unsigned short)bytes;
    return &directory->entry;
}

int closedir(DIR *opened)
{
    struct open_directory *directory = (struct open_directory *)opened;
    int status = 0;

    if (directory->handle != -1) {
        status = _findclose(directory->handle);
    }
    free(directory->pattern);
    free(directory);
    return status;
}

#endif

#else

/* ISO C wants a declaration in every file, and elsewhere this one holds
   no other. */
typedef int anti_rt_platform_windows_unused;

#endif
