# Batch 3l of the third phase of coverage

This step sorts the regions that `./c coverage` at the end of batch 3k
showed unreached in `src/antic/lower_eq.c`, `src/antic/lower_class.c` and
`src/antic/antl_write.c`, which 3k probed and left, and in
`src/anti/fmt.c`, `src/antic/antl_verify.c`, `src/rt/symbols.c`,
`src/antic/docpage.c` and `src/antic/ir.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each region first ran
through the antic, the anti or the unit tests of the coverage tree alone,
with a profile of its own. Eddie's direction is the Mac alone, so the VMs
were not touched. `src/antic/layout.c` waits for the answer on its guard
in `compute`. The probes found two defects, T263 and T264. T265 is the
next free id.

## Commits

- `9f35ad0e` lists T263, `53a5e9e3` fixes it.
- `c609cb3e` lists T264, `d104cd9f` fixes it.
- `dbde8428` tests `antl_verify.c`, `lower_eq.c`, `antl_write.c` and
  `symbols.c`.
- `3b1d4a2c` names the guards in `docs/notes/unreached-guards.md`.
- `7a711863` writes the commits of the fixes into their rows, and one
  commit records the times in `CLAUDE.md` with this report.

## Findings

- T263, major, fixed. A library file whose generic calls through a
  callee without a type that no symbol names, a struct literal or a field
  in place of the name of a function, passed the verifier, and antic
  crashed in `lower_is_aggregate` when a client compiled a copy.
  `stands_as` let every callee go without a type. `callee_read` now takes
  one without a type where it is a name or the callee of `x.hash()`, which
  the checker leaves untyped on a value of a type parameter, and the
  declaration of `sema_is_hash_call` moves to `sema.h`, since
  `header_sections` keeps the verifier out of `sema_checker.h`. The probe
  of `inits_found` met it. The format of the library file stays at 98.
- T264, minor, fixed. `anti doc` left the constants and the static fields
  of a class body out of every page, the user docs, the dev docs and the
  page of a library file, which carries the public ones with their text.
  `body_values` of `docpage.c` writes them after the functions. The form
  is a new `[provisional]` entry under "The doc command" in
  `docs/decisions.md`. The probe of `body_entries` met it.

## The files

- `antl_verify.c`: the callee of a copy written as an extern without its
  item, and an anonymous function in two places, each refused.
- `lower_eq.c`: `x.hash()` of a Mutex and of an array of them reaches the
  lock part of `lower_part_of`. `emit_identity` takes the new text of
  `eq_hash_agree`.
- `antl_write.c`: a struct with a unit break as a constant that a
  function of the library reads, whose break the writer writes as
  `IR_CONST_NONE`, in `bitfield_defaults`.
- `symbols.c`: a Mach-O object whose DWARF 5 table names its files in
  `__debug_line_str`, and a symbol the object lacks.
- `docpage.c`: the constants of a body, through the test of T264.
- `lower_class.c`, `fmt.c` and `ir.c`: guards alone.

`ir_lane_bytes` of a lane that is no number stays open: a library file
whose simd aggregate starts with a pointer reaches it, and its test waits.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.44%, 1320 missed | 98.45%, 1310 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.65%, 1292 missed | 98.67%, 1276 missed |
| Branches | 91.96%, 4502 missed | 91.98%, 4496 missed |

| File | Lines missed before | after | Branches missed before | after |
|---|---|---|---|---|
| `lower_eq.c` | 11 | 9 | 12 | 11 |
| `lower_class.c` | 11 | 11 | 19 | 19 |
| `antl_write.c` | 11 | 8 | 29 | 28 |
| `fmt.c` | 10 | 10 | 90 | 90 |
| `antl_verify.c` | 8 | 4 | 68 | 68 |
| `symbols.c` | 8 | 5 | 50 | 47 |
| `docpage.c` | 8 | 6 | 49 | 50 |
| `ir.c` | 10 | 10 | 15 | 15 |

The lines that stay are guards, the `}` after a `return` and `ir_lane_bytes`.
`body_values` adds a branch that no page reaches, a constant without a
type. The coverage suite ran 7466 tests, all passed. `./c coverage` took
1012 s.

## Suites

- The host suite ran before each of the five code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,7478`. The times stand in
  `CLAUDE.md`. Before the fix of T263, `header_sections` failed on the
  include of `sema_checker.h`, and the declaration moved. Before the
  commit of the tests, `emit_identity` failed on the new text of
  `eq_hash_agree`, and the manifest took it. Each suite ran again whole.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7476. Every test passed. The last part took 610 s
  under ASan and 633 s under UBSan.
- The host runs 7478 tests and each sanitizer tree 7476.
  `tests/known-failures.txt` holds the two lines of `host=windows` alone.

## Next candidates

`src/rt/platform_posix.c` misses 13 lines, `src/rt/fault.c` 13,
`src/rt/conf.c` 8, `text.c` 8 and `sema_declare.c` 7 with 50 branches.
None has a section in the note yet.

## Questions for Eddie

1. T264 writes a constant of a class body as `pub const MOST: int = 64`
   and a static field as `pub static atomic made: int`, after the
   functions, by the new `[provisional]` entry. Should the form stand?
2. The questions of the guard of `compute` in `layout.c` and of the
   refusal of `(b.g, 1)` from 3k are still open.
