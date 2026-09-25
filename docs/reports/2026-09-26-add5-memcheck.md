# Memory checks, round five

Status: BLOCKED. `antic --memory-checks` is built and catches the four errors on
the Mac. Two parts of the specification need changes outside this step, and the
step stopped there. The questions are in `docs/decisions-memcheck.md`.

## What was done

- `src/antic/memcheck.c` puts a call of `__asan_loadN` or `__asan_storeN`
  before every load, store, memcopy and simd operation on memory. It runs
  inside `select_module` after the layout. Addresses of the frame and of
  globals get no check.
- The module that links defines `__asan_default_options` with
  `detect_leaks=1`, since the leak check is off by default on macOS.
- `--memory-checks` turns `-g` on. windows-arm64 refuses it with
  `` `--memory-checks` is not available for windows-arm64 ``.
- The build copies the runtime of AddressSanitizer from the pinned clang
  into `lib/<target>/` of the runtime archive. That covers both macOS
  targets, both glibc directories and windows-x86_64. macOS links the dylib
  with an absolute rpath. The glibc mode links the whole archives and the
  `.syms` list. Windows links the import library and the thunk, and copies
  the DLL beside the program. A Linux program of the option links in the
  glibc mode.
- Tests: `memory_checks` runs `tests/traps/memory_checks.anti` on the host. A
  read after free, a double free, a write past a heap block and a leak each
  give their report. Each report names the function of the program. The test
  also checks the refusal and a build without the option. `unit` pins the link lines of
  the three systems.

Commit `32233de`. The host suite passed 1115 of 1115 there, in
`build/drive/logs/memcheck-suite-host.log`.

## What blocks

1. Lines in the reports. On macOS the runtime names a frame through
   `dladdr`. `atos` does not read the debug map of an antic link and gives no
   line. lldb does, so the debug information is fine. Linux has no
   symbolizer in the runtime archive, so a frame carries no name either.
   Both fixes lie outside the step: `llvm-symbolizer` and `dsymutil` in
   `anti-lang/llvm-tools`, or `anti_rt` answering `__sanitizer_symbolize_code`
   with its own reader in `src/rt/symbols.c`.
2. The leak check reports the arguments and the environment that
   `src/rt/start.c` allocates, in every program, because no pointer to them
   outlives `main`. Keeping them in static storage there fixes it. The test
   runs its case without errors under `detect_leaks=0` until then.

## Not done

- `anti build`, `anti test` and `anti run` do not take `--memory-checks` yet,
  and the dev cache key of `anti build` does not carry it. `src/anti/` is not
  touched.
- The ASan and UBSan suites have not run. Linux and Windows have not run a
  program of the option.
- A frame of the entry reads `anti.rt.main`, the second name of `main`.

## Failures on the way

- `__asan_default_options` returned the index of the global, 9, and the
  runtime crashed in its flag parser. The return now takes `ir_addr`.
- A relative `--runtime` gave a relative rpath. The rpath is now absolute.
- `antic --dev` on the test program cannot link alone, since a dev build
  links the objects of the other modules. The test builds in release.

## Provisional decisions

Every entry of `docs/decisions-memcheck.md` is `[provisional]`: the calls
over inline checks, the place in `select_module`, no check on frame and
global addresses, the line of a check, `-g` with the option, the options
hook, the files of the runtime archive, the rpath and the DLL copy, and the
glibc mode on Linux.

Logs: `build/drive/logs/memcheck-build.log`, `memcheck-test.log`,
`memcheck-suite-host.log`, `memcheck-docs.log`.
