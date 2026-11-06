# The answers of 2026-11-06 to anti.graphics

Eddie answered the four questions of `docs/reports/2026-11-06-graphics.md`.
The session recorded them, fixed T210 and let `anti test` of a project find
its modules without `-I`.

## What changed

- `3059fc0c` records the answers as "Answers of 2026-11-06, second set" in
  `docs/decisions.md`. The entries of `graphics.hex` and `graphics.hsv` and
  of the `say` chain lose the provisional tag, by Q1 and Q2. The entry of
  the feature freeze names what the answers let in.
- `2e3add0b` fixes T210, by Q3. The probe of an atomic call checks the
  base of every call quietly, and that check rewrote `graphics.PINK` into
  the name `PINK` of the imported item. The check of the call then read the
  base again and looked the bare name up in the module that wrote it, where
  it does not stand. A name rewritten from `module.name` now carries the
  mark `qualified` of `src/antic/ast.h`, and `check_name` takes its symbol
  as it stands. A field before a call, `hash` and a method of a `str`
  failed the same way and compile now. The three tests of T210 left
  `tests/known-failures.txt`, and its row reads fixed.
- `a025bddc` builds Q4. In a project and without `-I`, `anti test` runs
  the dev build of `anti build` for the host first. It writes or reuses
  the library file of every module of `src/` in `build/<target>/dev/lib/`
  and links nothing. The run then takes `src/` and that directory as its
  roots, the package name of the build and the library files of the
  dependency graph. Without a file it tests every module of `src/` that
  declares a test. `build_run` fills `struct build_tested` when the request
  names one. Outside a project and with `-I` the run is as before.
  `docs/tooling.md`, the overview and "Tests and fixtures" in
  `docs/decisions.md` describe it.

## Tests

- `matrix_modules_constant_named_with_its_module_in_contexts`, in three
  modes, holds `graphics.PINK.fade(0.5)`, `graphics.PINK.lerp(...)`,
  `graphics.PINK.r`, `reflect.BAD_INDEX.hash()` and `math.PI.hash()` in
  the fifteen places of the matrix. Before the fix antic refused it with
  60 messages of `unknown name`.
- `matrix_modules_constant_named_with_its_module_at_module_level`, in
  three modes, holds the field in a constant and in the defaults of a field
  and of a parameter. A call is no constant expression there.
- `modules_qualified_constants_release` and `_dev` hold a constant of a
  library named with its module and with an alias. A call, a field, a
  field and a call, `hash`, `.len` and `matches` follow it, in a client and
  in a second library. It failed with `unknown name` before the fix.
- `matrix_modules_function_of_a_constant_named_with_its_module` passes in
  its three modes.
- `anti_test_project_<case>` runs ten cases over a copy of
  `tests/anti-test/project/`, a project without `[package]` whose modules
  share an `internal` function. `all_dev` and `all_release` run `anti test`
  without a file, and `file_dev` and `file_release` with one. In
  `import_changed` a changed import reaches the next run. `earlier_run`
  fails when a library file of an earlier run stands before the one of the
  dev build, which the session checked with `lib/` turned off.
  `dependency_dev` and `dependency_release` test a module that imports a
  path dependency, and fail without the library files of the graph.
  `include_roots` shows that `-I` writes no library file, and `outside`
  runs without a manifest. Eight of them failed before the code, each for
  the reason it names.

## Acceptance

- In `build/pythagoras-tree`, `anti test --runtime build/host/runtime` with
  no `-I` and no file passes the 18 tests of `tree.geometry` in dev mode
  and with `--release`. `anti test src/tree/geometry.anti` passes them as
  well. A copy of its `src/` and `anti.toml` without `build/` passes them
  cold, and with `--memory-checks`. The repository of pythagoras-tree was
  not touched.
- `graphics.PINK.fade(0.5)` compiles and runs in a program of
  `anti.graphics`, as the matrix tests above show.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and `-I 4301,6900`
with `-j14`. Before `2e3add0b` it took 375 s, 268 s and 277 s. Before
`a025bddc` it took 375 s, 265 s and 271 s with one failure, `raw_output`.
That test found an `execute_process` of the new runner without
`ENCODING NONE`. After the fix the suite took 376 s, 267 s and 278 s. All
6886 tests passed, with the two skips of `mimalloc_environment`.

Each sanitizer suite ran its 6884 tests in the six parts of `asan` at
`a025bddc`: `asan` 245 s, 189 s, 259 s, 193 s, 442 s and 433 s, `ubsan`
250 s, 187 s, 258 s, 201 s, 458 s and 452 s. Every test passed. The VMs
are down, so no suite ran on Linux or Windows.

## Provisional entries

- `anti test` of a project without a file tests the modules of `src/`
  that declare a test, and none of `test/`.
- The library files of the modules under test stand in `lib/` of the work
  directory, which each run over a project empties first.
- `anti test --release` of a project takes the library files of the dev
  build. The objects of a dev run are compiled by the run as before,
  without the cache of `anti build`.

## Questions for Eddie

1. A dev run of `anti test` in a project reuses the library files of
   `anti build`. It compiles the objects of the imported modules again on
   every run, as it did before, since they carry `--tests` and no `-g`.
   Should the run take the objects of `anti build` instead?
2. `anti test --release` of a project reads the library files of the dev
   build, as Q4 names the dev mode. Should it read those of
   `build/<target>/release/lib/`, which a release build checks with
   `--warnings-as-errors`?
