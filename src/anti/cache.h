#ifndef ANTI_CACHE_H
#define ANTI_CACHE_H

/* The interface of cache.c: the keys of the files that `anti build` and
   `anti test` keep in the build directory of a mode between runs. */

#include <stdbool.h>

#include "cpu.h"
#include "driver.h"
#include "target.h"
#include "text.h"

/* The key of a cached output stands beside it, under its name and this
   suffix. */
#define CACHE_KEY_SUFFIX ".key"

/* What decides one output of the cache beside its input. The addendum
   names the version of the compiler and the target. The level joins them
   because it decides the instructions, and debug because it decides the
   lines. strict is --warnings-as-errors, which decides whether a warning
   stops the file, and checked is --memory-checks, which decides whether
   the object carries the checks. tests is --tests, which keeps the
   `tests` and `fixtures` blocks of a source. */
struct cache_flags {
    enum target target;
    enum cpu_level cpu;
    bool debug;
    bool strict;
    bool checked;
    bool tests;
};

/* Append the first line of the key of one output to out: the digest of
   input, the version of the compiler and the flags f. */
void cache_key(const char *input, const struct cache_flags *f,
               struct text *out);

/* Whether the output stands and the key beside it is key. */
bool cache_holds(const char *output, const struct text *key);

/* Forget the key of an output, so a compile that fails halfway leaves no
   key that the next run would trust. */
void cache_drop(const char *output);

/* Write key beside the output. Returns false when it cannot be written. */
bool cache_store(const char *output, const struct text *key);

/* Append `<label> <path> <digest>` of the file at path to out. Returns
   false with a message when the file cannot be read. */
bool cache_file(const char *label, const char *path, struct text *out);

/* Append the key that stands beside the output to out, under the line
   `key <output>`. Returns false with a message when there is none. */
bool cache_key_of(const char *output, struct text *out);

/* Append to out a line `import <path> <digest>` for every library file
   that library reaches through its imports, and for every library file
   that search names, apart from library itself. search holds the roots,
   the target and the library files of the walk, and its own list of
   library files is replaced by library and that list. Returns false when
   one cannot be read.

   DESIGN: decision 15 of "Decisions of 2026-10-11", by defect 10 of
   docs/audit/anti-tool.md. The key of a library file was over its own
   source, so a module whose import changed its interface was not checked
   again, and its object kept the layout of a struct that had grown. */
bool cache_imports(const struct options *search, const char *library,
                   struct text *out);

/* Append to out the line `archive <path> <digest>` of every archive and
   object of the runtime archive at runtime that a link of target t at
   level cpu may take, with the path below the archive, or the line
   `runtime none` where runtime is NULL. A program links them, so its key
   names them. Returns false with a message when one cannot be read.

   DESIGN: Q2 of "Answers of 2026-11-07" in docs/decisions.md. The key
   named the runtime by the digest of libanti_rt.a alone, and the native
   libraries by the version of the compiler, so a changed pin of raylib
   kept a program linked against the old libraylib.a. The files are those
   of lib/<target>/ itself, the native libraries among them, of
   lib/<target>/<level>/ and below it, the runtime, its bitcode and
   mimalloc, and of the library directory of the sysroot of the target,
   in the glibc mode of Linux as well. Which of them a link takes is known
   after the link alone, so the key names every one the link may take. */
bool cache_runtime(const char *runtime, enum target t, enum cpu_level cpu,
                   struct text *out);

#endif
