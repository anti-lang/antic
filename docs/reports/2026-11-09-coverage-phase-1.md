# The second answers of 2026-11-09 and the first phase of coverage

Eddie answered the two questions of
`docs/reports/2026-11-09-graphics-window.md` with yes, under "Answers of
2026-11-09, second set" in `docs/decisions.md`. The coverage run of the day
failed `libm_symbols` in the coverage tree alone and showed 12 functions
with a count of 0. This step builds the answers, fixes the coverage tree and
takes each of the 12 functions in turn. Eddie's direction of 2026-11-09 is
the Mac alone, so the VMs were not touched.

## Commits

- `257df2dc` records the answers, a line in the entry of the feature freeze,
  the note of `anti.graphics` and the overview.
- `992643fc` refuses a flag of a window changed after a query of the
  monitor.
- `4027eed5` leaves `libm_symbols` out of the coverage tree.
- `36ca1d7f` tests `alloc_out_of_memory_for` and `unexpected`.
- This report, with the times and the counts in `CLAUDE.md`.

## The answers

- Q1. `std_graphics_hidpi_off` and `std_graphics_window_points` stay in the
  host suite of the Mac. A comment in `tests/CMakeLists.txt` names the
  answer.
- Q2. A query of the monitor before `start` keeps the flags it opened the
  hidden window with in the private field `prepared_flags` of `Window`.
  `start` compares them with the flags of the fields and fails with
  `the field vsync of the window changed after a query of the monitor opened
  it`, through `fail` with an `f` string, so no new type enters the library. The
  comparison stands in `Window` and not in a display, so the Recorder
  follows the rule without a change. `width` and `height` are no flags. A
  flag set back to its value and a flag set before the query open the
  window. `std_graphics_window_flags` takes six cases over the Recorder in
  release mode, in dev mode and under `--memory-checks`, and failed before
  the change for the reason the cases name. The refusal stands before any
  call of the display, so the path of the hidden window of raylib needs no
  test of its own, and `std_graphics_window_points` still passes.
- `docs/anti-language-additions.md` describes neither the sizes nor the
  queries of a window, which the answers of 2026-11-09 left to the note and
  the overview. It stays as it is.

## libm_symbols

The runtime of a coverage tree carries the profile runtime, so `libm.o`
names `___llvm_profile_runtime` there. `tests/CMakeLists.txt` disables the
tests that read the bytes of the runtime in a coverage tree with
`set_tests_properties(... PROPERTIES DISABLED TRUE)` under
`ANTIC_COVERAGE`. `libm_symbols` stands in `src/native/CMakeLists.txt`,
which the configure step reads after `tests/`, so the same property is set
there. `ctest -R '^libm_symbols$'` gives "Not Run (Disabled)" in
`build/coverage` and passes in `build/host`.

## The 12 functions

| Function | What happened |
|---|---|
| `alloc_out_of_memory_for`, `src/antic/alloc.c` | Tested. `unit_alloc_oom` calls `anti_rt_out_of_memory` of `src/antic/rt_memory.c` and expects status 70 and the size. |
| `unexpected`, `src/antic/llvm_emit_function.c` | Tested. `unit_llvm_emit` translates a return of an operand of kind `IR_NONE`, which the lowering never writes. |
| `work_not_waited`, `src/antic/platform.c` | Reached by `antic_message_fault_work_not_waited`, which passes. It ends through `_Exit(1)` by its DESIGN comment, which runs no handler of exit, so no profile is written. No path writes its counts. |
| `find_crt_dir`, `src/antic/driver_link.c` | Other host. `--linker platform` for Linux, on a Linux host: `program_platform_linker` of anti-linux. The Mac refuses that link before. |
| `elf_id` and `symbolize_elf`, `src/rt/trace.c` | Other host. The platform layer of Linux gives ELF images. |
| `symbolize_pe`, `src/rt/trace.c` | Other host. The platform layer of Windows gives PE images. |
| `anti_rt_fault_system_error`, `src/rt/fault.c` | Other host. `ANTI_FAULT_SYSTEM` stands in the code of `_WIN32` alone, so the fault cases of Windows reach it. |
| `diagnostics_check`, `src/antic/diagnostic.c` | Candidate for removal. No caller: the checker calls `diagnostics_vcheck`. |
| `lower_calloc_function`, `src/antic/lower.c` | Candidate for removal. No caller. |
| `anti_rt_fault_fflush`, `src/rt/fault.c` | Candidate for removal, with `ANTI_FFLUSH`. No file uses the macro. |
| `stub`, `src/rt/plugin.c` | No input found. See below. |

The count of 74 of `anti_rt_out_of_memory` of `src/antic/rt_memory.c` was
the count of its namesake of `src/rt/mem.c`. llvm-cov merges two functions
of one name and one shape, so the function looked reached while
`alloc_out_of_memory_for`, its one callee, was not.

`stub` fills a slot of the sub-object of an older library that only
`reflect.call` may reach. `reflect.call` takes the index of a function of
the class of the object and calls through the table of the object at its
start, never through the table of the sub-object. A host built against the
interface of `tests/plugin/` loaded the library of `versions/one` with an
own function `extra` of the class. The call of `extra` gave its result, and
no index of the class reaches the stub. A call of the program that reaches
the slot is refused at the load, by the bitmap.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 97.71%, 1910 missed | 97.73%, 1898 missed |
| Functions | 99.74%, 12 missed | 99.78%, 10 missed |
| Lines | 97.86%, 2019 missed | 97.88%, 1998 missed |
| Branches | 90.36%, 5328 missed | 90.38%, 5321 missed |

The coverage suite ran 6994 tests, all passed, with 12 disabled,
`libm_symbols` among them. `./c coverage` took 990 s.

## Suites

- The host suite ran before each code commit in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7006`. Before the commit of the flags of a
  window it took 384 s, 271 s and 325 s, and before the commit of the two
  unit tests 384 s, 276 s and 334 s, every test passing apart from the two
  skips of `mimalloc_environment`.
- Before the commit of `libm_symbols` the parts took 385 s, 269 s and
  320 s, and `unit_alloc_oom` failed. Its runner had been edited for the
  next commit while the suite ran, so it asked the old program for the
  helper `runtime`. With that edit set aside the test passed alone, and
  the commit carries none of it.
- Both sanitizer suites ran after the last code commit in the six parts
  of `asan`, up to test 7004, and every test passed. `asan` took 252 s,
  188 s, 262 s, 197 s, 453 s and 469 s. `ubsan` took 256 s, 188 s, 261 s,
  208 s, 472 s and 499 s.

## Provisional lines

The [provisional] line of the first query of the monitor stays tagged, and
the line after it, the refusal, is Eddie's rule. No new provisional line.

## Questions for Eddie

1. `diagnostics_check`, `lower_calloc_function` and `anti_rt_fault_fflush`
   with `ANTI_FFLUSH` have no caller. Should they go?
2. `stub` of `src/rt/plugin.c` is reached by no program this step found.
   Should it stay as a guard, get a C test that calls the slot, or go?
3. anti runs the code of antic in its own process, and memory that runs
   out there prints `antic: out of memory`, with the name of antic. A fault
   case of `anti build` at the eleventh allocation, in the reader of the
   manifest, gives `antic: out of memory for 80 bytes`. The layer of the
   platform names the tool through `platform_set_tool`, by R163. Should
   `alloc.c` name anti as well when anti runs it?
