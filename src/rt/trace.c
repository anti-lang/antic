/* The capture, the modules and the symbols of a stack trace.

   DESIGN: a trace is taken in two steps. The capture walks the frames.
   Per frame it records the module that holds the address, the build id
   of that module and its load base. It reads nothing from a file.
   Symbolising reads the symbol table and the line table. It runs only
   when a program asks for the names, so an error that is handled costs
   the walk alone. */
/* pthread_getattr_np, dl_iterate_phdr and readlink sit behind feature
   macros, which the two systems spell differently. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include "trace.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rt.h"
#include "symbols.h"

#if defined(_WIN32)
#include <windows.h>
#include <DbgHelp.h>
#include "utf.h"
#else
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#if !defined(__APPLE__)
#include <link.h>
#endif
#endif

static struct anti_text text_of(const char *s)
{
    struct anti_text t;

    t.ptr = (const unsigned char *)(s != NULL ? s : "");
    t.len = s != NULL ? (int64_t)strlen(s) : 0;
    return t;
}

static struct anti_text text_part(const char *s, size_t n)
{
    struct anti_text t;

    t.ptr = (const unsigned char *)(s != NULL ? s : "");
    t.len = s != NULL ? (int64_t)n : 0;
    return t;
}

/* DESIGN: a static library for C has no notice, so the runtime names
   `anti_licenses` without needing it. ELF takes a weak reference, which
   is 0 without a definition. COFF names a default that the linker takes
   when nothing defines the notice. Mach-O reads the symbol from the table
   of the image at run time, as it does for every other image. */
#if defined(_WIN32)
const char anti_rt_no_licenses[1] = {0};
#pragma comment(linker, "/alternatename:anti_licenses=anti_rt_no_licenses")
extern const char anti_licenses[];
#elif !defined(__APPLE__)
extern const char anti_licenses[] __attribute__((weak));
#endif

/* The build id of a notice, the digits of its line `build <id>` after
   the begin marker, or an empty text. No byte at or past room is read,
   and a notice of this program or of a table in memory passes
   SIZE_MAX. */
static struct anti_text notice_id(const char *notice, size_t room)
{
    static const char head[] = "ANTI_LICENSES_BEGIN\nbuild ";
    size_t at = sizeof head - 1;

    if (notice == NULL || room < at || strncmp(notice, head, at) != 0) {
        return text_of(NULL);
    }
    while (at < room && notice[at] != '\n' && notice[at] != 0) {
        at++;
    }
    if (at == room || notice[at] != '\n') {
        return text_of(NULL);
    }
    return text_part(notice + sizeof head - 1, at - (sizeof head - 1));
}

#if !defined(_WIN32)
/* DESIGN: the files a symbol lookup reads stay in memory for the life of
   the program. They are the binaries of the modules on Linux and the
   objects of the debug map on macOS, a few of each. A second trace would
   read them again for nothing. */
struct loaded {
    char *path;
    uint8_t *bytes;
    size_t size;
};

enum { LOADED_MAX = 32 };

static struct loaded files[LOADED_MAX];
static size_t file_count;
static pthread_mutex_t files_lock = PTHREAD_MUTEX_INITIALIZER;

/* The bytes of the file at path, read whole, or NULL. The relocations of
   a Mach-O object are resolved once, as it is read. */
