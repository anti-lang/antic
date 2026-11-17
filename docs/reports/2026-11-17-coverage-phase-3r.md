# Batch 3r of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `48164d96` gave the ranking. The batch took the
sides of an error type in `src/antic/sema_call.c`, `sema_generic.c` and
`sema_expr.c` that batch 3q named. It took `lower_expr.c`, `sema_const.c`,
`sema_copies.c` and `antl_read_ir.c` as well. Each side ran first through
the antic or the unit tests of the coverage tree alone, with a profile of
its own, as 3q did. The section of 3p and 3q in
`docs/notes/unreached-guards.md` was read first. Eddie's direction is the
Mac alone, so the VMs were not touched. The probes found one defect,
T289. The next free id is T290.

## Commits

- `3b219eba`, `ab941fed`, `c579cbfc`, `c5b5bcc1` and `7fff2caf` test the
  sides of `sema_call.c`, `sema_generic.c`, `sema_const.c`, `sema_expr.c`
  and `lower_expr.c`.
- `e1da985c` tests damaged signature records and fields of aggregates in
  `unit_modules`, for `antl_read_ir.c`.
- `2cec9d03` lists T289, and `9388483d` fixes it.
- `8554b00d` records the rule of the fix and its commit.
- `9e0daabd`, `359711c8` and `d88aca91` name the guards. One commit
  records the times and the counts in `CLAUDE.md` with this report.

## Findings

- T289, major. The element of a tuple that holds a bound function
  stopped antic with a segmentation fault. It was read by its index into
  a `let`, through a pointer to the tuple or into another tuple literal.
  `build_value_into` took every field of a bound function type for the
  form `c.area`. It now takes that form for a field that names a
  function, and copies any other as its two words.

The specification is not silent, since a bound function is a value of
two words. The entry beside the rules of bound functions has no tag.
No `[provisional]` entry was added.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 41600.
`cmake -DFIRST=41601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 41601 to
41800 at the default size in 381 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.67%, 4122 missed | 92.71%, 4101 missed |
| Regions | 98.51%, 1267 missed | 98.51%, 1265 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.72%, 1227 missed |

| File | Branches missed before | after |
|---|---|---|
| `antl_read_ir.c` | 96 | 83 |
| `sema_generic.c` | 148 | 142 |
| `sema_call.c` | 152 | 149 |
| `sema_const.c` | 81 | 80 |
| `sema_expr.c` | 98 | 97 |
| `lower_expr.c` | 84 | 84 |

The fix of T289 adds four sides to `lower_expr.c`. No input reaches the
one of a field whose symbol is no function, so the side that
`never_extern` reaches leaves the count as it was. The first coverage run had 7565 tests and took 1095 s. Its one
failure was `fmt_canonical`, which read a test of this batch written
during the run before it stood in its canonical form. The run after the
fix had 7585 tests, all passed, and took 981 s. The sides the files keep
are guards of the note, or are counted in its last section.

## Suites

- The host suite ran before the code commits, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7597`. One run covered the six test
  commits and the list of T289, and one the fix. The times stand in
  `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7595. Every test passed.
- The host runs 7597 tests and each sanitizer tree 7595.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`antl_tree.c` with 75 missed branches and `sema_copies.c` with 80 got no
test. The probes of `sema_copies.c` reached guards alone, and
`antl_tree.c` was read and not probed. Most sides left in the three files
of the checker take a NULL operand or an error type after a message.

## Questions for Eddie

None new. Four stay open, and nothing here acts on them: `take` of an
`own ?fn` field, the handler of T278, the reach of T281 across modules
and the body of a `do ... while` in a `construct`, from batch 3q.
