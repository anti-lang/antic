# The third batch of coverage

This step sorts every region that the coverage report showed unreached in
`src/anti/test.c`, `src/antic/sema_copies.c` and `src/antic/driver_link.c`.
It follows the method of `docs/reports/2026-11-10-coverage-phase-2b.md`.
Eddie's direction is the Mac alone, so the VMs were not touched. The batch
adds tests, and it fixes one defect that a test showed. It deletes no code.

## Commits

- `c40f751c` tests the runs of `test.c` over a project.
- `d9ceddf3` tests the links of `driver_link.c`.
- `22fe67aa` lists T222.
- `1d2764e9` fixes T222.
- `91498b3c` tests the cache check of a second run over a project. The
  coverage run after `1d2764e9` showed it unreached.
- This report, with the times and the counts in `CLAUDE.md`.

`sema_copies.c` has no commit, since no input reaches a region of it.

## Findings

T222, fixed. A macOS program of `--memory-checks` makes the runtime
directory of its rpath absolute. When the system refused, antic ended with
1 and printed nothing. `driver_link_inputs_of` now names the directory
with `cannot find the directory of`, as every other caller in the file
does. The fix changes antic alone, so no pinned digest moved.

## test.c

Eighteen fault cases of `tests/anti-test/messages.cmake` run `anti test`
over a project of two modules. They refuse the library directory of the
run, the cache of the library file, of the object and of the runner in
`obj/`, their keys, the copy into the run, the object of an import, the
files the key of the runner reads and its link. One case runs twice, and
the second run cannot read the imports of its cached library file, see
question 5. `source_not_parsed` reaches a module the parser refuses.
`run_anti_test_project.cmake` gains the cases `injections`, `links` and
`unreadable_runtime`. The last is not registered on Windows.

The counts of the faults were found on the Mac. The messages name
`${HOST}`, so the cases run on every host, which no run has proved.

Unreachable, question 1:

- `unit_host` fails on none of the six targets, lines 108 and 614.
- A run over a project always carries a version, line 71.
- `driver_run` never returns `DRIVER_OBJECT_ONLY`, lines 218 and 353, see
  question 2.
- Outside a project the run names no library file of the graph, so the
  loop at line 338 has nothing to leave out.

## sema_copies.c

Each region is a guard behind the checker, which refuses the form first or
never builds it. Question 3:

- `copy_failed` in every form but the one of T095: a call that names no
  copy, lines 646, 674 and 734, a class nested in a generic, 695 and 762,
  a copy of its class that lacks the member, 771, and a generic named as
  a value, 1166. The checker refuses `let f = id;` with
  `` `id` is generic, and names a function where it is called ``. A
  library file carries no function of a class with type parameters of its
  own, so a client gets `` `Stack<int>` has no function `map` ``. The form
  without a name, line 122, stays in `tests/diagnostics-open.txt`.
- The form of T095 for a clone of the module itself, line 115.
- `type_item` without its generic, 449, and the guards of
  `member_of_copy`, 814 to 833.
- An index in `redirect`, 960: no slice or array meets `index`, so the
  copy turns every open index into a call.
- `set_index` and `for` over a slice or an array, 1347 to 1354 and 1389 to
  1394, for the same reason.
- A recheck that fails, 977, the guards of `receiver`, 1029, and those of
  `xparams` and `xi`, 1553, 1576 and 1584.
- Operands that are never NULL and maps that are never empty, at lines
  109, 132, 149, 159, 212 to 235, 259 to 269, 305, 370, 488 to 492, 537,
  634 to 641, 717 to 766, 784, 1004 to 1011, 1081, 1296, 1382, 1442, 1590
  to 1598 and 1699 to 1714.

## driver_link.c

Fault cases of `tests/antic_messages.cmake` refuse the second call of
xcrun, the version of the SDK in the sysroot and the version of an
imported SDK, after which the link takes the SDK of the Command Line
Tools. They refuse the directory and a path of a Windows link, the runtime
library, the object and the `.def` file of a Windows host of plugins, and
the DLL of AddressSanitizer of a Windows program of `--memory-checks`.
Stand-in runtime archives give the version of the SDK a carriage return
and give Windows a runtime library that is no COFF archive. An xcrun on
`PATH` prints nothing, and `native_level_arm64` refuses `armv8.2` for a
program of `anti.regex`.

`tests/anti-build/messages.cmake` refuses the root and the object of the
debug map of a release build, see question 6. Stand-in dsymutils write
lines of other kinds and leave the link without its bytes, its symbols or
its debug map.

Other host: `find_crt_dir`, lines 37 to 55, and the platform linker of
Windows, 248, run on a Linux or a Windows host alone. antic refuses the
platform linker of another system before the link. A path on another
root, 391, exists on Windows alone.

Unreachable, question 4:

- a host that is none of the six targets, line 71
- an object of `--lto none` that is NULL, 232
- `driver_windows_link_paths` without a `.def` file, 431, since both
  callers pass one
- a level of another architecture than the target's, 527
- a strip of Mach-O without the root of the debug map, 863, which
  `anti build` always gives

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.02%, 1657 missed | 98.07%, 1615 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.23%, 1673 missed | 98.29%, 1613 missed |
| Branches | 91.00%, 4978 missed | 91.11%, 4916 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `test.c` | 89.66%, 48 missed | 98.06%, 9 missed | 82.80% | 95.70% |
| `sema_copies.c` | 96.61%, 47 missed | 96.61%, 47 missed | 88.95% | 88.95% |
| `driver_link.c` | 94.01%, 37 missed | 96.46%, 22 missed | 82.99% | 93.79% |

The missed function of `driver_link.c` is `find_crt_dir`. The coverage
suite ran 7145 tests, all passed, and `./c coverage` took 961 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7155. Every test passed. `asan` took 247 s,
  187 s, 259 s, 188 s, 457 s and 449 s. `ubsan` took 251 s, 189 s,
  261 s, 197 s, 479 s and 472 s.

## Provisional lines

No new provisional line.

## Phase 2

| | Start of phase 1 | Now |
|---|---|---|
| Regions | 97.71% | 98.07%, 1615 missed |
| Functions | 99.74%, 12 missed | 99.87%, 6 missed |
| Lines | 97.86%, 2019 missed | 98.29%, 1613 missed |
| Branches | 90.36% | 91.11%, 4916 missed |

The phase found five defects. T218, an array of unions in a collection
serialized as `[[]]`, T220, the floor of an interface compared on eight
parts, and T222 are fixed. T219, a type argument that is an array of two
levels, and T221, an `&&` chain that narrows eight names, wait for Eddie.
Most of what stays unreached is a guard that the questions of 2a, 2b and
this report name.

## Questions for Eddie

1. Keep the guards of `test.c` listed above, or remove them?
2. `driver_run` turns a dev object of a module without `main` into 0,
   although `driver.h` says it returns `DRIVER_OBJECT_ONLY`, and `test.c`
   and `build.c` test for that status. Should it return the status, or
   should the comment and those tests go?
3. Keep the guards of `sema_copies.c` listed above, or remove them?
4. Keep the guards of `driver_link.c` listed above, or remove them?
5. A run of `anti test` whose cache cannot be read says `cannot read` and
   passes, since it compiles the file again. Should a cache that cannot
   be read be silent?
6. When the root of the debug map cannot be made absolute, the message
   names the program, `cannot find the directory of <program>`. Should it
   name the root?
