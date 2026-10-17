# The tests of the options

A step of the test run, on 2026-10-12, on `main`. It gave a test to every
option that `tests/options-open.txt` listed, to the options the code reads
together and to the wrong values of an option. No file under `src/`
changed.

## What the step added

- `tests/runners/run_usage.cmake` holds the usage. `antic --help`, `anti --help`
  and `anti -h` print it on the standard output and end with 0, and the
  tool without an argument prints the same bytes on the standard error
  and ends with 2. The runner also holds 74 command lines that a tool
  refuses with its usage, `antic_usage_<case>` and `anti_usage_<case>`.
- `tests/runners/run_anti_options.cmake` holds 13 cases, `anti_option_<case>`:
  `build --strip-docs`, `build --soname` for macOS, Linux and Windows,
  `build --target all` alone and with `--cpu`, `check -I` with a library
  file and with a source, `bind -D` with a name, a value and the shim,
  and `symbols inventory --from`.
- `tests/runners/run_antic_options.cmake` holds 20 cases, `antic_option_<case>`:
  `--cpu` beside `--target` for each of the six targets, `--checks` and
  `--asserts` with their `--no-` forms beside `--dev` and without it,
  `--warn-undocumented` beside `--doc-warnings`, `--warnings-as-errors`
  beside `-c`, `--front-end` and `--dev`, `--bundle-runtime` beside both
  kinds of `--lib`, `--soname` beside `--lib` and `--package-version`,
  `-o` beside `-S`, `-c` and the link, and `--keep-llvm`.
- 27 refusals of antic with one line and status 2: a wrong value of
  `--linker`, `--inject` and `--target`, an input that is no `.anti`
  file, `--lto` beside `--memory-checks` for macOS, and each of the 22
  options that take a value as the last word.
- `lto_refused_library` refuses `--lto` beside `-c`, and
  `program_hook_trace_trace_no_hooks` runs `--trace` beside `--no-hooks`.

A case of the two option runners runs the tool twice, without the option
and with it, and names what differs. The cases that expect a difference
were also run against a wrapper of the tool that drops the option, and
each failed there. `build/drive/logs/opt-mutate.log` holds the 29 runs.
Of the six targets of `--cpu`, two ran there.

`tests/options-open.txt` held 8 lines and holds none. `options_covered`
counts every option and command as passed by a test.

## The finding

- T099, major. `anti build --lib shared --soname` for Linux leaves
  `dist/` with `libshapes.so` alone, a copy and no link. Its soname is
  `libshapes.so.2`, which `build/` holds and `dist/` lacks.
  `anti_option_soname_linux_dist` expects both files and is the one new
  line of `tests/known-failures.txt`, which holds 325.

## Where the task and the code differ

- The task names `--dev` with `--checks`, `--lib` with `--bundle-runtime`
  and `--target` with `--cpu` as examples. `run_checks.cmake`,
  `run_asserts.cmake`, `run_clib.cmake` and `cpu_level_<level>` held a
  part of each. The new cases hold every cell, and none of the old tests
  changed.
- `antic --soname` had a test of its refusal alone, and the list did not
  name it. `antic_option_soname_linux` and `antic_option_soname_static`
  hold what it writes.
- The first form of `anti_option_soname_windows` compared the two
  libraries byte for byte and failed under ASan, where the two links
  fell into different seconds. The case now compares the headers and the
  exports. The last rule below is what it showed.
- The task names no id of the step, so the name of this report is a guess.

## Rules the documents do not state

- The usage of antic names no `--help`, `--no-reflect`, `--asserts`,
  `--no-asserts`, `--checks`, `--no-checks`, `--no-hooks`, `--trace` and
  `--no-trace`. antic takes all nine.
- `antic --help` among other arguments prints the usage and ends with 0.
  `anti --help` with a second word is refused with the usage and 2.
- An option of antic that takes a value as the last word gives
  `needs a value`. An option of anti there gives the whole usage and no
  line of its own, and so does `--lib` with a wrong kind.
- `--soname` beside `--lib static` changes no byte of the archive. `anti
  build` takes `--soname` and `--bundle-runtime` without `--lib` and ends
  with 0.
- `anti run` takes every option of `build`. It ignores `--target` and
  builds for the host, and `--lib static` fails for the `main` of the
  program. No test holds either.
- `anti build --target all --cpu v1` builds linux-x86_64, refuses
  linux-arm64 and ends with 1. What it built before stays.
- `anti check -I <dir>` finds an imported module as a library file under
  the root, or as a source the command names. A source under the root
  that the command does not name is not found, as with `antic -I`.
- A library of macOS without `--soname` has the path of its output as
  its install name and 0.0.0 as both versions.
- `--checks --no-checks` drops the checks and `--no-checks --checks`
  keeps them: the last one decides. No test holds it.
- `--dev --no-asserts` drops the call and keeps the text of the
  assertion as a constant of the LLVM text. No test holds it.
- The link of a Windows library writes its time into the file, so two
  links of one input differ in two bytes. A static library and the
  libraries of macOS and Linux are the same bytes each time.

## Counts and checks

- Tests: 3655 before and 3794 after, by `ctest --preset host -N`.
- Host suite: 3794 of 3794 passed in 407 s, 325 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/opt-host3.log` holds the run on the final tree, and
  `opt-host2.log` the run of 437 s that `CLAUDE.md` names.
- ASan: 3793 tests in three parts, `-I 1,1100`, `-I 1101,2100` and
  `-I 2101,3793`, in 250 s, 361 s and 221 s. The second part failed for
  `anti_option_soname_windows` alone. The rewritten case then passed
  three times in that tree, and the part did not run again.
  `opt-asan-1.log`, `opt-asan-2.log`, `opt-asan-sw.log` and
  `opt-asan-3.log` hold them.
- UBSan: the same three parts in 241 s, 346 s and 172 s, all passed, in
  `opt-ubsan-1.log`, `opt-ubsan-2.log` and `opt-ubsan-3.log`.
- The three trees built with zero warnings, in `opt-build6.log`,
  `opt-asan-build.log` and `opt-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `options_covered`, `diagnostics_covered`, `known_failures`,
  `repo_layout` and `raw_output` pass on the final tree.
- The listed test was read in `opt-t1b.log`. It fails for the missing
  `libshapes.so.2` in `dist/`, the reason its finding names.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the
  findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests. `anti_option_soname_macos` runs on a Mac
  alone and `anti_option_symbols_from` on the hosts other than Windows.
- `antic_option_output_program` and `antic_option_keep_llvm` name the
  files of a Windows link by the code and were not run there.
- The refusal `linking needs --runtime` got no new test. antic finds an
  installed runtime archive by itself, so the message depends on the
  machine. `error_missing_runtime` of the suite holds it already.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
21e9feed State the tests of the options, the counts and the suite times in CLAUDE.md
7bb1317c Test the options of anti that had no test
d378c144 Test the options that antic reads together and their wrong values
```

`git rev-parse HEAD origin/main` after that push:

```text
21e9feeda118c99496a38972fe018ccb8c402502
21e9feeda118c99496a38972fe018ccb8c402502
```

`git status --short` printed nothing after that push. The reply of the
session holds the three outputs after the last push, since the hash of
this commit is not known inside it.
