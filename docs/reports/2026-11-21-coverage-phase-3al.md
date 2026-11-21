# Batch 3al of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime, the facts the LLVM text hands to opt,
the closures, the C convention, the lowering of locks, the doc warnings,
the clauses, the workers and the checks of `--memory-checks`. This batch
read code that no batch of the phases of coverage had read: the LLVM
text of a function and its instructions in
`src/antic/llvm_emit_function.c`, which missed 41 of its 714 branches,
and the file helpers of `anti` in `src/anti/files.c`, which missed 22 of
its 180. It read the frame against the two entries on frames of `2^31`
bytes under "Compiler behaviour" in `docs/decisions.md`, the conversions
and the divisions against the DESIGN comments of the file, and the file
helpers against their DESIGN comments and the messages of their callers.
The batch found no defect. The next free id stays T384.

## Commits

- `ed9f0255` tests the sides of the frame limit and of the file helpers
  of `anti`.
- The docs commit records the times and the counts in `CLAUDE.md`, the
  sides of the batch in `docs/notes/unreached-guards.md` and this report.

## Findings

None. Every side the batch reached behaves as its rule says.

## Choices

- No gap procedure, no new message or option, and no DESIGN comment
  changed.
- The new refusals of the frame build for macos-arm64, as
  `frame_limit_arm64` does, and name ARM64 in their message.
- `frame_limit_align` takes an array of 2147483595 bytes, so the
  temporaries of `main` and the array leave the frame 4 bytes below the
  limit, and the alignment of the `u64` after it starts its slot past the
  limit. A change of the temporaries of `main` moves that side, and the
  refusal still holds.

## Read without a find

- The sum of the frame, in three cases. Each local stands below the
  limit and their sum above it. Two locals of `2^31` bytes give one
  message. A slot starts past the limit through its alignment. The entry on S21 of the second audit asks
  for each sum to be checked before it is made, and no test held it.
- The conversions of a float to every integer width in release mode: a
  value out of range saturates and NaN gives 0. The division of the least
  `i8` by -1 gives the least value with a remainder of 0, and a division
  by 0 gives 0 with a remainder of 0.
- `as f16` of a constant: `fold_conversion` of `src/antic/optimize.c`
  folds it in both modes, so the LLVM text never sees one.
- The failures of the file helpers. The close of the lock and of the
  copy of a program fails. So does the stat of the program whose
  permissions the copy takes. An old library file of `anti test` cannot
  be removed, and the read of a configuration of `anti symbols` fails.
  Each gives the message of its caller.
- A walk over a link that leads nowhere lists nothing for it, as the
  DESIGN of `walk` says.

## Fuzzer

`cmake -DFIRST=45601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 45601 to
45800 at the default size in 430 s, while the coverage build ran beside
it. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.34%, 3835 missed of 57600 | 93.37%, 3817 missed of 57600 |
| Regions | 98.48%, 1320 missed | 98.49%, 1313 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1262 missed | 98.71%, 1260 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/llvm_emit_function.c` | 41 of 714 | 39 of 714 |
| `src/anti/files.c` | 22 of 180 | 8 of 180 |

Every side the batch left is a guard that
`docs/notes/unreached-guards.md` names, in the section of the batch or
in an earlier one. The sides that the section names as unsorted are the
exception. One of those is a count of 0 in `files_array`. It stands
inside the argument of `ANTI_FAULT`, so the annotated source shows it in
the expansion of the macro alone. The "before" run had 8063 tests, all passed, in 1171 s,
and the "after" run 8071, all passed, in 1051 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,8083` before the commit of the tests: 390 s, 275 s and
  367 s. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8081. `CLAUDE.md` holds the times. Every test
  passed.
- The host runs 8083 tests and each sanitizer tree 8081.
  `tests/known-failures.txt` holds the two lines of T136 and T138 and the
  three lines of T382, as before the batch.

## Pinned outputs

None moved. The batch adds tests alone, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None from this batch. T382 and the question of batch 3aj on a doc line
that starts with `|` stay as batch 3ak left them. The
`[provisional]` entry of T383 in `docs/decisions.md` awaits review.
