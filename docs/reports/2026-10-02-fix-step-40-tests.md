# Fix step 40, the minor findings of the tests

Step 40 of "Fix steps" in `docs/audit/summary.md`, the session for the
tests: the rows of the minor table whose places lie in `tests/`, the
rule 4 row and the ten of the row "Tests". The detail is in
`docs/audit/tests.md`. Logs are in `build/drive/logs/s40t-*`.

Status: green and pushed.

## Findings

| Finding | Result |
|---|---|
| Rule 4, unchecked allocations, `tests/unit/test_utf.c:11` | Fixed in `fbc477e4`. The programs of `tests/` that link `antic_core` allocate through `src/antic/alloc.h`, and `one_allocator` fails on a call of `malloc`, `calloc` or `realloc` in their sources. It named the old `test_utf.c` and `test_x86_64.c` (`s40t-oneall-red.log`). The two runtime programs that cannot link it check each result. |
| Copied unit test helpers, `tests/unit/test_sema.c:28` | Fixed in `fbc477e4`, `704a7bb9` and `c8845ef6`. `tests/unit/pipeline.c` holds the front end, lowering, selection and allocation, the parse and the IR print once. Fifteen files call it. The unguarded reads of the first message are gone, and `optimizes` of `test_optimize.c` verified the module before lowering it, so the lowered IR was never verified. `anti_core` and `antic_rt_sources` replace the source lists written by hand. |
| Dev objects compiled in four places, `tests/run_checks.cmake:96` | Fixed in `ca3bf832`. One list, `ANTIC_DEV_MODULES`, registers every `dev_object_<name>`, and `antic_dev_inputs` gives a test its objects and fixtures from the module names. The checks, the traces and the module tests take the objects from the fixtures. |
| Registrations without a helper, `tests/CMakeLists.txt:479` | Fixed in `ca3bf832` and `eaaa26f9`. `antic_program_test`, `antic_modules_test` and `antic_dump_test` hold the arguments that 36, 17 and 32 registrations wrote out. |
| Test names, `tests/CMakeLists.txt:3678` | Fixed in `eaaa26f9` and `2dcb38e6`. Dumps are `dump_<stage>_<subject>[_<target>]`, refusals `error_<name>`, checks `check_<name>`, traps `trap_<name>`, and `unit_<file>` runs one file. No name holds a `/`. |
| Descriptor pinned in 28 texts, `tests/unit/test_lower.c:547` | Fixed in `c8845ef6`. `runtime_types` pins the three records of `anti.rt`, and the other texts leave their type lines out. The global descriptors of a class still list one value per field. |
| Smaller features without a test, `src/antic/main.c:265` | Fixed in `dfe5d620` for `--no-trace`, `trace.start`, a `protected fn` from derived classes, `import anti.regex.{Regex}` and the stale overview line. The reflection stub is left, see below. |
| Dead runner code, `tests/run_start.cmake:1` | Fixed in `8de724a1`. The last check of `cmake_pin` reads the two installers and failed on a planted version (`s40t-pin-red.log`). |
| One kind per directory, `tests/CMakeLists.txt:320` | Partly fixed in `2dcb38e6`: `simd_big` is a program of `tests/programs/`, `tests/checks/` joined `tests/traps/`, and `run_table.cmake` is `run_trap_table.cmake`. The rest needs new directories, see below. |
| Tests that drop out, `tests/CMakeLists.txt:1313` | Fixed in step 36, `07249ad`. No `find_program(... strings)` is left. |
| Comment above the wrong test, `tests/CMakeLists.txt:1305` | Fixed in step 36. The raymath comment stands above the raymath test. |

## Left: questions for Eddie

- The stub of a slot only `reflect.call` reaches, `src/rt/plugin.c:268`.
  `anti_rt_reflect_call` of `src/rt/call.c` resolves the object's own
  class through `anti_rt_object_of`, and that class lists only the
  functions the library carries. A host that loads the older library of
  `tests/plugin/versions/one` and calls every index lists 17 functions and
  never reaches the slot of `asked`. Nothing in Anti seems to reach the
  stub, so no test can. Should `reflect.call` through an interface pointer
  read the host's interface, or is the stub only for C?
- `tests/anti-build/`, `anti-test/` and `anti-symbols/` beside `check/`,
  `doc/`, `fmt/` and `bind/` for the projects of the `anti` commands, and
  the 123 scripts flat in `tests/`. Either needs a new directory under
  `tests/`. Which names should stand?

## Provisional entries added

Under "Repository layout" of `docs/decisions.md`: the names of the tests,
and the library `anti_core` with `antic_rt_sources`.

## Gates

No warnings in the three trees (`s40t-final-build.log`,
`s40t-asan-build.log`, `s40t-ubsan-build.log`). The first full host run
failed `raw_output`: `pipeline.c` lacked `binary_stdio.h`, fixed in
`16e8297b`.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1459 of 1459 | 119 s | `build/drive/logs/s40t-final-host.log` |
| asan | 1458 of 1458 | 310 s | `build/drive/logs/s40t-asan.log` |
| ubsan | 1458 of 1458 | 298 s | `build/drive/logs/s40t-ubsan.log` |

## State

Taken after the push of the code and before the commit of this report.

```console
$ git log --oneline -3
16e8297b Include binary_stdio.h in the shared helpers of the unit tests
2dcb38e6 Keep one kind of test per directory where no new directory is needed (minor, tests/CMakeLists.txt:320)
704a7bb9 Share the parse and header helpers of the unit tests (minor, tests/unit/test_sema.c:28)
$ git status --short
$ git rev-parse HEAD origin/main
16e8297b13e027bfe14893fa19d9edab92d4ae26
16e8297b13e027bfe14893fa19d9edab92d4ae26
```
