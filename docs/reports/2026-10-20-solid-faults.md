# Step faults: fault injection for the tests

The first session of the step. It builds the seam and the copies of the
tools and of the runtime that hold it. 159 tests make a call of the
system fail through them. Logs are under `build/drive/logs/`, named `faults-*`.

## What changed

- `src/rt/fault.h` is the one seam. `ANTI_FAULT(kind, failed, call)` and
  the five `ANTI_F` macros of a C stream stand at every call of the
  platform layer of the runtime on macOS and Linux, at every allocation of
  the runtime and of the glue of `anti.regex`, at the file helpers and the
  allocation of anti and at the I/O, the processes, the threads and the
  allocation of antic. Without `ANTI_FAULTS` each is the call itself.
- `tools/faults.cmake` adds the option `ANTIC_FAULTS`. A tree with it
  builds `antic_faults`, `anti_faults` and `runtime-faults/`, whose
  `lib/<host>/<level>/` holds the runtime of the seam and links every
  other entry to `runtime/`. antic, anti and `runtime/` stay as before, so
  no package and no pinned link holds the seam. `faults_not_shipped`
  reads their bytes. A Release build refuses the option.
- In a copy, `ANTI_FAULT=<kind>:<n>:<error>[,...]` makes the n-th call of
  a kind fail with an errno value. `src/rt/fault.c` holds the kinds.
- A case of a table of messages takes `FAULT`, and `<name>.faults` beside
  a program of `tests/programs/` or `tests/std/` holds its cases, run
  against `runtime-faults/` in a directory of their own. An expected file
  may start with `abort`. `known_failures` accepts the line of a fault
  test in a tree without the option and a line with `host=` on another
  host.
- The tests: 24 cases of antic, 56 of the commands of anti, 79 runs of
  eleven programs. `anti: cannot replace %s` leaves
  `tests/diagnostics-open.txt`.

## Build configuration, for Eddie to decide

The presets `host`, `asan`, `ubsan` and `coverage` turn `ANTIC_FAULTS`
on, so the gates run the fault tests, and `release` leaves it off. The
option costs a second compile of antic and anti in each tree. Turning it
off in a preset drops the fault tests of that tree and nothing else.

## Findings

No finding was closed. The step recorded four, each with its tests in
`tests/known-failures.txt`:

- T151, major. antic says it cannot open the licence text of the runtime
  and links the binary all the same. Its notice lacks the text, and antic
  ends with 0.
- T152, major. A library file read part of the way gets a second line
  with the bytes of an error buffer that nothing wrote. The dev link of
  `anti build`, `anti doc` and `anti bind --header` show it.
- T153, minor. anti ends with 1 and no message when it cannot make a
  directory: the object of `anti build`, `anti test`, `anti check`,
  `anti doc`, `anti bind` and `anti new`.
- T154, minor. A release build of the Mac writes the map without the
  lines of an object it cannot read, and ends with 0.

24 new tests show T139, in the C of `anti.fs`, the reader of TOML, the
compile of a pattern and the stack trace. Where memory runs out, each
gives a failure, an empty text or a smaller result. The program must
stop.

`tests/known-failures.txt` held 14 lines before and holds 49 after, 47 of
them expected to fail on the Mac.

## Observations that are no finding

- Under anti, memory that runs out before the work starts reads
  `antic: out of memory`, from the allocator of antic that anti shares.
- `rt.configure` that runs out of memory stops with `anti: out of memory
  at program start`, although main runs.
- `anti fmt` prints the name of a file before it writes it, also when the
  write then fails.
- `anti run` ends with 255 when the program does not start.
- A thread of the pool that does not start leaves its work to the others
  and to the caller. A signal route that fails catches nothing until the
  next registration, as "Standard library phase" in `docs/decisions.md`
  says.

## `[provisional]` entries

Two, under "Repository layout" in `docs/decisions.md`: the seam with the
option and its kinds, and where a fault test stands.

## Gates

- Builds without a warning of our code: `faults-host-build2.log`,
  `faults-asan-build.log` and `faults-ubsan-build.log`.
- Host, 5368 tests: `-I 1,2000` 356 s and `-I 2001,5368` 278 s, 2000 and
  3394 passed, 47 of them expected failures, the two
  `mimalloc_environment` tests skipped: `faults-host-3.log` and
  `faults-host-4.log`.
- ASan, 5366 tests in four parts, `-I 1,1150`, `1151,2250`, `2251,3800`
  and `3801,5366`: 1150, 1114, 1576 and 1573 passed in 251 s, 167 s,
  190 s and 508 s, `faults-asan-1.log` to `-4.log`.
- UBSan, the same parts: all passed in 250 s, 143 s, 146 s and 349 s,
  `faults-ubsan-1.log` to `-4.log`.
- `emit_identity` passes. Its manifest gained the 18 lines of the three
  new programs of `tests/programs/`, and no digest changed.
  `link_identity_macos-arm64` passes unchanged.
- The docs-style checker reports nothing of this step on the files it
  touched, `faults-docs-final.log`. Its one warning, line 513 of the
  notes, is older.

## Not run on the target

- The seams in the Linux half of the platform layers, `readlink` of the
  path of antic among them. The futex and `getrandom` stay outside. The
  fault tests on a Linux host would show the counts there.
- The counts of the cases of `anti run --memory-checks` and of the Linux
  soname, and every message with a strerror text. A suite of a fault tree
  on Linux and on Windows would show them.

## Left for the next session

- The Windows halves of `src/rt/platform_windows.c`,
  `src/antic/platform.c` and `src/anti/platform.c` take no seam yet.
- Regions of the lists still without a test: `anti_rt_getenv` and the
  other allocations of `conf.c` and `inject.c`, which need a variable of
  the environment in a case, the order of locks of a dev build in
  `lock.c`, `file_bytes` and the memory of the loader in `plugin.c`, the
  memory of `Object.deserialize` in `registry.c`, `anti sdk export`,
  `anti symbols`, and the fetches of `deps.c` and `repo.c`.
- A case of a program in dev mode, which `<name>.faults` does not take.

## State at the push

```text
$ git log --oneline -3
7bde1f7a Keep the seam to C11 atomics and the new programs in canonical form
f0b98e05 Test the child of a fork, the run of anti run --memory-checks and the path of antic when a call fails
1dee4ba0 Test the symbols archive of a release when a file does not open
$ git status --short
$ git rev-parse HEAD origin/main
7bde1f7aecfce3a56cab90957e223d3669e5432a
7bde1f7aecfce3a56cab90957e223d3669e5432a
```

This report is written after that push, and its commit follows it.
