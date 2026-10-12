# The flow area of the test matrix

The step `flow` of the test run, on 2026-10-12, on `main`. It wrote the
tests of the area flow of `docs/notes/test-matrix.md`. No file under `src/`
changed.

## What the step added

- `tests/matrix/` holds 114 sources: 82 programs, each in dev, release and
  `--memory-checks`, and 32 refusals.
- 24 programs `flow_<construct>` hold one construct each in every context
  its row left open, one function and one line of output per context.
  They cover 253 cells.
- 58 more programs and the refusals hold `return` without a value, the
  kinds of value, the exits of decision 19 and the places where two rules
  meet. "Flow" in the note lists them with the triples.
- `anti_test_flow` runs `tests/anti-test/com/example/flow` under
  `anti test`: 49 tests, each construct in a `tests` block and in a
  `fixtures` block. `tests/run_anti_test_module.cmake` is its runner.
- `flow_modules` holds each construct in a plain and in a generic function
  of a library file, `tests/modules/flow/com/example/flow`.
- The note marks 317 cells and 44 kinds. One cell cannot exist: a function
  of a `tests` block returns no value.
- Eight messages left `tests/diagnostics-open.txt`.
- `CLAUDE.md` holds the counts, the times and two rules under "Tests".

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 30 lines.

- T001, major. Decision 19 is not held. antic crashes on `return`,
  `break`, `continue`, `fail` and `try` inside a `defer`, and on `fail` and
  `try` inside an `undo`. It compiles the other three inside an `undo`.
- T002, major. antic crashes on a `yield` inside a `defer` of a handler.
- T003, major. antic crashes on a failing call that a `defer` in a `try`
  block holds.
- T004, major. A closure reads the name of an `if let` as a value that may
  be `none`, and the program is refused.
- T005, severe. A closure in a handler reads another word of memory for
  the error that `catch e` binds. The program prints a wrong number.
- T006, minor. The refusal of a `defer` in a `defer` stands at the
  statement after it.
- T007, major. An `if let` on a `?T` whose arm and `else` both return does
  not end the path of its function. The same form on a case of a variant
  does.

## Where the task and the code differ

- The task names labels and `unreachable`. Neither is built, as
  `docs/anti-syntax-overview.md` says, so neither has a test.
- "A cell that compiles is a program" gave 253 programs at first. They
  added 190 s to the host suite, since macOS checks every fresh executable
  in turn. "Repository layout" in `docs/decisions.md` keeps a suite under
  9 minutes by shared builds, so one program per construct holds its
  cells. A cell that shows a defect has a program of its own.
- No build of the matrix keeps a `tests` or a `fixtures` block or reads a
  library file. Those three contexts stand in the two tests above, which
  took two entries in `tests/CMakeLists.txt` and one new runner.
- The note wrote `dispatch f(a)` and an arm `A, B => s`. The parser takes
  `dispatch p -> f(a)` alone and one value per arm, and the note follows it.
- Decision 20 is of the area errors, and this step wrote no test of it.

## Rules the documents do not state

- Decision 19 does not say whether a `break` or a `continue` is refused
  when its loop stands inside the same `defer`. The tests hold that it
  compiles and runs, in `in_defer` and `in_undo` of `flow_break` and
  `flow_continue`.
- Decision 19 gives no text. The refusals expect `` `return` inside a
  `defer` `` and `` `break` inside an `undo` ``, each with its own word.
- `yield` inside a `defer` is in no document. The test of T002 expects a
  refusal with `` `yield` inside a `defer` ``.
- A `worker fn` that may fail is declared and called without a message. At
  `dispatch` and at `parallel` it is refused with `` `run` takes 1 argument
  beside the object, found 0 ``, which counts the out pointer as an
  argument. The two refusals hold that text.
- A `switch` whose arms all return and a `while true` do not end the path
  of a function.
- A deferred statement that holds a closure runs the closure at each exit.
  An `undo` is accepted in a function that cannot fail and never runs.
- `yield v` computes `v` before the deferred statements of the handler run.
- A `for` whose low end is above its high end runs no turn, with `by -1`
  as well.
- `switch` on a `char`, a `bool` or a value of a type parameter is refused.

## Counts and checks

- Tests: 1711 before and 1992 after, by `ctest --preset host -N`.
- Host suite: 1992 of 1992 passed in 376 s, 30 of them expected failures.
  Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/flow-host-suite2.log` holds the run.
- ASan: 1986 of 1986 passed in 503 s, and UBSan 1986 of 1986 in 445 s, in
  `flow-asan-suite.log` and `flow-ubsan-suite.log`. Both ran before T007
  changed three sources. The 28 tests that change touches passed in both
  trees after it, in `flow-asan-suite-delta.log` and
  `flow-ubsan-suite-delta.log`. Each tree registers 1991 tests now.
- The three trees built with zero warnings, in `flow-host-build2.log`,
  `flow-asan-build2.log` and `flow-ubsan-build2.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `build/drive/logs/flow-known-v.log` holds the output of eight listed
  tests. Each fails for the reason its finding names.
- Red before green: `anti_test_flow` and `flow_modules` each failed on a
  changed expected file, in `flow-antitest-red.log` and
  `flow-modules-red.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the note,
  the findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- The expected output of a generated program comes from a model of the
  rule. The generator held the model. It is a scratch script under
  `build/scratch/flow/` and is not committed. No cell of the 253 gave
  another value than its model, in any of the three modes.
- A refusal of decision 19 states the position of the exit. The fix may
  choose the `defer` instead, and then changes the `.err` file.
- `flow_modules` runs in release and dev, not under `--memory-checks`.

## Proof

`git log --oneline -3` after the push of the six commits, before the
commit of this report:

```text
7531de62 State the flow tests and the new counts in CLAUDE.md
0bb17fec Add the finding T007 on an if let whose arms all return
e96688de Mark the flow cells of the test matrix
```

`git rev-parse HEAD origin/main` after that push:

```text
7531de623f73492f481eadfb0530fab3b45c65d9
7531de623f73492f481eadfb0530fab3b45c65d9
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
