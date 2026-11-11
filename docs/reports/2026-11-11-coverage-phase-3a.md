# Batch 3a of the third phase of coverage

This step sorts the regions that `./c coverage` of 2026-11-10 showed
unreached in three files of the checker: `src/antic/sema_const.c`,
`src/antic/sema_expr.c` and `src/antic/sema_generic.c`. It follows the
method of `docs/reports/2026-11-10-coverage-phase-2b.md`. Eddie's
direction is the Mac alone, so the VMs were not touched. The batch adds
tests and fixes the five defects that they showed. It deletes no code but
one test that the fix of T232 left dead.

## Commits

- `52142878` tests `sema_const.c`. `4f2850d5` lists T231, `60bc6335`
  fixes it.
- `ea0f4409` tests `sema_expr.c`. `fcd2b431` lists T232 and T233,
  `1e4c07fa` fixes T232 and `de1aa250` fixes T233.
- `c4b8850a` tests `sema_generic.c`. `c6eaa15d` lists T234 and T235,
  `4f3792fc` fixes T234 and `2ed505d7` fixes T235.
- `9cb5e097` reaches three lines of the fixes that the coverage run after
  them showed unreached.
- `b370d840` names the guards in `docs/notes/unreached-guards.md`.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the five fixes in `docs/audit/tests-findings.md`.

## Findings

- T231, major, fixed. A length of a copy of a generic whose operation
  leaves its type once the copy puts its constant in place compiled and
  wrapped: `[(N as u8) as int]int` of `Box<300>` held 44 elements. A
  constant of the same operation is refused by R026, R029 and R031, and
  T107 and Q13 of "Answers of 2026-10-24" hold a copy to the rules of a
  constant. The check that Q13 added for `<<%` now finds a sum, a
  difference, a product, a negation and a narrowing conversion as well,
  with the message of the constant. `error_generic_length_overflow` and
  `error_generic_length_minus_least` got the message of the back end that
  the length is below 1 and now get that of the constant.
- T232, minor, fixed. Unary `-` and `~` checked an operand that is no
  literal twice, so `-missing` gave `unknown name` twice.
- T233, minor, fixed. A struct had an `operator fn iter` at module level
  that names an unknown type. A `for` over it gave a second message at the
  loop, which T161 removed for the hook of a class.
- T234, major, fixed. `v * 2.0` against `operator fn mul(self, k: T)` of a
  `Vec<f64>` was refused, where a variable of `f64` compiles, as T176
  holds for an operator of a module.
- T235, major, fixed. In the body of `Vec<T>`, `self.plus(*self)` against
  `other: Vec<U>` bound no `U`. In the body a value of the class has the
  generic itself as its type, and `unify` bound one copy against another
  alone. T234 is the same gap seen from `self`.

## sema_const.c

A program folds NaN on both sides of a comparison, a value of `size_of`
stored in a bitfield, every pair of sides of `&&` and `||` in a copy and
a length `size_of(T) * N`. Refusals reach a float of seventeen digits in
the message of an `as`, a pointer conversion and an unknown name in a
constant, a default that calls a function named `mul_high` or indexes by
a call, and `<<%` below zero inside a sum. Sixteen rows of guards stand
in the note.

## sema_expr.c

`error_expression_operands` holds twelve mistakes of an operand, one
message each: `dup` of a union and of a Mutex value, `?.` after an `int`
and on a field of a call, slices, `alloc`, a tuple, an array literal, a
conversion and an interpolation of a name that is not there, and `try`
before a local that may be `none`. A refusal gives an `own fn` field the
value `h.cb ?? f` of a `keep own` parameter. Five rows of guards stand in
the note, among them the arms of `check_expr_inner` for nodes that the
checker makes typed.

## sema_generic.c

A program compares copies of a class whose module gives it `operator fn
eq<T: Float>`. `error_generic_arguments_unbound` holds an operator of a
module and of a class that take an `int` where `[2]T` stands, and calls
that walk a tuple naming one copy twice and a variant of a type parameter.
The programs of T234 and T235 reach the code of those fixes. Nine rows of
guards stand in the note.

## Branches left

The sides of a condition that only an error type or a NULL operand takes
were not chased where the line around them is reached, about 400 sides in
the three files. Most are `!sema_is_error(t)` after a check that gave a
message, or a short circuit of a test whose other operand decides. The
false sides of `one_value` in `sema_fold_symbolic`, the sign of a
negation spelled after `-` and the unsigned sides of `defined_integer`
were not sorted.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.08%, 1609 missed | 98.12%, 1575 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.30%, 1611 missed | 98.33%, 1585 missed |
| Branches | 91.16%, 4899 missed | 91.24%, 4859 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `sema_const.c` | 97.21%, 37 missed | 97.58%, 33 missed | 91.60% | 92.54% |
| `sema_expr.c` | 98.66%, 30 missed | 98.75%, 28 missed | 89.76% | 90.72% |
| `sema_generic.c` | 98.06%, 41 missed | 99.02%, 21 missed | 88.42% | 89.88% |

The run before the batch measured the tree at `692feb41`, which gave
4899 missed branches where the report of batch 6 gave 4900. The lines
that stay unreached in the three files are the guards of the note. The
last coverage suite ran 7193 tests, all passed, and `./c coverage` took
967 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- The first run of the fix of T231 failed `error_generic_length_overflow`
  and `error_generic_length_minus_least`, whose files of expected output
  the fix changes, and the refusal of `300 as byte`, which now spells the
  type as the file does. Before the fix of T234 was committed, its own test
  failed alone. It held `v * 2`, and an integer literal is no `f64`, so
  the test holds `2.0`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7203. Every test passed. `asan` took 248 s,
  187 s, 265 s, 187 s, 448 s and 435 s. `ubsan` took 251 s, 189 s,
  262 s, 199 s, 474 s and 457 s.

## Provisional lines

No new provisional line. The fixes follow T107, T161, T176 and Q13.

## Questions for Eddie

None. The guards stay by the standing answer, and the five defects
follow rules that the specifications and the findings decide.
