/* The platform layer of the runtime, src/rt/platform.h, on the host. It
   checks the steps of a long sleep and the environment in full length
   and in UTF-8. It also checks the path rules every system shares, the
   forms of a Windows path, the clocks, the entropy and a library opened
   from a path outside ASCII. Then the locks, the condition variables,
   the threads and the word of a Mutex across threads it starts, and the
   image an address lies in. Last the files and the directories under a
   name outside ASCII, the walk of the stack, the module of an address and
   the debugger library of the system. Then the memory at an alignment,
   the arguments and the environment of the process and what the
   processor reports. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/cpu_level.h"
#include "../../src/rt/platform.h"
#include "../../src/rt/rt.h"
#include "../../src/rt/std.h"
#include "check.h"
#include "path_rules.h"

#if defined(_WIN32)
#include <windows.h>
#endif

int check_failures;

#define PLUGIN_UTF8 PLUGIN_DIR "/platform-\xc3\xbc/" PLUGIN_NAME
#define FILE_UTF8 PLUGIN_DIR "/platform-\xc3\xbc/file-\xc3\xa9.txt"
#define MOVED_UTF8 PLUGIN_DIR "/platform-\xc3\xbc/moved-\xc3\xa9.txt"

/* The nanoseconds a wait of total sleeps in its steps, which is at least
   total and less than a millisecond more. Each step waits at most a day
   and none is INFINITE. The count of steps goes to steps. */
static int64_t slept(int64_t total, int64_t *steps)
{
    int64_t left = total;
    int64_t done = 0;

    *steps = 0;
    while (left > 0) {
        uint32_t step = anti_rt_sleep_step(left);

        CHECK(step > 0);
        CHECK(step <= ANTI_RT_SLEEP_STEP_MAX);
        CHECK(step != 0xFFFFFFFFu);
        left -= (int64_t)step * 1000000;
        done += (int64_t)step * 1000000;
        (*steps)++;
    }
    return done;
}

static void sleep_steps(void)
{
    int64_t steps;
    /* 2^32 milliseconds, which a Sleep of 32 bits takes as 0. */
    int64_t wrapped = (int64_t)4294967296 * 1000000;
    /* 4294967295 milliseconds, which a Sleep takes as INFINITE. */
    int64_t infinite = (int64_t)4294967295 * 1000000;

    CHECK(anti_rt_sleep_step(1) == 1);
    CHECK(anti_rt_sleep_step(999999) == 1);
    CHECK(anti_rt_sleep_step(1000000) == 1);
    CHECK(anti_rt_sleep_step(1000001) == 2);
    CHECK(anti_rt_sleep_step(wrapped) == ANTI_RT_SLEEP_STEP_MAX);
    CHECK(anti_rt_sleep_step(INT64_MAX) == ANTI_RT_SLEEP_STEP_MAX);
    CHECK(slept(1, &steps) == 1000000 && steps == 1);
    CHECK(slept(wrapped, &steps) == wrapped);
    CHECK(steps == 50);
    CHECK(slept(infinite, &steps) == infinite);
    CHECK(slept(infinite + 1, &steps) == infinite + 1000000);
}

/* Set the variable name to value on the host, in UTF-8 bytes. */
static void put(const char *name, const char *value)
{
#if defined(_WIN32)
    wchar_t wide_name[64];
    wchar_t *wide = malloc((strlen(value) + 1) * sizeof *wide);

    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 64);
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, (int)strlen(value) + 1);
    SetEnvironmentVariableW(wide_name, wide);
    free(wide);
#else
    setenv(name, value, 1);
#endif
}

static void environment(void)
{
    enum { LENGTH = 5000 };
    char *long_value = malloc(LENGTH + 1);
    char *value = NULL;

    /* A value past the 1024 bytes of the ANSI reader of old. */
    memset(long_value, 'a', LENGTH);
    long_value[LENGTH] = '\0';
    put("ANTI_RT_PLATFORM_TEST", long_value);
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_TEST", &value) == 0);
    CHECK(value != NULL && strcmp(value, long_value) == 0);
    free(value);
    free(long_value);

    /* A value outside ASCII, which the ANSI code page does not hold. */
    put("ANTI_RT_PLATFORM_TEST", "/tmp/\xc3\xbc\xe2\x82\xac/anti.toml");
    value = NULL;
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_TEST", &value) == 0);
    CHECK(value != NULL &&
          strcmp(value, "/tmp/\xc3\xbc\xe2\x82\xac/anti.toml") == 0);
    free(value);

    value = NULL;
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_UNSET", &value) == 0);
    CHECK(value == NULL);
}

