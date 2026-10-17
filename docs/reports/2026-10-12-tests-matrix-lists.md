# The lists of the matrix and the two guards

The step of the test run that writes the lists the matrix steps work from,
on 2026-10-12, on `main`. The task names no id for the step, so this report
takes `matrix-lists`. No file under `src/` changed.

## What the step added

- `docs/notes/test-matrix.md` holds the constructs, the contexts, the kinds
  of value and the matrix in eight areas. Each list names the file it was
  read from.
- `tests/runners/run_diagnostics_covered.cmake` is the test `diagnostics_covered`,
  and `tests/runners/run_options_covered.cmake` the test `options_covered`.
- `tests/diagnostics-open.txt` and `tests/options-open.txt` hold what has no
  test today.
- `tests/runners/run_diagnostics_covered_self.cmake` and
  `tests/runners/run_options_covered_self.cmake` are the tests
  `diagnostics_covered_runner` and `options_covered_runner`. Nine and nine
  cases build a small tree each, one the runner accepts and eight it
  refuses, each with the text of its message.
- `CLAUDE.md` names the guards under "Tests" and holds the new counts.

## The counts

The lists, from `src/antic/lexer.c`, the parser files and `ast.h`:

- 100 keywords, 64 symbols, one reserved word and 27 contextual words.
- 10 item kinds, 22 statement kinds, 38 expression kinds and 10 type kinds.
- 19 contexts of a statement or an expression, 7 of an item and 38 kinds.

The matrix has 206 rows:

| Area | Rows | Cells held | Cells open | Kinds held | Kinds open |
|---|---|---|---|---|---|
| Flow | 27 | 137 | 318 | 47 | 44 |
| Values | 40 | 297 | 456 | 423 | 617 |
| Types | 32 | 131 | 83 | 0 | 0 |
| Ownership | 18 | 79 | 170 | 45 | 80 |
| Errors | 15 | 49 | 124 | 11 | 30 |
| Threads | 29 | 71 | 335 | 52 | 144 |
| Classes | 26 | 47 | 9 | 0 | 0 |
| Modules | 19 | 29 | 1 | 0 | 0 |
| All | 206 | 840 | 1496 | 578 | 915 |

The 18 cells of the rules of decisions 19 to 27 are open beside these.

The guards:

- `diagnostics_covered`: 748 messages, 494 expected by a test and 254 open.
  antic gives 598 with 116 open, and anti 150 with 138 open.
- `options_covered`: 134 options and commands, 124 passed by a test and 10
  open. The open ones are `--help`, `--soname` and `--strip-docs` of antic,
  and of anti `--help`, `-h`, `symbols --from`, `build --strip-docs`,
  `build --soname`, `check -I` and `bind -D`.

## How the cells were read

`antic --tests --dump-ast` printed the tree of 694 of the 710 `.anti` files
under `tests/`, and `--dump-types` the checked tree of 513. A script under
`build/scratch/matrix/` read each tree and noted, per construct, the contexts
of its ancestors and the kinds of its types. The scripts are scratch work and
are not committed. "How the lists were read" in the note states the method
and its three limits.

## Where the task and the code differ

- The task has the guard read the four functions of `diagnostics.h`. Nearly
  every message reaches them through `sema_error_at`, `parser_error_here`
  and five more functions, so the runner reads those calls as well.
- "Every error message of `src/anti/`" is every text written to stderr with
  `fprintf`, `fputs` and `vfprintf`. A format with fewer than two words
  beside `anti:` hands a text on and counts as none. Six formats do.
- "A test passes an option" cannot be read from `tests/CMakeLists.txt`
  alone: the dump tests pass `--dump-${stage}`. The guard asks ctest for the
  command line of every registered test.
- The expected outputs include the C files of `tests/unit/`. They hold about
  135 messages of antic that no `.err` file holds.
- `raw_output` refused the first runner, which read the output of ctest into
  a variable. ctest now writes into a file. `feb50d4e` holds the change.
- A group of a CMake pattern that matched nothing leaves `CMAKE_MATCH_<n>`
  unset. The first runner skipped `src/anti/bind.c` for that reason, and
  the self-test found it.

## Counts and checks

- Tests: 1707 before and 1711 after, by `ctest --preset host -N`.
- Host suite: 1711 of 1711 passed, 0 expected failures, in 346 s, in
  `build/drive/logs/matrix-lists-host-suite.log`. Two skip, the
  `mimalloc_environment` tests.
- ASan: 1710 of 1710 passed in 483 s, in
  `build/drive/logs/matrix-lists-asan-suite.log`.
- UBSan: 1710 of 1710 passed in 442 s, in
  `build/drive/logs/matrix-lists-ubsan-suite.log`.
- The three trees built with zero warnings of our own code, in
  `build/drive/logs/matrix-lists-host-build2.log`,
  `matrix-lists-asan-build.log` and `matrix-lists-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected file
  of either changed.
- Red before green: each guard failed with an empty list, in
  `build/drive/logs/matrix-lists-diag-red.log` and
  `matrix-lists-opt-red.log`. Twelve changes of the runners, one per check,
  each failed a case of a self-test, in `matrix-lists-mutations.log`. Two of
  them left a self-test green at first, and two cases were added for them.
- The docs-style checker reports `0 error(s), 0 warning(s)` on
  `docs/notes/test-matrix.md`, `CLAUDE.md` and this report.
- Defects found: none. No id from T001 on is taken.

## Not done

- Neither VM ran the guards. `options_covered` reads the tests a tree
  registers, so a host that registers other tests may count another option
  as passed.
- The guard reads none of the 112 lines of `src/antic/` that name stderr
  themselves, such as the one of `antic: unknown option`.
- About a dozen messages of anti reach a test in part, as `is no archive`
  does. The guard counts them as open.
- antic takes `--dependency` from one test alone, and `anti build` passes
  it in every build with a dependency. The guard sees the first.
- The matrix has no row for a module of the standard library. Its rows are
  constructs of the language.
- A held cell says that a source holds the construct in that place, and not
  what its test checks.

## Rules the documents do not state

None of the language. The step ran no new program. One fact of the tools:

- `antic --tests --dump-ast` prints no function of a `tests` or a `fixtures`
  block, although `--tests` keeps both blocks.

## Proof

`git log --oneline -3` after the push of the four commits, before the
commit of this report:

```text
e55738a4 State the two guards and the matrix note in CLAUDE.md
feb50d4e Write the test listing of options_covered into a file
cde8de2a Add the note of the test matrix
```

`git rev-parse HEAD origin/main` after that push:

```text
e55738a45bd05c74452e676d32e0f7174c087b91
e55738a45bd05c74452e676d32e0f7174c087b91
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
