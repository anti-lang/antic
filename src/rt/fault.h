/* The seam of fault injection: the one way a call of the system goes in
   the platform layer of the runtime, in the file helpers of anti and in
   the I/O of antic, so that a test can make it fail.

   DESIGN: a build for tests alone defines ANTI_FAULTS, which the CMake
   option ANTIC_FAULTS of tools/faults.cmake chooses at configure time.
   Such a build compiles src/rt/fault.c beside the code, and the variable
   ANTI_FAULT of the environment then makes the n-th call of a named kind
   fail with a named error:

       ANTI_FAULT=open:2:ENOENT
       ANTI_FAULT=alloc:1:ENOMEM,write:3:ENOSPC

   Each entry is a kind, a count from 1 and the name of an errno value.
   The calls of each kind are counted for the whole process, over every
   thread. The call that fails is not made: errno takes the error and the
   seam gives the value of a failed call. A build without ANTI_FAULTS
   compiles every seam to the call itself, so antic, anti and the runtime
   that ship hold no trace of it. The test faults_not_shipped reads their
   symbols.

   ANTI_FAULT(kind, failed, call) stands where call stood. kind is one of
   the names after ANTI_RT_FAULT_ below, failed the value of a failed
   call, NULL, -1 or errno for a call that gives its error. A stream of C
   goes through the five ANTI_F macros, since a failed read or write sets
   the error flag of the stream, which ANTI_FERROR then reads. */

#ifndef ANTI_RT_FAULT_H
#define ANTI_RT_FAULT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* The kinds of call. tests name each by the word after the prefix, in
   small letters: alloc, open and so on. */
enum anti_rt_fault_kind {
    /* malloc, calloc, realloc and the aligned allocations. */
    ANTI_RT_FAULT_ALLOC,
    /* fopen, open, opendir and the loading of a library. */
    ANTI_RT_FAULT_OPEN,
    /* fread, read and readdir. */
    ANTI_RT_FAULT_READ,
    /* fwrite, write and fflush. */
    ANTI_RT_FAULT_WRITE,
    /* fclose and close. */
    ANTI_RT_FAULT_CLOSE,
    /* remove, unlink and rmdir. */
    ANTI_RT_FAULT_REMOVE,
    /* rename. */
    ANTI_RT_FAULT_RENAME,
    /* mkdir. */
    ANTI_RT_FAULT_MKDIR,
    /* stat, fstat and realpath. */
    ANTI_RT_FAULT_STAT,
    /* fseek, ftell and lseek. */
    ANTI_RT_FAULT_SEEK,
    /* The start of a process: posix_spawn, fork and pipe. */
    ANTI_RT_FAULT_RUN,
    /* The wait for a process or a thread. */
    ANTI_RT_FAULT_WAIT,
    /* The start of a thread. */
    ANTI_RT_FAULT_THREAD,
    /* The set-up of a lock or a condition. */
    ANTI_RT_FAULT_LOCK,
    /* A sleep. */
    ANTI_RT_FAULT_SLEEP,
    /* The memory of the copy of the environment at the start of a
       program, apart from alloc, so that a count of alloc does not follow
       the size of the environment. */
    ANTI_RT_FAULT_ENV,
    ANTI_RT_FAULT_KINDS
};

#if defined(ANTI_FAULTS)

/* Count a call of kind. True when the variable ANTI_FAULT names this
   call, and then errno holds the error it names. A malformed variable
   ends the process with status 125 and a line on standard error. */
bool anti_rt_fault(enum anti_rt_fault_kind kind);

/* fread through the seam of the kind read. A read that fails reads
   nothing and marks the stream, which anti_rt_fault_ferror then reports
   until anti_rt_fault_fclose. */
size_t anti_rt_fault_fread(void *p, size_t size, size_t n, FILE *f);

/* fwrite through the seam of the kind write. A write that fails writes
   nothing and marks the stream as a read does. */
size_t anti_rt_fault_fwrite(const void *p, size_t size, size_t n, FILE *f);

/* ferror, which also reports a stream whose read or write failed in the
   seam. */
int anti_rt_fault_ferror(FILE *f);

/* fflush through the seam of the kind write. A flush that fails flushes
   nothing, marks the stream and gives EOF. */
int anti_rt_fault_fflush(FILE *f);

/* fclose through the seam of the kind close, which forgets the mark of
   the stream. A close that fails closes the stream all the same, as
   fclose does, and gives EOF. */
int anti_rt_fault_fclose(FILE *f);

#define ANTI_FAULT(kind, failed, call) \
    (anti_rt_fault(ANTI_RT_FAULT_##kind) ? (failed) : (call))
#define ANTI_FREAD anti_rt_fault_fread
#define ANTI_FWRITE anti_rt_fault_fwrite
#define ANTI_FERROR anti_rt_fault_ferror
#define ANTI_FFLUSH anti_rt_fault_fflush
#define ANTI_FCLOSE anti_rt_fault_fclose

#else

#define ANTI_FAULT(kind, failed, call) (call)
#define ANTI_FREAD fread
#define ANTI_FWRITE fwrite
#define ANTI_FERROR ferror
#define ANTI_FFLUSH fflush
#define ANTI_FCLOSE fclose

#endif

#endif
