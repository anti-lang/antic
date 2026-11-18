# Batch 3v of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer.
`./c coverage` at HEAD `c10ef565` gave the ranking. The batch took
`src/antic/parser_item.c` with 50 missed branches and `src/anti/syms.c`
with 55, which no batch had chosen, and `src/rt/trace.c` with 62 and
`src/antic/lower_stmt.c` with 63, which batch 3u named and did not test.
Each side ran first through the antic or the anti of the coverage tree
alone, with a profile of its own. The sections of the four files in
`docs/notes/unreached-guards.md` were read first. Eddie's direction is
the Mac alone, so the VMs were not touched. The probes found one defect,
T293. The next free id is T294. The batch before found one, and this one
found one, so the phase goes on.

## Commits

- `87051cad` tests `parser_item.c`. 41 refusals of the matrix take the
  parse errors of an item that no test gave. Two programs name the fields
  of a class with the contextual words of a field and give `compatible` a
  whole number.
- `89b50822` tests `syms.c`: 19 cases of the messages of `anti symbols`
  give a trace, an index, a map, a debug link and the files of `check`
  of the forms no test gave, and `unit_syms` breaks the CRC of the index
  of a deployment archive.
- `7b86f133` tests the entry of the object of a trace when memory runs
  out, in `faults_trace_copy.faults`.
- `da8e450a` lists T293, and `b1a4e648` fixes it. `b8c0c810` takes every
  kind of member into its fixture.
- `c49c852c` and `965ec0ce` name the guards, and `6f6d99b7` records the
  commit of the fix. One commit records the times and the counts in
  `CLAUDE.md` with this report.

## Findings

- T293, minor. A field of a class body without its comma ends the run of
  fields where a member follows, by T069. `anti fmt` kept the statement
  of the field open across the line break and indented the header of the
  member one tab deeper than its body, so `--check` refused a canonical
  source. `field_ends` of `src/anti/fmt.c` now closes the statement at a
  line that opens a member, as `parser_starts_member` reads it, in the
  body of a class or an enum. A line that ends after the `:` or the `=`
  of the field still goes on with it. `docs/notes/fmt.md` gives the rule.

No `[provisional]` entry was added. The fix makes `anti fmt` read a field
as antic reads it.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 42400.
`cmake -DFIRST=42401 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 42401 to
42600 at the default size in 379 s, beside the first coverage run. No
seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 92.91%, 3990 missed | 93.06%, 3909 missed |
| Regions | 98.52%, 1261 missed | 98.51%, 1266 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.73%, 1223 missed | 98.72%, 1228 missed |

| File | Branches missed before | after |
|---|---|---|
| `parser_item.c` | 50 | 2 |
| `syms.c` | 55 | 16 |
| `trace.c` | 62 | 63 |
| `lower_stmt.c` | 63 | 63 |
| `fmt.c` | 72 of 1146 | 81 of 1222 |

The fix of T293 adds 76 branches to `fmt.c`, and the guards of its code
stand among the 81 sides left there. The case of `trace.c` reaches the
entry of the object. Five sides of `text_part`, `module_id` and
`anti_rt_trace_frame` there moved between the runs, since which frames a
trace meets depends on the run. The
first coverage run had 7611 tests, all passed, and its suite took
1087 s. The run after the fixture of `b8c0c810` had 7682, all passed,
and took 976 s. The 2 sides left in `parser_item.c`, the 16 in `syms.c`
and the 63 in `trace.c` are guards of the note, paths of Linux and
Windows, or sides that need a binary of Anti that no case of a table of
messages writes, as the last section of the note counts them.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,7694` four times: once with the tests of the three files and
  the list of T293, where the two new programs leaked their texts under
  `--memory-checks`, once after they freed them, once before the fix of
  T293 and once before `b8c0c810`. The times stand in `CLAUDE.md`.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7692. Every test passed.
- The host runs 7694 tests and each sanitizer tree 7692.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

`lower_stmt.c` kept its 63 sides. Three of them are guards of the note.
Most of the rest test the block after a path that ended inside a
statement, or a call with a handler that writes no result, which no probe
reached. The damaged-file sides of `antl_read_ir.c` with 82 missed
branches and of `antl_read.c` with 46 got no probe here. `sema_call.c`,
`sema_generic.c`, `sema_expr.c`, `sema_safety.c`, `lower_expr.c`,
`sema_copies.c`, `sema_const.c`, `fmt.c`, `sema_stmt.c` and `sema_class.c`
got none either.

## Questions for Eddie

- `extern struct S;` gets `` expected `fn` `` at `struct` and then
  `` expected `{` `` at `;`. The recovery of the module goes on at the
  `struct` the first message names, as it goes on at the `fn` after a
  constant without its `;`. T101 holds one message per mistake in a class
  body. Is this one mistake, so the second message should go?

Six stay open, and nothing here acts on them: `take` of an `own ?fn`
field, the handler of T278, the reach of T281 across modules, the body of
a `do ... while` in a `construct`, whether the two forms of a function
type compare, from T290, and whether the verifier of a library file holds
every name of a local to a binding before it, from batch 3u.
