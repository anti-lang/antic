# Batch 3ak of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime, the facts the LLVM text hands to opt,
the closures, the C convention, the lowering of locks and the doc
warnings. This batch read code that no batch of the phases of coverage had
read: the clauses `allow` and `unchecked` in `src/antic/warnings.c`, the
lowering of `parallel`, `dispatch` and `join` in
`src/antic/lower_worker.c`, the checks of `--memory-checks` in
`src/antic/memcheck.c`, the text form of the IR in `src/antic/ir_print.c`
and the rows of the runtime in `src/antic/rt_abi.c`. Together they missed
32 of 584 branches. It read the clauses against "Errors, warnings and
checks" in `docs/decisions.md`, and the workers against decision 23 of
"Decisions of 2026-10-11" and "Threading" there. The batch found two
defects, T382 and T383, and fixed one. T382 stays open for a choice of
Eddie. The next free id is T384.

## Commits

- `673be4e3` tests the sides of the clauses, the workers and the memory
  checks.
- `3cf28ea2` lists T382 and T383.
- `b8b6152c` fixes T383.
- `f3504ff1` tests two more sides of the fix of T383.
- The docs commit records the times and the counts in `CLAUDE.md`, the
  commit of the fix in `docs/audit/tests-findings.md`, the sides of the
  batch in `docs/notes/unreached-guards.md` and this report.

## Findings

- T382, severe, open. A `dispatch` of a worker whose parameter is not
  `own` takes a local class value that holds an `own` field. The block of
  the local ends before the job runs. The job prints `local 8` where
  `local 42` is right, and `--memory-checks` reports a use after free in
  the thunk. The context of the job holds the bytes of the local, so its
  `own` field points at the object the caller tore down. The pointer-free
  test of a worker lets an `own` field through, and the job and the caller
  then reach one object.
- T383, minor, fixed. A clause `allow(unused-allow, ...)` around a clause
  `allow(undocumented, ...)` was reported in a build without
  `--doc-warnings`, so a release build refused the program. The clause
  inside waits for the build that runs its check, and the clause around it
  now waits with it.

## Choices

- T383 takes the gap procedure, with one `[provisional]` line under
  "Errors, warnings and checks" in `docs/decisions.md`. A clause of
  `unused-allow` or `unused-unchecked` that covers a clause of its kind
  whose check did not run waits with that clause. The reason of the entry
  before it, that another build decides for a check that did not run,
  gives the rule. No new message or option was needed, and no DESIGN
  comment changed.
- T382 stays open. Each fix changes a DESIGN comment or the rule a matrix
  program states, as the question below says.

## Read without a find

- The second round of the unused clauses. A clause of `unused-allow` or
  `unused-unchecked` silences the warning of a clause inside it, and one
  that silences nothing is a warning.
- `unused-result` at a function of the runtime: a row of the class none
  warns, and a row with other effects does not.
- The thunks of `dispatch` in a plain function and in two copies of a
  generic function of a library, in both modes. A worker named without a
  call takes its defaults in `parallel` and in `dispatch`.
- `join_all` of a dev build under `--no-hooks`, and `shuffle` of a
  vector on the heap under `--memory-checks`.

## Fuzzer

`cmake -DFIRST=45401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 45401 to
45600 at the default size in 426 s, while the coverage build ran beside
it. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.33%, 3841 missed of 57586 | 93.35%, 3833 missed of 57600 |
| Regions | 98.48%, 1321 missed | 98.48%, 1320 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1262 missed | 98.71%, 1262 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/warnings.c` | 5 of 62 | 3 of 76 |
| `src/antic/lower_worker.c` | 4 of 82 | 3 of 82 |
| `src/antic/memcheck.c` | 4 of 90 | 3 of 90 |
| `src/antic/ir_print.c` | 14 of 330 | 11 of 330 |
| `src/antic/rt_abi.c` | 5 of 20 | 3 of 20 |

The fix adds 14 branches, and the tests of `f3504ff1` reach the two of
them that the test of T383 left. Every side the batch left is a guard,
apart from the sides of `src/antic/ir_print.c` and of the outline in
`src/antic/antic.c` that the section of the batch in
`docs/notes/unreached-guards.md` names as unsorted. The "before" run
read `tests/errors/unused_result.anti` after the batch had extended it,
so it measured 3 of 20 for `src/antic/rt_abi.c`, and the run after batch
3aj, on the same file, gives the 5 of the table. The same run failed
`warning_unused_result` and `known_failures` for the files the batch
changed while it ran. It had 8052 tests in 1175 s, and the "after" run
8063, all passed, in 1031 s. `src/rt/mem.c` and `src/rt/sync.c` moved by
one or two, as the threads of the suite take other paths from run to
run.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. `CLAUDE.md` holds the
  times. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8073. Every test passed.
- The host runs 8075 tests and each sanitizer tree 8073.
  `tests/known-failures.txt` holds the two lines of T136 and T138, as
  before the batch, and the three lines of T382.

## Pinned outputs

None moved. The fix changes a warning alone, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

- T382: how does a `dispatch` hold a local argument that owns something,
  given to a parameter that is not `own`? The ways are these. The local
  moves into the job, as it does into an `own` parameter. The job takes a
  copy, as `dup` makes one, and tears it down. Or the checker refuses such
  a local there, and a fresh value or `dup(x)` stands in its place. Each
  changes the DESIGN comment of `worker_args` in
  `src/antic/sema_thread.c`: "the arguments of a `dispatch` are stored
  per job, by decision 23 of "Decisions of 2026-10-11", so the job holds
  them as a call holds its arguments. A local passed to an `own`
  parameter moves into the job, when job is true, and the caller tears it
  down no more." The first two change the DESIGN comment of
  `call_worker` in `src/antic/lower_worker.c` as well: "a fresh argument
  of a parameter that is not `own` has no owner but the job of a
  `dispatch`, as the argument of a call has no owner but its caller. Its
  thunk tears it down in the context once the worker has returned, when
  drop is true, and the worker copies what it keeps." The DESIGN comment
  of `pointer_free` in `src/antic/types.c` gives the reason for `parallel`
  alone: "an `own` field belongs to one object, so no two chunks reach
  the same memory through either." `tests/matrix/ownership_values_in_a_dispatch.anti`
  states that such a local "stays with the caller".

The `[provisional]` entry of T383 in `docs/decisions.md` awaits review.
No finding of the batch overlaps the open question of batch 3aj on a doc
line that starts with `|`.
