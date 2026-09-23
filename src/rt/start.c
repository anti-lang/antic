/* The C entry point of every Anti program. It converts the command-line
   arguments and the environment to []str. It calls the program's main
   through anti.rt.main, the runtime entry symbol that antic defines, and
   returns the result as the process exit code. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "cpu_level.h"
#include "rt.h"
#include "std.h"
#include "utf.h"

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

/* The layouts of Anti's str and []str. */
struct anti_str {
    const unsigned char *ptr;
    int64_t len;
};

struct anti_slice {
    struct anti_str *ptr;
    int64_t len;
};

/* DESIGN: the entry always passes args and env. All six calling
   conventions pass both slices, or pointers to them, in registers. A main
   with fewer parameters ignores those registers. */

/* A symbol with a dot is not a C identifier, so the declaration names it
   with an assembler label. Mach-O adds '_' to C symbols, ELF does not.
   The COFF symbol is an identifier, and MSVC has no assembler labels. */
#if defined(_WIN32)
extern int64_t _A4anti2rt_main(struct anti_slice args, struct anti_slice env);
#define anti_main _A4anti2rt_main
#else
#if defined(__APPLE__)
#define ANTI_ENTRY_SYMBOL "_anti.rt.main"
#else
#define ANTI_ENTRY_SYMBOL "anti.rt.main"
#endif
extern int64_t anti_main(struct anti_slice args, struct anti_slice env)
    __asm__(ANTI_ENTRY_SYMBOL);
#endif

/* DESIGN: the runtime takes every argument under the reserved prefix
   `--anti.` in one pass before main sees the list. It hands the rest on
   in their order. src/rt/conf.c holds the options and the keys they set. An
   option the runtime does not know ends the program before main, with
   the options it does know. */
static const char option_prefix[] = "--anti.";

/* Take the options of the runtime out of args, then read the file they
   or the environment name. */
static void runtime_options(struct anti_slice *args)
{
    int64_t kept = args->len > 0 ? 1 : 0;
    int64_t i;

    for (i = 1; i < args->len; i++) {
        const char *arg = (const char *)args->ptr[i].ptr;
        const char *value;
        int64_t n;
        if (strncmp(arg, option_prefix, sizeof option_prefix - 1) != 0) {
            args->ptr[kept++] = args->ptr[i];
            continue;
        }
        arg += sizeof option_prefix - 1;
        value = strchr(arg, '=');
        n = value != NULL ? (int64_t)(value - arg) : (int64_t)strlen(arg);
        if (value != NULL) {
            value++;
        }
        anti_rt_conf_option(arg, n, value);
    }
    args->len = kept;
    anti_rt_conf_start();
}

static void *allocate(size_t size)
{
    void *p = malloc(size == 0 ? 1 : size);

    if (p == NULL) {
        anti_rt_fail_exit(70, "anti: out of memory at program start");
    }
    return p;
}

/* A str holding UTF-8 bytes, with a NUL after them. */
static struct anti_str make_str(unsigned char *bytes, size_t length)
{
    struct anti_str s;

    bytes[length] = 0;
    s.ptr = bytes;
    s.len = (int64_t)length;
    return s;
}

#if defined(_WIN32)
/* The UTF-16 units of s up to its 0 unit, as UTF-8. */
static struct anti_str from_utf16(const uint16_t *s)
{
    size_t n = 0;

    while (s[n] != 0) {
        n++;
    }
    unsigned char *bytes = allocate(3 * n + 1);
    return make_str(bytes, anti_utf16_to_utf8(s, n, bytes));
}

static uint16_t *copy_units(const wchar_t *s, size_t n)
{
    uint16_t *units = allocate((n + 1) * sizeof *units);
    size_t i;

    for (i = 0; i <= n; i++) {
        units[i] = (uint16_t)s[i];
    }
    return units;
}

static struct anti_slice arguments(void)
{
    const wchar_t *line = GetCommandLineW();
    size_t n = wcslen(line);
    uint16_t *units = copy_units(line, n);
    uint16_t *split = allocate((2 * n + 2) * sizeof *split);
    struct anti_slice args;
    size_t count = anti_split_command_line(units, split);
    const uint16_t *p = split;
    size_t i;

    args.ptr = allocate(count * sizeof *args.ptr);
    args.len = (int64_t)count;
    for (i = 0; i < count; i++) {
        args.ptr[i] = from_utf16(p);
        while (*p != 0) {
            p++;
        }
        p++;
    }
    free(units);
    free(split);
    return args;
}

static struct anti_slice environment(void)
{
    wchar_t *block = GetEnvironmentStringsW();
    const wchar_t *p = block;
    struct anti_slice env;
    size_t count = 0;
    size_t i;

    for (p = block; p != NULL && *p != 0; p += wcslen(p) + 1) {
        count++;
    }
    env.ptr = allocate(count * sizeof *env.ptr);
    env.len = (int64_t)count;
    for (i = 0, p = block; i < count; i++, p += wcslen(p) + 1) {
        size_t n = wcslen(p);
        uint16_t *units = copy_units(p, n);
        env.ptr[i] = from_utf16(units);
        free(units);
    }
    if (block != NULL) {
        FreeEnvironmentStringsW(block);
    }
    return env;
}

int main(void)
{
    struct anti_slice args;
    struct anti_slice env;

    /* DESIGN: the raw-bytes rule. An Anti program writes the bytes it was
       given, and the runtime never translates them. The C streams of
       Windows start in text mode, which writes CRLF for each LF. */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    anti_cpu_check();
    anti_rt_init();
    args = arguments();
    runtime_options(&args);
    env = environment();
    return (int)anti_main(args, env);
}
#else
extern char **environ;

/* Byte strings from the system as str values of valid UTF-8. */
static struct anti_slice strings(char **list, size_t count)
{
    struct anti_slice slice;
    size_t i;

    slice.ptr = allocate(count * sizeof *slice.ptr);
    slice.len = (int64_t)count;
    for (i = 0; i < count; i++) {
        size_t n = strlen(list[i]);
        unsigned char *bytes = allocate(3 * n + 1);
        slice.ptr[i] = make_str(
            bytes, anti_utf8_repair((const unsigned char *)list[i], n, bytes));
    }
    return slice;
}

int main(int argc, char **argv)
{
    struct anti_slice args;
    size_t count = 0;

    anti_cpu_check();
    anti_rt_init();
    args = strings(argv, (size_t)argc);
    runtime_options(&args);
    while (environ != NULL && environ[count] != NULL) {
        count++;
    }
    return (int)anti_main(args, strings(environ, count));
}
#endif
