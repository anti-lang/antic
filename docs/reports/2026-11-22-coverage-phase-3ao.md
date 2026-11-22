# Batch 3ao of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `a020b4e2` gave the ranking. The batch read two
files that no batch chosen by branches had read: the statements and the
function bodies of the checker in `src/antic/sema_stmt.c`, which missed
81 of its 1550 branches, and the atomics, the joins and the workers in
`src/antic/sema_thread.c`, which missed 45 of its 490. It read them
against the specifications, `docs/decisions.md` and the DESIGN comments
of the two files. Each side ran first through the antic of the coverage
tree alone, with a profile of its own. The batch found one defect, T390,
and fixed it. The next free id is T391.

## Commits

- `5d569254` tests ten sides of the two files.
- `ef69160a` lists T390.
- `86611dda` fixes T390.
- `3708bafa` records the times and the counts in `CLAUDE.md`, the commit of
  the fix in `docs/audit/tests-findings.md` and the sides of the batch in
  `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Findings

- T390, severe, fixed. `compare_swap` on an `own` field of a concurrent
  class that `unchecked(unguarded-field)` marks stored the pointer it was
  given. The field then owned an object that the local still held, so
  `delete(n)` and `delete(s)` freed it twice, and the run under
  `--memory-checks` stopped on a SEGV. `unchecked_swap` of
  `src/antic/sema_thread.c` read the form of the field, which is plain for
  an `own` field, and not its `own` mark. The call is now refused:
  `` `compare_swap` takes an atomic field or a plain one, and `top` is an
  `own` field ``.

## Choices

- T390 adds one message. The entry of 2026-09-24 under "Concurrent
  classes" lets `compare_swap` take a plain field, and an `own` field
  cannot be atomic. Neither message of the call fits the case, since each
  names a fix that does not apply. The fix of the defect therefore needs a
  message of its own, which the freeze lets in. A `[provisional]` entry
  under "Concurrent classes" records the refusal. No DESIGN comment
  changed.
- The tests of the sides are six programs and four refusals of the
  matrix, besides the refusal of T390.

## Read without a find

- A `let` that names a function type takes a snapshot and owns it, as 4
  of "Answers of 2026-11-21" gives it. A `let` of `?fn` refuses one, since
  it borrows the function it holds.
- A switch arm `(size_of(i64) * 2) as u8` beside `16` keeps both arms, and
  the first runs, as the entry of R010 says for a size with an operator.
- `join_all` of a slice to a bound read at run time, and of 65 jobs, adds
  no job to those the checker follows. A second `dispatch` of an object
  whose job is in flight starts nothing, so the test of the bound uses two
  objects.
- A `dispatch` fills in a left-out `?fn` parameter from its default. A
  function is not a constant, so no default of a plain `fn` exists.
- The warning `never-fails` of an anonymous function stands in
  `src/antic/sema_closure.c`, which the suite reaches already.

## Fuzzer

`cmake -DFIRST=46201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 46201 to
46400 at the default size in 155 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.38%, 3818 missed of 57708 | 93.40%, 3807 missed of 57710 |
| Regions | 98.49%, 1312 missed | 98.51%, 1302 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1260 missed | 98.72%, 1257 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_stmt.c` | 81 of 1550 | 73 of 1550 |
| `src/antic/sema_thread.c` | 45 of 490 | 36 of 492 |

The fix adds 2 branches to `src/antic/sema_thread.c`, and the suite
reaches both. Every side the batch left is a guard that
`docs/notes/unreached-guards.md` names, in the section of the batch or in
the sections of the two files. The "before" run had 8102 tests in
1093 s, all passed. The "after" run had 8123, all passed, in 1058 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. It took 386 s, 276 s and
  378 s before the commit of the tests of the sides, up to test 8134. It
  took 385 s, 276 s and 367 s before the list of T390, up to test 8135,
  and 383 s, 273 s and 373 s before its fix. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8133. `CLAUDE.md` holds the times. Every test
  passed.
- The host runs 8135 tests and each sanitizer tree 8133.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

The batch changed no program of `tests/programs/` and no runtime, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. T390. `compare_swap` now refuses an `own` field. The other choice is
   to let it take one and count the swap as a move of its second
   argument, as `=` does. A swap that fails then leaves a value that the
   checker counts as moved. One that succeeds drops the object the field
   held without freeing it. The refusal is the smaller option. Which one stands?

T385, T387 and the question on `anti fmt` and the lines of a table wait
for him as before.
