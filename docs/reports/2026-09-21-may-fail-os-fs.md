# `may fail` in anti.os and anti.fs

The third module step of the rewrite of the standard library to the
`may fail` form. `7ce1f4c` and `ec8a9c7` fix two defects of antic that
the rewrite exposed, and `1b0f8a0` is the rewrite.

## What there was to rewrite

- anti.os had three failing functions, `user_config_dir`,
  `user_data_dir` and `user_cache_dir`, written by hand with an out
  pointer, and the helpers `owned`, `unix_dir` and `windows_dir` in the
  same form.
- anti.fs did not exist. "Standard library phase" in `docs/decisions.md`
  gives its scope: open, read, write and close a file, its size, the
  listing of a directory, remove and rename. The spec examples call
  `fs.open`, `fs.close`, `fs.remove` and `f.size`.
- No function of either module has two answers, and "Tuples" in
  `docs/anti-language-additions.md` advises a named struct where a
  tuple crosses a module boundary. So no tuple enters either API.

## What changed

- `7ce1f4c`. `try f()` as an operand, after `return`, as an argument,
  inside an operator or as a condition, got a null out pointer, and its
  error pointer was read as the value. `src/lower.c` gives such a call
  a slot of the frame. `tests/programs/handled_operand` covers each
  position.
- `ec8a9c7`. `reserve_slots` never entered a `try` block or its
  handler, a `switch` arm, a `defer` or `undo`, the `else` of a `let` or
  the handler of a call, so a local there that needs a slot got none.
  It walks all of them now. `tests/programs/nested_locals` covers each
  block. The two locals of the `try` block of `tests/programs/may_fail`
  gain slots, so its six digests in the manifest change. Every other
  line of that assembly is the same.
- `1b0f8a0`. `std/anti/fs.anti` and `rt/fs.c` are new, and fs follows
  os in the list of modules. `std/anti/os.anti` gives the three
  directories as `-> str may fail`. `tests/std/fs.anti` writes, reads,
  measures, lists, renames and removes a file in the working directory,
  and reads the error of each call on a missing path. The test of the
  user directories forwards one call with `try`.
- The spec examples: `defer fs.close(f) catch fatal`, `undo
  fs.remove(path) catch e { e.print() }`, `fs.open(path,
  fs.Mode.Read)` and `if try f.size() == 0`. Each compiles and runs.
  The Errors status of the overview, the user-directory rule of the
  additions, the table of `docs/site/standard-library/index.md` and
  `CLAUDE.md` follow.

## What failed and how it was fixed

- `std_fs` failed first, on the missing module.
- The Windows cross builds of the runtime refused `_wfopen`, which the
  C runtime deprecates. `rt/fs.c` calls `_wfsopen` with `_SH_DENYNO`,
  which is the same call without the deprecation.
- `std_userdirs` crashed after the rewrite of anti.os, whose
  `return try windows_dir(...)` met the first defect. A scratch program
  under `build/drive/scratch/` reduced it to `return try f()`. An `int`
  result failed IR verification and a `str` result crashed.
- The `try` block of the syntax overview, with `let f = fs.open(...)`
  inside it, failed IR verification. That is the second defect. It was
  there before this step, which a build of `HEAD` showed.
- `emit_identity` failed on the digests of `may_fail` and on the two new
  programs. A diff against the old compiler showed the change stays in
  `main`. The manifest was written with `-DWRITE=yes` on this Mac, once
  for each of the two commits.
- The docs-style checker flagged a banned phrase in a provisional entry
  and three long sentences in `rt/fs.c`. All four were rewritten.
- `tools/scripts/format_anti.py` without `--check` rewrote two new
  files in place, and it breaks a `for` body and an `enum`. Both files
  were restored by hand. It flags `std/anti/os.anti` at `HEAD` as well,
  so it is no gate. `Mode` stands on one line, as the enums of anti.log
  do.

## Checks

- Zero warnings from the build of antic, the runtime and the six cross
  runtimes. The link on this Mac prints `ld: warning: ignoring
  -lto_library`, which every earlier build log in `build/drive/logs/`
  holds too.
- 524 tests on the host and 523 under each sanitizer preset, all
  passing, without a sanitizer report in either log. The suite ran once
  for each commit: 521, 523 and 524 tests.
- The fs test links for all six targets. It passes as a dev build and
  as a macos-x86_64 build at `--cpu v1` under Rosetta.
- Logs: `build/drive/logs/test-c1.log`, `test-c2.log`, `test.log`,
  `build.log`, `asan-build.log`, `asan-test.log`, `ubsan-build.log`,
  `ubsan-test.log`, `cross.log`, `dev.log`, `docs-style.log`,
  `docs-style-c.log` and `docs-style-anti.log`.

## Provisional entries

Three, under "Standard library phase" in `docs/decisions.md`:

- The shape of `File`, `Mode`, `open`, `read`, `write`, `size` and
  `close`, with the mode passed until default values are built.
- `fs.list` gives one block without `.` and `..`, in the order of the
  system, freed with `free(names.ptr)`.
- Every failure is `SystemError.from_errno()`, and a path with a NUL
  byte fails with `EINVAL`. Windows takes the wide functions of the C
  runtime. `rename` onto a file that exists replaces it on Linux and
  macOS and fails on Windows.

## Questions

- `rename` follows C, so it replaces an existing file on Linux and
  macOS and fails on Windows. Should anti.fs give one meaning on every
  system, with `MoveFileExW` and `MOVEFILE_REPLACE_EXISTING` on
  Windows?
- Should `fs.list` sort its names, so that a listing reads the same on
  every file system?
- Neither VM has run `std_fs` yet. On Windows it exercises
  `_wfindfirst64` and the errors of the UCRT streams.
