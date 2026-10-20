/* Fault injection for the tests: the n-th call of a kind fails with an
   error that the variable ANTI_FAULT names. fault.h states the form of
   the variable and the seam. Only a build of ANTIC_FAULTS compiles this
   file, into antic_faults, anti_faults and the runtime of
   runtime-faults/, see tools/faults.cmake. */

#include "fault.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The names of the kinds in the variable, in the order of the enum. */
static const char *const kind_names[ANTI_RT_FAULT_KINDS] = {
    "alloc", "open", "read",  "write", "close", "remove", "rename", "mkdir",
    "stat",  "seek", "run",   "wait",  "thread", "lock",  "sleep",
};

/* The errno values a test may name. Each stands in the errno.h of every
   system Anti runs on. */
static const struct {
    const char *name;
    int code;
} errors[] = {
    {"EPERM", EPERM},     {"ENOENT", ENOENT}, {"EINTR", EINTR},
    {"EIO", EIO},         {"EBADF", EBADF},   {"ECHILD", ECHILD},
    {"EAGAIN", EAGAIN},   {"ENOMEM", ENOMEM}, {"EACCES", EACCES},
    {"EBUSY", EBUSY},     {"EEXIST", EEXIST}, {"EXDEV", EXDEV},
    {"ENOTDIR", ENOTDIR}, {"EISDIR", EISDIR}, {"EINVAL", EINVAL},
    {"ENFILE", ENFILE},   {"EMFILE", EMFILE}, {"EFBIG", EFBIG},
    {"ENOSPC", ENOSPC},   {"EROFS", EROFS},   {"EPIPE", EPIPE},
    {"ENAMETOOLONG", ENAMETOOLONG},           {"ELOOP", ELOOP},
};

/* The entries a variable may hold. */
#define FAULTS_MAX 8

struct fault {
    enum anti_rt_fault_kind kind;
    int64_t at;
    int code;
};

/* DESIGN: the state of the seam is global, since a count runs over the
   whole process. Every thread of a program may reach a seam, so each
   value is read and written with the atomic builtins. The variable is
   read once, by the first call: state goes from 0 to 1 while that call
   reads it and to 2 when the entries stand, and a call that finds 1
   waits for 2. The entries are written before state becomes 2 and never
   after. */
static int state;
static struct fault faults[FAULTS_MAX];
static int fault_count;
static int64_t counts[ANTI_RT_FAULT_KINDS];

/* The streams whose read or write failed, until fclose. */
#define STREAMS_MAX 8
static FILE *failed_streams[STREAMS_MAX];

/* DESIGN: a malformed variable is a mistake of the test that sets it,
   and a seam that went on without it would let that test pass for the
   wrong reason. So the process ends, with a status no other path of
   antic, anti or a program gives. */
static _Noreturn void malformed(const char *text, const char *why)
{
    fprintf(stderr, "ANTI_FAULT=%s: %s\n", text, why);
    exit(125);
}

/* The kind named by the len bytes at name, or ANTI_RT_FAULT_KINDS. */
static enum anti_rt_fault_kind kind_named(const char *name, size_t len)
{
    int i;

    for (i = 0; i < ANTI_RT_FAULT_KINDS; i++) {
        if (strlen(kind_names[i]) == len &&
            memcmp(kind_names[i], name, len) == 0) {
            return (enum anti_rt_fault_kind)i;
        }
    }
    return ANTI_RT_FAULT_KINDS;
}

/* The errno value named by the len bytes at name, or 0. */
static int code_named(const char *name, size_t len)
{
    size_t i;

    for (i = 0; i < sizeof errors / sizeof errors[0]; i++) {
        if (strlen(errors[i].name) == len &&
            memcmp(errors[i].name, name, len) == 0) {
            return errors[i].code;
        }
    }
    return 0;
}

/* The count of the len digits at text, or 0 for none, another byte or a
   count past 10^9. */
static int64_t count_of(const char *text, size_t len)
{
    int64_t n = 0;
    size_t i;

    if (len == 0 || len > 9) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        n = n * 10 + (text[i] - '0');
    }
    return n;
}

