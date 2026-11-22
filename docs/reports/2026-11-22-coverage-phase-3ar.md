# Batch 3ar of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `e0e6051d` gave the ranking. The files above them
that miss more branches were read by earlier batches chosen by branches.
The batch read two files that no such batch had read: the expressions of
lowering in `src/antic/lower_expr.c`, which missed 87 of its 1012
branches, and the constant expressions of the checker in
`src/antic/sema_const.c`, which missed 83 of its 1160. Batch 3aq fixed
T394 in the second file and did not read it. The batch read both against
the specifications, `docs/decisions.md` and the DESIGN comments of the
two files. Each side ran first through the antic of the coverage tree
alone, with a profile of its own. The batch found no defect. The next
free id stays T396.

## Commits

- `cfde6280` tests twelve sides of the two files.
- `00779c89` records the times and the counts in `CLAUDE.md`
  and the sides of the batch in `docs/notes/unreached-guards.md`.
- The docs commit of this report.

## Tests

Six programs of the matrix and one refusal:

- `types_bitfield_of_the_full_width`: `u64 : 64` and `i64 : 64` in a
  literal, an assignment and the default of a class field.
- `flow_condition_reads_through_a_pointer`: `*p` as the condition of an
  `if` and as an operand of `&&` in a `while`.
- `classes_destroy_through_a_dereference`: `destroy(*pq)` of a heap
  object and `free(q)` after it.
- `values_flags_of_two_carries_in_a_chain`: `a + b + f.carry + g.carry`.
- `types_generic_length_folds_unsigned_and_mul_high`: lengths of a copy
  that fold `u64`, a division of the least value of `int` and
  `mul_high` of `u64` and of `int`.
- `values_constants_compare_with_none_and_text`: constants that compare
  an optional with `none` on either side and a `str` with a literal, and
  the negation of a float constant.
- `types_enum_value_holds_a_dup`: an enum value of `dup(3)`, refused.

## Read without a find

- The constant folder and the code of a run agree on `%`, `/`, `>>`,
  `<<%`, `~`, `mul_high`, the wrapping and the saturating operators of
  `i8`, `u8`, `i16`, `u16`, `i32`, `i64` and `u64`, on the order of texts
  and on the conversions and the divisions of `f32` and `f64`.
- A chain of two carries takes the flags of its last step, by the DESIGN
  of `lower_flag_operation`. `r + 210 + f.carry + f.carry` gives a carry
  of 0 where the inner sum carried. The specification takes a carry in
  as the last operand of `+`. The DESIGN reads a carry after another
  carry as the sum plus 0 and the carry.
- `join(dispatch t -> add(2))` runs with an object on the heap. With the
  address of a local, E1 refuses it. By the correction of E1 of
  2026-10-25, antic proves a join only where a `let` holds the job.
- A function of another module as a value, `cside.labs` of an
  `extern fn`, compiles in release and in dev mode. The checker writes it
  as a name.

## Fuzzer

`cmake -DFIRST=46801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 46801 to
47000 at the default size in 152 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.50%, 3761 missed of 57822 | 93.51%, 3753 missed of 57822 |
| Regions | 98.51%, 1303 missed | 98.51%, 1302 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.71%, 1259 missed | 98.71%, 1259 missed |

| File | Branches missed before | after |
|---|---|---|
| `src/antic/lower_expr.c` | 87 of 1012 | 84 of 1012 |
| `src/antic/sema_const.c` | 83 of 1160 | 74 of 1160 |

Every side the batch left in the two files is a guard.
`docs/notes/unreached-guards.md` names each, in the section of the batch
or in those of the two files. The "before" run had 8202 tests in 1086 s,
and the run after the tests 8221 in 1089 s. Every test passed in each.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,8233` before the commit of the tests: 382 s, 281 s and 394 s.
  Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 8231. `asan` took 246 s, 180 s, 266 s, 168 s, 131 s
  and 614 s, and `ubsan`
  254 s, 181 s, 265 s, 168 s, 101 s and 636 s. Every test passed.
- The host runs 8233 tests and each sanitizer tree 8231.
  `tests/known-failures.txt` holds the two lines of T136 and T138, the
  three lines of T385 and the three lines of T387, as before the batch.

## Pinned outputs

No commit of the batch changes code, so
`tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` stand as they were.

## Questions for Eddie

The batch found no defect, so it is the first empty batch of the
phase. T385, T387, the question on `anti fmt` and the lines of a table,
question 1 of batch 3ao on `compare_swap` of an `own` field and question
1 of batch 3aq on the two messages of `provides` wait for him as before.
