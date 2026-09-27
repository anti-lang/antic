# Memory checks, round five, second session

Status: done on the Mac. The first session, in
`docs/reports/2026-09-26-add5-memcheck.md`, built `antic --memory-checks`
and stopped on two questions. Eddie answered both under "Generics and
collections" in `docs/decisions.md`, and this session built the answers and
the rest of the step.

## What was done

- The runtime marks each block it keeps from start to exit through
  `anti_rt_memory_kept`, a weak function of `src/rt/init.c` that does
  nothing. The module that links a program of `--memory-checks` replaces it
  with a call of `__lsan_ignore_object`. The runtime marks the arguments and
  the environment, and the handle, the text and the code of each pattern
  literal. On macOS it also marks the thread-local block of each worker of
  the pool.
- The options hook adds `abort_on_error=0`, so a report on macOS ends the
  program with status 1 and not SIGABRT.
- `anti build`, `anti run` and `anti test` take `--memory-checks`. Each
  passes it to antic in either mode, with `-g`. The cache key of a dev
  object carries it.
- `anti run` and `anti test` run the program with `symbolize=0` and read its
  standard error through `process_run_lines`. Each frame gets its function,
  file and line from the readers of `src/rt/symbols.c`, the lookup of
  `anti symbols resolve`. A frame in a universal Mach-O file takes the
  slice it names, so the frames of AddressSanitizer and dyld carry names.
- Tests: `memory_checks` runs its case `clean` with the leak check on, with a
  pattern literal and the worker pool. It checks exit status 1 for each
  report and the hook and the marking of three targets in the assembly. It
  runs on every host but windows-arm64, decided by the host. The new
  `anti_memory_checks` runs a read after free in both modes, a double free,
  a write past a heap block, a leak and the case `clean` through `anti run`,
  and a double free in a test through `anti test`. It pins the line of
  each frame of the program and shows that the cache keeps objects with
  and without the checks apart.

## Suites

At `c58a6df`, the host suite passed 1162 of 1162, the ASan suite 1161 of
1161 and the UBSan suite 1161 of 1161. Logs:
`build/drive/logs/mc2-suite-host.log`, `mc2-suite-asan.log` and
`mc2-suite-ubsan.log`. The builds are in `mc2-build-final.log`,
`mc2-asan-build.log` and `mc2-ubsan-build.log`. Only the vendored raylib
sources warn, under their own flags.

## Failures on the way

- The leak check reported 13 KB of the start in every program. The marks of
  the arguments and the environment fixed it.
- A sweep of 209 programs of `tests/programs` and `tests/std` under the
  option found no access error. Beside the leaks the programs make
  themselves, it found the thread-local blocks of the pool's workers on
  macOS. The workers now mark them. Log: `build/drive/logs/mc2-sweep.log`.
- `anti run` ended with signal 6, since macOS aborts after a report. The
  options hook turns that off.
- The strings of the Windows runtime of AddressSanitizer hold "detect_leaks
  is not supported on this platform", and it dies on that option. The first
  session's hook set it there. Windows now gets no hook.
- `link_identity_macos-arm64` failed once the start marked its blocks. The
  digest in `tests/link-identity/` is the one of the Mac.
- The test registered its run only where configure found the runtime of
  AddressSanitizer in the runtime archive, which a fresh tree lacks. It now
  decides by the host.

## Provisional decisions

Every entry of `docs/decisions-memcheck.md` is `[provisional]`. The new
ones are the weak `anti_rt_memory_kept` and its replacement, what the
runtime marks, `abort_on_error=0`, no hook and no leak report on Windows,
the form of a rewritten frame, the slice of a universal file, the
symbolizer of AddressSanitizer on Windows, `-g` in release with the option
and the cache key.

## Not done

- Linux and Windows have not run a program of the option. The glibc link
  and the ELF frames of `anti run` are untested on a Linux host.
- The windows-x86_64 program reports no leaks, which the specification
  asks for on every system but windows-arm64. Its runtime has none.
- The status lines of `docs/anti-syntax-overview.md` and `CLAUDE.md` still
  say `--memory-checks` is not built. The step's fence leaves them to the
  step that folds `docs/decisions-memcheck.md` in.
