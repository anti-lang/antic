# Batch 3ag of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
batches before it read readers of input, writers whose output a reader
reads again and the reflection runtime against the descriptors. This
batch read a writer that no batch had read: the facts the LLVM text
gives opt, in `src/antic/llvm_emit_facts.c`, `src/antic/llvm_emit_nan.c`
and the attributes of `src/antic/llvm_emit_call.c`. It read them against
the rules that allow each fact, and then the readers of the runtime that
write a value the facts describe. Eddie's direction is the Mac alone, so
the VMs were not touched. The batch found four defects, T363 to T366,
and fixed two. Two stay open for a choice of Eddie. The next free id is
T367.

## Commits

- `a3f21394` lists T363 to T366.
- `65068ce2` fixes T363 and `23abd583` T364.
- The docs commit records the times in `CLAUDE.md`, the commits of the
  fixes in `docs/audit/tests-findings.md`, the sides of the batch in
  `docs/notes/unreached-guards.md` and this report.

## Findings

- T363, severe, fixed. `Object.deserialize` stored a number that the
  enum of its field does not declare. It did so in a field, an element
  of an array, the value of a `?T` and a field of a struct. A release build
  loads an enum with the `!range` of its values, so opt folded a test of
  the value that the field failed.
- T364, severe, fixed. `reflect.set` wrote such a number into an enum
  field, with the same result.
- T365, severe, open. `reflect.call` passes such a number to an enum
  parameter, and the function runs with it. The trampoline is shared by
  the text of a signature, which names the integer of the enum alone.
- T366, severe, open. The parameter of an `own` pointer is `noalias` in
  the LLVM text. A plain pointer to the same object may stand beside it.
  A release build then loses a write through the plain pointer, which
  the dev build keeps. "Attributes and metadata" of
  `docs/work-order-llvm-back-end.md` gives a reason for the attribute.
  The owner is the only holder by the rules of the language, it says,
  and no rule says so.

## Choices

- T363 and T364 take the gap procedure, with one `[provisional]` line
  each under "Object model" in `docs/decisions.md`. A field, an element
  and a part of an enum carry a descriptor of the enum, with one record
  per value and the value in its word. The type id stays the integer, so
  the DESIGN comment of `type_id` holds and every walk keeps its sizes.
  `reflect.set` fails with `WRONG_KIND`, since the freeze lets in no new
  code. No new message or option was needed.
- T365 and T366 stay open. Each fix changes a form that another decision
  fixed, so each waits for the questions below.

## Pinned outputs

- `tests/emit-identity/programs.sha256` took the new program of T366.
  The fix of T363 wrote it again, since its descriptors move the
  constants of every program with an enum field.
- The golden `tests/dump/arith.dev.macos-arm64.ll` holds the descriptor
  of `arith.Kind`, and `records_type_ids` of `tests/unit/test_lower.c`
  names it.
- Both fixes change the runtime, so the digest of
  `tests/link-identity/return42.macos-arm64.sha256` is written twice.

## Read without a find

- The `!tbaa` tree against "Aliasing of views". It read the fields of a
  base and of a class, a union, a simd struct and a narrowed `?T`. A
  tuple and a generic copy over a type and its alias name one node.
- The ranges of the loads of a `bool` and an enum, and the `nsw` and
  `nuw` of the step of a range loop.
- The canonical NaN of the result of an export function, and the memory
  effects of the rows of the runtime against their C bodies.

## Fuzzer

`cmake -DFIRST=44601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 44601 to
44800 at the default size in 158 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.38%, 3777 missed of 57016 | 93.38%, 3777 missed of 57036 |
| Regions | 98.51%, 1282 missed | 98.51%, 1281 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1236 missed | 98.73%, 1236 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/llvm_emit_facts.c` | 12 | 12 |
| `src/antic/llvm_emit_nan.c` | 20 | 20 |
| `src/antic/lower_desc.c` | 65 | 65 |

The fixes add 20 branches, and the tests of T363 and T364 reach each
side. The sides of the two files of the facts that the suite misses
stand in the section of the batch in `docs/notes/unreached-guards.md`.
`src/anti/cache.c`, `src/anti/fmt.c`, `src/rt/platform_posix.c` and
`src/rt/sync.c` moved by one to three, as the threads and the clock of
the suite take other paths from run to run. The "before" run failed
`fmt_canonical` and `known_failures`, since the tests of the list stood
in the tree while it ran. The "after" run had 7977 tests, all passed,
and `./c coverage` took 1051 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. `CLAUDE.md` names the
  tests that failed on a pinned output before it was written again.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7987. Every test passed.
- The host runs 7989 tests and each sanitizer tree 7987.
  `tests/known-failures.txt` holds the two lines of `host=windows`, the
  line of T365 and the two lines of T366, the program and its variant
  for macos-x86_64.

## Questions for Eddie

- T365: how should `reflect.call` learn the values of an enum parameter?
  The text of a signature could name the enum, so each enum gets a
  trampoline that checks its values. Or the record of a function could
  carry a descriptor per parameter, which changes `struct anti_function`
  for a plugin built before. The first touches the spelling of a
  signature, which the question of 3af on the structural hash reads.
- T366: should `noalias` leave the parameter of an `own` pointer? The
  other way is a rule that no plain pointer reaches the object while the
  call runs, which no checker can hold in general.

The [provisional] entries of T363 and T364 in `docs/decisions.md` await
review. No finding of the batch overlaps answers A or D of "Answers of
2026-11-19" or the questions of the reports before.
