# Batch 3aj of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime, the facts the LLVM text hands to opt,
the closures, the C convention and the lowering of locks. This batch read
code that no batch had read: the doc warnings of `anti check` in
`src/antic/sema_doc.c`, which missed 29 of its 332 branches, and the
section of the lock orders of a library file in `src/antic/antl_locks.c`,
which missed 7 of its 50. It read the doc warnings against "Doc markup"
in `docs/tooling-addendum.md`, the entries on the doc warnings in
`docs/decisions.md` and I of "Answers of 2026-11-19". The batch found
four defects, T378 to T381, and fixed all four. The next free id is T382.

## Commits

- `ec4492fd` tests the sides of the doc warnings and of the lock section.
- `5ee7d639` lists T378 to T381.
- `93e11b87` fixes T378, `bd3ea838` T379, `c086582d` T380 and `6975c7f2`
  T381.
- The docs commit records the times and the counts in `CLAUDE.md`, the
  commits of the fixes in `docs/audit/tests-findings.md`, the sides of the
  batch in `docs/notes/unreached-guards.md` and this report.

## Findings

- T378, minor, fixed. A line of a doc comment that starts with `1)` gave
  no warning of a numbered list, and lines that start with `#5` and
  `3.14` gave a warning of a heading and of a numbered list. I of
  "Answers of 2026-11-19" reads a heading as a word of `#` alone and an
  item as a word of digits and `.` or `)`, as the comment of
  `opens_markup` in `src/anti/fmt.c` says. `src/std/anti/text.anti` and
  `src/std/anti/graphics.anti` lose a warning each, for `1.29` and `0.0`.
- T379, minor, fixed. A `//#` note on a `pub` function of a class body
  without a `///` comment gave no warning. The note warning read the
  items at module level alone, where `--warn-undocumented` reads the
  members as well.
- T380, minor, fixed. Strong emphasis, `**never**`, gave no warning. An
  opener that the same delimiter followed opened nothing.
- T381, minor, fixed. A type parameter named in the doc comment of its
  item, `T` of `larger<T: Ordered>`, resolved to nothing.

## Choices

- No fix needed the gap procedure. Each follows a rule that the
  specification, an entry or an answer states. No new message or option
  was needed, and no DESIGN comment changed.
- `docs/notes/check.md` names the rule of each fix of the markup and of
  the names.

## Read without a find

- The lock section of a library file, written and read again, and cut at
  every length. Its writer writes three counts of 0 for a module without
  locks, and the reader keeps no section of zero entries.
- The forms of the subset in the doc warnings. Those are fenced blocks,
  spans cut over two lines, `-` lists and links. An HTML tag that closes
  or closes itself is a warning.

## Fuzzer

`cmake -DFIRST=45201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 45201 to
45400 at the default size in 225 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.29%, 3854 missed of 57450 | 93.33%, 3832 missed of 57474 |
| Regions | 98.48%, 1320 missed | 98.48%, 1318 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1259 missed | 98.71%, 1259 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/sema_doc.c` | 29 of 332 | 8 of 356 |
| `src/antic/antl_locks.c` | 7 of 50 | 6 of 50 |

The fixes add 24 branches. Of the 8 left in `src/antic/sema_doc.c`, two
are guards and six sides that a test can reach, four of them in the code
of the fix of T378. The section without functions of `ec4492fd`
reaches one side of `src/antic/antl_locks.c`. Its cut files reach the
section of the lock orders, but the test of the bytes left refuses each
count before a loop of the reader fails inside an entry. The section of the
batch in `docs/notes/unreached-guards.md` sorts both.
`src/rt/platform_posix.c`, `src/rt/sync.c` and `src/rt/mem.c` moved by
one or two, as the threads of the suite take other paths from run to
run. The "before" run had 8030 tests, all passed, in 1527 s, and the
"after" run 8038, all passed, in 1085 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, while `fseventsd` of the
  Mac ran beside it, so the first part took about 600 s each time.
  `CLAUDE.md` holds the times. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8048. Every test passed.
- The host runs 8050 tests and each sanitizer tree 8048.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  line of T365 and the three lines of T377, as before the batch.

## Pinned outputs

None moved. The doc warnings write no code, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

- A line of a doc comment that starts with `|` is a warning of a table,
  whatever follows, and `doc_markup.anti` pins `| one | two |` alone as
  one. Markdown renders a table only with a row of dashes under its
  first row. Should the warning ask for that row? No DESIGN comment
  stands in the way, and the case `warning_doc_markup` would change.

No finding of the batch overlaps T365, T377 or the question on `=` of a
local that owns a snapshot.
