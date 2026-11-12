# The names of the table reads

This step builds Eddie's second answer of 2026-11-12. It answers the
question of `docs/reports/2026-11-12-complete-library-files.md`. His
answer was "yes". "Answers of 2026-11-12" in `docs/decisions.md` holds
it. A library file now names each function its code reads from the
table of a class of another module. The read refuses a later file of the
class that lacks the function or holds it at another slot. The format version is 95. It
fixes T243. Eddie's direction is the Mac alone, so the VMs were not
touched.

## Commits

- `a2acc349` records the answer, its line in the freeze and the record in
  `docs/notes/modules.md`.
- `658e8215` fixes T243 and raises the format version to 95.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fix in `docs/audit/tests-findings.md`.

## The record

The last section of the IR of a library file, after the classes, lists the
reads. Each record holds the module and the name of the class, the name of
the function, the count of its parameters with `self` and the slot.
`struct ir_table_read` of `src/antic/ir.h` holds it, and
`lower_note_table_read` of `src/antic/lower_desc.c` adds one per distinct
read at the three places lowering reads an entry:

- a call through the table, in `table_target` of `src/antic/lower_expr.c`;
- a bound function, `let f = c.bump;`, in the same file;
- the thunk of a body reached through an interface, `lower_reach_thunk` of
  `src/antic/lower_class.c`.

Four kinds of class take no record: the root, a declared type, a copy of a generic and a class of the module itself. Each user of a copy makes it from
the tree of the later file. The root and the declared types belong to the
compiler.

`check_table_read` of `src/antic/antl_read_ir.c` finds the class in the
file of its module, which every file reads after. It lays out the table of
that class with `sema_table_of` and finds the entry by its name and count,
as `lower_table_index` does. It gives one of three messages:

- `` needs struct `M.C` `` for a class the later file lacks. A class that
  no signature of the file names stands in no type table, so this read is
  the first to see it.
- `` needs function `M.C.f` `` for a function the class lacks, the message
  of T241 and T242.
- `` calls `M.C.f` at slot N of its table, and the library holds it at
  slot K `` for a function at another slot.

## A slot that moved is refused

The read refuses a moved slot and never binds the call to the new one. Four
precedents agree:

- Eddie's question named it. The read "would then refuse a later file
  whose class lacks it or holds it in another slot", and he answered yes.
- "Versions" in `docs/decisions.md`. A table that changed other than by an
  addition at the end gives a chain that diverges, and the load of the
  plugin is refused.
- R127. A C function that a file declares with another signature than a
  library before it is refused, and the read adapts nothing.
- The IR. The slot stands in the integer of the offset of a load, and
  `call ... table @<descriptor> N` repeats it. Binding would rewrite the
  unoptimised IR of the file, and a file that disagrees with its library was
  built for another table.

## Tests

`tests/modules/stale/` gains `Mover`, `Source`, `Holder` and `Gone` in the
first file of `counter`, and the later file adds a function before
`Mover.bump` and one before `Source.get`. The users `bound_user`,
`moved_user`, `reach_user` and `gone_user` compile against the first file.
Before the fix each of their programs compiled with `-S` and exited with 0.

- `error_modules_later_file_without_a_function_of_the_table`, T243, passes
  and left `tests/known-failures.txt`.
- `error_modules_later_file_without_a_bound_function`.
- `error_modules_later_file_with_a_function_at_another_slot`.
- `error_modules_later_file_with_a_function_of_an_interface_at_another_slot`.
- `error_modules_later_file_without_a_class_of_the_table`.

`scale_antl` of `tests/unit/test_modules.c` holds the version 95 and the
empty count of the reads. Its refused version is 96, so the message of a
version mismatch keeps its test. The two hex listings of
`tests/modules/` hold the new version and the count. The tree holds no
library file. Every `.antl` is written by the build or by the suite.

## Suites

- The host suite ran before the fix, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7259`: 396 s, 275 s and 364 s. Every test
  passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the fix, in the six parts of `asan`, up
  to test 7257. Every test passed. `asan` took 248 s, 186 s, 266 s,
  189 s, 457 s and 435 s. `ubsan` took 254 s, 189 s, 263 s, 199 s,
  475 s and 471 s.

## Provisional lines

None.

## Questions for Eddie

1. A table that a stale file fills. A class `Sub` of a library `x` inherits
   `Base` of a library `y`. `x` is built against the first file of `y`,
   and the later file of `y` adds a function to `Base`. The program calls
   `p.b()` on a `*x.Sub` from its own source. It reads the slot of `b` from
   the later table. The table of `Sub` stands in the file of `x`, laid out
   against the first `Base`. A probe by hand linked and exited with
   133. The step changes nothing there, since its record covers the reads
   of a table and not the tables a file writes. Should the file of `x`
   carry the table of the base it was built against? The read would then
   refuse a later base that changed it. That is another change of the format.
   Until you answer, the defect has no row and no test. The next step
   would list it as T244.
2. A copy of a generic class of another library takes no record. Each of
   its users makes the copy from the tree of the later file. No
   test reaches a library file whose copy was made from an earlier tree.
   Should the next step probe that path as it probed the others?
