/* The C functions behind the modules anti.io, anti.text, anti.license,
   anti.mem and anti.fs of the standard library, with the layout of a
   Builder and the value of errno for an `extern fn`. */

#ifndef ANTI_STD_H
#define ANTI_STD_H

#include <stddef.h>
#include <stdint.h>

#include "str.h"

/* Write count bytes to standard output for stream 1, else standard error. */
void anti_rt_write(int32_t stream, const unsigned char *bytes, size_t count);

/* Flush both streams and end the process with status. */
_Noreturn void anti_rt_exit(int32_t status);

/* The str of a NUL-terminated C string, without the NUL. */
struct anti_text anti_rt_text_from_c(const unsigned char *bytes);

/* The str of a copy of a NUL-terminated C string that the runtime keeps
   until the program ends, one copy per distinct text. A SystemError keeps
   its message in one. In src/rt/text.c. */
struct anti_text anti_rt_text_kept(const unsigned char *bytes);

/* The str of the len bytes at bytes, which it neither copies nor checks.
   The str lives as long as the bytes do. */
struct anti_text anti_rt_text_slice(const unsigned char *bytes, int64_t len);

/* The offset of the first byte of the len bytes at bytes that starts no
   well-formed UTF-8 sequence, or -1 when they are valid UTF-8. */
int64_t anti_rt_text_invalid(const unsigned char *bytes, int64_t len);

/* 1 when the a_length bytes at a are the b_length bytes at b, else 0. */
int anti_rt_same_bytes(const unsigned char *a, int64_t a_length,
                       const unsigned char *b, int64_t b_length);

/* -1, 0 or 1 as the a_length bytes at a come before, equal or after the
   b_length bytes at b, compared as unsigned bytes. A run that begins the
   other comes first. `<` on two `str` values calls it, which gives the
   order of the code points for UTF-8. */
int64_t anti_rt_compare_bytes(const unsigned char *a, int64_t a_length,
                              const unsigned char *b, int64_t b_length);

/* The fields of `anti.text.Builder`, which the class declares in this
   order after the table pointer that every object begins with. The
   runtime appends to one when it serializes an object. Memory that runs
   out for its bytes ends the program through anti_rt_out_of_memory, and
   so does a count of bytes that no size holds. */
struct anti_builder {
    const void *table;
    unsigned char *room;
    int64_t length;
    int64_t capacity;
};

/* Append the len bytes at bytes, and keep a NUL after the last byte. A
   len of 0 or below appends nothing. */
void anti_rt_builder_append(struct anti_builder *b,
                            const unsigned char *bytes, int64_t len);

/* The bytes of b as a str, which b keeps. It is empty for a builder
   that holds no room yet, and stands until the next change of b. */
struct anti_text anti_rt_builder_text(const struct anti_builder *b);

/* Empty b and keep its room for the next appends. */
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

/* Read length bytes of text as a float with mantissa bits after its point
   and exponent bits of exponent. An f64 has 52 and 11, an f32 23 and 8,
   and an f16 10 and 5. The text is an optional sign, digits with an
   optional point, and an optional exponent after `e` or `E`. The value
   rounds to the nearest float, a tie to the even one, and one past the
   largest is infinite. Returns 1 and the bits of the float, or 0 for any
   other text and leaves bits alone. */
int anti_rt_read_float(const unsigned char *bytes, int64_t length,
                       int mantissa, int exponent, uint64_t *bits);

/* The lines of anti_licenses between its markers. */
struct anti_text anti_rt_license_text(void);

/* The version of the runtime, from the package line of the notice. It
   is empty when the program carries no notice. */
struct anti_text anti_rt_runtime_version(void);

/* The value of errno, which is a macro and therefore not an Anti
   extern. */
int32_t anti_rt_errno(void);

/* size bytes of the C library at a multiple of align, a power of two, or
   NULL. A size below zero and an align that is no power of two give
   NULL. In src/rt/mem.c. */
void *anti_rt_mem_alloc(int64_t size, int64_t align);

/* Release what anti_rt_mem_alloc gave. NULL does nothing. */
void anti_rt_mem_free(void *p);

/* End the program through the failure routine because size bytes found
   no memory. The compiler calls it where malloc gave NULL for one object
   of `alloc T { }`, `alloc T(args)` or a singleton. The runtime and the
   standard library call it wherever else memory runs out, by decision 4
   of "Decisions of 2026-10-11". In src/rt/mem.c. */
_Noreturn void anti_rt_out_of_memory(int64_t size);

/* Memory for count elements of size bytes each at align, a power of two,
   or NULL. Every byte is zero when zeroed is not 0. `free` of C releases
   it. The compiler calls it for `alloc(T, n)`, with 16 or the alignment
   that `align(N)` raises above it. In src/rt/mem.c. */
void *anti_rt_alloc_aligned(int64_t count, int64_t size, int64_t align,
                            int64_t zeroed);

/* The memory at p, which malloc or realloc gave, or NULL, grown to size
   bytes by realloc, never NULL. Out of memory ends the program through
   anti_rt_out_of_memory, and the memory at p stays. In src/rt/mem.c. */
void *anti_rt_grow(void *p, int64_t size);

/* The C side of anti.fs, in src/rt/fs.c. A path is the bytes of a str and
   their count. Every function reports a failure through errno. */

/* The C stream of the file, opened in mode, or NULL. The modes are those
   of anti_rt_file_open, which `Mode` of anti.fs numbers in the same
   order. The caller closes it with fclose, which `close` of anti.fs
   calls. */
void *anti_rt_fs_open(const unsigned char *path, int64_t len, int32_t mode);

/* The size in bytes of the file of the stream, or -1. */
int64_t anti_rt_fs_size(void *file);

/* The bytes of the file at path, a NUL path, with a NUL after them and
   their count in length, or NULL. errno is ENOMEM when the memory ran
   out. The caller frees the bytes with free. anti.fs does not call it:
   the readers of the runtime do, for the configuration, the index of a
   directory of plugins and the debug files of a trace. */
unsigned char *anti_rt_fs_read(const char *path, int64_t *length);

/* The names of the directory, without `.` and `..`, in one block that the
   caller frees, with their number in count, or NULL. */
struct anti_text *anti_rt_fs_list(const unsigned char *path, int64_t len,
                                  int64_t *count);

/* Remove the file, and give 0 or -1. */
int32_t anti_rt_fs_remove(const unsigned char *path, int64_t len);

/* Give the file at from the path to, and give 0 or -1. */
int32_t anti_rt_fs_rename(const unsigned char *from, int64_t from_len,
                          const unsigned char *to, int64_t to_len);

#endif
