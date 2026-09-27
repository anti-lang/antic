# Decisions of "Memory checks"

The entries below belong under "Memory checks" in `docs/decisions.md`. A later
step folds them in.

- [provisional] A check is a call of `__asan_loadN` or `__asan_storeN`. It
  passes the address and the number of bytes. It is not the inline test of the
  shadow memory. Reason: the register allocator already treats a call, so
  neither target needs code of its own.
- [provisional] The checks go in inside `select_module`. The back end has laid
  the types out by then. It has also expanded the bitfields and every simd
  operation it has no instruction for. Reason: the IR above the back end keeps
  no size, and the bitfields and lanes are plain loads and stores by then.
- [provisional] An access gets no check when its address is a slot of the
  frame or a global. A copy of such an address and an offset from one get none
  either. Reason: without redzones in
  the frame and around globals, the runtime reports nothing there.
- [provisional] A check carries the line of the access it guards. Reason: the
  frame of a report then names the line of the statement.
- [provisional] `--memory-checks` turns `-g` on, for antic and for a release
  build of `anti build`, `anti run` and `anti test`. Reason: the specification
  says a report carries the names and lines of `-g`.
- [provisional] The module that links defines `__asan_default_options`, which
  gives `detect_leaks=1:abort_on_error=0`. Reason: the leak check is off by
  default on macOS, and a report there ends the program with SIGABRT, which
  writes a crash report. The program exits with status 1 instead, as on
  Linux.
- [provisional] On windows-x86_64 the module that links defines neither the
  options hook nor `anti_rt_memory_kept`, and a program reports no leaks.
  Reason: the runtime of AddressSanitizer for Windows has no leak check, and
  it ends at start a program that sets `detect_leaks=1`.
- [provisional] The runtime marks a block it keeps with
  `anti_rt_memory_kept(p)` of `src/rt/rt.h`. Its own definition is weak and
  does nothing. The module that links a program of `--memory-checks` defines
  it with a call of `__lsan_ignore_object`. Reason: the runtime archive is
  one per target and level, so the runtime cannot name a function of
  AddressSanitizer that only some of its programs link. ld64.lld refuses an
  undefined weak reference, and a weak definition works on ELF and Mach-O
  alike.
- [provisional] The runtime marks the arguments and the environment with
  every string of both, before it takes out the options of the runtime. It
  marks the handle, the text and the code of PCRE2 of each pattern literal.
  On
  macOS each worker of the pool marks its thread-local block as it starts.
  Reason: dyld allocates that block on the heap, the worker runs until
  exit, and the leak check reads no pointer to it. Taking the address makes
  the block, about a kilobyte per worker, in every program on macOS.
- [provisional] `anti run` and `anti test` add `symbolize=0` to
  `ASAN_OPTIONS` of the program and read its standard error line by line. A
  frame `#N 0x<pc> (<module>+0x<offset>)` whose module the readers of
  `src/rt/symbols.c` read is written `#N 0x<pc> in <function>
  <file>:<line>`, the form of the runtime's own symbolizer. Where no line
  is known, the module stands in place of the line. Every other line is
  written as it came. The offset is looked up as it stands, since the
  runtime already printed the instruction before the return address.
  Reason: this is the lookup of `anti symbols resolve`, on the module
  itself instead of its debug twin.
- [provisional] The runtime of AddressSanitizer and dyld are universal
  Mach-O files. A frame in one takes the slice of the processor it names
  after the path. Reason: those frames then carry a name too.
- [provisional] On Windows `anti run` and `anti test` leave the symbolizing
  of AddressSanitizer on. Reason: Anti's symbolizer reads no PDB and would
  leave every frame of Windows raw.
- [provisional] The cache key of a dev object of `anti build` carries
  `memory-checks`. The key of a library file does not. Reason: the checks
  change the object, and the library file is the same with and without
  them.
- [provisional] The runtime archive carries the runtime of AddressSanitizer of
  the pinned clang in `lib/<target>/`: the dynamic library on both macOS
  targets, `libclang_rt.asan.a`, `libclang_rt.asan_static.a` and their `.syms`
  in the two glibc directories, and the DLL, its import library and the thunk
  on windows-x86_64. Reason: a user of the language gets no clang.
- [provisional] A macOS program links the dynamic library with an rpath to the
  absolute directory of it in the runtime archive. A Windows program gets a
  copy of the DLL beside it. Reason: the loader finds both without an
  environment variable.
- [provisional] A Linux program of `--memory-checks` links in the glibc mode.
  Reason: the runtime of AddressSanitizer is built against glibc, and musl has
  none.

## Answered questions

The two open questions of the first session have their answers under
"Generics and collections" in `docs/decisions.md`. `anti run` and `anti
test` put the report through Anti's symbolizer, and the runtime marks what
it keeps until exit with the leak checker's own call. The entries above
build both.
