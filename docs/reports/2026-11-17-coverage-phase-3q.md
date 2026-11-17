# Batch 3q of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `599f9c3e` gave the ranking. The batch took the
three files that batch 3p named first and no batch had chosen by
branches: `src/antic/sema_class.c` with 97 missed branches,
`src/anti/fmt.c` with 90 and `src/antic/sema_safety.c` with 88. Each side
ran first through the antic or the anti of the coverage tree alone, with
a profile of its own, as 3p did. The sections of the three files in
`docs/notes/unreached-guards.md` were read first. Eddie's direction is
the Mac alone, so the VMs were not touched. The probes found three
defects, T286 to T288, all in `sets_stmt` of `sema_class.c`. The next
free id is T289. The phase goes on, since this batch found defects.

## Commits

- `2925d132`, `dffa21f5` and `28cf80ba` test the sides of
  `sema_class.c`, `fmt.c` and `sema_safety.c`.
- `8a4c39c0` lists T286 to T288.
- `4b766025`, `74d95490` and `9bd75c07` fix T286, T287 and T288.
- `d898a3d7` records the rule of the fixes and their commits.
- `f3260227` tests the path of a `switch` on an integer without `else`,
  which the coverage run after the fix of T288 left unreached.
- `48b4ee07` names the guards. One commit records the times and the
  counts in `CLAUDE.md` with this report.

## Findings

- T286, minor. A `construct` that sets its field in a `try` block whose
  handler is `catch fatal` was refused with the message of the field. The
  handler ends the program, so it adds no path now.
- T287, minor. A `construct` whose last statement is a `while true`
  without a `break` of its own, or a `do` whose body ends with `return`,
  was refused at its closing brace. The loop now ends the path as
  `sema_ends_path` reads it.
- T288, minor. A `construct` that sets its field in every arm of a
  `switch` without `else` on an enum or a variant was refused. Such a
  `switch` runs one of its arms, as `switch_ends` reads it.

Each fix follows R003 of "Rules of 2026-10-13". It follows the entry of
"Object model" as well, which asks a `construct` to set every field on
each path that succeeds. The entry after it records the three, without a tag.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 41400.
`cmake -DFIRST=41401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 41401 to
41600 at the default size in 385 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.58%, 4174 missed | 92.66%, 4126 missed |
| Regions | 98.51%, 1267 missed | 98.51%, 1268 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.72%, 1229 missed |

| File | Branches missed before | after | Lines missed before | after |
|---|---|---|---|---|
| `sema_class.c` | 97 | 70 | 2 | 4 |
| `fmt.c` | 90 | 72 | 10 | 10 |
| `sema_safety.c` | 88 | 84 | 10 | 10 |

The two lines of `sema_class.c` that the run after the fixes missed are
the path of a `switch` on an integer without `else`, which `f3260227`
then tests. The coverage suite ran 7565 tests, all passed, and took
984 s. The sides the three files keep are guards of the note, or are
counted in its last section.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7577`. One run covered the three test
  commits and the list of T286 to T288. The times stand in `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7575. Every test passed.
- The host runs 7577 tests and each sanitizer tree 7575.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Next candidates

By branches, `sema_call.c` misses 152, `sema_generic.c` 148 and
`sema_expr.c` 98, most of them sides of an error type that no batch
chased. Of the files no batch chose by branches, `antl_read_ir.c` misses
96, most of them values of a damaged library file, `lower_expr.c` 84,
`sema_const.c` 81, `sema_copies.c` 80 and `antl_tree.c` 75. The probes of
`sema_const.c` and `sema_copies.c` in this batch reached none of their
sides, and `lower_expr.c` was read and not probed.

## Questions for Eddie

1. The body of a `do ... while` runs at least once, by "Core language"
   and the correction C3 in `src/antic/sema_stmt.c`. The entry of
   "Object model" says that what a loop body assigns counts only inside
   it, since a loop body may not run. A `construct` that sets its field
   in the body of a `do` that goes on is therefore refused. Should the
   body of a `do` count as a path that runs, as definite assignment
   counts it?

Three questions stay open: `take` of an `own ?fn` field, the handler of
T278 and the reach of T281 across modules. Nothing here acts on them.
