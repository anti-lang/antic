# Batch 3n of the third phase of coverage

This step takes the two leftovers of batch 3m, `rt.configure("")` and the
first guard of `sema_simd_meets`. It then sorts the files with the most
missed lines that no batch of the third phase had sorted:
`sema_operator.c`, `sema_closure.c`, `lower.c`, `lower_hook.c`,
`driver_library.c`, `antl_io.c`, `linker.c`, `llvm_run.c` and
`userdirs.c` of antic, `lock.c`, `signal.c`, `cpu.c` and `regex.c` of the
runtime, and `bind.c`, `doc.c`, `sdk.c`, `units.c`, `zip.c`,
`jsontree.c` and `bindexpr.c` of `anti`. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`: each region first ran
through the antic, the anti or the `anti_faults` of the coverage tree
alone, with a profile of its own. The summary of batch 3m stood for the
code of HEAD, whose commits after it change `docs/` alone, so the step
started from it. Eddie's direction is the Mac alone, so the VMs were not
touched. `src/antic/layout.c`, the framework region of
`driver_search.c` and T265 stay as they are. The probes found no defect.
T267 is still the next free id.

## Commits

- `e7e54401` tests `sema_closure.c`, `sema_operator.c` and `sema_simd.c`.
- `f0bb15d9` tests `conf.c`, `cpu.c`, `sdk.c` and `driver_library.c`.
- `2f627399` names the guards in `docs/notes/unreached-guards.md`. One
  commit records the times in `CLAUDE.md` with this report.

## The files

- `sema_simd.c`: a struct with a field of a simd struct whose lanes its
  declaration refuses, compared with `==`, reaches the first guard of
  `sema_simd_meets`. It gets the refusal of the lanes and that of `==`.
  The same struct with `f32` lanes gets the second alone, since a simd
  struct meets no `eq`, so each message names a mistake of its own, by
  the precedent of T096 and T232. The other caller that asks for `add`,
  `class_operator`, stops at the copy of the operator before it.
- `sema_operator.c`: a module-level `operator fn hash` of `Box<Box<T>>`
  is not the hash of `Box<int>`, whose copy of the operator fails, and
  `Box<int>` keeps its default hash in the three modes of the matrix.
- `sema_closure.c`: an anonymous function that may fail without
  `import anti.lang`, and a snapshot of a local whose value names
  nothing, each with its one message.
- `conf.c`: `rt.configure("")` ends with status 70 and `cannot read the
  configuration file`, as for a file that is not there. No decision gives
  an empty path a meaning of its own, so it names a file of no name.
- `cpu.c`: `unit_cpu` reads the level without `ANTI_CPU_LEVEL` and with
  a value that names no level. Both give the level of the machine.
- `sdk.c`: the case of T155 fails the third stat, the kind of the stub
  through a link. The new case `fault_sdk_export_entry_kind_unread` fails
  the second, the kind of the entry itself. The first branch of the fix
  of T155 refuses it, and no test reached that branch before.
- `driver_library.c`: `own_tools` bundles a Windows library over an
  archive whose `anti_rt.lib` holds a member that is no object. antic
  names the member, says it cannot bundle the runtime and writes no
  library.
- The other files: guards alone, and the `}` after a `return` of
  `src/rt/object.c`. The lines of `src/rt/f16.h` that the summary counts
  as missed are reached by one copy of the header or more, so the note
  leaves them out.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.48%, 1291 missed | 98.49%, 1280 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.69%, 1253 missed | 98.71%, 1238 missed |
| Branches | 92.06%, 4452 missed | 92.08%, 4438 missed |

| File | Lines missed before | after | Branches missed before | after |
|---|---|---|---|---|
| `sema_simd.c` | 7 | 5 | 35 | 34 |
| `sema_operator.c` | 5 | 3 | 66 | 65 |
| `sema_closure.c` | 4 | 0 | 30 | 28 |
| `driver_library.c` | 6 | 2 | 22 | 19 |
| `cpu.c` | 4 | 2 | 5 | 3 |
| `sdk.c` | 5 | 4 | 12 | 11 |
| `conf.c` | 8 | 8 | 9 | 8 |

The lines of the other files of the step stay as they were, all guards.
The coverage suite ran 7495 tests, all passed. `./c coverage` took
1006 s.

## Suites

- The host suite ran before each of the two code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,<last>`. The times stand in
  `CLAUDE.md`. Before the second commit, `host_names` failed on the new
  case of `own_tools`, which named `CMAKE_EXECUTABLE_SUFFIX` in a script
  of `cmake -P`. The case now takes the suffix as the runner takes its
  separator, and the suite ran again whole.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7505. Every test passed. The last part took 581 s
  under ASan and 612 s under UBSan.
- The host runs 7507 tests and each sanitizer tree 7505.
  `tests/known-failures.txt` holds the two lines of `host=windows` and
  the three of T265, as before.

## Next candidates

Every file with more than three missed lines now has a section in the
note or runs on another host. By branches, `src/anti/bindclang.c` misses
101 with 1 line and has no section, `src/rt/regex.c` 48, `bindexpr.c`
32 and `doc.c` 30, where the sides of a condition are not sorted yet.

## Questions for Eddie

1. T265 still waits for the answer to the question of 3m. Should the
   fix check every default once every type of the file has its fields,
   and remove `complete_defaults`?
2. The questions of the guard of `compute` in `layout.c`, of the refusal
   of `(b.g, 1)` from 3k and of the form of T264 from 3l are still open.