/* Every case of path_rules.h, which the platform layer of the tools
   answers the same way. */
static void paths(void)
{
    size_t i;

    for (i = 0; i < PATH_RULE_COUNT; i++) {
        const struct path_rule *r = &path_rules[i];
        const char *last = anti_rt_path_last_separator(r->path);

        CHECK(anti_rt_path_is_absolute(r->path) == r->absolute);
        CHECK(r->separator < 0 ? last == NULL
                               : last == r->path + r->separator);
    }
}

/* The monotonic clock never goes back, and the wall clock stands after
   2026-01-01. A sleep of a millisecond passes at least one on the
   monotonic clock. The entropy fills every byte it is asked for, and two
   draws of 32 bytes differ. */
static void clocks_and_entropy(void)
{
    int64_t before = anti_rt_monotonic();
    int64_t after;
    unsigned char one[32];
    unsigned char two[32];

    anti_rt_sleep(1000000);
    after = anti_rt_monotonic();
    CHECK(after - before >= 1000000);
    CHECK(anti_rt_wall() > (int64_t)1767225600 * 1000000000);
    memset(one, 0, sizeof one);
    memset(two, 0, sizeof two);
    CHECK(anti_rt_entropy(one, sizeof one) == 0);
    CHECK(anti_rt_entropy(two, 5) == 0);
    CHECK(anti_rt_entropy(two + 5, sizeof two - 5) == 0);
    CHECK(memcmp(one, two, sizeof one) != 0);
}

/* The library of an empty plugin table, copied by the build into a
   directory whose name is outside ASCII. */
static void library_outside_ascii(void)
{
    char text[64];
    void *handle = anti_rt_library_open(PLUGIN_UTF8);

    if (handle == NULL) {
        fprintf(stderr, "%s: %s\n", PLUGIN_UTF8,
                anti_rt_library_error(text, sizeof text));
    }
    CHECK(handle != NULL);
    if (handle != NULL) {
        CHECK(anti_rt_library_symbol(handle, "anti_rt_provides") != NULL);
        anti_rt_library_close(handle);
    }
    CHECK(anti_rt_library_open(PLUGIN_UTF8 ".missing") == NULL);
    CHECK(strlen(anti_rt_library_error(text, sizeof text)) > 0);
}

/* The state the threads below share. stage moves 0, 1, 2 under the
   lock of the monitor, and each side waits on its own condition. */
enum { TO_THREAD, TO_MAIN };
enum { ROUNDS = 20000 };

static struct anti_rt_monitor *shared;
static int stage;
static struct anti_rt_word word;
static int64_t counted;

static void counter(void)
{
    int64_t i;

    for (i = 0; i < ROUNDS; i++) {
        anti_rt_word_lock(&word);
        counted++;
        anti_rt_word_unlock(&word);
    }
    anti_rt_monitor_hold(shared);
    stage++;
    anti_rt_monitor_wake_all(shared, TO_MAIN);
    anti_rt_monitor_release(shared);
}

static void answer(void)
{
    anti_rt_monitor_hold(shared);
    while (stage == 0) {
        anti_rt_monitor_wait(shared, TO_THREAD);
    }
    stage = 2;
    anti_rt_monitor_wake_one(shared, TO_MAIN);
    anti_rt_monitor_release(shared);
}

/* A thread waits on one condition until main wakes it, and main waits on
   the other until it answers. Two threads count under the word of a
   Mutex beside main, and none of the counts is lost. A named lock holds
   and gives back. */