static const struct loaded *load(const char *path, bool object)
{
    const struct loaded *found = NULL;
    size_t i;

    pthread_mutex_lock(&files_lock);
    for (i = 0; i < file_count; i++) {
        if (strcmp(files[i].path, path) == 0) {
            found = files[i].bytes != NULL ? &files[i] : NULL;
            pthread_mutex_unlock(&files_lock);
            return found;
        }
    }
    if (file_count < LOADED_MAX) {
        struct loaded *l = &files[file_count];
        FILE *f = fopen(path, "rb");
        size_t room = 0;
        l->path = malloc(strlen(path) + 1);
        if (l->path != NULL) {
            memcpy(l->path, path, strlen(path) + 1);
            file_count++;
        }
        while (f != NULL && l->path != NULL) {
            size_t n;
            if (l->size == room) {
                uint8_t *more = realloc(l->bytes, room == 0 ? 65536 : 2 * room);
                if (more == NULL) {
                    free(l->bytes);
                    l->bytes = NULL;
                    break;
                }
                l->bytes = more;
                room = room == 0 ? 65536 : 2 * room;
            }
            n = fread(l->bytes + l->size, 1, room - l->size, f);
            if (n == 0) {
                break;
            }
            l->size += n;
        }
        /* A read error leaves a short image, which the readers would take
           for the whole file. The entry then holds no bytes, as for a
           file that does not open. */
        if (f != NULL && ferror(f) != 0) {
            free(l->bytes);
            l->bytes = NULL;
            l->size = 0;
        }
        if (f != NULL) {
            fclose(f);
        }
        if (l->bytes != NULL && object) {
            anti_macho_relocate(l->bytes, l->size);
        }
        found = l->path != NULL && l->bytes != NULL ? l : NULL;
    }
    pthread_mutex_unlock(&files_lock);
    return found;
}

/* The lowest and the highest address of the stack of this thread. */
static void stack_bounds(uintptr_t *low, uintptr_t *high)
{
#if defined(__APPLE__)
    pthread_t self = pthread_self();
    *high = (uintptr_t)pthread_get_stackaddr_np(self);
    *low = *high - pthread_get_stacksize_np(self);
#else
    pthread_attr_t attr;
    void *start;
    size_t size;

    *low = 0;
    *high = UINTPTR_MAX;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        if (pthread_attr_getstack(&attr, &start, &size) == 0) {
            *low = (uintptr_t)start;
            *high = (uintptr_t)start + size;
        }
        pthread_attr_destroy(&attr);
    }
#endif
}

/* DESIGN: the walk follows the chain of frame records, which antic keeps
   in every function on both architectures. A record holds the frame
   pointer of the caller and the return address. The walk stops at a
   record outside the stack of the thread and at one that does not move
   up the stack. It stops at a return address of 0 as well. A C frame
   without a record then ends the trace rather than the program. */
int64_t anti_rt_trace_walk(uint64_t *into, int64_t room, int64_t skip)
{
    void **fp = __builtin_frame_address(0);
    uintptr_t low;
    uintptr_t high;
    int64_t n = 0;

    stack_bounds(&low, &high);
    while (fp != NULL && n < room) {
        uintptr_t at = (uintptr_t)fp;
        void **next;
        uintptr_t back;
        if (at < low || at > high - 2 * sizeof(void *) ||
            at % sizeof(void *) != 0) {
            break;
        }
        next = fp[0];
        back = (uintptr_t)fp[1];
        if (back == 0) {
            break;
        }
        if (skip > 0) {
            skip--;
        } else {
            into[n++] = (uint64_t)back;
        }
        if ((uintptr_t)next <= at) {
            break;
        }
        fp = next;
    }
    return n;
}
#endif

