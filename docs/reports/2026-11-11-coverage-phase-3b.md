# Batch 3b of the third phase of coverage

This step sorts the regions that `./c coverage` at `9cb5e097` showed
unreached in the three readers of a library file:
`src/antic/antl_tree.c`, `src/antic/antl_read_ir.c` and
`src/antic/antl_read.c`. It follows the method of
`docs/reports/2026-11-11-coverage-phase-3a.md`. A library file that is
damaged, cut short or written by hand is an input a user can hand to
antic, so every refusal that a test can write bytes for counts as
reachable. Eddie's direction is the Mac alone, so the VMs were not
touched. The batch adds no executable. Every case stands in
`unit_modules`, whose time stayed at 7 s.

## Commits

- `1a74921e` tests `antl_read.c`.
- `78f6790d` tests `antl_read_ir.c`.
- `d5d8d7d6` lists T236, `83ab04e4` fixes it.
- `fb6cc967` tests `antl_tree.c`.
- `407da741` names the guards in `docs/notes/unreached-guards.md`.
- This report, with the times and the counts in `CLAUDE.md` and the
  commit of the fix in `docs/audit/tests-findings.md`.

## Findings

- T236, major, fixed. A copy of a generic, a function and its data, that
  one library file declares and a later one defines crashed antic.
  `read_signature` turned the declaration into the definition, and a
  declaration has no temporaries for its parameters, so `read_body` read
  them from `NULL`. No file the writer writes declares a copy, and a file
  written by hand does. `take_definition` gives the parameters their
  temporaries, and the function the export mark and the object of the
  file that defines it, as `new_function` gives a definition. The DESIGN
  of `read_signature` and `merge_copy_global` holds the rule.

## The unit tests

- `build_changed` takes a change of the IR before the writer reads it, as
  `build_damaged` takes one of the trees, and `with_targets` writes
  addresses of any target.
- The unit pipeline takes `compile_copies`, as the driver sets it for a
  library, so a module compiled against the file of a generic makes its
  copies.
- `ops_source` compiles against `anti.lang` of `src/std/` and a stub of
  `anti.text.Builder`, and each file reads after the files it imports,
  so a refusal names the damage and not a missing module. Lowering
  `fail "text"` in the unit pipeline crashes, since the maker of the
  error comes from the IR of a library file there, so `tfail` never
  fails.
- A crash of a unit group failed its test whatever WILL_FAIL said, so
  T236 could not be listed. Every `unit_<file>` now runs through
  `run_expect.cmake`, which reports a crash as an ordinary failure, as
  "Tests" in `CLAUDE.md` asks of a listed test.

## antl_read.c

The type table of `scale_antl` takes entries of the test's own: a struct
whose thread safety, flags, constant or default of a function names
nothing the reader takes, a pointer with a mark past two bits, a `?T` of
a pointer that may not be none, `anti.lang.Job` over a part past the
table and as a class, and symbolic lengths of every kind and of the
operations the checker does not fold. The marks of an item take seven
bits. A library built against a module that declared a struct, read
beside a later file of that module, gets `needs struct`. Each refusal
has a case beside it that reads. The two lines left are a `}` after a
`return`, which belong to the measurement.

## antl_read_ir.c

A constant of the IR deeper than `TYPES_NEST_MAX` is refused at the
link past the limit. Two declarations of one C function merge their
extension and `never`. A function two files define is refused with its
message. Class records name parts past the fields of the class, empty
paths, an `init` past the functions and no module. A global holds the
address of a function, and of targets past the file. Every line is
reached.

## antl_tree.c

Each of six enumerations of a tree stands one past its last value. The
alignment and the kind of a format specification stand above ASCII, and
the lanes of a shuffle are short of its fields. A generic calls a
function of a class of another library, read beside a later file of
that library without it. The reader takes the record, as
`library_symbol` does for an item. The lines left guard the writer and the default of
`collect`, seven rows of the note.

## Branches left

The sides of a condition that only a file cut short takes, `!r->failed`
of each case of `antl_read_value` among them, were not chased, nor the
range of a result of a signature, the defaults of a parameter on a type
that is no function and the tree branches of `read_extern` and
`tree_owner`.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.12%, 1575 missed | 98.16%, 1539 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.33%, 1585 missed | 98.39%, 1525 missed |
| Branches | 91.24%, 4859 missed | 91.32%, 4815 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `antl_tree.c` | 97.84%, 36 missed | 98.26%, 29 missed | 91.82% | 93.02% |
| `antl_read_ir.c` | 96.91%, 32 missed | 100.00% | 90.52% | 91.88% |
| `antl_read.c` | 97.89%, 23 missed | 99.82%, 2 missed | 90.99% | 93.99% |

The lines that stay unreached are the guards of the note and the two
`}` of `antl_read.c`. The coverage suite ran 7193 tests, all passed,
and `./c coverage` took 966 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7205`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Before the listing of T236, `unit_modules` crashed in `read_body`, the
  reason its row names, and `run_expect.cmake` reported it as a failure.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7203. Every test passed. `asan` took 247 s,
  186 s, 265 s, 188 s, 456 s and 432 s. `ubsan` took 251 s, 188 s,
  266 s, 197 s, 475 s and 459 s.

## Provisional lines

None. The fix follows the DESIGN of `read_signature`.

## Questions for Eddie

1. A generic of one library calls a public function of a class of
   another, and the user reads it beside a later file of that library
   whose class lost the function. `library_symbol` takes the symbol from
   the record of the extern, so the read passes and the link of a copy
   finds no such function. For a struct and for a generic the reader
   refuses the same staleness with `needs struct` and `needs generic`.
   Should a function of a class, and an item, be refused the same way at
   the read? `member_gone` of `unit_modules` holds today's behaviour.
