# Complete library files

This step builds Eddie's answer to the question of batch 3b of coverage.
"Answers of 2026-11-12" in `docs/decisions.md` holds it. Every function that
a program uses is found in the library file of its module. Otherwise
antic stops at the read with an error that names the function. A library file written
by hand counts as an input, as the batch 3b report holds. Eddie's
direction is the Mac alone, so the VMs were not touched.

## Commits

- `7fbf0f4b` records the answer, its line in the freeze and the message
  in `docs/notes/modules.md`.
- `c920731e` lists T241, T242 and T243.
- `8b588dc9` fixes T241.
- `5d8e8c3d` fixes T242.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## The paths of a use

Each path was tried with a library `mu` built against a first file of
`counter`. It was read beside a later file whose class or module lacks
the function.

- A call in the program's own source. The checker refuses it with
  `` `Counter` has no function `bump` ``, as before. No change.
- A call from a copy of a generic, of a function of a class or of one at
  module level. The read passed, and the link of the copy found no such
  function. T241, major, fixed.
- A call by name from the code of another library file: a function at
  module level, a `final` function of a class or one without `self`. The
  read passed, and the link stopped with an undefined symbol. T242,
  major, fixed.
- A call through the table of a class from the code of another library
  file, a function that is neither `final` nor without `self`. The read
  passed, the program linked, and it exited with 133. T243, severe, open.
- A struct or a generic that the later file lacks. `needs struct` and
  `needs generic` refuse them, as before.

## The fixes

- T241. `library_symbol` of `src/antic/antl_tree.c` took the record of the
  extern. It now refuses a function of another library that the file of
  that library lacks. The record of a function of a class read from a
  library file names no owner, so `class_has` looks among the functions
  of the classes of the library. A function of the file's own module, or
  of the root, keeps its record, by the DESIGN of `must_be_found`.
- T242. `read_signature` of `src/antic/antl_read_ir.c` added the
  declaration. `declares_missing` refuses a declaration of a function of
  another library that the program lacks. Every file reads after the
  files of its imports, so the program holds their functions. A copy of
  a generic stays the exception, by the DESIGN of `read_signature`.
- `member_gone` now compiles its user against the first file of `mg`, as
  the driver does. `owner_gone` keeps a record that names its owner, and
  `item_gone` takes a function at module level.

## The message

`needs function M.F`, and `M.C.f` for a function of a class, such as
`` needs function `com.example.counter.Counter.step` ``. It follows
`` needs generic `M.G` `` and `` needs struct `M.S` `` of the same reader:
the kind, then the qualified name in backticks. The IR names a function of
a class `C.f` beside its module, so both readers write the same text.
`antl_fail` is no function the guard `diagnostics_covered` reads. The
unit cases and the `PATTERN` of the four cases of `tests/CMakeLists.txt`
hold the message.

## T243 waits for a change of the format

A call through the table of a class is `call ... table @<descriptor> N`
in the IR. It names the descriptor and the number of the slot alone. A
library file therefore carries no name of the function it calls, and the
reader cannot name it. A later class that adds a function before `bump`
moves it to another slot, and the call reaches the wrong function in
silence. The fix needs the name of each function the file calls through
a table, or of the slot it expects. That is a change of the library file,
whose format version is 94, so the step stops there as its task says.
`error_modules_later_file_without_a_function_of_the_table` holds the rule
and stays in `tests/known-failures.txt`.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7251`: 368 s, 256 s and 306 s before the
  listing, 370 s, 255 s and 304 s before the fix of T241, and 371 s,
  245 s and 309 s before the fix of T242. Every test passed apart from
  the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the fix of T242, in the six parts of
  `asan`, up to test 7249. Every test passed. `asan` took 241 s, 184 s,
  261 s, 185 s, 454 s and 436 s. `ubsan` took 248 s, 185 s, 260 s,
  194 s, 465 s and 470 s.

## Provisional lines

None.

## Questions for Eddie

1. T243. A library file calls a function of a class of another library
   through its table, by the number of the slot alone. Should the format
   carry the name of the function for each such call? The read would then
   refuse a later file whose class lacks it or holds it in another slot.
   It raises the format version from 94 to 95. The alternative checks the
   count of slots alone. It finds a function that left the end of the
   table, but it names no function and misses a slot that moved. It does
   not hold the rule.
