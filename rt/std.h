/* The C functions behind the modules anti.io, anti.text and anti.license of
   the standard library. */
#ifndef ANTI_STD_H
#define ANTI_STD_H

#include <stddef.h>
#include <stdint.h>

/* The layout of Anti's str, as a function returns it. */
struct anti_text {
    const unsigned char *ptr;
    int64_t len;
};

/* Write count bytes to standard output for stream 1, else standard error. */
void anti_rt_write(int32_t stream, const unsigned char *bytes, size_t count);

/* Flush both streams and end the process with status. */
void anti_rt_exit(int32_t status);

/* Print the text of a failed assertion to standard error and abort. The
   compiler built the text, so this adds only a newline. */
void anti_rt_assert_failed(const unsigned char *text, int64_t length);

/* Name the test that is running, which a failed assertion reports before
   its position. The runner of `anti test` is the one caller. */
void anti_rt_test_running(const unsigned char *name, int64_t length);

/* Which values a failed dev-mode check prints after its text. The
   compiler names the file, the line and the operation. The kind names
   the labels of the values. */
enum anti_check {
    ANTI_CHECK_BOUNDS,      /* An index and a length. */
    ANTI_CHECK_OVERFLOW,    /* The two operands of + - or *. */
    ANTI_CHECK_VALUE,       /* One signed value. */
    ANTI_CHECK_VALUE_U,     /* One unsigned value. */
    ANTI_CHECK_LEFT,        /* The signed left operand of / or %. */
    ANTI_CHECK_LEFT_U,      /* The unsigned left operand of / or %. */
    ANTI_CHECK_SHIFT        /* A shift count and the width of its type. */
};

/* Print the text of a failed check, the values the kind names, and
   abort. */
void anti_rt_check_failed(const unsigned char *text, int64_t length,
                          int32_t kind, int64_t a, int64_t b);

/* Print the name of the class a checked cast wanted and abort. The name
   is not a C string, so its length comes with it. */
void anti_rt_cast_failed(const unsigned char *name, int64_t length);

/* Print the name of the class an object with a zero table was taken for
   and abort. */
void anti_rt_table_unset(const unsigned char *name, int64_t length);

/* The str of a NUL-terminated C string, without the NUL. */
struct anti_text anti_rt_text_from_c(const unsigned char *bytes);
struct anti_text anti_rt_text_slice(const unsigned char *bytes, int64_t len);

/* The fields of `anti.text.Builder`, which the class declares in this
   order after the table pointer that every object begins with. The
   runtime appends to one when it serializes an object. */
struct anti_builder {
    const void *table;
    unsigned char *room;
    int64_t length;
    int64_t capacity;
};

void anti_rt_builder_append(struct anti_builder *b,
                            const unsigned char *bytes, int64_t len);
struct anti_text anti_rt_builder_text(const struct anti_builder *b);
void anti_rt_builder_clear(struct anti_builder *b);

/* Put count copies of byte at position at, and move what stood there
   after them. */
void anti_rt_builder_fill(struct anti_builder *b, int64_t at, int64_t byte,
                          int64_t count);

/* Hand the bytes to the caller, who frees them, and leave the builder
   empty. */
struct anti_text anti_rt_builder_take(struct anti_builder *b);

/* Append value, with precision digits after the point, or with the
   fewest that read back as the same value when precision is negative. A
   precision above 100000000 counts as 100000000. exponent writes the
   form `d.ddde+XX`, and single reads the digits back as a float. */
void anti_rt_builder_float(struct anti_builder *b, double value,
                           int64_t precision, int64_t exponent, int64_t single);

/* The lines of anti_licenses between its markers. */
struct anti_text anti_rt_license_text(void);

#endif

/* The value of errno, which is a macro and therefore not an Anti
   extern. */
int32_t anti_rt_errno(void);

/* The message of an errno value, as a NUL-terminated C string that the
   caller does not free. */
const unsigned char *anti_rt_errno_text(int32_t code);

/* Nanoseconds from a clock that never moves backwards, whose zero has no
   meaning of its own. */
int64_t anti_rt_monotonic(void);

/* Nanoseconds since 1970-01-01T00:00:00Z. */
int64_t anti_rt_wall(void);

/* Wait for the given nanoseconds, or return at once for a count that is
   not positive. */
void anti_rt_sleep(int64_t nanoseconds);

/* The C side of anti.fs, in rt/fs.c. A path is the bytes of a str and
   their count. Every function reports a failure through errno. */

/* The C stream of the file, opened for reading, or for writing when
   writing is not 0, or NULL. */
void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t writing);

/* The size in bytes of the file of the stream, or -1. */
int64_t anti_rt_fs_size(void *file);

/* The names of the directory, without `.` and `..`, in one block that the
   caller frees, with their number in count, or NULL. */
struct anti_text *anti_rt_fs_list(const unsigned char *path, int64_t len,
                                  int64_t *count);

/* Remove the file, and give 0 or -1. */
int32_t anti_rt_fs_remove(const unsigned char *path, int64_t len);

/* Give the file at from the path to, and give 0 or -1. */
int32_t anti_rt_fs_rename(const unsigned char *from, int64_t from_len,
                          const unsigned char *to, int64_t to_len);
