# Batch 3w of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `f051eb13` gave the ranking. The batch took the
files of the parser besides `src/antic/parser_item.c`, which batch 3v
showed with many parse errors no test gave: `parser_decl.c` with 43
missed branches, `parser_expr.c` with 32, `parser_stmt.c` with 26,
`parser_type.c` with 22, `parser.c` with 21 and `parser_clause.c` with
17. Each side ran first through the antic of the coverage tree alone,
with a profile of its own. The sections of the parser in
`docs/notes/unreached-guards.md` were read first. Eddie's direction is the
Mac alone, so the VMs were not touched. The batch found three defects,
T294 to T296, and a fourth that waits for Eddie. The next free id is
T297. The batch before found one, and this one found three, so the phase
goes on.

The question of batch 3v, whether `extern struct S;` with its two
messages is one mistake, is settled by T071 and T101 of
`docs/audit/tests-findings.md`: one mistake gives one message. It is
T294.

## Commits

- `93874c11` lists T294, and `5dbbfceb` fixes it.
- `62ccbb6d`, `510bb236` and `c017bba7` test `parser_decl.c`, then
  `parser_expr.c` and `parser_type.c`, then `parser.c`, `parser_stmt.c`
  and `parser_clause.c`: 74 tests of the matrix, 62 refusals and 12
  programs.
- `8f61385a` lists T295 and T296. `e64f4932` fixes T295, and `f956af18`
  fixes T296.
- `801a99ff` tests five sides that the first sort took for guards, and
  `648a49b0` tests the paths of `parser_skip_declaration` that the first
  coverage run after the fixes showed unreached.
- `032cc4c6` records the commits of the fixes, and `561051ce` names the
  guards. One commit records the times and the counts in `CLAUDE.md` with
  this report.

## Findings

- T294, minor. Another word than `fn` after `extern` got `expected fn`,
  and the recovery of the module went on at that word, since it starts an
  item, so `extern struct Handle;` got `expected {` as well. The same held
  for a C union, a C constant, a C type, a variant, a constraint and
  `pub`, `export`, `internal` or `worker` after `extern`, ten refusals in
  all. The extern arm of `item_level` now passes the rest of the
  declaration with `parser_skip_declaration` of `parser.c`.
- T295, minor. A function, a constant or a field in the body of a struct,
  a union, an enum or a variant got its message, and the rest of the body
  then read as items of the module with a second message. T101 left a
  class body at its closing brace and no other body.
  `types_union_with_a_function.err` and two cases of `unit_parser` held
  the second message. `struct_item`, `variant_item` and `enum_item` now
  leave a body that failed past its closing brace.
- T296, minor. A `;` where an operand stands inside brackets, as in
  `twice(;)`, ended the statement for the recovery, and the `)` after it
  started a statement with a second message. `parser_sync_statement` now
  passes the closing brackets and the `;` after them.

No `[provisional]` entry was added. Each fix makes the parser hold the
rule of one message per mistake that T071 and T101 settle.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 42600.
`cmake -DFIRST=42601 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 42601 to
42800 at the default size in 406 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.07%, 3903 missed | 93.25%, 3805 missed |
| Regions | 98.51%, 1265 missed | 98.52%, 1263 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1228 missed | 98.72%, 1227 missed |

| File | Branches missed before | after |
|---|---|---|
| `parser_decl.c` | 43 | 10 |
| `parser_expr.c` | 32 | 10 |
| `parser_stmt.c` | 26 | 19 |
| `parser_type.c` | 22 | 12 |
| `parser.c` | 21 of 294 | 14 of 332 |
| `parser_clause.c` | 17 | 0 |

The fixes of T294 and T296 add 38 branches to `parser.c`. The first
coverage run had 7682 tests, all passed, and its suite took 1121 s. The
run after the fixes had 7807 and took 1006 s, and it showed the paths of
`parser_skip_declaration` over a body and at the end unreached. The last
run had 7811, all passed, and took 991 s. The sides left in the six files
are guards of the note or sides that its line for batch 3w names.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` nine times, twice with a failure of `header_sections`.
  The times stand in `CLAUDE.md`.
- Both sanitizer suites ran in the six parts of `asan` once before the
  last four refusals, up to test 7817, and once after them, up to test
  7821. Every test passed both times.
- The host runs 7823 tests and each sanitizer tree 7821.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`lower_stmt.c` with 63 missed branches and the damaged-file sides of
`antl_read_ir.c` with 82 and of `antl_read.c` with 46 got no probe here,
nor did `sema_call.c`, `sema_generic.c`, `sema_expr.c`, `lower_expr.c`,
`sema_safety.c`, `fmt.c`, `sema_const.c`, `sema_copies.c`, `sema_stmt.c`
and `sema_class.c`.

## Questions for Eddie

- A class body that the parser refuses where the end of the file cuts it,
  `class C { x: int, protected internal`, leaves the parser one past its
  last token. `class_item` sets the position after the closing brace that
  `closing_brace` gives, and that is the TOKEN_EOF when the body has
  none. The module loop then reads past the tokens, and the antic of the
  ASan tree reports a heap-buffer-overflow in `parser_check` on that
  source. The host build reads what lies there, and a doc comment leaves
  a zeroed slot that reads as the end, so no test of the host tree fails.
  A unit test that builds a parser by hand fails on the host, and
  `header_sections` refuses it, since only the parser includes
  `parser_parser.h`. The workflow of the known failures lists a test that
  fails on the host, so the batch did not list this defect, gave it no
  id and did not fix it. The fix is one line in `class_item`:
  `p->pos = close < p->last ? close + 1 : p->last`. `leave_body` of the
  fix of T295 leaves a body without its closing brace where it failed, so
  the other bodies stay inside the tokens. How should a defect that
  AddressSanitizer alone shows be listed?

Six stay open, and nothing here acts on them: `take` of an `own ?fn`
field, the handler of T278, the reach of T281 across modules, the body of
a `do ... while` in a `construct`, whether the two forms of a function
type compare, from T290, and whether the verifier of a library file holds
every name of a local to a binding before it, from batch 3u.
