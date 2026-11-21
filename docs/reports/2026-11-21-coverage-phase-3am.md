# Batch 3am of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime, the facts the LLVM text hands to opt,
the closures, the C convention, the lowering of locks, the doc warnings,
the clauses, the workers, the checks of `--memory-checks`, the LLVM text
of a function and the file helpers of `anti`. This batch read code that
no batch chosen by branches had read: the safety check `escaping-local`
in `src/antic/sema_escape.c`, which missed 55 of its 666 branches, and
the passes of the optimizer in `src/antic/optimize.c`, which missed 93 of
its 1116. Batches 3c and 3i had sorted the lines of the two files alone.
It read the check against E1 of "Compile-time checks of 2026-10-20" in
`docs/decisions.md`, its correction of 2026-10-25 and its
`[provisional]` entries, and the optimizer against the DESIGN comments of
the file. The batch found three defects, T384 to T386, and fixed two.
T385 stays open for a choice of Eddie. The next free id is T387.

## Commits

- `9b723528` tests the sides of the conversions of float constants and
  of a `join` after `?.`.
- `e36b729c` lists T384 to T386.
- `0d51be4b` fixes T384 and `88dd4394` T386.
- `e7454b23` records the times and the counts in `CLAUDE.md`, the commits
  of the fixes in `docs/audit/tests-findings.md` and the sides of the
  batch in `docs/notes/unreached-guards.md`. `54f08bf2` names there the
  side of the fix of T386 that no test reaches.
- The docs commit of this report.

## Findings

- T384, minor, fixed. `let (p, n) = t` of a tuple local `t = (&x, 5)`
  gave the `int` `n` the reference to `x` that `t` holds. E1 then refused
  a `return n`, a store of `n` into a local of an outer block and one into
  a field behind a parameter. A name of a destructuring now holds the
  reference of the value only where its type can hold one, as the name an
  arm of a `switch` binds does.
- T385, minor, open. A tuple local `t = (&x, &y)` of an inner block holds
  the references of two locals. Its record names one, the deeper `x`, so
  its second part, taken by `let (a, b) = t` or read as `t.1`, cannot be
  stored into `q` of the outer block of `y`. E1 refuses both, though the
  part holds `&y`, and the check refuses only what it proves.
- T386, minor, fixed. A `switch` between a `dispatch` that takes `&c`
  and its `join`, with an arm that ends in `fallthrough`, made E1 refuse
  the `dispatch`. The scan for a way out of the block counted every
  `fallthrough` as one, though it goes on to the next arm of a `switch`
  inside the block. A `switch` the scan enters now keeps its own
  `fallthrough`, as a loop keeps its `break`.

## Choices

- No gap procedure, no new message or option, and no DESIGN comment
  changed. The comment of `may_leave` names the `switch` beside the loop
  and the handler.
- T385 stays open, since each fix changes the DESIGN comment of
  `struct escape_local`, as the question below says.
- `fold_conversion_limits` of `tests/programs/` runs in release mode
  alone, since a dev build stops a conversion out of range.

## Read without a find

- The conversions of a float constant that `fold_conversion` of
  `src/antic/optimize.c` leaves to the back end. The batch read a value
  below the range of a signed type, one above it, one below 0 for an
  unsigned type, and NaN. Release
  mode gives the least or the greatest value and 0, as the DESIGN of the
  file says. The tests of batch 3al took their operands from a loop, so
  the fold saw none of them.
- A `join` in an argument of a call after `?.` joins the job on no path
  E1 can prove, and the `dispatch` is refused.
- The slot pass: two offsets of one slot are apart only in the three
  cases of its DESIGN. A struct has no field of 0 bytes, since an array
  length is at least 1 and a struct has a field.
- The store forwarding: a call, a copy of memory and every simd
  operation wipe what the pass holds. An atomic operation is a call in
  the IR.

## Fuzzer

`cmake -DFIRST=45801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 45801 to
46000 at the default size in 380 s, while the coverage build ran beside
it. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.37%, 3821 missed of 57600 | 93.37%, 3818 missed of 57614 |
| Regions | 98.49%, 1313 missed | 98.49%, 1313 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1260 missed | 98.71%, 1260 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/optimize.c` | 93 of 1116 | 92 of 1116 |
| `src/antic/sema_escape.c` | 55 of 666 | 57 of 680 |

The fix of T386 adds 14 branches to `src/antic/sema_escape.c`. Every
side the batch left is a guard that `docs/notes/unreached-guards.md`
names, in the section of the batch or in an earlier one. The sides that
the section names as unsorted are the exception. The "before" run had
8071 tests, all passed, in 1155 s, and the "after" run 8083, all passed,
in 1069 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. It took 392 s, 276 s and
  369 s before the tests of the sides, up to test 8086. Up to test 8095
  it took 391 s, 273 s and 376 s before the list. It took 388 s, 273 s
  and 373 s before the fix of T384, and 397 s, 276 s and 377 s before
  that of T386. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8093. `CLAUDE.md` holds the times. Every test
  passed.
- The host runs 8095 tests and each sanitizer tree 8093.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T382 and the three lines of T385.

## Pinned outputs

`tests/emit-identity/programs.sha256` gains the six lines of
`fold_conversion_limits`, written with `WRITE=yes`, and no other line
moved. `tests/link-identity/return42.macos-arm64.sha256` stands as it
was, since the batch changed no runtime.

## Questions for Eddie

- T385: should E1 follow the reference of each part of a tuple, a struct
  or an array local, so a part that holds the reference of an outer local
  may be stored where that local lives? The fix changes the DESIGN
  comment of `struct escape_local` in `src/antic/sema_escape.c`: "The
  pass records each local and parameter with the depth of its block and
  the line of the brace that ends it, and the local whose reference it
  holds." The other way keeps the check as it is, and the refusal
  stands, which `unchecked(escaping-local, "...")` overrules.
  `tests/matrix/checks_escaping_local_passes_a_tuple_part_with_an_outer_reference.anti`
  holds the test, with its three lines in `tests/known-failures.txt`.
- Eddie answered T382 and the question of batch 3aj on a doc line that
  starts with `|` on 2026-11-21, after this batch had started. The step
  that follows it builds both.
