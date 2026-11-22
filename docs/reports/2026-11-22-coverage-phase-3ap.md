# Batch 3ap of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `6169f494` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the types of a
compilation in `src/antic/types.c`, which missed 58 of its 1002 branches,
and the helpers of the checker in `src/antic/sema.c`, which missed 50 of
its 674. It read them against the specifications, `docs/decisions.md`
and the DESIGN comments of the two files. Each side ran first through
the antic of the coverage tree alone, with a profile of its own. The
batch found two defects, T391 and T392, and fixed both. The next free id
is T393.

## Commits

- `77d6c8b3` tests twenty-one sides of the two files.
- `d5ce68a8` lists T391 and T392.
- `1a4bf376` fixes T391.
- `4cb50a66` fixes T392.
- `6c9e60c0` tests three more sides.
- `da070aa5` tests two deferred uses on two lines, a side that the run
  of coverage after the batch showed left.
- `89d5a25e` records the times and the counts in `CLAUDE.md`, the commits
  of the fixes in `docs/audit/tests-findings.md` and the sides of the
  batch in `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Findings

- T391, minor, fixed. `if let got = e { }` on a value that has an error of
  its own, `nope()`, or that `if let` refuses, an `int`, declared no
  name. A read of `got` in the block then got `` unknown name `got` `` as
  a second message of one mistake. The branch of `check_switch` in
  `src/antic/sema_stmt.c` for a value of the error type declared the
  name of an arm that binds the fields of a case alone. It now declares
  the bare name of an `if let` as a local of the error type as well, as
  the DESIGN of the branch says of the name an arm binds.
- T392, minor, fixed. `Inner` without type arguments in the body of
  `class Outer<T>`, for `class Inner<U>` nested in it, got
  `` `Outer.Inner` takes 2 type arguments ``. `Inner<int>` is right there,
  and `Inner<int, int>` gets `takes 1 type argument, found 2`.
  `resolve_named` of `src/antic/sema.c` counted the parameters of the
  class around the type as well. It now counts those of the nested type
  alone, as the provisional entry on nested types under "Generics and
  collections" gives it.

## Choices

- Neither fix adds a message or an option. No DESIGN comment changed.
- The tests of the sides are five programs and eighteen refusals of the
  matrix, and one refusal of `tests/errors/` with `--tests`.
  `modules_variant_without_fields` carries a variant whose cases hold no
  fields through a library file.

## Read without a find

- A struct literal of `Mutex` gets `` unknown struct `Mutex` ``. The table
  of the types that the compiler declares marks the types a literal
  builds, and `Mutex` is not one, so the literal finds no struct.
- The length `-(~size_of(H))` prints as `-~size_of(H)`, since R046 keeps
  the parentheses before a text that starts with `-` alone.
- A refused base that names another class leaves the warning of an
  abstract class that nothing fills. A base of another module names no
  class of the module.
- The name of a `catch` that is a function or a constant of the module
  gets no warning `shadowed-catch`, by the DESIGN of
  `sema_warn_catch_shadow`.

## Fuzzer

`cmake -DFIRST=46401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 46401 to
46600 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.41%, 3804 missed of 57710 | 93.46%, 3774 missed of 57722 |
| Regions | 98.51%, 1302 missed | 98.51%, 1299 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1257 missed | 98.72%, 1257 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/types.c` | 58 of 1002 | 49 of 1002 |
| `src/antic/sema.c` | 50 of 674 | 31 of 674 |

The fix of T391 adds 12 branches to `src/antic/sema_stmt.c`. The file
missed 73 of 1550 before and 74 of 1562 after. The one new side is a
guard of the arm of an `if let`, which the note names. Every side the
batch left in the two files is a guard. `docs/notes/unreached-guards.md`
names each, in the section of the batch or in those of the two files. The "before" run had 8123 tests in
1054 s, all passed. The "after" run had 8162, all passed, in 1068 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. It took 383 s, 274 s and
  378 s before the tests of the sides, up to test 8165.
  It took 342 s, 216 s and 359 s before the list of T391 and T392, up to
  test 8168. It took 379 s, 273 s and 374 s before the fix of T391, and
  374 s, 264 s and 370 s before that of T392. It took 381 s, 273 s and
  377 s before three more sides, up to test 8173. It took 378 s, 271 s
  and 377 s before the test of two lines, up to test 8174. Every test
  passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8172. `CLAUDE.md` holds the times. Every test
  passed.
- The host runs 8174 tests and each sanitizer tree 8172.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

The batch changed no program of `tests/programs/` and no runtime, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

None from this batch. T385, T387, the question on `anti fmt` and the
lines of a table, and question 1 of batch 3ao on `compare_swap` of an
`own` field wait for him as before.
