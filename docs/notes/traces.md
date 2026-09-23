# Stack traces and error origins

The choices inside the runtime of `anti.lang.StackTrace`, in `src/rt/trace.c`,
`src/rt/backtrace.c` and `src/rt/symbols.c`, and inside the lowering of `fail`. The
rules are in "Error origin and stack traces" of
`docs/anti-language-additions.md`, and the settled points are in
`docs/decisions.md` under "Error origins and stack traces".

## What `fail` writes

`fail` loads `at.line` of the error it gives. When the line is 0 it copies
the constant location of the statement into `at` and asks
`anti_rt_backtrace_on`. When that says yes it calls `StackTrace.capture(0)`
and stores the trace in `frames`. The checker resolves `Error`, its two
fields and `capture` once per statement, and lowering reads the fields by
name, so the order of the fields in `src/std/anti/lang.anti` is its own.

An error that has a position already costs a release build one load, one
compare and one branch. The first `fail` of an error adds one call of the
runtime.

## The default of the build

A library file is compiled once and serves dev builds and release builds, so
the `fail` in it cannot carry the mode. The pass over the whole program
writes `anti_rt_backtrace_default`, a struct of one `int64_t`. It does so in
a program that reaches `anti_rt_backtrace_on`, as it writes the registry. The function
stands alone in `src/rt/backtrace.c`, so a program that captures a trace and
never fails links no reference to the default. `--anti.backtrace` of the
command line wins over the default.

## The walk

On Linux and macOS the walk follows the chain of frame records from its own
frame, which antic keeps in every function that calls another. A leaf keeps
none, and a leaf never stands above another frame. The walk stops at a record
outside the stack of the thread and at one that does not move up the stack.
It stops at a return address of 0 as well. The bounds of the stack come from
`pthread_get_stackaddr_np` on macOS and `pthread_getattr_np` on Linux. A C
frame without a record then ends the trace rather than the program.

Windows unwinds one frame at a time with `RtlVirtualUnwind`, over the unwind
data that antic writes for every function. Clang for the x64 convention of
Windows sets `rbp` at an offset into the frame, so there is no chain to follow
through C code. `RtlCaptureStackBackTrace` follows the chain of frame records
on ARM64 and loses the caller of every function that builds none.

## The module of a frame

macOS asks `dladdr`, which gives the path and the header of the image.
Linux asks `dl_iterate_phdr`, which visits the program first. glibc names it
with an empty text and musl with `/proc/self/exe` in a static program, so the
path of the first module comes from `/proc/self/exe`. Windows
asks `GetModuleHandleExW` and `GetModuleFileNameW`.

The build id comes from the notice of the module. macOS reads the symbol
`anti_licenses` from the symbol table of the image in memory and keeps the
answer per image. An image of the system cache is never read. Linux takes the
notice of its own image through a weak reference, and the one of another
image from the symbol table of its file. Windows takes its own through
`/alternatename`, which names an empty default when nothing defines the
notice, and the one of a library through `GetProcAddress`. A static library
for C has no notice, so none of the three references pulls the licence
object of the runtime into a C program.

## Symbols and lines

The lookup takes the byte before a return address, which lies in the call
and on its line.

- Mach-O. The function is the nearest symbol of the first section at or
  below the address. It is read from `__LINKEDIT` of the image in memory. The line
  comes from the debug map: the object that an `N_OSO` entry names, the
  function by its `N_FUN` entries, and the line table of the object at the
  same offset into the function. The object is read whole once, and its
  relocations of `__debug_line` are resolved in place.
- ELF. The function is the nearest symbol of an executable section, and the
  line comes from `.debug_line`. Both are read from the file of the module.
  Every build keeps its symbol table there, and a `-g` build its line table.
- Windows. DbgHelp reads the PDB that every Windows link writes, under one
  lock, since it serves one thread at a time. The record of the program
  names the PDB by file name alone. The directory of each module therefore
  goes on the search path before the first lookup there. The symbol record of
  each function names it `stack_trace.inner`, as ELF and Mach-O do. A public
  symbol of the COFF form, `_A11stack_trace_inner`, reads as the same name. The
  names it gives are kept once each for the life of the program.

The line reader takes DWARF 2 to 5. The file of a row is the name that the
file table holds, which antic writes as the path under the search root.

A name under `anti.rt.` loses a tie at one address, so the entry of a
program is named `module.main` and not `anti.rt.main`.

## Tests

`trace_symbols_<target>` links a `-g` program for each ELF and Mach-O target
and looks its functions up with `symbols_probe`, which compiles
`src/rt/symbols.c` for the host. The readers of the Linux runtime therefore run
on a Mac. The `trace_*` tests run programs in release, in dev mode and with
`--anti.backtrace`, and match their output against patterns, because a trace
holds addresses.
