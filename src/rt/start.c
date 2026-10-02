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
#include "platform.h"
#include "rt.h"
#include "std.h"

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
   with fewer parameters ignores those registers. The symbol has a dot,
   which platform.h names with a label. */
extern int64_t anti_main(struct anti_slice args, struct anti_slice env)
    ANTI_RT_ENTRY_LABEL;

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

/* DESIGN: the arguments and the environment live until exit, and no
   pointer to them outlives main. The leak check of a program of
   --memory-checks would report them, so the start marks each block of
   both slices as kept. Before runtime_options drops the options of the
   runtime from the list, so their strings are marked as well. */
static void kept(struct anti_slice s)
{
    int64_t i;

    anti_rt_memory_kept(s.ptr);
    for (i = 0; i < s.len; i++) {
        anti_rt_memory_kept(s.ptr[i].ptr);
    }
}

/* The strings of the platform layer as a []str. The strings stay, and
   the list that held them goes back. */
static struct anti_slice slice_of(char **list, size_t count)
{
    struct anti_slice slice;
    size_t i;

    if (list == NULL) {
        anti_rt_fail_exit(70, "anti: out of memory at program start");
    }
    slice.ptr = allocate(count * sizeof *slice.ptr);
    slice.len = (int64_t)count;
    for (i = 0; i < count; i++) {
        slice.ptr[i].ptr = (const unsigned char *)list[i];
        slice.ptr[i].len = (int64_t)strlen(list[i]);
    }
    free(list);
    return slice;
}

/* DESIGN: the status of `main` is an int of 64 bits, and the C main
   gives an int of 32. Windows keeps 32 bits of an exit code, and Linux
   and macOS keep the low 8 of those, as "Program entry" in
   docs/decisions.md says. A status of more bits keeps its low 32, read
   as two's complement through arithmetic C defines, since a conversion
   of a value out of the range of int is left to the implementation. */
static int exit_status(int64_t status)
{
    uint32_t low = (uint32_t)status;

    if (low <= (uint32_t)INT32_MAX) {
        return (int)low;
    }
    return (int)(low - (uint32_t)INT32_MAX - 1u) + INT32_MIN;
}

int main(int argc, char **argv)
{
    struct anti_slice args;
    struct anti_slice env;
    size_t count = 0;
    char **list;

    anti_rt_streams_binary();
    anti_rt_cpu_check();
    anti_rt_init();
    list = anti_rt_process_arguments(argc, argv, &count);
    args = slice_of(list, count);
    kept(args);
    runtime_options(&args);
    list = anti_rt_process_environment(&count);
    env = slice_of(list, count);
    kept(env);
    return exit_status(anti_main(args, env));
}