#if defined(__APPLE__)
/* The bytes of a 32-bit field of a header, little-endian. */
static uint32_t field32(const uint8_t *at)
{
    return (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 |
           (uint32_t)at[3] << 24;
}

/* The distance between where the image lies and where it was linked:
   its header against the address of its __TEXT segment. The walk keeps
   every command inside the size the header gives them, as the reader of
   src/rt/symbols.c does, and gives 0 where one leaves it. */
static intptr_t image_slide(const uint8_t *header)
{
    uint32_t count = field32(header + 16);
    uint64_t end = 32 + (uint64_t)field32(header + 20);
    uint64_t at = 32;
    uint32_t i;

    for (i = 0; i < count && at + 8 <= end; i++) {
        uint32_t kind = field32(header + at);
        uint32_t size = field32(header + at + 4);
        uint64_t vmaddr;
        if (size < 8 || at + size > end) {
            return 0;
        }
        if (kind == 0x19 && size >= 72 &&
            strncmp((const char *)header + at + 8, "__TEXT", 16) == 0) {
            memcpy(&vmaddr, header + at + 24, 8);
            return (intptr_t)((uintptr_t)header - (uintptr_t)vmaddr);
        }
        at += size;
    }
    return 0;
}

/* The build ids the capture found, by the header of each image, so the
   symbol table of an image is read once. */
struct known_id {
    const uint8_t *header;
    struct anti_text id;
};

enum { KNOWN_MAX = 32 };

static struct known_id known[KNOWN_MAX];
static size_t known_count;

/* The build id of the image at header, read by the symbol
   `anti_licenses`, which only an Anti binary has. An image of the system
   cache has none, and its table is not read. */
static struct anti_text image_id(const uint8_t *header)
{
    struct anti_macho_table t;
    struct anti_text id = text_of(NULL);
    uint32_t flags;
    uint64_t vaddr;
    intptr_t slide;
    size_t i;

    pthread_mutex_lock(&files_lock);
    for (i = 0; i < known_count; i++) {
        if (known[i].header == header) {
            id = known[i].id;
            pthread_mutex_unlock(&files_lock);
            return id;
        }
    }
    memcpy(&flags, header + 24, 4);
    slide = image_slide(header);
    if ((flags & 0x80000000u) == 0 &&
        anti_macho_table(header, SIZE_MAX, true, slide, &t) &&
        anti_macho_symbol(&t, "anti_licenses", &vaddr)) {
        id = notice_id((const char *)(uintptr_t)(vaddr + (uint64_t)slide),
                       SIZE_MAX);
    }
    if (known_count < KNOWN_MAX) {
        known[known_count].header = header;
        known[known_count].id = id;
        known_count++;
    }
    pthread_mutex_unlock(&files_lock);
    return id;
}

void anti_rt_trace_frame(uint64_t address, struct anti_raw_frame *out)
{
    Dl_info info;

    memset(out, 0, sizeof *out);
    out->address = address;
    out->module = text_of(NULL);
    out->build_id = text_of(NULL);
    if (address == 0 || dladdr((void *)(uintptr_t)(address - 1), &info) == 0 ||
        info.dli_fbase == NULL) {
        return;
    }
    out->module = text_of(info.dli_fname);
    out->base = (uint64_t)(uintptr_t)info.dli_fbase;
    out->build_id = image_id(info.dli_fbase);
}

void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out)
{
    struct anti_macho_table t;
    struct anti_found found;
    const uint8_t *header = (const uint8_t *)(uintptr_t)frame->base;
    const char *object;
    const char *symbol;
    uint64_t start;
    uint64_t vaddr;
    intptr_t slide;

    memset(out, 0, sizeof *out);
    memset(&found, 0, sizeof found);
    out->address = frame->address;
    out->function = text_of(NULL);
    out->file = text_of(NULL);
    if (header == NULL || frame->address == 0) {
        return;
    }
    slide = image_slide(header);
    if (!anti_macho_table(header, SIZE_MAX, true, slide, &t)) {
        return;
    }
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    vaddr = frame->address - 1 - (uint64_t)slide;
    if (anti_macho_function(&t, vaddr, &found)) {
        out->function = text_part(found.function, found.function_length);
    }
    if (anti_macho_debug_map(&t, vaddr, &object, &symbol, &start)) {
        const struct loaded *l = load(object, true);
        if (l != NULL && anti_macho_object_line(l->bytes, l->size, symbol,
                                                vaddr - start, &found) &&
            found.file != NULL) {
            out->file = text_part(found.file, found.file_length);
            out->line = found.line;
        }
    }
}
#elif !defined(_WIN32)
/* The path of the program, which the list of modules gives no name. */
static char program_path[4096];
static pthread_once_t program_once = PTHREAD_ONCE_INIT;

static void read_program_path(void)
{
    ssize_t n = readlink("/proc/self/exe", program_path,
                         sizeof program_path - 1);

    program_path[n > 0 ? n : 0] = 0;
}

/* anti_elf_loaded_room reads the program headers as the 56 bytes of
   the 64-bit form, which every Linux target has. */
_Static_assert(sizeof(ElfW(Phdr)) == 56, "a program header is 56 bytes");

/* The module an address lies in, as the list of modules gives it. */
struct module_of {
    uintptr_t address;
    const char *name;
    uintptr_t bias;
    const ElfW(Phdr) *headers;      /* read by anti_elf_loaded_room */
    size_t header_count;
    size_t visited;
    bool found;
    bool program;
};

