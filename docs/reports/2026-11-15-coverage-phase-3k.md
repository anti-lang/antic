# Batch 3k of the third phase of coverage

This step sorts the regions that `./c coverage` at the end of batch 3j
showed unreached in `src/antic/lower_owning.c`, `src/antic/sema.c`,
`src/rt/toml.c`, `src/antic/ast_dump.c`, `src/anti/main.c` and
`src/antic/driver_search.c`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`. Each region first ran
through the antic or the anti of the coverage tree alone, with a profile
of its own. Eddie's direction is the Mac alone, so the VMs were not
touched. `src/antic/layout.c` waits for the answer on its guard in
`compute`. The probes found three defects, T260 to T262. T263 is the next
free id.

## Commits

- `d107353e` lists T260, `f195075d` fixes it.
- `7eeef9e9` lists T261 and T262, `ef1384c3` fixes T262 and `d004436c`
  fixes T261.
- `2514695b` tests `sema.c`, `ast_dump.c` and `toml.c`.
- `a747ef0c` tests `lower_owning.c`.
- `fe313898` names the guards in `docs/notes/unreached-guards.md`.
- Three commits write the commit of each fix into its row, one records
  the times and the counts in `CLAUDE.md`, and this report.

## Findings

- T260, severe, fixed. A `keep own` parameter in a tuple or an array
  literal, `(f, 1)` and `[f]`, gave its snapshot two owners, a trap in
  dev and release mode and a double free under `--memory-checks`. An
  `own` field of function type was copied into a tuple, and the repeat
  form took an `own fn`. The element of a literal without a context now
  takes an `own fn` through `require_owned`, as an `own fn` field does: a
  `keep own` parameter moves, and an existing value is refused. The
  repeat form refuses an `own fn`. The probe of the clear of an `own fn`
  in `lower_owning.c`, which 3j left unsorted, met it.
- T262, major, fixed. A constraint whose interface names a parameter of
  the same generic, `C: collection.Iterable<E>` or `T: Before<T>`,
  refused every call: `List<int>` had no `Iterable<E>` for `E = int`.
  `check_meets` now substitutes the arguments of the use into the
  interface, at a call, in the check that waits for the types of the
  module and in the copy of an operator.
- T261, major, fixed. `antic -c` of a module with `T: Before<T>` ended
  with a bus error. The writer visited the interfaces of a parameter
  before it put the parameter in the table. The parameter now
  comes first, and the reader binds the interfaces once the table is
  whole. The format version goes from 97 to 98.

The probe of the guard at line 331 of `antl_write.c` met T261 and T262.

## The files

- `lower_owning.c`: a tuple that holds a `keep own` parameter moves into
  an array, so the clear of the `own fn` in it runs. That tuple exists
  since the fix of T260.
- `sema.c`: a `catch` name in the base of a call of a function of a class
  shadows a local, and the probe of an atomic call reads that base
  quietly. A length `[-(-N)]int` reaches the unary arm of
  `params_alone`.
- `toml.c`: `Object.deserialize` makes a `toml.Document` with no read
  document, as `std/deserialize_regex_search.anti` does for a `Regex`.
- `ast_dump.c`: the dump of the checked tree holds `take`.
- `main.c` of `anti` and `driver_search.c`: guards alone. The merge of
  one framework that two library files name is reachable, but its test
  needs the link against Apple's SDK of `run_framework.cmake`, so it
  waits for a later batch.

`lower_eq.c`, `lower_class.c` and `antl_write.c` were probed and not
sorted. The probes of `T(args)` on a class without `construct` and of
`compare_at` on a lock part met earlier refusals.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.43%, 1329 missed | 98.44%, 1320 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.63%, 1309 missed | 98.65%, 1292 missed |
| Branches | 91.94%, 4511 missed | 91.96%, 4502 missed |

| File | Lines missed before | after | Branches missed before | after |
|---|---|---|---|---|
| `lower_owning.c` | 12 | 6 | 39 | 37 |
| `sema.c` | 11 | 7 | 53 | 51 |
| `toml.c` | 11 | 9 | 36 | 35 |
| `ast_dump.c` | 11 | 6 | 24 | 23 |
| `main.c` of `anti` | 10 | 10 | 58 | 58 |
| `driver_search.c` | 10 | 10 | 11 | 11 |

The fixes add code: `sema_generic.c` misses one more branch and
`sema_expr.c` three, and `antl_read.c` gained 24 lines, all reached. The
coverage suite ran 7466 tests, all passed. `./c coverage` took 1005 s.

## Suites

- The host suite ran before each of the seven code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,<last>`. The times stand in
  `CLAUDE.md`. Before the list of T260 `fmt_canonical` failed on three
  new files, which `anti fmt` then wrote. Before the fix of T260
  `known_failures` failed on the row still open. Each was fixed and its
  test run again alone, and every other test had passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7476. Every test passed. The last part took 575 s
  under ASan and 596 s under UBSan.
- The host runs 7478 tests and each sanitizer tree 7476.
  `tests/known-failures.txt` holds the two lines of `host=windows` alone.

## Next candidates

`lower_class.c` and `lower_eq.c` miss 11 lines each, `antl_write.c` 11,
`antl_verify.c` 8 with 68 branches, `src/rt/symbols.c` 8 with 50 and
`docpage.c` 8 with 49. None has a section in the note yet.

## Questions for Eddie

1. The fix of T260 refuses `(b.g, 1)`, an `own` field of function type
   in a tuple, with `` an `own fn` value is not copied by `=`, use `dup` ``,
   the message of the same copy by `=`. `let g = b.g;` borrows the value
   instead, by the entry on `dup` under "Anonymous functions and
   closures". A tuple that borrows would hold the snapshot without owning
   it, and could outlive the field. Should the refusal stand?
2. The question of the guard of `compute` in `layout.c` from 3j is still
   open.
