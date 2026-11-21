# Batch 3ai of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again, the reflection runtime against the descriptors, the facts
the LLVM text hands to opt and the closure checks and lowering. This
batch read code that no batch had read: the lowering of locks, channels
and `select` in `src/antic/lower_sync.c`, which missed 7 of its 88
branches, and the classification of the C convention in
`src/antic/abi.c` with the members of a layout in `src/antic/layout.c`.
It compared the signatures antic writes with the ones clang writes for
the same C structs, on macos-arm64 and macos-x86_64. The batch found
three defects, T373 to T375, and fixed two. One stays open for a choice
of Eddie. The next free id is T376.

## Commits

- `35a4e7e2` lists T373 to T375.
- `86da0898` fixes T374 and `6d8f2750` T373.
- `bce41cd4` tests `sync` through pointers and an arm of `select` that
  binds no name.
- The docs commit records the times in `CLAUDE.md`, the commits of the
  fixes in `docs/audit/tests-findings.md`, the sides of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T373, severe, fixed. A struct of one or two simd structs of 16 bytes
  passed as the lanes of each. macos-arm64 passed a float aggregate of
  lanes or two integer registers. macos-x86_64 passed two eightbytes of
  floats. C passes the vector types of the fields: a homogeneous vector
  aggregate in vector registers, and one vector register under System V.
  A C function read `one 1` where `one 30` is right.
- T374, severe, fixed. A struct of floats whose `align(N)` leaves bytes
  after them passed as a homogeneous float aggregate on macos-arm64. C
  passes one of 16 bytes in two integer registers and a larger one as a
  pointer to a copy. The program ended with a segmentation fault.
- T375, severe, open. A struct of three or four simd structs of 16 bytes
  passes as a pointer to a copy on macos-arm64. C passes it in three or
  four vector registers. The program ends with a bus error.

## Choices

- T373 and T374 take the gap procedure, with one `[provisional]` line
  each under "Compiler behaviour" in `docs/decisions.md`. A float
  aggregate is homogeneous when its members fill it. A simd struct of 16
  bytes inside an aggregate is one member, a vector of its lanes. One to
  four vectors of any lanes that fill the aggregate are a homogeneous
  vector aggregate, as clang counts vectors of one size alike. Under
  System V a vector is SSE and SSEUP, and a union of a vector and an
  integer stays INTEGER and SSE. No new message or option was needed.
- The words of `struct abi_param` grow from 16 to 24 bytes, so that
  `[2 x <2 x double>]` fits.
- T375 stays open. Its fix records the members of an aggregate above 32
  bytes, which changes the DESIGN comment of `LAYOUT_MEMBER_LIMIT` in
  `src/antic/layout.h`. That comment says every convention passes a
  larger aggregate in memory, and an AAPCS64 vector aggregate of four
  members holds 64 bytes. It waits for the question below.

## Read without a find

- `sync a, b` through pointers, its exits by `return`, `break`,
  `continue` and `fail`, and `sync p, p` as one lock.
- The teardown of what `recv` and `select` give, a `select` in a loop
  and the copy of the context of `dispatch` in `src/rt/threads.c`.
- The conversions of a float to an integer, which saturate and give 0
  for NaN in both modes. The listing of a directory in `src/rt/fs.c` and
  the parser of a C type of `src/anti/bindtype.c`.
- Nineteen further shapes of struct against clang on both Mac targets,
  arrays of floats, mixed eightbytes, `bool` fields and nested structs
  among them. Each signature puts the bytes in the same registers.

## Fuzzer

`cmake -DFIRST=45001 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 45001 to
45200 at the default size in 220 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.37%, 3793 missed of 57236 | 93.38%, 3793 missed of 57280 |
| Regions | 98.52%, 1282 missed | 98.52%, 1281 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1231 missed | 98.74%, 1231 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/abi.c` | 3 of 136 | 7 of 176 |
| `src/antic/layout.c` | 16 of 406 | 16 of 410 |
| `src/antic/lower_sync.c` | 7 of 88 | 6 of 88 |

The fixes add 44 branches. Two of the four new misses of
`src/antic/abi.c` are a guard and two a union that a test can reach.
The section of the batch in `docs/notes/unreached-guards.md` sorts them.
The tests of `bce41cd4` reach the arm without a name. `sync` through
pointers was reached already, and `sync a, b` of two locals is the side
that stays. `src/rt/platform_posix.c`, `src/rt/sync.c`, `src/rt/mem.c`
and `src/rt/signal.c` moved by one to four, as the threads of the suite
take other paths from run to run. The "before" run had 8004 tests, all
passed, in 1063 s, and the "after" run 8016, all passed, in 1516 s,
while other programs of the Mac ran.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. `CLAUDE.md` names the
  tests that failed before two programs and three C files stood right.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8026. Every test passed.
- The host runs 8028 tests and each sanitizer tree 8026.
  `tests/known-failures.txt` holds the two lines of `host=windows`, the
  line of T365 and the two lines of T366. It holds the three lines each
  of T370 and T371 and the line of T375.

## Questions for Eddie

- T375: may the members of an aggregate be recorded up to 64 bytes? The
  DESIGN comment of `LAYOUT_MEMBER_LIMIT` in `src/antic/layout.h` stops
  at 32, with the reason that every convention passes a larger aggregate
  in memory. AAPCS64 passes four vectors of 16 bytes in four vector
  registers. The other way is a test of the vectors on the IR type of the
  aggregate, outside the members. The comment would then hold for
  scalars alone.

The [provisional] entries of T373 and T374 in `docs/decisions.md` await
review. No finding of the batch overlaps T365, T366, T370, T371 or the
questions of `docs/reports/2026-11-20-answers-2026-11-20.md` and
`docs/reports/2026-11-20-coverage-phase-3ah.md`.
