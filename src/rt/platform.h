/* The platform layer of the runtime: what the rest of src/rt asks of the
   system it runs on.

   DESIGN: rule 22 of docs/c-guidelines.md. A `#if` on the host system
   stands only in this header, in platform_posix.c and in
   platform_windows.c. The build keeps one list of runtime sources, so
   both .c files are compiled for every target. Each holds its code
   inside one `#if` of its own system. The other file of a
   target then compiles to nothing. Every function below takes and gives
   text as UTF-8, whatever the system uses underneath. */
#ifndef ANTI_RT_PLATFORM_H
#define ANTI_RT_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 1 in the runtime of a Windows target and 0 elsewhere. anti.os asks it. */
int64_t anti_rt_is_windows(void);

#if defined(_WIN32)
/* The start of a Windows program: the C runtime of ucrtbase.dll set up
   for the program and its initialisers run, then program called with the
   arguments of the command line and its status given to exit.
   mainCRTStartup of platform_entry.c, the entry point the linker names,
   calls it. */
_Noreturn void anti_rt_windows_start(int (*program)(int, char **));
#endif

/* The architecture the runtime is compiled for, as a name of the layer.
   clang names it the same on every system, and MSVC by names of its
   own. */
#if defined(__x86_64__) || defined(_M_X64)
#define ANTI_RT_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ANTI_RT_ARM64 1
#endif

/* DESIGN: the C main of src/rt/start.c calls the main of the program
   through anti.rt.main, the entry symbol that antic defines. A symbol with
   a dot is no C identifier, so the declaration names it with an assembler
   label, which this macro gives. Mach-O adds `_` to the symbols of C and
   ELF does not. COFF spells the symbol as an identifier, which the label
   names as it stands. */
#if defined(_WIN32)
#define ANTI_RT_ENTRY_LABEL __asm__("_A4anti2rt_main")
#elif defined(__APPLE__)
#define ANTI_RT_ENTRY_LABEL __asm__("_anti.rt.main")
#else
#define ANTI_RT_ENTRY_LABEL __asm__("anti.rt.main")
#endif

/* DESIGN: the C main of src/rt/start.c takes the arguments and the
   environment of the process from the layer, as strings of valid UTF-8
   ended by a NUL. macOS and Linux give bytes, argv and the environment of
   the C library, and the layer repairs a byte that is no UTF-8 into
   U+FFFD. Windows gives UTF-16, the command line split by the rules of
   the C runtime of Microsoft and GetEnvironmentStringsW. The argv of the
   C runtime there is in the ANSI code page and is not read. Each string
   is a block of its own that lives until exit, and the list one block
   the caller frees. NULL when memory runs out. */

/* The arguments of the process into a list of *count strings. argc and
   argv are the ones of main. */
char **anti_rt_process_arguments(int argc, char **argv, size_t *count);

/* The variables of the environment as name=value, into a list of *count
   strings. */
char **anti_rt_process_environment(size_t *count);

/* Make stdout and stderr write every byte as it stands. The C streams of
   Windows start in text mode, which writes CRLF for each LF, and the
   streams of macOS and Linux write bytes already. */
void anti_rt_streams_binary(void);

/* Memory of size bytes, at least 1, at align, a power of two, or NULL.
   anti_rt_aligned_free gives it back, and nothing else may: Windows keeps
   it apart from the memory of malloc. NULL is no memory and is left. */
void *anti_rt_aligned_alloc(size_t size, size_t align);
void anti_rt_aligned_free(void *p);

/* DESIGN: the processor check of src/rt/cpu.c reads what the machine
   offers. x86_64 reads it with the instructions CPUID and XGETBV, the same
   on every system, which C has no words for, so the layer gives the two.
   ARM64 has the system answer by a query of its own, and the layer gives
   the level the answer means. */
#if defined(ANTI_RT_X86_64)
/* eax, ebx, ecx and edx of CPUID for leaf and sub-leaf, into out. */
void anti_rt_cpuid(uint32_t leaf, uint32_t sub, uint32_t out[4]);
/* XCR0, which says whether the system saves the SSE and the AVX state.
   Only a processor whose CPUID gives OSXSAVE runs the instruction. */
