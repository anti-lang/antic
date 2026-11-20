# Batch 3ah of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime against the descriptors and the
facts the LLVM text hands to opt. This batch read checks that no batch
had read: the checks of the closures in `src/antic/sema_closure.c`,
which missed 28 of its 252 branches, the lowering of a closure and a
snapshot in `src/antic/lower_function.c` and the dev checks of
`src/antic/lower_check.c`. The batch found four defects, T369 to T372,
and fixed two. Two stay open for a choice of Eddie. The next free id is
T373.

## Commits

- `1603756d` lists T369 to T372.
- `309bcd12` fixes T372 and `78b8717f` T369.
- `4abb6375` tests the calls through a capture and three refusals of an
  anonymous function, and `954be6e7` two more sides of the fix of T369.
- The docs commit records the times in `CLAUDE.md`, the commits of the
  fixes in `docs/audit/tests-findings.md`, the sides of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T369, major, fixed. A tuple local declared outside the block of a
  variable that a closure captures took the closure. So did an element
  of an array local. The lifetime check read a local of function type
  alone. An element of such a tuple and a name of its destructuring
  handed the closure to a local of an outer block. So did the variable
  of a `for` over such an array. A generic took such a tuple
  as a type argument and could keep it on the heap. A loop over such an
  array printed `0` and a NUL byte where the closure added 5.
- T370, severe, open. A local declared outside a loop keeps a
  `snapshot fn` made in its first turn. The call gives the value of the
  last turn. `lower_snapshot` of
  `src/antic/lower_function.c` gives a snapshot in the frame one slot
  per expression, so each turn writes over the record the local points
  at.
- T371, severe, open. A `snapshot fn` in a local holds a captured `str`
  without its bytes. Once the program frees the text, the call prints
  the bytes of a later one, and `--memory-checks` reports a use after
  free. The specification copies the bytes into the snapshot.
- T372, major, fixed. The dev check of `as` to an enum passed a source
  of the other sign whose bits widened to 64 equal a declared value. A
  `u64` above the largest `i64` met the `-1` of an enum over `i64`. A
  negative value met the largest `u64` of an enum over `u64`.

## Choices

- T369 takes the gap procedure, with one `[provisional]` line under
  "Anonymous functions and closures" in `docs/decisions.md`. A tuple or
  an array holds a closure as a local of function type does, so a value
  that outlives a capture is refused with "`t` outlives `x`, which the
  closure captures". A type argument holds each part of a function in
  the plain form, as it holds a function. No new message was needed.
- The listing first refused every closure in a tuple or an array that a
  `let`, a `=` or a call keeps. `program_eq_hash_agree` compares and
  hashes a tuple of two parameters of function type, and
  `error_expression_conversions` destructures a tuple of a snapshot, so
  the fix holds what outlives a capture alone. The three refusals of the
  listing gave way to four of those forms, and two of their positions
  in the listing were wrong.
- T372 adds the test of the sign only for an enum with a value whose top
  bit is set. Every other enum keeps its code, as the excerpt of
  `docs/site/developers-guide/08-structs-enums.md` shows.
- T370 and T371 stay open. Each fix changes the `[provisional]` entry
  that puts a snapshot of a local in the frame. It changes the DESIGN
  comment of `lower_snapshot` as well, so each waits for the question
  below.

## Read without a find

- The captures of nested anonymous functions and a closure that
  outlives a parameter of the anonymous function around it.
- The writes through a capture at a `concurrent` parameter: a call of
  a method, `&` of a captured struct and a write through a captured
  pointer.
- The closure of a generic, refused at its type parameter, and a pair
  of code and context copied by `=`.
- The checks of a bitfield, its saturation, unary `-`, a shift, a
  division and the bounds of a slice in `src/antic/lower_check.c`.

## Fuzzer

`cmake -DFIRST=44801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 44801 to
45000 at the default size in 159 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.37%, 3789 missed of 57134 | 93.37%, 3792 missed of 57236 |
| Regions | 98.52%, 1278 missed | 98.52%, 1284 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1231 missed | 98.73%, 1231 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_closure.c` | 28 of 252 | 20 of 290 |
| `src/antic/lower_check.c` | 6 of 124 | 5 of 132 |

The fixes add 102 branches. `src/antic/sema_expr.c`,
`src/antic/sema_stmt.c` and `src/antic/types.c` miss two or three more,
the sides of the fix of T369 that the section of the batch in
`docs/notes/unreached-guards.md` sorts. The commit `954be6e7` tests two
of them after the measurement. The "before" run had 7984 tests, all
passed, in 1067 s, and the "after" run 8004, all passed, in 926 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. `CLAUDE.md` names the
  tests that failed before a test or an excerpt stood right.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8014. Every test passed.
- The host runs 8016 tests and each sanitizer tree 8014.
  `tests/known-failures.txt` holds the two lines of `host=windows`, the
  line of T365, the two lines of T366 and the three lines each of T370
  and T371.

## Questions for Eddie

- T370 and T371: where does a snapshot that a local holds live? The
  `[provisional]` entry of 2026-09 puts it in the frame, as at a
  `concurrent` parameter, and its reason is that the caller waits. A
  local does not wait. The snapshot could go on the heap with the local
  as its owner, freed at the end of its block, which changes the entry
  and the DESIGN of `lower_snapshot`. Or a snapshot in a local made in a
  loop and kept outside it, and one that captures a `str`, could be
  refused, which needs two new messages.
- An enum without values: `types_enum_without_values` converts `0` to
  one with `as` in dev mode, and the check passes it, since it emits no
  test for an enum without values. The entry of T363 says no number fits
  such an enum, and `Object.deserialize` refuses every number. Should
  the check of `as` stop every value as well? The test would then fill
  its field another way.

The [provisional] entry of T369 in `docs/decisions.md` awaits review. No
finding of the batch overlaps T365, T366 or the question of the symbols
of a copy in `docs/reports/2026-11-20-answers-2026-11-20.md`.
