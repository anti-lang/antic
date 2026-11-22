# Batch 3an of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `22025232` gave the ranking. The batch read two
files that no batch chosen by branches had read: the safety check
`index-out-of-range` in `src/antic/sema_index.c`, which missed 53 of its
540 branches, and the safety check `lock-order` and the warning
`blocking-under-lock` in `src/antic/sema_lock.c`, which missed 46 of its
500. Batch 3e had sorted the lines of the two files alone. It read the
first against C2 of "Compile-time checks of 2026-10-20" in
`docs/decisions.md`, its `[provisional]` entries and the DESIGN comments
of the file. It read the second against C4, C5 and theirs. Each side ran
first through the antic of the coverage tree alone, with a profile of its
own. The batch found two defects, T388 and T389, and fixed both. The next
free id is T390.

## Commits

- `9f27f170` tests five sides of the two files.
- `a7482e54` lists T388 and T389.
- `79f7f91a` fixes T388 and `6b7a09b2` T389.
- `50443fe7` records the times and the counts in `CLAUDE.md`, the commits
  of the fixes in `docs/audit/tests-findings.md` and the sides of the
  batch in `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Findings

- T388, minor, fixed. The flags form `(r, f) = x + 2;` assigns `r`, but
  `mark_changed` of `src/antic/sema_index.c` marked a name alone as
  changed. `r` kept the value 9 of its `let`, and C2 refused `xs[r]` on
  an array of length 4, where `r` holds 3. A tuple as the target of an
  assignment now marks each name in it.
- T389, minor, fixed. The checker moves the `catch fatal` of a call that
  gives a `?*T` onto the guard of its `let`. `whole_stmt` read the
  handlers of calls alone. A turn of a `for` that may end the program at
  such a `let` therefore counted as one that runs to its end. C2 refused
  `xs[i]` on the last turn, which the program never reaches. The guard of
  a `let` now ends a turn as the handler of a call does, through the one
  helper `hands_on`.

## Choices

- No gap procedure, no new message or option, and no DESIGN comment or
  `[provisional]` entry changed. The fix of T389 holds the provisional
  entry on the turns of C2, which names `catch fatal` among what may end
  a turn. The comment of `struct whole_walk` names the `let`.
- The tests of the sides are four programs of the matrix and one refusal
  of `tests/errors/` against the library tree `lock_orders`, which
  `tests/CMakeLists.txt` lists.

## Read without a find

- Two places that each take `b` while they hold `a`, beside one that
  takes them in the other order, get one refusal each. The entry on C4
  refuses each edge that closes a cycle. The second place closes the
  cycle again once the first leaves the graph, so each place needs its
  own `unchecked`.
- A class below may replace a public function of a class. A call of such
  a function goes through the table, and C4 does not follow it, as its
  entry says.
  A probe whose base function takes a lock that its replacement does not
  passed.
- A Mutex of a base reached through a pointer of a class below takes the
  name of the class that declares it. So does the hidden lock of a
  synchronized base reached through a class below. Two nested types of
  one name in two classes are two locks.
- C2 reads `by -k` as the values of `by k`, as `docs/decisions.md` does.
  Three things end a turn for it: a handler of a call that returns, an
  arm that returns and a `return` in a loop inside the body. It reads
  `?.`, whose result is a pointer, as a form whose arguments may not run.
- A constant that `size_of` gives folds to an integer, and C2 follows it.

## Fuzzer

`cmake -DFIRST=46001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 46001 to
46200 at the default size in 357 s, while the coverage build ran beside
it. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.37%, 3826 missed of 57696 | 93.38%, 3818 missed of 57708 |
| Regions | 98.49%, 1314 missed | 98.50%, 1311 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1260 missed | 98.71%, 1260 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_index.c` | 53 of 540 | 51 of 552 |
| `src/antic/sema_lock.c` | 46 of 500 | 43 of 500 |

The two fixes add 12 branches to `src/antic/sema_index.c`, and the suite
reaches every one of them. Every side the batch left is a guard that
`docs/notes/unreached-guards.md` names, in the section of the batch or
in the sections of the two files. The "before" run had 8089 tests in
1092 s, and one failed: `known_failures`, since the batch wrote its list
while the run read it. The "after" run had 8102, all passed, in 1154 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, up to test 8114. It took
  401 s, 282 s and 393 s before the commits of the tests of the sides and
  of the list. It took 390 s, 275 s and 378 s before the fix of T388, and
  392 s, 275 s and 379 s before that of T389. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8112. `CLAUDE.md` holds the times. Every test
  passed.
- The host runs 8114 tests and each sanitizer tree 8112.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

The batch changed no program of `tests/programs/` and no runtime, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None from this batch. T385, T387 and the question on `anti fmt` and the
lines of a table wait for him as before.