uint64_t anti_rt_xcr0(void);
#elif defined(ANTI_RT_ARM64)
/* The level of enum anti_cpu_level of src/rt/cpu_level.h that the system
   reports for the machine. */
int32_t anti_rt_arm64_level(void);
#endif

/* DESIGN: a monitor is a lock of the system with two condition
   variables, numbered 0 and 1. A thread waits on one while it holds the
   lock, and the wait gives the lock back until a wake. A monitor is the
   one form of a lock and a condition the runtime has: the pool of
   src/rt/threads.c, the channels and the selects of src/rt/sync.c and
   every lock at file scope. */
struct anti_rt_monitor;

/* The locks the runtime keeps at file scope. Each is a monitor with an
   initializer of the system, so it is ready before main, and the storage
   stands in the platform file. A new one is a new name here. */
enum anti_rt_lock {
    ANTI_RT_LOCK_CONF,          /* the keys of src/rt/conf.c */
    ANTI_RT_LOCK_PLUGINS,       /* the slots of src/rt/loaded.c */
    ANTI_RT_LOCK_SIGNALS,       /* the functions of src/rt/signal.c */
    ANTI_RT_LOCK_POOL,          /* the pool and its jobs, src/rt/threads.c */
    ANTI_RT_LOCK_SELECT,        /* the selects that wait, src/rt/sync.c */
    ANTI_RT_LOCK_ORDERS,        /* the orders of the locks, src/rt/lock.c */
    ANTI_RT_LOCK_TRACE_TEXTS,   /* the kept texts of src/rt/trace.c */
    ANTI_RT_LOCK_TRACE_FILES,   /* the files and build ids, src/rt/trace.c */
    ANTI_RT_LOCK_DEBUG,         /* the debugger library of the system */
    ANTI_RT_LOCK_COUNT
};

struct anti_rt_monitor *anti_rt_monitor_of(enum anti_rt_lock which);

/* A monitor on the heap, or NULL when the system or the memory gives
   none. anti_rt_monitor_free ends it, and no thread may hold it then. */
struct anti_rt_monitor *anti_rt_monitor_new(void);
void anti_rt_monitor_free(struct anti_rt_monitor *m);

void anti_rt_monitor_hold(struct anti_rt_monitor *m);
void anti_rt_monitor_release(struct anti_rt_monitor *m);

/* Wait on condition, 0 or 1, while the thread holds m. A wait may end
   without a wake, so the caller checks its own state in a loop. */
void anti_rt_monitor_wait(struct anti_rt_monitor *m, int condition);
void anti_rt_monitor_wake_one(struct anti_rt_monitor *m, int condition);
void anti_rt_monitor_wake_all(struct anti_rt_monitor *m, int condition);

static inline void anti_rt_lock_hold(enum anti_rt_lock which)
{
    anti_rt_monitor_hold(anti_rt_monitor_of(which));
}

static inline void anti_rt_lock_release(enum anti_rt_lock which)
{
    anti_rt_monitor_release(anti_rt_monitor_of(which));
}

/* DESIGN: the word of a Mutex and of the hidden lock of a synchronized
   object is one word of the program's own memory, and the system keeps
   nothing for it until a thread has to wait: a futex word on Linux,
   os_unfair_lock on macOS and SRWLOCK on Windows. Zero is the unlocked
   state of all three, so memory that nothing wrote holds an unlocked
   word. The word is four bytes on Linux and macOS and eight on Windows,
   which the layout of the compiler gives per target. The platform files
   check that the struct has the size and the alignment of the lock of
   the system. */
#if defined(_WIN32)
struct anti_rt_word {
    void *opaque;
};
#else
struct anti_rt_word {
    uint32_t opaque;
};
#endif

void anti_rt_word_lock(struct anti_rt_word *w);
void anti_rt_word_unlock(struct anti_rt_word *w);

/* Start a thread that runs body and ends with it. Nothing joins the
   thread. Returns 0, or -1 when the system starts none. */
int anti_rt_thread_start(void (*body)(void));

/* The block of the heap that holds the thread-local variables of the
   calling thread in the image of the runtime, made by the call, or NULL
   where the system keeps them elsewhere. Only macOS has one. */
const void *anti_rt_thread_block(void);

