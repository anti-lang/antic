# Batch 3i of the third phase of coverage

This step sorts the regions that `./c coverage` showed unreached in the
eight candidates that the report of batch 3h left:
`src/antic/pattern.c`, `src/antic/optimize.c`, `src/antic/sema_free.c`,
`src/antic/sema_hash.c`, `src/rt/atomic.c`, `src/antic/llvm_emit.c`,
`src/anti/bindwrite.c` and `src/antic/sema_safety.c`. It follows the
method of `docs/reports/2026-11-12-coverage-phase-3d.md`. Each region
first ran through the antic or the anti of the coverage tree alone, with
a profile of its own, so a probe showed the count of each line it
reached. Eddie's direction is the Mac alone, so the VMs were not
touched. The batch found one defect, T259, and fixed it. T260 is the
next free id.

## Commits

- `d68de982` tests `sema_free.c` and `sema_hash.c`.
- `10275081` lists T259, `1fbac4f6` fixes it, and `14a6ebd6` writes the
  commit of the fix into its row.
- `3824202a` tests `atomic.c` and `llvm_emit.c`.
- `9e3347a4` tests `pattern.c`, `bindwrite.c` and `sema_safety.c`.
- `923a1ca7` names the guards in `docs/notes/unreached-guards.md`.
- The commit of the times and the counts in `CLAUDE.md`, and this
  report.

## sema_free.c and T259

`free(p)` after `delete(p)` and `p as *byte` after a move are refused.
So is a read of an `own` field of `self` after `other.adopt(self.node)`,
whose message names the object and the function. A probe showed that
`Holder.drop(self.node)` and `Holder.adopt(other, self.node)` passed a
read of the field after the call. `walk_call` read the `own` marks of a
callee that is a name alone, and `T.f` keeps its callee as a field. The
same move of a local through `T.f` is refused, since the checker marks
it. T259 holds it, and the fix reads the function of either callee. The
message of `T.f` names the function alone, as the message of a local
moved in that form does. A function of another module was checked as
well and is refused already.

## sema_hash.c

A program drops the default hash of a struct, a variant and a tuple that
hold a class value. It drops that of a struct with a `Regex` too. None
gets the warning `unused-result`, since each part hashes through a
function. A refusal drops the hash of a `?*T`, which has no effects.

## atomic.c and llvm_emit.c

`test_rt_bounds.c` called the subtraction, the masks and the
compare-and-swap of one byte and of eight alone. It now calls every
operation at every width and checks that the bytes after a value stay as
they were. The 16 lines that stay unreached follow the `switch` of each
operation, which every arm leaves through `return`, so they belong to
the measurement. `test_llvm_emit.c` declares two C functions whose names
are no LLVM identifiers and checks their quotes and escapes. No Anti
item has such a name.

## pattern.c, bindwrite.c and sema_safety.c

A program matches `\N`, `\R`, `\X`, `\P{L}` in a class, a relative
reference, `(?n)`, an empty group and a repeat of one. The 18 lines of
`pattern.c` that stay unreached are guards behind PCRE2 and the
`return` after two switches. The description `api_types` of `anti bind`
holds a function as a parameter, written as a function pointer, and as
a field, which leaves its struct out with a warning. A synchronized
class returns the result of a function of the module and the `.ptr` of
a slice its caller gave. The `unchecked` clause in the header of a
concurrent class covers the fields of a type nested in it.

## optimize.c

Every line that the suite misses is a guard. The guards are the
defaults of two folds, a function without blocks and a block without
instructions. Two more are operands of a kind the callers never compare
and an offset that no field of a slot has.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.32%, 1419 missed | 98.41%, 1349 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.56%, 1379 missed | 98.60%, 1335 missed |
| Branches | 91.80%, 4592 missed | 91.88%, 4547 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `pattern.c` | 98.25%, 18 missed | 98.25%, 18 missed | 90.61% | 90.75% |
| `optimize.c` | 98.52%, 18 missed | 98.52%, 18 missed | 95.34% | 95.34% |
| `sema_free.c` | 98.11%, 18 missed | 99.06%, 9 missed | 93.01% | 93.98% |
| `sema_hash.c` | 96.65%, 17 missed | 98.82%, 6 missed | 90.98% | 92.62% |
| `atomic.c` | 69.23%, 16 missed | 69.23%, 16 missed | 78.12% | 100.00% |
| `llvm_emit.c` | 97.81%, 15 missed | 100.00%, 0 missed | 91.86% | 95.64% |
| `bindwrite.c` | 97.90%, 14 missed | 98.80%, 8 missed | 92.61% | 93.47% |
| `sema_safety.c` | 98.06%, 13 missed | 98.51%, 10 missed | 84.08% | 84.76% |

The lines that stay unreached in the eight files are guards of the note
or belong to the measurement. The sides of their conditions that the
batch did not sort stand under "Not in this note" there. The coverage
suite ran 7410 tests after, all passed. `./c coverage` took 1037 s.

## Suites

- The host suite ran before each of the five code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,<last>`. The times stand in
  `CLAUDE.md`. Every test passed apart from the two skips of
  `mimalloc_environment`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7420. Every test passed. The times stand in
  `CLAUDE.md`. The last part, `-I 6001,7420`, took 509 s under ASan and
  543 s under UBSan.
- The host runs 7422 tests and each sanitizer tree 7420.
  `tests/known-failures.txt` holds the two lines of `host=windows` alone.

## Next candidates

The files with the most missed lines that the note of unreached guards
has no section for yet are `src/antic/layout.c`, `src/rt/platform_posix.c`
and `src/rt/fault.c`, 13 lines each, then `src/anti/check.c`,
`src/antic/lower_simd.c`, `src/antic/lower_hash.c`,
`src/antic/sema_thread.c`, `src/antic/lower_owning.c` and
`src/anti/cache.c`, 12 each. `sema_thread.c` misses 52 branches and
`sema.c` 53.

## Questions for Eddie

None.
