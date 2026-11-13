# The digests of generics and the signatures of functions

This step builds Eddie's answer to the two questions of
`docs/reports/2026-11-12-base-tables-and-copies.md`. His answer was "yes,
do both". "Answers of 2026-11-12" in `docs/decisions.md` holds it. A
library file now carries a digest of the tree of each generic it copied.
It carries the signature of each function it reaches in another library
as well. The
format version is 97. Eddie's direction is the Mac alone, so the VMs were
not touched.

## Commits

- `3b0d5814` records the answer, its line in the freeze and the records
  in `docs/notes/modules.md`.
- `bcc90df0` lists T247 to T251.
- `cf3c28f6` fixes T247 to T251 and raises the format version to 97.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fixes in `docs/audit/tests-findings.md`.

The five fixes share one change of the format, so they stand in one
commit.

## The digest

`struct antl_digest` of `src/antic/antl_io.h` holds an FNV-1a hash. While
the reader reads the declaration and the trees of a generic, every byte
it takes goes into the hash of that generic. A type stands there as its
qualified name, and a symbol of another item as the digest of its
record, so the hash follows the generic and not the indices of the file.
The doc text stays out. For a generic type the hash takes the base, the
fields and the declarations of the functions from the type table as
well. `antl_read_generics` of `src/antic/antl_tree.c` gives each generic
its digest in `struct item`.

A copy names its generic in `copied_from`. After the table writes, the
IR of a library file lists one `struct ir_generic_copy` per generic of
another module that it copied: the module, the name, whether it is a
function and the digest. `note_generic_copy` of `src/antic/lower.c`
adds them. `check_generic_copy` of `src/antic/antl_read_ir.c` refuses a
later tree with another digest. The checks of the tables run first, so
a copy whose table changed keeps the message of T245.

The DESIGN of `read_signature` now gives the digest as the reason that
two copies with the same arguments are the same code. The decision
itself, one copy per program, stands.

## The signatures

`types_signature` of `src/antic/types.c` writes the type of a function
without `self`, with the module of every struct, as `fn(int) -> float
may fail`. `sema_entry_signature` writes that of an entry of a table.
The records of the table reads and the slots of the table writes carry
it, and the slots of a copy carry the signature of the entry of the
generic. Every function of the IR of a library file carries its own, set
where lowering declares it. An entry of the root that no class
replaces carries none and is not judged.

## The paths probed

- A copy of a generic function and one of a generic class whose later
  generic changed a body alone. Both gave 1 where the later file gives
  2. T247, severe, fixed.
- A call through the table of a class whose later function gives an
  `f64` where the first gives an `int`. Exit 0 where both give 3.
  T248, severe, fixed.
- A class that replaces that function and writes its table against the
  base. A call through the base printed `4.4e-323` for 9. T249, severe,
  fixed. A class that inherits the function without replacing it names
  the function of the base in its table, and the check of T250 refuses
  it.
- A call by name of a function at module level with another result.
  Exit 0 where the first file gives 2. T250, severe, fixed.
- A generic of a library file that calls that function, copied by the
  program. The copy wrote `call i64` of a function that returns `f64`.
  T251, severe, fixed.

## The messages

- `` holds a copy of `M.G` made from another tree than the library
  holds ``.
- `` calls `M.C.f` as `S`, and the library holds it as `T` `` for a
  table read.
- `` fills the table of `X` with `M.C.f` as `S`, and the library holds
  it as `T` `` for a table write.
- `` needs function `M.f` as `S`, and the library holds it as `T` `` for
  a call by name and for a generic.

## Tests

`tests/modules/stale/` gains `tally`, `Jar`, `Changer` and `half` in both
files of `counter`, and the users `tally`, `jar`, `changer`, `turner`,
`half` and `halver`. Six error programs hold the messages. Each failed
for its reason before the fix, since antic exited with 0.

`scale_antl` of `tests/unit/test_modules.c` holds the version 97, the
signature of `scale` and the empty count of the copies. Its refused
version is 98. The two hex listings of `tests/modules/` hold the new
version and the new bytes.

The refusal of a generic the later file lacks in `check_generic_copy`
is reached by no test. `read_signature` refuses a copy of a generic
function first, and the type table or the table writes refuse a copy of
a generic type first.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`, up to test 7288. Before the
  listing it took 382 s, 266 s and 317 s. `fmt_canonical` failed there
  on one error program that `anti fmt` wraps otherwise. The program and
  the patterns of four cases were corrected, and the tests they reach
  ran again alone and passed. Before the fix it took 386 s, 266 s and
  314 s. `header_sections` failed there, since `antl_digest_type` stood
  under the section of `antl_io.c`. The declaration moved, and the
  suite ran again whole: 386 s, 265 s and 313 s. Every test passed apart
  from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the fix, in the six parts of `asan`, up
  to test 7286. Every test passed. `asan` took 246 s, 187 s, 266 s,
  187 s, 457 s and 454 s. `ubsan` took 256 s, 189 s, 264 s, 201 s,
  469 s and 511 s.

## Provisional lines

None.

## Questions for Eddie

1. The lines of a generic. The digest takes the line and the column of
   every node of a tree. They reach the copy, in the lines of `-g` and in
   the values of `here`. A comment added above a generic moves
   its lines, so every library that copied it must be built again. Should
   the positions stay out of the digest? A copy would then keep the lines
   and the `here` values of the earlier file.