/* The processors the machine that runs the program has online, at
   least 1. */
int64_t anti_rt_processors(void);

/* The value of the environment variable name in UTF-8, in memory the
   caller frees, into *value. *value is NULL when the variable is unset
   or empty. Returns 0, or -1 when memory runs out. Windows reads the
   variable as UTF-16, so a value of any length and any character comes
   through. A value that is no valid UTF-16 counts as unset. */
int anti_rt_getenv(const char *name, char **value);

/* 1 when path names a file without a directory to start from: `/` on
   every system, and `\` and a drive letter with a colon on Windows. */
int anti_rt_path_is_absolute(const char *path);

/* The last separator of the directories of path, or NULL when it has
   none. `/` on every system, and `\` as well on Windows. */
const char *anti_rt_path_last_separator(const char *path);

/* DESIGN: a path in the calls of files below is UTF-8 ended by a NUL.
   Each call reports a failure through errno alone, so anti.fs builds
   every error with SystemError.from_errno on every system. Windows takes
   its paths as UTF-16, through the wide functions of the C runtime,
   which set errno as well. The Win32 calls set an error of their own and
   are not used. A path that is no valid UTF-8 fails there with EINVAL. */

/* How anti_rt_file_open opens a file. Every mode is binary, so no byte
   is translated on Windows. Writing creates the file or empties the one
   that is there, and appending creates it or writes after its end. */
#define ANTI_FILE_READ 0
#define ANTI_FILE_WRITE 1
#define ANTI_FILE_APPEND 2

/* Open the file in mode, one of the three above. NULL with errno set
   when it fails, and EINVAL for any other mode. */
FILE *anti_rt_file_open(const char *path, int mode);

/* The offset of the stream in 64 bits, or -1 with errno set. */
int64_t anti_rt_file_tell(FILE *file);

/* Move the stream to offset from whence, SEEK_SET, SEEK_CUR or SEEK_END.
   Returns 0, or -1 with errno set. */
int anti_rt_file_seek(FILE *file, int64_t offset, int whence);

/* 1 when the stream reads a directory and 0 otherwise. macOS and Linux
   open a directory as a stream, and Windows opens none. */
int anti_rt_file_is_directory(FILE *file);

/* Remove the file, or rename it. Each returns 0, or -1 with errno set. */
int anti_rt_file_remove(const char *path);
int anti_rt_file_rename(const char *from, const char *to);

/* Call each with the name of every entry of the directory at path, in
   UTF-8 and the order the system gives them, `.` and `..` included. each
   returns 0 to go on, or -1 with errno set to stop the listing. Returns
   0, or -1 with errno set, from each or from the system. An empty path
   names no directory and gives ENOENT. */
int anti_rt_directory_list(const char *path,
                           int (*each)(void *context,
                                       const unsigned char *name,
                                       size_t length),
                           void *context);

/* The system's loader of shared libraries. open takes a path in UTF-8
   and gives NULL when it fails, and error then gives the reason. A reason
   the system writes itself goes to text, of size bytes. The reason
   dlerror gives is kept per thread by the C library. The handle open
   gives belongs to the caller, and close gives it back. */
void *anti_rt_library_open(const char *path);
void anti_rt_library_close(void *handle);
void *anti_rt_library_symbol(void *handle, const char *name);
const char *anti_rt_library_error(char *text, size_t size);

/* The base of the image of the program or of a library that address
   lies in, or NULL when it lies in none. The loader of the system holds
   a lock of its own while it answers. */
const void *anti_rt_library_image(const void *address);

/* DESIGN: a stack trace asks the system three things: the walk of the
   frames, the module an address lies in and, on Windows, the debugger
   library for a name and a line. src/rt/trace.c reads the symbol tables
   and the line tables itself. The format of the module tells it which
   reader, a value of the run and not a `#if`. */

/* The return addresses of the caller and the calls above it, at most
   room of them into into, less the skip innermost. Gives the count.
   anti.lang.StackTrace calls it. */
int64_t anti_rt_trace_walk(uint64_t *into, int64_t room, int64_t skip);