static int visit_module(struct dl_phdr_info *info, size_t size, void *context)
{
    struct module_of *m = context;
    int i;

    (void)size;
    m->visited++;
    for (i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        uintptr_t from = info->dlpi_addr + p->p_vaddr;
        if (p->p_type == PT_LOAD && m->address >= from &&
            m->address - from < p->p_memsz) {
            m->name = info->dlpi_name;
            m->bias = info->dlpi_addr;
            m->headers = info->dlpi_phdr;
            m->header_count = info->dlpi_phnum;
            m->found = true;
            m->program = m->visited == 1;
            return 1;
        }
    }
    return 0;
}

/* The module of address with its path, where the program itself has the
   path of its executable. dl_iterate_phdr visits the program first. glibc
   names it with an empty text and musl `/proc/self/exe` in a static
   program, so its path is read from that link. */
static bool module_at(uintptr_t address, struct module_of *m)
{
    memset(m, 0, sizeof *m);
    m->address = address;
    dl_iterate_phdr(visit_module, m);
    if (m->found && m->program) {
        pthread_once(&program_once, read_program_path);
        m->name = program_path;
    }
    return m->found;
}

/* The build id of the module. The one of this runtime reads its own
   notice. Another one is read by the symbol `anti_licenses` of its file,
   which only an Anti binary has. The file on disk may no longer be the
   one mapped, so the notice is read only inside a loaded segment of the
   module. */
static struct anti_text module_id(const struct module_of *m)
{
    struct module_of self;
    const struct loaded *l;
    uint64_t vaddr;
    uint64_t room;

    if (module_at((uintptr_t)anti_rt_trace_walk, &self) &&
        self.bias == m->bias) {
        return notice_id(anti_licenses, SIZE_MAX);
    }
    l = load(m->name, false);
    if (l == NULL || !anti_elf_symbol(l->bytes, l->size, "anti_licenses",
                                      &vaddr)) {
        return text_of(NULL);
    }
    room = anti_elf_loaded_room((const uint8_t *)m->headers,
                                m->header_count, vaddr);
    if (room == 0) {
        return text_of(NULL);
    }
    return notice_id((const char *)(m->bias + (uintptr_t)vaddr),
                     room < SIZE_MAX ? (size_t)room : SIZE_MAX);
}

void anti_rt_trace_frame(uint64_t address, struct anti_raw_frame *out)
{
    struct module_of m;

    memset(out, 0, sizeof *out);
    out->address = address;
    out->module = text_of(NULL);
    out->build_id = text_of(NULL);
    if (address == 0 || !module_at((uintptr_t)address - 1, &m)) {
        return;
    }
    out->module = text_of(m.name);
    out->base = (uint64_t)m.bias;
    out->build_id = module_id(&m);
}

void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out)
{
    struct anti_found found;
    const struct loaded *l;
    char path[4096];
    uint64_t vaddr;

    memset(out, 0, sizeof *out);
    memset(&found, 0, sizeof found);
    out->address = frame->address;
    out->function = text_of(NULL);
    out->file = text_of(NULL);
    if (frame->address == 0 || frame->module.len <= 0 ||
        (size_t)frame->module.len >= sizeof path) {
        return;
    }
    memcpy(path, frame->module.ptr, (size_t)frame->module.len);
    path[frame->module.len] = 0;
    l = load(path, false);
    if (l == NULL) {
        return;
    }
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    vaddr = frame->address - 1 - frame->base;
    if (anti_elf_function(l->bytes, l->size, vaddr, &found)) {
        out->function = text_part(found.function, found.function_length);
    }
    if (anti_elf_line(l->bytes, l->size, vaddr, &found) &&
        found.file != NULL) {
        out->file = text_part(found.file, found.file_length);
        out->line = found.line;
    }
}
#else
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

/* The texts a Windows lookup makes, kept for the life of the program so
   a frame may point at them. Equal texts are kept once. */
struct kept {
    struct kept *next;
    size_t length;
    char bytes[1];
};

static struct kept *kept_texts;
static SRWLOCK kept_lock = SRWLOCK_INIT;

static struct anti_text keep(const char *s, size_t n)
{
    struct kept *k;

