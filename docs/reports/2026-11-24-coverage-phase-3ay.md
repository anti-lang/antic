# Batch 3ay of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `a5b90f9f` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the driver of antic
in `src/antic/driver.c`, which missed 26 of its 896 branches, and the
pattern methods, the match as a condition and `if let` on a value that
may be `none` in `src/antic/sema_pattern.c`, which missed 25 of its 438.
It read them against "Regular expressions" in
`docs/anti-language-additions.md`, the entries of patterns under
"Regular expressions" in `docs/decisions.md`, `docs/notes/patterns.md`
and the DESIGN comments of both files. It probed each side with the antic
of the host tree and of the coverage tree. The batch found T429 and fixed
it. The next free id is T430. The batch before found six, and this one
found one, so the phase goes on.

## Findings

- T429, major, fixed. A match whose failure `catch none` counts as `none`
  overflowed the stack of antic where it stood as a condition, in `if`,
  after `!`, `&&` and `||`, as `if line.matches(r) catch none { }` and
  `if first(line, r) catch none { }` with a function that may fail.
  `test_of` of `sema_pattern.c` moves the call into a new node and writes
  the comparison with `none` over the old one. A failing call names its
  own node as the place of its result, so the moved call named the
  comparison, and the copy of a tree in `sema_copies.c` walked the two
  without end. The moved call now names itself.

## Commits

- `f00bedec` lists T429, and `4ceefc1d` fixes it.
- `616a0ba2` tests the sides of the two files. `762eb89a` tests a match
  of a function that cannot fail as a condition, the other side of the
  line the fix added.
- `28d150c5` records the times, the counts, the hash of the fix and the
  section "The sides of batch 3ay" of `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Choices

- The case `anti_test_message_project_cached_library_walk_unread` of
  `tests/anti-test/messages.cmake` makes the thirty-third read fail, a
  read of the walk of the imports of a cached library file. That read
  goes through `read_file` of `driver.c` without a message, and the run
  compiles the file again, by Q5 of "Answers of 2026-11-11". The profile
  of the coverage tree named the read.
- `antic -c` of a module whose `extern fn` has another signature than the
  same function of a library it imports writes its library file. A
  program that imports it is refused by the reader of library files, as
  R127 says. The library file of one module is no program, so the batch
  counts no defect there. `write_library` lowers into an IR that holds no
  function of a library, so its check of two signatures never fails, and
  the row of `write_library` under `src/antic/driver.c` in
  `docs/notes/unreached-guards.md` stands.

## Fuzzer

`cmake -DFIRST=48201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 48201 to
48400 at the default size in 162 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.61%, 3726 missed of 58320 | 93.63%, 3715 missed of 58324 |
| Regions | 98.50%, 1318 missed of 88011 | 98.50%, 1319 missed of 88015 |
| Functions | 99.88%, 6 missed of 4803 | 99.88%, 6 missed of 4803 |
| Lines | 98.70%, 1278 missed of 98627 | 98.70%, 1278 missed of 98630 |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/driver.c` | 26 of 896 | 25 of 896 |
| `src/antic/sema_pattern.c` | 25 of 438 | 13 of 442 |

The "before" run had 8438 tests in 1099 s. The run after the batch had
8460 in 1099 s, before the commit `762eb89a`. Every test passed in each.
Every side of `sema_pattern.c` that the after run misses has a row in
`docs/notes/unreached-guards.md`, apart from the side of line 783 that
`762eb89a` reaches. Every side of `driver.c` that it misses has a row
there, under its own heading or in the section of the batch.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code, and `CLAUDE.md` holds each
  time. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8473. `asan` took 243 s, 179 s, 272 s, 168 s, 108 s
  and 640 s, and `ubsan` 254 s, 181 s, 270 s, 169 s, 117 s and 773 s.
  Every test passed.
- The host runs 8475 tests and each sanitizer tree 8473.
  `tests/known-failures.txt` holds the two lines of T136 and T138.

## Pinned outputs

The fix moves no output, so `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

1. A match of one pattern literal does not reach a variable that holds a
   match of another, and a match of a compiled pattern does not reach one
   of a literal. Both are refused with `` expected `?Match`, found
   `?Match` ``, which names two types alike. A pointer to the match of a
   literal is refused at a `*Match` parameter the same way, with
   `` expected `*Match`, found `*Match` ``, while the match itself reaches
   a `Match` parameter. Should the message name the literal of each type,
   and in what form? Should a pointer to the match of a literal reach
   `*Match` as the value reaches `Match`?
