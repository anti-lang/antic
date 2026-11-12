# The tables of bases and the copies of generics

This step applies Eddie's two answers of 2026-11-12 to the two paths that
`docs/reports/2026-11-12-table-call-names.md` left open. "Answers of
2026-11-12" in `docs/decisions.md` holds the answers and the extension.
No new answer of Eddie's was needed. The first answer covers every function
a program uses. The second chose to carry what the read needs in the
library file. Eddie's direction is the Mac alone, so the VMs were not
touched.

## Commits

- `9e491a40` records the extension of the rule and the records in
  `docs/notes/modules.md`.
- `f4383870` lists T244.
- `5963823c` fixes T244 and raises the format version to 96.
- `0898fb00` lists T245 and T246.
- `8e77faf2` fixes T245.
- `4cac5d5c` fixes T246.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## The record of the table writes

After the reads, the IR of a library file lists each table its module
writes against a class of another module. `struct ir_table_write` of
`src/antic/ir.h` holds one record:

- the qualified name of the class whose table the file writes, for the
  message;
- the module and the name of the class it was laid out against;
- the name and the count of parameters with `self` of every entry of
  that class, in order.

That class is the nearest class of another module in the chain of the
class, or in the chain of the interface of a sub-object. For a copy of a
generic of another module it is the generic itself. A copy takes the
functions of its generic, so the two tables hold the same entries.
`note_table_write` of `src/antic/lower_desc.c` adds the records in
`lower_class_table` and `lower_interface_table`. The root and the types
the compiler declares take none.

`check_table_write` of `src/antic/antl_read_ir.c` finds the class or the
generic in the later file of its module and lays out its table with
`sema_table_of`. It compares the entries in order and names the first
that differs. It never rewrites the table of the file, which the IR holds
as constants.

## The paths probed

Each path was tried with a library built against a first file and read
beside a later one.

- A class that inherits a class of another library, or implements an
  interface of one, whose later file moved, removed or added a function.
  The read passed and the program linked. Calls exited with 133, and one
  reached the function the later class lacks and gave 8. T244, severe,
  fixed.
- A copy of a generic class whose later generic moved a function, called
  from the program. Exit 133. A file that calls through the table of the
  copy beside the copy of a later library gave 9, the result of another
  function. A final function the later generic lacks still ran. T245,
  severe, fixed.
- A copy of a generic function whose generic the later file lacks. The
  call ran the copy of the first tree. T246, major, fixed.
- A module-level function that the body of a stale copy calls and the
  later file lacks. `needs function`, by the fix of T242. No defect.
- A copy that another library holds. A file that reads the table of a
  copy writes the copy and its table as well. The record of the write
  covers the read, so the reads of copies need no record. The DESIGN of
  `lower_note_table_read` gave another reason, that each user makes its
  copies from the later tree. The probe showed it wrong, and the comment
  now gives this one. The decision itself stands.
- Functions of a class with type parameters of their own do not cross a
  library file, and a struct holds no functions. Neither path exists.

## The messages

- `` fills the table of `X` with `M.C.f` at slot N, and the library holds
  it at slot K ``, for an entry at another slot.
- `` fills the table of `X` without `M.C.g`, and the library holds it at
  slot K ``, for an entry the file lacks.
- `` needs function `M.C.f` `` for an entry the later class lacks,
  `` needs struct `M.C` `` for a class it lacks and
  `` needs generic `M.G` `` for a generic function it lacks.

## Tests

`tests/modules/stale/` gains `Shrinker`, `Grower`, `Bag`, `Tin` and
`pick` in both files of `counter`. The first tree gains the users
`pusher`, `filler`, `keeper`, `grower`, `copy`, `copy_read`, `tin` and
`pick`, and the later tree `fresh_user`. Eight error programs hold the
messages. Each failed for its reason before its fix, since antic exited
with 0.

`scale_antl` of `tests/unit/test_modules.c` holds the version 96 and the
empty count of the writes, and its refused version is 97. The two hex
listings of `tests/modules/` hold the new version and the count.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. Before the listing of T244, up to
  test 7267, it took 377 s, 264 s and 313 s. A first run of the third
  part failed in 12 tests, since this session edited two hex listings
  and three sources while it ran. The tests passed alone, and the part
  ran again whole. Before the fix of T244 it took 354 s, 252 s and
  304 s. Up to test 7276 it took 379 s, 265 s and 306 s before the
  listing of T245 and T246, 360 s, 232 s and 308 s before the fix of
  T245, and 376 s, 264 s and 305 s before the fix of T246. Every test
  passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the fix of T246, in the six parts of
  `asan`, up to test 7274. Every test passed. `asan` took 250 s, 188 s,
  266 s, 188 s, 455 s and 443 s. `ubsan` took 253 s, 190 s, 267 s,
  200 s, 478 s and 463 s.

## Provisional lines

None.

## Questions for Eddie

1. A copy whose generic changed a body alone. The table and the functions
   of the later generic match the copy, so the read passes. The program
   keeps the first copy it reads. A probe read a stale library before
   one built against the later generic. The later library's own call
   gave 1, the body of the first tree, where its source gives 2. The
   DESIGN of `read_signature` holds that copies of one tree with the same
   arguments are the same code, and a stale file breaks that. Should a
   library file carry a digest of the tree of each generic it copied,
   and the read refuse a later file whose tree differs? That would be
   another change of the format, and it refuses every change of a
   generic, a body included.
2. The signature of an entry. The records of the reads and of the writes
   name an entry by its name and its count of parameters, as a table
   keys it. A later base whose function keeps both and changes a type
   passes the read. A probe changed `b(self) -> int` to `-> f64`, and the
   call through the table of the stale class printed `0.000000`. Should
   the records carry the signature of each entry, as the chain of
   "Versions" hashes it? That is another change of the format.