static void locks_and_threads(void)
{
    int64_t i;

    CHECK(anti_rt_processors() >= 1);
#if defined(__APPLE__)
    CHECK(anti_rt_thread_block() != NULL);
#else
    CHECK(anti_rt_thread_block() == NULL);
#endif
    shared = anti_rt_monitor_new();
    CHECK(shared != NULL);
    if (shared == NULL) {
        return;
    }
    CHECK(anti_rt_thread_start(answer) == 0);
    anti_rt_monitor_hold(shared);
    stage = 1;
    anti_rt_monitor_wake_all(shared, TO_THREAD);
    while (stage != 2) {
        anti_rt_monitor_wait(shared, TO_MAIN);
    }
    stage = 0;
    anti_rt_monitor_release(shared);

    CHECK(anti_rt_thread_start(counter) == 0);
    CHECK(anti_rt_thread_start(counter) == 0);
    for (i = 0; i < ROUNDS; i++) {
        anti_rt_word_lock(&word);
        counted++;
        anti_rt_word_unlock(&word);
    }
    anti_rt_monitor_hold(shared);
    while (stage != 2) {
        anti_rt_monitor_wait(shared, TO_MAIN);
    }
    anti_rt_monitor_release(shared);
    anti_rt_word_lock(&word);
    CHECK(counted == 3 * ROUNDS);
    anti_rt_word_unlock(&word);
    anti_rt_monitor_free(shared);

    for (i = 0; i < ANTI_RT_LOCK_COUNT; i++) {
        anti_rt_lock_hold((enum anti_rt_lock)i);
        anti_rt_lock_release((enum anti_rt_lock)i);
    }
}

/* Two variables of this program lie in one image, and a symbol of a
   library lies in another. */
static void images(void)
{
    void *handle = anti_rt_library_open(PLUGIN_UTF8);
    const void *program = anti_rt_library_image((const void *)&shared);

    CHECK(program != NULL);
    CHECK(anti_rt_library_image((const void *)&stage) == program);
    CHECK(handle != NULL);
    if (handle != NULL) {
        const void *symbol =
            anti_rt_library_symbol(handle, "anti_rt_provides");
        const void *library = anti_rt_library_image(symbol);
        CHECK(library != NULL);
        CHECK(library != program);
        anti_rt_library_close(handle);
    }
}

/* What a listing saw: the two names the test looks for, and the count
   of entries after which it stops. */
struct seen {
    int file;
    int plugin;
    int stop_after;
    int entries;
};

static int saw(void *context, const unsigned char *name, size_t length)
{
    struct seen *s = context;

    s->entries++;
    if (length == strlen("file-\xc3\xa9.txt") &&
        memcmp(name, "file-\xc3\xa9.txt", length) == 0) {
        s->file++;
    }
    if (length == strlen(PLUGIN_NAME) &&
        memcmp(name, PLUGIN_NAME, length) == 0) {
        s->plugin++;
    }
    if (s->entries == s->stop_after) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

/* A file under a name outside ASCII is written, measured, listed,
   renamed and removed, and each failure reports through errno. */
static void files(void)
{
    struct seen s = {0, 0, 0, 0};
    FILE *f = anti_rt_file_open(FILE_UTF8, 1);
    FILE *dir;

    CHECK(f != NULL);
    if (f == NULL) {
        return;
    }
    CHECK(fwrite("abcdef", 1, 6, f) == 6);
    CHECK(anti_rt_file_tell(f) == 6);
    CHECK(anti_rt_file_seek(f, 2, SEEK_SET) == 0);
    CHECK(anti_rt_file_tell(f) == 2);
    CHECK(anti_rt_file_seek(f, 0, SEEK_END) == 0);
    CHECK(anti_rt_file_tell(f) == 6);
    CHECK(anti_rt_file_is_directory(f) == 0);
    CHECK(fclose(f) == 0);

    CHECK(anti_rt_directory_list(PLUGIN_DIR "/platform-\xc3\xbc", saw, &s) ==
          0);
    CHECK(s.file == 1);
    CHECK(s.plugin == 1);
    s.entries = 0;
    s.stop_after = 1;
    errno = 0;
    CHECK(anti_rt_directory_list(PLUGIN_DIR "/platform-\xc3\xbc", saw, &s) ==
          -1);
    CHECK(errno == ENOMEM);
    CHECK(s.entries == 1);
    errno = 0;
    CHECK(anti_rt_directory_list(PLUGIN_DIR "/platform-missing", saw, &s) ==
          -1);
    CHECK(errno == ENOENT);
    errno = 0;
    CHECK(anti_rt_directory_list("", saw, &s) == -1);
    CHECK(errno == ENOENT);

    /* POSIX opens a directory as a stream, and Windows opens none. */
    dir = anti_rt_file_open(PLUGIN_DIR, 0);
    if (dir != NULL) {
        CHECK(anti_rt_is_windows() == 0);
        CHECK(anti_rt_file_is_directory(dir) == 1);
        fclose(dir);
    }

    CHECK(anti_rt_file_rename(FILE_UTF8, MOVED_UTF8) == 0);
    errno = 0;
    CHECK(anti_rt_file_open(FILE_UTF8, 0) == NULL);
    CHECK(errno == ENOENT);
    f = anti_rt_file_open(MOVED_UTF8, 0);
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(anti_rt_file_seek(f, 0, SEEK_END) == 0);
        CHECK(anti_rt_file_tell(f) == 6);
        fclose(f);
    }
    CHECK(anti_rt_file_remove(MOVED_UTF8) == 0);
    errno = 0;
    CHECK(anti_rt_file_remove(MOVED_UTF8) == -1);
    CHECK(errno == ENOENT);
    errno = 0;
    CHECK(anti_rt_file_rename(FILE_UTF8, MOVED_UTF8) == -1);
    CHECK(errno == ENOENT);
    if (anti_rt_is_windows() != 0) {
        /* Windows reads a path as UTF-16, and bytes that are no UTF-8
           name no file. */
        errno = 0;
        CHECK(anti_rt_file_open("\xff.txt", 1) == NULL);
        CHECK(errno == EINVAL);
    }
}