/* Read the entries of the variable into faults. */
static void read_variable(void)
{
    const char *text = getenv("ANTI_FAULT");
    const char *at = text;

    if (text == NULL || text[0] == '\0') {
        return;
    }
    while (*at != '\0') {
        const char *end = strchr(at, ',');
        const char *first;
        const char *second;
        size_t len;
        struct fault f;

        if (end == NULL) {
            end = at + strlen(at);
        }
        len = (size_t)(end - at);
        first = memchr(at, ':', len);
        second = first != NULL ? memchr(first + 1, ':', (size_t)(end - first - 1))
                               : NULL;
        if (second == NULL) {
            malformed(text, "an entry is not <kind>:<n>:<error>");
        }
        f.kind = kind_named(at, (size_t)(first - at));
        f.at = count_of(first + 1, (size_t)(second - first - 1));
        f.code = code_named(second + 1, (size_t)(end - second - 1));
        if (f.kind == ANTI_RT_FAULT_KINDS) {
            malformed(text, "an entry names no kind of fault.h");
        }
        if (f.at == 0) {
            malformed(text, "an entry counts from 1");
        }
        if (f.code == 0) {
            malformed(text, "an entry names no error of fault.c");
        }
        if (fault_count == FAULTS_MAX) {
            malformed(text, "more than 8 entries");
        }
        faults[fault_count++] = f;
        at = *end == ',' ? end + 1 : end;
    }
}

/* Read the variable on the first call, and wait for that read on any
   other call that comes before it ends. */
static void ready(void)
{
    int expected = 0;

    if (__atomic_load_n(&state, __ATOMIC_ACQUIRE) == 2) {
        return;
    }
    if (__atomic_compare_exchange_n(&state, &expected, 1, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        read_variable();
        __atomic_store_n(&state, 2, __ATOMIC_RELEASE);
        return;
    }
    while (__atomic_load_n(&state, __ATOMIC_ACQUIRE) != 2) {
    }
}

bool anti_rt_fault(enum anti_rt_fault_kind kind)
{
    int64_t n;
    int i;

    ready();
    if (fault_count == 0) {
        return false;
    }
    n = __atomic_add_fetch(&counts[kind], 1, __ATOMIC_RELAXED);
    for (i = 0; i < fault_count; i++) {
        if (faults[i].kind == kind && faults[i].at == n) {
            errno = faults[i].code;
            return true;
        }
    }
    return false;
}

/* Mark f as a stream whose read or write failed. A table that is full
   leaves the mark out, and ferror then reads the stream alone. */
static void mark(FILE *f)
{
    int i;

    for (i = 0; i < STREAMS_MAX; i++) {
        FILE *empty = NULL;
        if (__atomic_compare_exchange_n(&failed_streams[i], &empty, f, false,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            return;
        }
    }
}

/* Whether f is marked, and with forget its mark taken away. */
static bool marked(FILE *f, bool forget)
{
    int i;

    for (i = 0; i < STREAMS_MAX; i++) {
        FILE *held = f;
        if (forget) {
            if (__atomic_compare_exchange_n(&failed_streams[i], &held, NULL,
                                            false, __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE)) {
                return true;
            }
        } else if (__atomic_load_n(&failed_streams[i], __ATOMIC_ACQUIRE) ==
                   f) {
            return true;
        }
    }
    return false;
}

size_t anti_rt_fault_fread(void *p, size_t size, size_t n, FILE *f)
{
    if (anti_rt_fault(ANTI_RT_FAULT_READ)) {
        mark(f);
        return 0;
    }
    return fread(p, size, n, f);
}

size_t anti_rt_fault_fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    if (anti_rt_fault(ANTI_RT_FAULT_WRITE)) {
        mark(f);
        return 0;
    }
    return fwrite(p, size, n, f);
}

int anti_rt_fault_ferror(FILE *f)
{
    return ferror(f) || marked(f, false);
}

int anti_rt_fault_fflush(FILE *f)
{
    if (anti_rt_fault(ANTI_RT_FAULT_WRITE)) {
        if (f != NULL) {
            mark(f);
        }
        return EOF;
    }
    return fflush(f);
}

/* The stream is closed in either case, as fclose closes it when it
   fails, so no test leaks it. */
int anti_rt_fault_fclose(FILE *f)
{
    bool failed = anti_rt_fault(ANTI_RT_FAULT_CLOSE);
    int saved = errno;
    int closed;

    marked(f, true);
    closed = fclose(f);
    if (failed) {
        errno = saved;
        return EOF;
    }
    return closed;
}