enum anti_rt_image_format {
    /* The header of the image lies at base, and its symbol table is read
       in memory. The line table stands in the objects of its debug map. */
    ANTI_RT_IMAGE_MACHO,
    /* The file at path holds the symbol table and the line table, and
       base is the bias of its addresses in memory. */
    ANTI_RT_IMAGE_ELF,
    /* The debugger library reads the PDB, through anti_rt_debug_lookup. */
    ANTI_RT_IMAGE_PE
};

/* The room of a path the layer gives, its NUL included. */
#define ANTI_RT_PATH_ROOM 4096

/* The module an address lies in, as the loader of the system knows it. */
struct anti_rt_module {
    enum anti_rt_image_format format;
    /* Where the module lies in memory, which the text of a trace counts
       offsets from. */
    uint64_t base;
    /* ELF alone: the program headers of the module in memory, 56 bytes
       each, and their count. */
    const void *headers;
    size_t header_count;
    /* true when the layer found the notice `anti_licenses` of the module,
       which notice then gives in memory or NULL for none. false leaves it
       to the reader of the format: Mach-O on macOS, and ELF on Linux for
       every module but the runtime's own. */
    bool notice_read;
    const char *notice;
    /* The path of the file in UTF-8, empty when it does not fit. */
    char path[ANTI_RT_PATH_ROOM];
};

/* Fill out with the module that holds the byte at address. false for an
   address outside every module. */
bool anti_rt_module_at(uint64_t address, struct anti_rt_module *out);

/* The room of the name of a symbol the debugger library gives, its NUL
   included. */
#define ANTI_RT_SYMBOL_ROOM 513

/* What the debugger library of the system knows of the code at an
   address. A text is empty and line 0 where it knows nothing. */
struct anti_rt_debug_answer {
    char function[ANTI_RT_SYMBOL_ROOM];   /* as the symbol table spells it */
    char file[ANTI_RT_PATH_ROOM];
    int64_t line;
};

/* Ask the debugger library of the system for the code at address in the
   module at base: DbgHelp on Windows, which reads the PDB of every
   Windows link. macOS and Linux have none, and the answer is empty. */
void anti_rt_debug_lookup(uint64_t address, uint64_t base,
                          struct anti_rt_debug_answer *out);

/* DESIGN: a signal handler runs on a stack the program knows nothing
   about, so it does the least it can. On macOS and Linux it writes one
   byte into a pipe of the runtime's own, and a thread of the runtime
   reads the pipe and calls deliver. That self-pipe is what makes the
   function of the program an ordinary function. Anything else would run
   Anti code inside a handler, where a call of malloc or of the runtime
   is undefined. Windows has no signals of that kind. Its console control
   handler runs on a thread of its own, and the C runtime calls a handler
   of raise on the thread that raised. Both call deliver directly.
   deliver returns 1 when the program has a function for the signal. */

/* Start the route of signals to deliver, once. The caller holds
   ANTI_RT_LOCK_SIGNALS. Returns 0, or -1 when the system gave no pipe or
   no thread, and a later call then tries again. */
int anti_rt_signal_route(int (*deliver)(int64_t sig));

/* Send the signal sig through the route from now on. */
void anti_rt_signal_catch(int64_t sig);

/* Fill the count bytes at out from the random source of the system:
   arc4random on macOS, getrandom on Linux and rand_s on Windows. Returns
   0, or -1 when the system gave fewer bytes. */
int anti_rt_entropy(void *out, size_t count);

/* DESIGN: a Sleep of Windows takes 32 bits of milliseconds, and the
   largest of them means to wait for ever. A long wait is therefore a
   loop of steps of at most a day. A step rounds up to the whole
   millisecond, so that a wait is never shorter than asked, as nanosleep
   guarantees on the other systems. */
#define ANTI_RT_SLEEP_STEP_MAX 86400000

/* The milliseconds of the next Sleep of a wait with nanoseconds left,
   which is above 0. */
static inline uint32_t anti_rt_sleep_step(int64_t nanoseconds)
{
    int64_t milliseconds = nanoseconds / 1000000 +
                           (nanoseconds % 1000000 != 0 ? 1 : 0);

    return milliseconds > ANTI_RT_SLEEP_STEP_MAX
               ? (uint32_t)ANTI_RT_SLEEP_STEP_MAX
               : (uint32_t)milliseconds;
}

#endif
