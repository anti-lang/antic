# Batch 3av of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `d2e3d363` gave the ranking. The batch read two
files that no batch chosen by branches had read: the check
`use-after-free` in `src/antic/sema_free.c`, which missed 31 of its 548
branches, and the checks of simd structs in `src/antic/sema_simd.c`,
which missed 31 of its 358. It read both against "Compile-time checks of
2026-10-20" and "Simd structs" in `docs/decisions.md`, the specifications
and the DESIGN comments of the two files, and probed each side with the
antic of the host tree. The probes of E2 led to the same two defects in
C1, in `src/antic/sema_uninit.c`. The batch found six defects, T413 to
T418, and fixed all six. The next free id is T419. The batch before found
five, and this one found six, so the phase goes on.

## Findings

- T413, minor, fixed. E2 read a `defer` inside a deferred statement, and
  the `defer` of a closure there, where it stood. A `defer` that held a
  `defer` reading `p.n` and then `delete(p)` passed, and the program read
  freed memory on every run. A deferred statement keeps the `defer`
  statements inside it in a list of its own now, so each runs where its
  block is left.
- T414, major, fixed. `walk_assign` marked an `=` that tears nothing down
  outside a deferred statement alone. A `defer` that held `delete(self.n)`
  and then `self.n = alloc Node { }` tore the freed node down, and the
  program of
  `--memory-checks` stopped in `anti_rt_delete_from`. The builds without
  the checks printed what they expect, so the memory checks test alone was
  listed.
- T415, minor, fixed. `free(p)` in a closure after `destroy(p)` in the
  body was refused, while the same `free(p)` in the body passes.
- T416, minor, fixed. E2 took a `try` before a call inside a `try` block
  as an exit of the function. It forwards to the handler of the block, by
  R016. A read in the handler of a value the block deleted passed, and an
  `undo` of the function that never runs there was refused.
- T417, minor, fixed. C1 had the defect of T416.
- T418, minor, fixed. C1 read a `defer` inside a deferred statement, and
  the `defer` of a closure there, nowhere.

## Commits

- `b5fff73c` lists T413 to T418 with ten programs of `tests/matrix/`.
- `c87867c8`, `352b2d3f`, `2c4ddc3b`, `573c07f7`, `e4961a2a` and
  `8f167746` fix T413 to T418. The fixes of T413 and T416 write again the
  expected message of one refusal each, which the list commit wrote with
  the wrong lines.
- `17bbb3ab` tests the sides of E2 and of the checks of simd structs.
- `a75e9eba` tests three sides that the coverage run after `17bbb3ab`
  still missed. It adds the library tree `tests/modules/moves`, whose
  function with an `own` parameter is no copy of a generic.
- `dfdd10f8` records the times, the counts, the hashes of the fixes and
  the section "The sides of batch 3av" of
  `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Fuzzer

`cmake -DFIRST=47601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 47601 to
47800 at the default size in 157 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.55%, 3734 missed of 57934 | 93.60%, 3706 missed of 57920 |
| Regions | 98.51%, 1303 missed of 87433 | 98.51%, 1301 missed of 87400 |
| Functions | 99.87%, 6 missed of 4774 | 99.87%, 6 missed of 4774 |
| Lines | 98.71%, 1261 missed of 98074 | 98.71%, 1261 missed of 98060 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_free.c` | 31 of 548 | 25 of 542 |
| `src/antic/sema_simd.c` | 31 of 358 | 10 of 358 |
| `src/antic/sema_uninit.c` | 27 of 488 | 27 of 480 |

The "before" run had 8315 tests in 1088 s. The run after the batch, at
`a75e9eba`, had 8360 in 1109 s. Every test passed in each. The fixes of
T413 and T418 remove the tests of a missing list of pending statements,
and the fix of T415 adds `outer_of`. Every side of `sema_free.c` and
`sema_simd.c` that the after run misses has a row in the note, under the
heading of its file or under "The sides of batch 3av".

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. The parts took 299 s to
  381 s, 235 s to 282 s and 425 s to 481 s. `CLAUDE.md` holds each.
  Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8370. `asan` took 251 s, 180 s, 270 s, 165 s,
  113 s and 641 s, and `ubsan` 258 s, 183 s, 268 s, 165 s, 126 s and
  676 s. Every test passed.
- The host runs 8372 tests and each sanitizer tree 8370.
  `tests/known-failures.txt` holds the two lines of T136 and T138, three
  lines each of T385, T387, T404 and T407, and the line of T412.

## Pinned outputs

No fix moves an output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None new. T385, T387, T404, T407, T412, the question on `anti fmt` and
the lines of a table, question 1 of batch 3ao on `compare_swap` of an
`own` field, question 1 of batch 3aq on the two messages of `provides`,
the two questions of batch 3as and the question of batch 3au on the
field marks of a page of `anti doc` wait for him as before.