/* The walk gives the return address into this function first. The module
   of a function of the program is the program, and of a function of a
   library that library. The debugger library knows nothing on the
   systems that have none. */
static void traces(void)
{
    struct anti_rt_module program;
    struct anti_rt_module library;
    struct anti_rt_debug_answer answer;
    const void *image = anti_rt_library_image((const void *)&shared);
    uint64_t frames[8];
    uint64_t here = (uint64_t)(uintptr_t)traces;
    int64_t n = anti_rt_trace_walk(frames, 8, 0);
    void *handle;

    CHECK(n >= 1);
    if (n >= 1) {
        CHECK(anti_rt_library_image((const void *)(uintptr_t)(frames[0] -
                                                              1)) == image);
    }
    CHECK(anti_rt_trace_walk(frames, 0, 0) == 0);
    CHECK(anti_rt_trace_walk(frames, 8, 1) == n - 1 || n == 8);

    CHECK(!anti_rt_module_at(0, &program));
    CHECK(anti_rt_module_at(here, &program));
    CHECK(strstr(program.path, "rt_platform_tests") != NULL);
    CHECK(program.base != 0 || program.format == ANTI_RT_IMAGE_ELF);
#if defined(__APPLE__)
    CHECK(program.format == ANTI_RT_IMAGE_MACHO);
    CHECK(program.base == (uint64_t)(uintptr_t)image);
    CHECK(!program.notice_read);
#elif defined(_WIN32)
    CHECK(program.format == ANTI_RT_IMAGE_PE);
    CHECK(program.base == (uint64_t)(uintptr_t)image);
    /* The program has no notice, and the linker takes the empty default
       of the platform file. */
    CHECK(program.notice_read);
    CHECK(program.notice != NULL && program.notice[0] == '\0');
#else
    CHECK(program.format == ANTI_RT_IMAGE_ELF);
    CHECK(program.headers != NULL && program.header_count > 0);
    CHECK(program.notice_read);
    CHECK(program.notice == NULL);
#endif

    handle = anti_rt_library_open(PLUGIN_UTF8);
    CHECK(handle != NULL);
    if (handle != NULL) {
        uint64_t symbol = (uint64_t)(uintptr_t)anti_rt_library_symbol(
            handle, "anti_rt_provides");
        CHECK(anti_rt_module_at(symbol, &library));
        CHECK(strstr(library.path, PLUGIN_NAME) != NULL);
        CHECK(strstr(library.path, "platform-\xc3\xbc") != NULL);
        CHECK(library.base != program.base);
        CHECK(library.format == program.format);
        CHECK(library.notice_read == (library.format == ANTI_RT_IMAGE_PE));
        CHECK(library.notice == NULL);
        anti_rt_library_close(handle);
    }

    anti_rt_debug_lookup(here, program.base, &answer);
    if (program.format != ANTI_RT_IMAGE_PE) {
        CHECK(answer.function[0] == '\0');
        CHECK(answer.file[0] == '\0');
        CHECK(answer.line == 0);
    }
}

