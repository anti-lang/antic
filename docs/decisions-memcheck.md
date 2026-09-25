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
- [provisional] `--memory-checks` turns `-g` on. Reason: the specification
  says a report carries the names and lines of `-g`.
- [provisional] The module that links defines `__asan_default_options`, which
  gives `detect_leaks=1`. Reason: the leak check is off by default on macOS.
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

## Open questions

- How does a report get the file and line of each frame? On macOS the runtime
  names each frame with `dladdr`. `atos` does not read the debug map of an
  antic `-g` link, so the frames carry function names and no lines. On Linux the
  runtime archive holds no symbolizer, so a frame carries only the module and an
  offset. Two ways reach the lines, and both lie outside this step. One:
  `anti-lang/llvm-tools` ships `llvm-symbolizer` and `dsymutil`, a macOS link
  runs `dsymutil`, and the options hook names the symbolizer. Two: `anti_rt`
  answers the hook `__sanitizer_symbolize_code` of the runtime with its own
  reader of `src/rt/symbols.c`.
- May `src/rt/start.c` keep the arguments and the environment in static
  storage? The start allocates both and drops the last pointer to them when
  `main` returns. The leak check then reports them in every program.
