/* The platform layer on macOS and Linux. See platform.h. */
#if !defined(_WIN32)

/* clock_gettime, nanosleep, dladdr, sysconf, syscall, fseeko,
   pthread_getattr_np, dl_iterate_phdr, readlink and posix_memalign sit
   behind a feature macro, and the two systems spell it differently. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#include <os/lock.h>
#include <sys/sysctl.h>
#else
#include <link.h>
#include <sys/auxv.h>
#include <sys/random.h>
#include <sys/syscall.h>
#endif

#include "cpu_level.h"
#include "platform.h"
#include "rt.h"
#include "std.h"
#include "utf.h"

/* DESIGN: anti.os builds the per-user directories, and the conventions
   differ between Windows and the two Unix platforms. The runtime is
   compiled once per target, so the answer is a constant of the build.
   Reading it from the environment instead would take the layout of
   Windows on any machine that happens to carry LOCALAPPDATA. */
int64_t anti_rt_is_windows(void)
{
    return 0;
}

/* The C library gives the message of strerror in storage of its own. */
const unsigned char *anti_rt_errno_text(int32_t code)
{
    return (const unsigned char *)strerror((int)code);
}

/* See the DESIGN comment of the declaration in std.h. */
int32_t anti_rt_last_error(void)
{
    return 0;
}

const unsigned char *anti_rt_last_error_text(int32_t code)
{
    (void)code;
    return (const unsigned char *)"";
}

struct anti_rt_monitor {
    pthread_mutex_t lock;
    pthread_cond_t conditions[2];
};

#define MONITOR_INIT                                                       \
    {PTHREAD_MUTEX_INITIALIZER,                                            \
     {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER}}

static struct anti_rt_monitor monitors[] = {
    MONITOR_INIT, MONITOR_INIT, MONITOR_INIT,
    MONITOR_INIT, MONITOR_INIT, MONITOR_INIT,
    MONITOR_INIT, MONITOR_INIT, MONITOR_INIT,
};