    AcquireSRWLockExclusive(&kept_lock);
    for (k = kept_texts; k != NULL; k = k->next) {
        if (k->length == n && memcmp(k->bytes, s, n) == 0) {
            break;
        }
    }
    if (k == NULL) {
        k = malloc(sizeof *k + n);
        if (k != NULL) {
            memcpy(k->bytes, s, n);
            k->bytes[n] = 0;
            k->length = n;
            k->next = kept_texts;
            kept_texts = k;
        }
    }
    ReleaseSRWLockExclusive(&kept_lock);
    return k != NULL ? text_part(k->bytes, k->length) : text_of(NULL);
}

void anti_rt_trace_frame(uint64_t address, struct anti_raw_frame *out)
{
    HMODULE module = NULL;
    HMODULE self = NULL;
    wchar_t name[1024];
    uint16_t units[1024];
    unsigned char bytes[3 * 1024 + 1];
    DWORD n;
    DWORD i;
    FARPROC notice;

    memset(out, 0, sizeof *out);
    out->address = address;
    out->module = text_of(NULL);
    out->build_id = text_of(NULL);
    if (address == 0 ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)(uintptr_t)(address - 1), &module)) {
        return;
    }
    out->base = (uint64_t)(uintptr_t)module;
    n = GetModuleFileNameW(module, name, 1024);
    if (n > 0 && n < 1024) {
        for (i = 0; i < n; i++) {
            units[i] = (uint16_t)name[i];
        }
        out->module = keep((const char *)bytes,
                           anti_utf16_to_utf8(units, n, bytes));
    }
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(uintptr_t)anti_rt_trace_walk, &self);
    if (module == self) {
        out->build_id = notice_id(anti_licenses, SIZE_MAX);
        return;
    }
    notice = GetProcAddress(module, "anti_licenses");
    if (notice != NULL) {
        out->build_id = notice_id((const char *)(uintptr_t)notice, SIZE_MAX);
    }
}

typedef BOOL(WINAPI *sym_initialize)(HANDLE, PCSTR, BOOL);
typedef DWORD(WINAPI *sym_set_options)(DWORD);
typedef BOOL(WINAPI *sym_from_addr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL(WINAPI *sym_line)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL(WINAPI *sym_search_path)(HANDLE, PWSTR, DWORD);
typedef BOOL(WINAPI *sym_set_search_path)(HANDLE, PCWSTR);

/* DbgHelp serves one thread at a time, so one lock covers every call. */
static SRWLOCK help_lock = SRWLOCK_INIT;
static bool help_tried;
static sym_from_addr help_from_addr;
static sym_line help_line;
static sym_search_path help_get_path;
static sym_set_search_path help_set_path;

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

void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out)
{
    union {
        SYMBOL_INFO info;
        char room[sizeof(SYMBOL_INFO) + 512];
    } symbol;
    IMAGEHLP_LINE64 line;
    DWORD64 displacement = 0;
    DWORD column = 0;
    DWORD64 pc;

    memset(out, 0, sizeof *out);
    out->address = frame->address;
    out->function = text_of(NULL);
    out->file = text_of(NULL);
    if (frame->address == 0) {
        return;
    }
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    pc = (DWORD64)(frame->address - 1);
    AcquireSRWLockExclusive(&help_lock);
    if (!help_tried) {
        open_help();
    }
    if (frame->base != 0) {
        search_module(frame->base);
    }
    memset(&symbol, 0, sizeof symbol);
    symbol.info.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.info.MaxNameLen = 512;
    if (help_from_addr != NULL &&
        help_from_addr(GetCurrentProcess(), pc, &displacement, &symbol.info)) {
        size_t n = strnlen(symbol.info.Name, symbol.info.NameLen);
        char name[512];
        size_t named = anti_coff_demangle(symbol.info.Name, n, name,
                                          sizeof name);
        out->function = named > 0 ? keep(name, named)
                                  : keep(symbol.info.Name, n);
    }
    memset(&line, 0, sizeof line);
    line.SizeOfStruct = sizeof line;
    if (help_line != NULL &&
        help_line(GetCurrentProcess(), pc, &column, &line) &&
        line.FileName != NULL) {
        out->file = keep(line.FileName, strlen(line.FileName));
        out->line = (int64_t)line.LineNumber;
    }
    ReleaseSRWLockExclusive(&help_lock);
}
#endif

