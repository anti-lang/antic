# The registration of the matrix and the known failures

The step `harness` of the test run, on 2026-10-12, on `main`. The task
names no id for the step, so this report takes `harness`. The suite now
registers a file of `tests/matrix/` by its name and expects the tests of
`tests/known-failures.txt` to fail. No file under `src/` changed.

## What the step added

- `tests/matrix/` stands in the list of the test `repo_layout` and in the
  Layout table of `CLAUDE.md`. The directory holds no file yet, so git
  holds no directory. The first matrix step adds it.
- A program `<name>.anti` with `<name>.expected` registers
  `matrix_<name>_dev`, `matrix_<name>_release` and
  `matrix_<name>_memory_checks` through `antic_program_test`. A refusal
  `<name>.anti` with `<name>.err` registers `matrix_<name>_refused` through
  `add_expect_test`. No new runner builds or runs a program.
- `antic_dev_imports` reads the modules of the standard library a program
  imports, and those import. `antic_dev_object` registers the test that
  compiles the dev object of one on first use.
- `tests/run_matrix_files.cmake` is the test `matrix_files`. It refuses a
  file of the directory that registers nothing.
- `tests/known-failures.txt` holds its header and no line.
  `docs/audit/tests-findings.md` holds its header and the empty table.
  `tests/known_failures.cmake` reads the list for the configure step, which
  sets `WILL_FAIL`, and for `tests/run_known_failures.cmake`, the test
  `known_failures`.
- `tests/run_known_failures_self.cmake` is the test
  `known_failures_runner`: 17 cases, five lists the runner accepts and
  twelve it refuses, each with the text of its message.
- `run_program.cmake`, `program_expect` and `run_expect.cmake` take a time
  limit, and a matrix test passes 60 seconds.
- `CLAUDE.md` holds the rules under "Tests". `docs/decisions.md` holds
  three `[provisional]` entries under "Repository layout" for what
  decisions 28 and 30 leave open.

## The proof in the scratch copy

`build/matrix-proof/antic` is an export of the index, configured with
`-DANTI_DEPS_DIR` and built in 38 s. Its `tests/matrix/` holds the program
`while_sum`, the refusal `undefined_name` and `wrong_total`, whose expected
file holds 7 where the program prints 6. The list names the three tests of
`wrong_total` under T001. Each case but the last ran the 14 tests that
match `^(matrix_.*|known_failures.*)$`. The logs are
`build/drive/logs/harness-proof-<case>.log`.

| Case | What the suite did | Log |
|---|---|---|
| The listed defect | 14 of 14 passed. The three listed tests failed with `expected the bytes 370a, got 360a` | `1`, `1v` |
| A listed test that passes | `matrix_while_sum_release` failed, and ctest gave 8 | `2-listed-pass` |
| A line that names no test | `known_failures` failed | `3a-no-test` |
| An id without a row | `known_failures` failed | `3b-no-row` |
| An open row without a line | `known_failures` failed | `3c-open-row` |
| The list changed and the tree not configured again | `known_failures` failed | `3d-stale` |
| A source with no expected file | `matrix_files` failed | `3e-orphan` |
| A listed program that never ends | its three tests failed in the runner after 60.9 s and counted as expected | `5-hang` |

`while_sum` imports `anti.io` and `anti.text`. The copy registered
`dev_object_io` for it, which no test of the tree compiled before.

The first `wrong_total` did not compile. Its three listed tests passed in
0.01 s each, for a reason its finding did not name. "Tests" in `CLAUDE.md`
therefore says to read the output of a test before listing it.

## Where the task and the code differ

- The task gives `known_failures` three faults. The runner refuses six
  more:
  - a line of another form, and a test listed twice
  - a line whose row is marked `fixed`
  - a second row of one id, and a row without a grade and a state
  - a tree configured with another list
- ctest counts a test that its own time limit stops, or whose command
  crashes, as failed whatever `WILL_FAIL` says. A matrix test runs through
  `cmake -P`, which reports both as an ordinary failure. A test whose
  command is a program, as `unit_<file>` and `dev_object_<name>` are,
  cannot be listed for a crash.
- `antic_test_lto` gave a test of `--memory-checks` the option `--lto`
  under `ANTIC_TEST_LTO`, which antic refuses on macOS. It now leaves such
  a test alone.

## Counts and checks

- Tests: 1704 before and 1707 after, by `ctest --preset host -N`. The new
  ones are `matrix_files`, `known_failures` and `known_failures_runner`.
- Host suite: 1707 of 1707 passed, 0 expected failures, in 353 s, in
  `build/drive/logs/harness-host-suite.log`. Two skip, the
  `mimalloc_environment` tests.
- ASan: 1706 of 1706 passed in 490 s, in
  `build/drive/logs/harness-asan-suite.log`.
- UBSan: 1706 of 1706 passed in 435 s, in
  `build/drive/logs/harness-ubsan-suite.log`.
- The three trees built with zero warnings of our own code, in
  `build/drive/logs/harness-host-build.log`, `harness-asan-build.log` and
  `harness-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- Red before green: `known_failures_runner` failed without the runner, in
  `build/drive/logs/harness-kf-red.log`. Six changes of the runner, one
  per check, each failed one case of it.
- The docs-style checker reports `0 error(s), 0 warning(s)` on `CLAUDE.md`,
  `docs/decisions.md`, `docs/audit/tests-findings.md` and this report.
- Defects found: none. No id from T001 on is taken.

## Not done

- Neither VM ran the new tests. On windows-arm64 the test of
  `--memory-checks` is registered and disabled, and no host proved it.
- A line of the list has no form for a defect that shows on one host
  alone. Such a test passes on the other hosts and fails their suites.
- The list reaches the tests of `tests/CMakeLists.txt`. The 49 tests of
  `src/native/` cannot be listed.
- The ASan suite took 490 s, 50 s under the limit of 9 minutes. `CLAUDE.md`
  holds 402 s of 2026-10-04. The step did not look for the cause.
- The fresh build of the scratch copy printed six warnings of
  `alloc-override.c` of mimalloc, a third-party source under the flags of
  its own project. `build/drive/logs/harness-proof-build.log` holds them.

## Rules the documents do not state

None of the language. The three programs of the proof stand in the scratch
copy alone. The choices of the step are the three `[provisional]`
entries under "Repository layout" in `docs/decisions.md`.

## Proof

`git log --oneline -3` after the push of the four commits, before the
commit of this report:

```text
094bf032 State the rules of the suite in CLAUDE.md
bd76a644 Register the tests of tests/matrix by their file names
d16c1793 Add the list of the known failures and its test
```

`git rev-parse HEAD origin/main` after that push:

```text
094bf032cfb6625e5293b21d41bdab823df7b72b
094bf032cfb6625e5293b21d41bdab823df7b72b
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