_Static_assert(sizeof monitors / sizeof monitors[0] == ANTI_RT_LOCK_COUNT,
               "one initializer per name of enum anti_rt_lock");

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
    if (pthread_mutex_init(&m->lock, NULL) != 0) {
        free(m);
        return NULL;
    }
    if (pthread_cond_init(&m->conditions[0], NULL) != 0) {
        pthread_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    if (pthread_cond_init(&m->conditions[1], NULL) != 0) {
        pthread_cond_destroy(&m->conditions[0]);
        pthread_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    return m;
}

void anti_rt_monitor_free(struct anti_rt_monitor *m)
{
    pthread_cond_destroy(&m->conditions[1]);
    pthread_cond_destroy(&m->conditions[0]);
    pthread_mutex_destroy(&m->lock);
    free(m);
}

void anti_rt_monitor_hold(struct anti_rt_monitor *m)
{
    pthread_mutex_lock(&m->lock);
}

void anti_rt_monitor_release(struct anti_rt_monitor *m)
{
    pthread_mutex_unlock(&m->lock);
}

void anti_rt_monitor_wait(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_wait(&m->conditions[condition], &m->lock);
}

void anti_rt_monitor_wake_one(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_signal(&m->conditions[condition]);
}

void anti_rt_monitor_wake_all(struct anti_rt_monitor *m, int condition)
{
    pthread_cond_broadcast(&m->conditions[condition]);
}

#if defined(__APPLE__)

_Static_assert(sizeof(struct anti_rt_word) == sizeof(os_unfair_lock) &&
                   _Alignof(struct anti_rt_word) == _Alignof(os_unfair_lock),
               "the word of a Mutex is an os_unfair_lock");

void anti_rt_word_lock(struct anti_rt_word *w)
{
    os_unfair_lock_lock((os_unfair_lock_t)w);
}

void anti_rt_word_unlock(struct anti_rt_word *w)
{
    os_unfair_lock_unlock((os_unfair_lock_t)w);
}

#else

/* The private operations of futex(2), which the headers of musl do not
   name. */
#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

/* DESIGN: the word is 0 when free, 1 when held and 2 when held with a
   thread waiting, the mutex of Drepper's "Futexes Are Tricky". An
   unlock that finds 2 wakes one waiter, and a free lock costs one
   compare-and-swap to take and one exchange to give back. */
void anti_rt_word_lock(struct anti_rt_word *w)
{
    uint32_t c = 0;

    if (__atomic_compare_exchange_n(&w->opaque, &c, 1, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2) {
        c = __atomic_exchange_n(&w->opaque, 2, __ATOMIC_ACQUIRE);
    }
    while (c != 0) {
        syscall(SYS_futex, &w->opaque, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
        c = __atomic_exchange_n(&w->opaque, 2, __ATOMIC_ACQUIRE);
    }
}

void anti_rt_word_unlock(struct anti_rt_word *w)
{
    if (__atomic_exchange_n(&w->opaque, 0, __ATOMIC_RELEASE) == 2) {
        syscall(SYS_futex, &w->opaque, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    }
}

#endif

/* The body of a thread, on the heap until the thread takes it. */
struct start {
    void (*body)(void);
};

#if defined(__APPLE__)
/* DESIGN: on macOS the thread-local variables of a thread live in one
   heap block per image, which dyld allocates on the first access and
   frees when the thread ends. Taking the address of this variable makes
   the block of the runtime's image. */
static _Thread_local char thread_block;

const void *anti_rt_thread_block(void)
{
    return &thread_block;
}
#else
const void *anti_rt_thread_block(void)
{
    return NULL;
}
#endif

static void *thread_main(void *start)
{
    void (*body)(void) = ((struct start *)start)->body;

    free(start);
    body();
    return NULL;
}

int anti_rt_thread_start(void (*body)(void))
{
    struct start *start = malloc(sizeof *start);
    pthread_t thread;

    if (start == NULL) {
        return -1;
    }
    start->body = body;
    if (pthread_create(&thread, NULL, thread_main, start) != 0) {
        free(start);
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

int64_t anti_rt_processors(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);

    return n > 0 ? (int64_t)n : 1;
}

int anti_rt_getenv(const char *name, char **value)
{
    const char *text = getenv(name);
    size_t length;

    *value = NULL;
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    length = strlen(text);
    *value = malloc(length + 1);
    if (*value == NULL) {
        return -1;
    }
    memcpy(*value, text, length + 1);
    return 0;
}

int anti_rt_path_is_absolute(const char *path)
{
    return path[0] == '/';
}

const char *anti_rt_path_last_separator(const char *path)
{
    return strrchr(path, '/');
}

FILE *anti_rt_file_open(const char *path, int writing)
{
    return fopen(path, writing != 0 ? "wb" : "rb");
}

int64_t anti_rt_file_tell(FILE *file)
{
    return (int64_t)ftello(file);
}

int anti_rt_file_seek(FILE *file, int64_t offset, int whence)
{
    return fseeko(file, (off_t)offset, whence) == 0 ? 0 : -1;
}

int anti_rt_file_is_directory(FILE *file)
{
    struct stat info;

    return fstat(fileno(file), &info) == 0 && S_ISDIR(info.st_mode) ? 1 : 0;
}

int anti_rt_file_remove(const char *path)
{
    return remove(path) == 0 ? 0 : -1;
}

int anti_rt_file_rename(const char *from, const char *to)
{
    return rename(from, to) == 0 ? 0 : -1;
}

int anti_rt_directory_list(const char *path,
                           int (*each)(void *context,
                                       const unsigned char *name,
                                       size_t length),
                           void *context)
{
    DIR *dir = opendir(path);
    int status = 0;
    int saved;

    if (dir == NULL) {
        return -1;
    }
    for (;;) {
        struct dirent *entry;
        /* readdir gives NULL at the end and on a failure, and only a
           failure sets errno. */
        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            status = errno != 0 ? -1 : 0;
            break;
        }
        if (each(context, (const unsigned char *)entry->d_name,
                 strlen(entry->d_name)) != 0) {
            status = -1;
            break;
        }
    }
    saved = errno;
    closedir(dir);
    errno = saved;
    return status;
}

void *anti_rt_library_open(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void anti_rt_library_close(void *handle)
{
    dlclose(handle);
}

void *anti_rt_library_symbol(void *handle, const char *name)
{
    return dlsym(handle, name);
}

const char *anti_rt_library_error(char *text, size_t size)
{
    const char *reason = dlerror();

    (void)text;
    (void)size;
    return reason != NULL ? reason : "cannot open the file";
}

const void *anti_rt_library_image(const void *address)
{
    Dl_info info;

    if (dladdr(address, &info) == 0) {
        return NULL;
    }
    return info.dli_fbase;
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

/* Copy name into the path of out, or leave the path empty when it is
   NULL or does not fit. */
static void copy_path(struct anti_rt_module *out, const char *name)
{
    size_t length = name != NULL ? strlen(name) : 0;

    if (name == NULL || length >= sizeof out->path) {
        out->path[0] = '\0';
        return;
    }
    memcpy(out->path, name, length + 1);
}

static void module_init(struct anti_rt_module *out,
                        enum anti_rt_image_format format)
{
    out->format = format;
    out->base = 0;
    out->headers = NULL;
    out->header_count = 0;
    out->notice_read = false;
    out->notice = NULL;
    out->path[0] = '\0';
}

#if defined(__APPLE__)

/* dyld knows the image of every address. The reader of Mach-O finds the
   notice of each in the symbol table of its header, so the layer reads
   none. */
bool anti_rt_module_at(uint64_t address, struct anti_rt_module *out)
{
    Dl_info info;

    module_init(out, ANTI_RT_IMAGE_MACHO);
    if (dladdr((const void *)(uintptr_t)address, &info) == 0 ||
        info.dli_fbase == NULL) {
        return false;
    }
    out->base = (uint64_t)(uintptr_t)info.dli_fbase;
    copy_path(out, info.dli_fname);
    return true;
}

#else

/* DESIGN: a static library for C has no notice, so the runtime names
   `anti_licenses` without needing it. ELF takes a weak reference, which
   is 0 without a definition. */
extern const char anti_licenses[] __attribute__((weak));

/* The path of the program, which the list of modules gives no name. */
static char program_path[ANTI_RT_PATH_ROOM];
static pthread_once_t program_once = PTHREAD_ONCE_INIT;

static void read_program_path(void)
{
    ssize_t n = readlink("/proc/self/exe", program_path,
                         sizeof program_path - 1);

    program_path[n > 0 ? n : 0] = 0;
}

/* The reader of ELF takes the program headers as the 56 bytes of the
   64-bit form, which every Linux target has. */
_Static_assert(sizeof(ElfW(Phdr)) == 56, "a program header is 56 bytes");

/* The module an address lies in, as the list of modules gives it. */
struct module_of {
    uintptr_t address;
    const char *name;
    uintptr_t bias;
    const ElfW(Phdr) *headers;
    size_t header_count;
    size_t visited;
    bool found;
    bool program;
};

static int visit_module(struct dl_phdr_info *info, size_t size, void *context)
{
    struct module_of *m = context;
    int i;

    /* dl_iterate_phdr passes the size of info, which only a caller that
       reads fields past dlpi_phnum needs. */
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

static bool find_module(uintptr_t address, struct module_of *m)
{
    memset(m, 0, sizeof *m);
    m->address = address;
    dl_iterate_phdr(visit_module, m);
    return m->found;
}

/* dl_iterate_phdr visits the program first. glibc names it with an empty
   text and musl `/proc/self/exe` in a static program, so its path is read
   from that link. The runtime reads its own notice by the weak reference
   above. The reader of ELF finds the notice of every other module in its
   file. */
bool anti_rt_module_at(uint64_t address, struct anti_rt_module *out)
{
    struct module_of m;
    struct module_of self;

    module_init(out, ANTI_RT_IMAGE_ELF);
    if (!find_module((uintptr_t)address, &m)) {
        return false;
    }
    out->base = (uint64_t)m.bias;
    out->headers = m.headers;
    out->header_count = m.header_count;
    if (m.program) {
        pthread_once(&program_once, read_program_path);
        copy_path(out, program_path);
    } else {
        copy_path(out, m.name);
    }
    if (find_module((uintptr_t)anti_rt_trace_walk, &self) &&
        self.bias == m.bias) {
        out->notice_read = true;
        out->notice = anti_licenses;
    }
    return true;
}

#endif

void anti_rt_debug_lookup(uint64_t address, uint64_t base,
                          struct anti_rt_debug_answer *out)
{
    /* The system has no debugger library, and the address and the base
       serve the one of Windows alone. */
    (void)address;
    (void)base;
    out->function[0] = '\0';
    out->file[0] = '\0';
    out->line = 0;
}

/* The function the reader calls with each signal of the pipe. It is set
   once, under ANTI_RT_LOCK_SIGNALS and before the reader starts. A signal
   number of C fits the byte the handler writes. */
static int (*deliver_signal)(int64_t sig);

/* Written once, under ANTI_RT_LOCK_SIGNALS and before any handler that
   reads the write end is installed. */
static int pipe_ends[2] = {-1, -1};

/* The handler writes one byte and nothing else. Every function it could
   call beside write is undefined in a handler. */
static void on_raise(int sig)
{
    unsigned char byte = (unsigned char)sig;
    ssize_t written = write(pipe_ends[1], &byte, 1);

    /* A write that fails loses the signal, and a handler can do nothing
       else about it. */
    (void)written;
}

static void signal_reader(void)
{
    unsigned char byte;

    while (read(pipe_ends[0], &byte, 1) == 1) {
        deliver_signal((int64_t)byte);
    }
}

/* A pipe whose reader did not start is closed, so a later call starts
   afresh. */
int anti_rt_signal_route(int (*deliver)(int64_t sig))
{
    if (deliver_signal != NULL) {
        return 0;
    }
    if (pipe(pipe_ends) != 0) {
        return -1;
    }
    deliver_signal = deliver;
    if (anti_rt_thread_start(signal_reader) != 0) {
        deliver_signal = NULL;
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        pipe_ends[0] = -1;
        pipe_ends[1] = -1;
        return -1;
    }
    return 0;
}

void anti_rt_signal_catch(int64_t sig)
{
    signal((int)sig, on_raise);
}

int anti_rt_entropy(void *out, size_t count)
{
#if defined(__APPLE__)
    arc4random_buf(out, count);
    return 0;
#else
    unsigned char *at = out;

    while (count > 0) {
        ssize_t n = getrandom(at, count, 0);
        if (n <= 0) {
            return -1;
        }
        at += n;
        count -= (size_t)n;
    }
    return 0;
#endif
}

/* DESIGN: the monotonic clock never moves backwards and has no relation
   to the wall clock. A duration is therefore measured with the first and
   a date with the second. Both are given in nanoseconds, which holds 292
   years in an int64_t and is the resolution every target offers. */

int64_t anti_rt_monotonic(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + (int64_t)now.tv_nsec;
}

int64_t anti_rt_wall(void)
{
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    return (int64_t)now.tv_sec * 1000000000 + (int64_t)now.tv_nsec;
}

void anti_rt_sleep(int64_t nanoseconds)
{
    struct timespec wait;

    if (nanoseconds <= 0) {
        return;
    }
    wait.tv_sec = (time_t)(nanoseconds / 1000000000);
    wait.tv_nsec = (long)(nanoseconds % 1000000000);
    while (nanosleep(&wait, &wait) != 0) {
        /* A signal interrupted the wait, and wait holds what is left. */
    }
}

/* The count strings of list as valid UTF-8, each in a block of its own,
   in a list of one block. NULL when memory runs out, with every block of
   the call given back. */
static char **utf8_list(char *const *list, size_t count)
{
    char **out = malloc((count + 1) * sizeof *out);
    size_t i;

    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        size_t n = strlen(list[i]);
        unsigned char *bytes = malloc(3 * n + 1);
        size_t written;
        if (bytes == NULL) {
            while (i > 0) {
                free(out[--i]);
            }
            free(out);
            return NULL;
        }
        written = anti_rt_utf8_repair((const unsigned char *)list[i], n, bytes);
        bytes[written] = 0;
        out[i] = (char *)bytes;
    }
    out[count] = NULL;
    return out;
}

char **anti_rt_process_arguments(int argc, char **argv, size_t *count)
{
    *count = argc > 0 ? (size_t)argc : 0;
    return utf8_list(argv, *count);
}

#if defined(__APPLE__)
/* DESIGN: environ is defined by the start code of an executable, so a
   shared library on macOS cannot link against it. The runtime of a
   library for C asks _NSGetEnviron of libSystem, which every image may
   call. */
static char **environment_list(void)
{
    return *_NSGetEnviron();
}
#else
extern char **environ;

static char **environment_list(void)
{
    return environ;
}
#endif

char **anti_rt_process_environment(size_t *count)
{
    char **list = environment_list();
    size_t n = 0;

    while (list != NULL && list[n] != NULL) {
        n++;
    }
    *count = n;
    return utf8_list(list, n);
}

void anti_rt_streams_binary(void)
{
}

/* DESIGN: posix_memalign gives memory at any power of two that is a
   multiple of the size of a pointer, and free releases it. A smaller
   alignment is raised to that size, which every smaller one divides. */
void *anti_rt_aligned_alloc(size_t size, size_t align)
{
    void *p = NULL;

    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }
    if (posix_memalign(&p, align, size) != 0) {
        return NULL;
    }
    return p;
}

void anti_rt_aligned_free(void *p)
{
    free(p);
}

/* DESIGN: the runtime marks what it keeps until exit through a function
   that a program of --memory-checks replaces, see rt.h. The definition
   here is weak on ELF and Mach-O, so the one of the program wins and
   every other program pays one call that returns. */
__attribute__((weak)) void anti_rt_memory_kept(const void *p)
{
    /* The block serves the leak checker of the program that replaces
       this definition alone. */
    (void)p;
}

#if defined(ANTI_RT_X86_64)

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

    /* xgetbv by its bytes, so that the assembler needs no xsave option. */
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0"
                     : "=a"(low), "=d"(high)
                     : "c"(0));
    return (uint64_t)high << 32 | low;
}

#elif defined(ANTI_RT_ARM64) && defined(__APPLE__)

static int feature(const char *name)
{
    int32_t value = 0;
    size_t size = sizeof value;

    if (sysctlbyname(name, &value, &size, NULL, 0) != 0) {
        return 0;
    }
    return value != 0;
}

int32_t anti_rt_arm64_level(void)
{
    if (feature("hw.optional.arm.FEAT_SB") &&
        feature("hw.optional.arm.FEAT_FRINTTS")) {
        return ANTI_CPU_ARMV8_5;
    }
    if (feature("hw.optional.arm.FEAT_LSE") &&
        feature("hw.optional.arm.FEAT_FP16")) {
        return ANTI_CPU_ARMV8_2;
    }
    return ANTI_CPU_ARMV8_0;
}

#elif defined(ANTI_RT_ARM64)

/* The AT_HWCAP bits of the features the levels need. A musl header of the
   sysroot declares none of them. */
#define ANTI_HWCAP_ATOMICS (1UL << 8)
#define ANTI_HWCAP_FPHP (1UL << 9)
#define ANTI_HWCAP_ASIMDHP (1UL << 10)
#define ANTI_HWCAP_SB (1UL << 29)
#define ANTI_HWCAP2_FRINT (1UL << 8)
#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif

int32_t anti_rt_arm64_level(void)
{
    unsigned long one = getauxval(AT_HWCAP);
    unsigned long two = getauxval(AT_HWCAP2);

    if ((one & ANTI_HWCAP_SB) != 0 && (two & ANTI_HWCAP2_FRINT) != 0) {
        return ANTI_CPU_ARMV8_5;
    }
    if ((one & ANTI_HWCAP_ATOMICS) != 0 && (one & ANTI_HWCAP_FPHP) != 0 &&
        (one & ANTI_HWCAP_ASIMDHP) != 0) {
        return ANTI_CPU_ARMV8_2;
    }
    return ANTI_CPU_ARMV8_0;
}

#endif

#else

/* ISO C wants a declaration in every file, and on Windows this one holds
   no other. */
typedef int anti_rt_platform_posix_unused;

#endif