/* A text that grows, for the text of a trace. It turns NULL when the
   memory runs out. */
struct growing {
    unsigned char *bytes;
    size_t used;
    size_t room;
};

/* The room of one formatted line of the text of a trace. */
enum { LINE_ROOM = 128 };

static void add(struct growing *g, const void *bytes, size_t n)
{
    if (g->bytes == NULL) {
        return;
    }
    if (g->used + n + 1 > g->room) {
        size_t room = 2 * (g->used + n + 1);
        unsigned char *more = realloc(g->bytes, room);
        if (more == NULL) {
            free(g->bytes);
            g->bytes = NULL;
            return;
        }
        g->bytes = more;
        g->room = room;
    }
    memcpy(g->bytes + g->used, bytes, n);
    g->used += n;
    g->bytes[g->used] = 0;
}

/* Add the line snprintf wrote when it gave n, or give the bytes back
   and 0 when it failed or cut the line short. */
static int add_line(struct growing *g, const char *line, int n)
{
    if (n < 0 || (size_t)n >= LINE_ROOM) {
        free(g->bytes);
        g->bytes = NULL;
        return 0;
    }
    add(g, line, (size_t)n);
    return 1;
}

/* The number of the module of frame i: the count of distinct modules
   that frames before it reach first, or -1 outside every module. */
static int64_t module_number(const struct anti_raw_frame *frames, int64_t i)
{
    int64_t seen = 0;
    int64_t j;
    int64_t k;

    if (frames[i].base == 0) {
        return -1;
    }
    for (j = 0; j <= i; j++) {
        if (frames[j].base == 0) {
            continue;
        }
        for (k = 0; k < j && frames[k].base != frames[j].base; k++) {
        }
        if (k < j) {
            continue;
        }
        if (frames[j].base == frames[i].base) {
            return seen;
        }
        seen++;
    }
    return -1;
}

/* DESIGN: the text of a trace names every module once, in the order the
   frames reach them, as `module <n> <build id> <base> <path>`. Each frame
   follows as its address and `<n>+<offset>` into its module, or `-`
   outside every module. That is the raw form a resolver matches to the
   symbols of a build by the id. */
unsigned char *anti_rt_trace_text(const struct anti_raw_frame *frames,
                                  int64_t count)
{
    struct growing g;
    char line[LINE_ROOM];
    int64_t modules = 0;
    int64_t i;
    int n;

    g.room = 256;
    g.used = 0;
    g.bytes = malloc(g.room);
    if (g.bytes == NULL) {
        return NULL;
    }
    g.bytes[0] = 0;
    for (i = 0; i < count; i++) {
        const struct anti_raw_frame *f = &frames[i];
        if (module_number(frames, i) != modules) {
            continue;
        }
        n = snprintf(line, sizeof line, "%smodule %" PRId64 " ",
                     g.used > 0 ? "\n" : "", modules++);
        if (!add_line(&g, line, n)) {
            return NULL;
        }
        if (f->build_id.len > 0) {
            add(&g, f->build_id.ptr, (size_t)f->build_id.len);
        } else {
            add(&g, "-", 1);
        }
        n = snprintf(line, sizeof line, " 0x%016" PRIx64 " ", f->base);
        if (!add_line(&g, line, n)) {
            return NULL;
        }
        add(&g, f->module.ptr, (size_t)f->module.len);
    }
    for (i = 0; i < count; i++) {
        const struct anti_raw_frame *f = &frames[i];
        int64_t number = module_number(frames, i);
        if (number < 0) {
            n = snprintf(line, sizeof line, "%s0x%016" PRIx64 " -",
                         g.used > 0 ? "\n" : "", f->address);
        } else {
            n = snprintf(line, sizeof line,
                         "%s0x%016" PRIx64 " %" PRId64 "+0x%" PRIx64,
                         g.used > 0 ? "\n" : "", f->address, number,
                         f->address - f->base);
        }
        if (!add_line(&g, line, n)) {
            return NULL;
        }
    }
    return g.bytes;
}