/* Memory at every alignment from 1 to 4096 lies on it, holds what is
   written to it and goes back. The runtime may mark it as kept. */
static void aligned_memory(void)
{
    size_t align;

    for (align = 1; align <= 4096; align *= 2) {
        unsigned char *p = anti_rt_aligned_alloc(100, align);
        CHECK(p != NULL);
        if (p == NULL) {
            continue;
        }
        CHECK((uintptr_t)p % align == 0);
        memset(p, 0xA5, 100);
        CHECK(p[0] == 0xA5 && p[99] == 0xA5);
        anti_rt_memory_kept(p);
        anti_rt_aligned_free(p);
    }
    anti_rt_aligned_free(NULL);
}

/* A list of the layer and its count strings. A program keeps them until
   exit, and the test gives them back. */
static void free_list(char **list, size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        free(list[i]);
    }
    free(list);
}

/* The arguments of the process as UTF-8, which rt_platform passes as
   `one`, `two words` and u with diaeresis. macOS and Linux take them from
   argv and repair a byte that is no UTF-8. Windows reads them from the
   command line in UTF-16. */
static void process_arguments(int argc, char **argv)
{
    size_t count = 0;
    char **list = anti_rt_process_arguments(argc, argv, &count);

    CHECK(list != NULL);
    if (list == NULL) {
        return;
    }
    CHECK(count == 4);
    CHECK(count == (size_t)argc);
    if (count == 4) {
        CHECK(list[0][0] != '\0');
        CHECK_STR(list[1], "one");
        CHECK_STR(list[2], "two words");
        CHECK_STR(list[3], "\xc3\xbc");
    }
    free_list(list, count);
#if !defined(_WIN32)
    {
        char name[] = "program";
        char broken[] = "a\xff" "b";
        char *given[3] = {name, broken, NULL};

        count = 0;
        list = anti_rt_process_arguments(2, given, &count);
        CHECK(list != NULL && count == 2);
        if (list != NULL && count == 2) {
            CHECK_STR(list[0], "program");
            CHECK_STR(list[1], "a\xef\xbf\xbd" "b");
        }
        free_list(list, count);
    }
#endif
}

/* The environment of the process holds a variable set by the program,
   with its value outside ASCII, as name=value in UTF-8. */
static void process_environment(void)
{
    size_t count = 0;
    size_t i;
    int found = 0;
    char **list;

    put("ANTI_RT_PLATFORM_START", "\xc3\xbc\xe2\x82\xac");
    list = anti_rt_process_environment(&count);
    CHECK(list != NULL && count > 0);
    for (i = 0; list != NULL && i < count; i++) {
        if (strcmp(list[i], "ANTI_RT_PLATFORM_START=\xc3\xbc\xe2\x82\xac") ==
            0) {
            found = 1;
        }
    }
    CHECK(found);
    free_list(list, count);
}

/* What the processor reports. CPUID of x86_64 gives leaf 1 at least, and
   XCR0 has the x87 state set whenever the system turned XSAVE on. ARM64
   gives one level of the table, armv8.5 on Apple Silicon. */
static void processor(void)
{
#if defined(ANTI_RT_X86_64)
    uint32_t zero[4] = {0, 0, 0, 0};
    uint32_t one[4] = {0, 0, 0, 0};

    anti_rt_cpuid(0, 0, zero);
    CHECK(zero[0] >= 1);
    CHECK(zero[1] != 0);
    anti_rt_cpuid(1, 0, one);
    if ((one[2] & 1u << 27) != 0) {
        CHECK((anti_rt_xcr0() & 1u) != 0);
    }
#elif defined(ANTI_RT_ARM64)
    int32_t level = anti_rt_arm64_level();

    CHECK(level == ANTI_CPU_ARMV8_0 || level == ANTI_CPU_ARMV8_2 ||
          level == ANTI_CPU_ARMV8_5);
#if defined(__APPLE__)
    CHECK(level == ANTI_CPU_ARMV8_5);
#endif
#else
#error "the platform layer names no processor of the host"
#endif
}

int main(int argc, char **argv)
{
    sleep_steps();
    environment();
    paths();
    clocks_and_entropy();
    library_outside_ascii();
    locks_and_threads();
    images();
    files();
    traces();
    aligned_memory();
    process_arguments(argc, argv);
    process_environment();
    processor();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
