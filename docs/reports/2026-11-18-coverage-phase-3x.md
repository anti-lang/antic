# Batch 3x of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `11dd67e0` gave the ranking. The batch took first
the defect that batch 3w left without an id, then `src/antic/lower_stmt.c`
with 63 missed branches and the damaged-file sides of
`src/antic/antl_read_ir.c` with 82 and of `src/antic/antl_read.c` with
46. Each side ran first through the antic or the unit tests of the
coverage tree alone, with a profile of its own. The sections of the three
files in `docs/notes/unreached-guards.md` were read first. Eddie's
direction is the Mac alone, so the VMs were not touched. The batch found
one defect, T297. The next free id is T298. The batch before found
three, and this one found one, so the phase goes on.

The question of batch 3w, how to list a defect that AddressSanitizer
alone shows, is settled by T143 and T157 of
`docs/audit/tests-findings.md`. Each got an id and a row with the test
that the sanitizer tree runs, added in the commit of its fix. Neither got
a line in `tests/known-failures.txt`, since no test of the host tree
fails for it. T297 follows them.

## Commits

- `5fec985b` fixes T297, with its refusal of the matrix and its row.
- `cdaf8dd4` tests `lower_stmt.c`, `antl_read_ir.c` and `antl_read.c`: six
  programs of the matrix and two groups of `unit_modules`.
- `35cbac58` records the commit of the fix, and `1bb065b0` names the
  guards. One commit records the times and the counts in `CLAUDE.md`
  with this report.

## Findings

- T297, major. A class body that the end of the source cuts after a
  member the parser refuses, `class C { x: int, protected internal`, has
  no closing brace, so `closing_brace` gives the index of the TOKEN_EOF.
  `class_item` of `src/antic/parser_item.c` set the position one past it,
  and the loop of the module read past the end of the token array. The
  antic of the ASan tree stopped with a heap-buffer-overflow in
  `parser_check`. `class_item` now leaves such a body at the end.
  `matrix_classes_body_cut_after_a_refused_member_refused` failed in the
  asan tree with the overflow before the fix and passes after it. A doc
  comment of the module hides the read, so the source holds none.
- The search for the same cause found no other place. `class_item` was
  the one place of the parser that could set the position past the
  TOKEN_EOF. `leave_body` of the fix of T295 moves past a closing brace
  inside the tokens alone, and keeps a cut body where it failed. The
  ASan antic read every cut of 111 sources of the matrix at each blank
  and bracket. It read 15947 cuts with their doc comments and 11425
  without them, the second run with the fix. No cut showed a report.

No `[provisional]` entry was added. The fix makes the parser hold the
DESIGN of `class_item`.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 42800.
`cmake -DFIRST=42801 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 42801 to
43000 at the default size in 173 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.24%, 3810 missed | 93.27%, 3794 missed |
| Regions | 98.52%, 1264 missed | 98.52%, 1263 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.72%, 1227 missed |

| File | Branches missed before | after |
|---|---|---|
| `lower_stmt.c` | 63 | 57 |
| `antl_read_ir.c` | 82 | 79 |
| `antl_read.c` | 46 | 44 |

The fix of T297 adds 2 branches to `parser_item.c`. The first coverage
run had 7811 tests, all passed, and its suite took 1013 s. The run after
the tests had 7830, all passed, and took 1003 s. A `defer` or an `undo`
whose call never returns is the one way a block ends inside the code of a
statement, and five of the programs take each exit it reaches. The other
sides of `lower_stmt.c` test a block that a value ended, which no
expression does, or a symbol that the checker always sets. Truncation
reaches the end of the records of an `inject` field and of a `provides`
line, and the counts before them keep the sub-objects and the `mutable`
fields whole.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` three times, once with a failure of `fmt_canonical`,
  since the new refusal lacked the line break at its end. The times stand
  in `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7840. Every test passed.
- The host runs 7842 tests and each sanitizer tree 7840.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

The parser sides that batch 3w left unsorted got a probe of `internal`
after a marker of a member, which `parser_item.c` refuses before
`parser_decl.c` reads it, and no test. `header.c` with 59 missed
branches has sides that a module name with a digit and an aligned class
reach, which need a case of `run_clib.cmake` with its C program. They got
no test. `sema_safety.c` with 83 got probes of pointers into the fields
of a synchronized class, which reached no new side. `sema_call.c`,
`sema_generic.c`, `sema_expr.c`, `lower_expr.c`, `sema_copies.c`,
`fmt.c`, `sema_const.c`, `sema_stmt.c`, `sema_class.c` and `antl_tree.c`
got none. The index of an aggregate past the table in the sub-object of a
class record reaches a failure of `read_classes` that no case writes.

## Questions for Eddie

- A body of a struct, an enum or a variant that the end of the file cuts
  after a refused member gets a second message, as `variant V { A, fn`
  gets `expected identifier` at `fn` and at the end. The DESIGN of
  `leave_body` keeps a cut body where it failed, by the fix of T295, so
  the module reads the word as an item. A cut class body gets one
  message. Is the end of the file a second mistake there, or should the
  other bodies leave at the end as a class body does?

Six stay open, and nothing here acts on them: `take` of an `own ?fn`
field, the handler of T278, the reach of T281 across modules, the body of
a `do ... while` in a `construct`, whether the two forms of a function
type compare, from T290, and whether the verifier of a library file holds
every name of a local to a binding before it, from batch 3u.
