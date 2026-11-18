# Batch 3u of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `ccc8ac94` gave the ranking. The batch took
`src/antic/antl_verify.c` and `src/antic/whole.c` with 65 missed
branches each, which batches 3s and 3t named and did not test. Each side
ran first through the unit tests of the coverage tree alone, with a
profile of its own. The sections of both files in
`docs/notes/unreached-guards.md` were read first. Eddie's direction is
the Mac alone, so the VMs were not touched. The probes found one defect,
T292. The next free id is T293. The batch before found two, and this one
found one, so the phase goes on.

## Commits

- `19716b8c` tests the rules of `antl_verify.c` that no damaged file of
  `unit_modules` broke. It damages the trees of `tree_source` and of a
  new generic of the forms `tree_source` lacks: the repeat form of an
  array, `in`, `take`, an index as a target, a destructuring, `==` and a
  hash that call an operator of a part, `sync` and the two walks of an
  iterator. Pokes give the record of a function the kind of a parameter
  and of a local, and the flag of a simd struct to a struct of bitfields
  and to one with a unit break.
- `51b02bae` tests copies that `merge_copies` keeps apart in
  `unit_whole`. A library file written by hand may make one copy of a
  pair differ from the other in twenty ways, and three keep them alike.
- `41e33323` lists T292, and `9b1b55bd` fixes it.
- `3d83b4c1` names the guards, and `d350f7cb` records the commit of the
  fix. One commit records the times and the counts in `CLAUDE.md` with
  this report.

## Findings

- T292, major. A library file whose generic takes a tuple apart,
  `let (l, r) = pair;`, without the symbol of the value the names come
  from passed the verifier. antic then crashed on a null pointer in
  `lower_let` when a client compiled a copy of the generic. `check_stmt`
  of `antl_verify.c` asked a `let` for its symbol or for names, while the
  checker gives every `let` its symbol and lowering reads it. The
  verifier now asks every `let` for its symbol.

No `[provisional]` entry was added. The entry of the verifier of library
files already holds it to every rule the checker makes true of a tree.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 42200.
`cmake -DFIRST=42201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 42201 to
42400 at the default size in 371 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.80%, 4050 missed | 92.91%, 3989 missed |
| Regions | 98.52%, 1263 missed | 98.52%, 1261 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1225 missed | 98.73%, 1223 missed |

| File | Branches missed before | after |
|---|---|---|
| `antl_verify.c` | 65 | 27 |
| `whole.c` | 65 | 41 |

The fix of T292 removes the side of a `let` with names and no symbol.
The first coverage run had 7611 tests, all passed, and took 1084 s. The
run after had 7611, all passed, and took 980 s. The cases of this batch
stand inside `unit_modules` and `unit_whole`, so the count of tests stays.
The 27 sides left in `antl_verify.c` are guards of the note, or a field
that names an extern function, a `dispatch` in a generic and the flag of
a simd struct on a class or on a struct of no fields, which no case
writes. The 41 left in `whole.c` are guards of its section, or caps and
indexes past their lists that lowering and the reader never give, which
the last section of the note counts.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,7623` three times: once before the two test commits, once
  before the list of T292 and once before its fix. The times stand in
  `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7621. Every test passed.
- The host runs 7623 tests and each sanitizer tree 7621.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`sema_call.c` with 149 missed branches, `sema_generic.c` with 142,
`sema_expr.c` with 97, `sema_safety.c` and `lower_expr.c` with 84,
`antl_read_ir.c` with 82, `sema_copies.c` and `sema_const.c` with 80,
`fmt.c` with 72, `sema_stmt.c` and `sema_class.c` with 68, `trace.c` of
the runtime with 64 and `lower_stmt.c` with 63 got no probe here.
Earlier batches of the third phase found most sides of the files of the
checker to be guards or an error type after a message.

## Questions for Eddie

- A tree of a library file may name a local or a parameter that no
  `let`, no loop and no parameter of the tree binds. `unit_modules`
  showed it while it probed the extern of a parameter. The writer keeps
  such a symbol in the table of the tree. The verifier passes the file,
  and a client compiles a copy of the generic without a message. The
  DESIGN of `antl_verify.c` lists no rule of scope, so this batch added
  none. Should the verifier hold every name of a local to a binding that
  stands before it?

Five stay open, and nothing here acts on them: `take` of an `own ?fn`
field, the handler of T278, the reach of T281 across modules, the body of
a `do ... while` in a `construct`, from batch 3q, and whether the two
forms of a function type compare, from T290.
