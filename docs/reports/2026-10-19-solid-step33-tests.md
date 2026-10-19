# Step 33, the minor findings of the tests

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. This session took the tests, the last
area, from the rows of the minor tables whose places lie in `tests/`, as
`docs/audit/tests.md` opens them. It also took the chapters of the book
in the comments of the tests, which the earlier sessions left to it. The
logs stand in `build/drive/logs/step33-tests/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| An index of the token lists, `test_modules.c:54` | `e566538c` | The two unit files wrote the next token list with no test of the count. `compilation_bound` aborted on the host before. |
| Two unit files run the stages themselves, `test_whole.c:49` | `e566538c` | `test_whole.c`, `test_modules.c`, `test_contract.c` and `test_sync.c` lexed, parsed and checked over structs of their own. `pipeline.c` holds `struct compilation` now. |
| Finding numbers two audits use, `run_antl_verify.cmake:1` | `ca217a98` | 61 comments cited M3, S12 and others alone. Each names its audit, read from the date of its commit. |
| Chapters of the book in the comments | `a6b43227` | Five comments named a chapter. The comments of `tests/CMakeLists.txt` lost theirs in `f320553b`. |
| Test names without their kind, `CMakeLists.txt:309` | `a12bfe01` | 49 tests were `<subject>_modules_<mode>`, and eleven programs of `tests/unit/` ran as `rt_*`, `files_oom` and `alloc_oom`. |
| Expected texts beyond their subject, `test_lower.c:1277` | `e6b364c1` | 65 lines of expected IR held the bytes of check messages. `ir_print_own` leaves them out, and `check_texts` failed before. |
| Registrations written out, `CMakeLists.txt:479` | `13c3b67b` | `-DANTIC` with `-DRUNTIME` stood at 84 sites, `run_asm.cmake` at seven and `run_warnings.cmake` at six. |
| A build file in the order of its history, `CMakeLists.txt:1` | `b3795a6a` | The file grew in the order of the features. It holds the helpers, then nine sections by kind. The runners moved in `b906e240`. |
| 25 files no unit test reaches, `driver.c:1` | `c35521ad` | 20 are reached now, among them every file of the driver through the commands of `anti` run in the process. `memreport.c` was reached before. |

`1f161b5f` writes the commits into the summary.

The reorder keeps every registration. `ctest --show-only=json-v1`
before and after gives the same 5147 names, commands and properties,
apart from two. `one_allocator` lists the same sources in another
order. `error_modules_soname_without_a_version` named its output with
`plugin_suffix` above the line that set it, so the name had no suffix.
`run_find_order` passes.

## Findings left

- The directories that hold more than one kind, `tests/dump/` and
  `tests/unit/` among them. Splitting them takes new directories under
  `tests/`, which is Eddie's decision.
- Four of the 25 files. The two `main.c` files are the entries of the
  tools. `bindclang.c` runs the clang beside `anti`, and the unit test
  program stands elsewhere, so it would run a clang of the host. `sdk.c`
  runs tar and xz over Apple's SDK. The tests of `anti bind --clang` and
  `anti sdk` reach them.

## New defects

- `4d5e460d`: `antic_native_tests` of `src/native/CMakeLists.txt`, from
  the build session of this step, made every configure print six author
  warnings of `cmake_parse_arguments`. The registered commands stay the
  same. No row, since the fix is in the same step.
- The latent missing suffix of `error_modules_soname_without_a_version`
  above. `19ea9500` rewraps a comment that the rename of
  `modules_shared_class` lengthened, which `fmt_canonical` refused.

`tests/known-failures.txt` holds 29 lines before and after. One line now
names `unit_rt_oom_fs`. No `[provisional]` entry, no new feature, option
or command, and no change to `emit_identity` or any
`link_identity_<target>`.

## Gates

- The three builds report no warnings: `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5153 tests, `-I 1,2500` 387 s and `-I 2501,5153` 270 s. All 2500
  and 2660 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `host-1.log` and `host-2.log`.
- ASan, 5151 tests, `-I 1,1500`, `-I 1501,3000` and `-I 3001,5151`,
  303 s, 201 s and 529 s. All 1500, 1526 and 2158 pass:
  `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 287 s, 159 s and 365 s. All pass:
  `ubsan-1.log`, `ubsan-2.log` and `ubsan-3b.log`. The first run of the
  third part, `ubsan-3.log`, stopped `matrix_values_aggregates_dev` at
  the 60 s of the matrix runner. It started among the first tests of the
  part, under the full load, and took 34 s in the second run.
- The docs-style checker reports nothing on each `.md` file the session
  touched: `docs1.log` to `docs5.log`.

## Not run on the target

- `c35521ad`, the suffix of opt and llc in `test_llvm_run.c` and the
  link and the run of `test_commands.c`. `unit_llvm_run` and
  `unit_commands` on Linux and Windows.
- `b3795a6a`, the configure of the reordered file on Linux and Windows,
  where other branches of it run. The suite of each VM.
- `b3795a6a`, `plugin_exe` and `plugin_suffix` set with the helpers,
  which a Windows configure reads. `plugin_host` on anti-windows.

## Questions for Eddie

- Should `tests/dump/` and `tests/unit/` split into directories of one
  kind each?
- `matrix_values_aggregates_dev` takes 34 s of its 60 under UBSan. Should
  the limit or the program change?

## State at the push

```text
$ git log --oneline -3
19ea9500 Wrap the module comment of points.anti as anti fmt writes it
1f161b5f Record the tests minor findings of step 33 in the summary
c35521ad Reach the files of antic and anti that no unit test reached
$ git status --short
$ git rev-parse HEAD origin/main
19ea95004dce3908c2f48c186597468b54802ed1
19ea95004dce3908c2f48c186597468b54802ed1
```

Step 33 is complete with this area. Every area of the step has its
report of 2026-10-19: `docs/reports/2026-10-19-solid-step33.md` for the
front end and one named after each other area.
