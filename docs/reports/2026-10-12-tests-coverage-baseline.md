# The coverage measurement and its baseline

The step `coverage-baseline` of the test run, on 2026-10-12, on `main`.
`./c coverage` now measures what the suite reaches in antic, anti and the
part of the runtime they compile. This report holds the first measurement,
of the suite at `d6c44aa1`.

## What the step added

- The preset `coverage` of `CMakePresets.json`, a Debug build in
  `build/coverage`. The pinned clang compiles antic, anti and the nine
  programs of `tests/unit/` with `-fprofile-instr-generate
  -fcoverage-mapping`.
- `tools/coverage.cmake` holds the flags, the directory of the profiles
  and the targets `coverage_clear` and `coverage_report`.
- `tools/coverage-report.cmake` merges the raw profiles with llvm-profdata
  and runs llvm-cov. It writes `build/coverage/summary.txt` and
  `build/coverage/html/`.
- `./c coverage` builds the tree, removes the old profiles, runs the suite
  and writes the report. `./c coverage part 1/2` runs one part per call,
  and the last part reports the profiles of every part. `./c coverage
  report` writes the report again.
- The test `coverage_report` runs the report script on a program of three
  files, with two profiles that reach one function each.
- `tests/runners/run_profile.cmake` and `tests/runners/run_anti_build.cmake` set
  `LLVM_PROFILE_FILE` for the one program that reads it.
- `CLAUDE.md` describes the mode under "Building", and
  `docs/decisions.md` records it under "Repository layout" with one
  `[provisional]` line for the choices inside it.

## The baseline

`./c coverage` ran the 1704 tests in 360 s, and all passed, in
`build/drive/logs/cov-final.log`. 51 raw profiles were merged.

| Directory | Files | Lines | Not reached | Reached | Functions | Not reached | Reached |
|---|---|---|---|---|---|---|---|
| `src/antic/` | 114 | 58715 | 4206 | 92.84% | 2682 | 7 | 99.74% |
| `src/anti/` | 34 | 12727 | 1331 | 89.54% | 544 | 4 | 99.26% |
| `src/rt/` | 31 | 5300 | 1371 | 74.13% | 349 | 63 | 81.95% |
| All three | 179 | 76742 | 6908 | 91.00% | 3575 | 74 | 97.93% |

| Directory | Regions | Not reached | Reached | Branches | Not reached | Reached |
|---|---|---|---|---|---|---|
| `src/antic/` | 50887 | 3650 | 92.83% | 34849 | 5509 | 84.19% |
| `src/anti/` | 10956 | 1109 | 89.88% | 6797 | 1555 | 77.12% |
| `src/rt/` | 5076 | 1239 | 75.59% | 3089 | 1004 | 67.50% |
| All three | 66919 | 5998 | 91.04% | 44735 | 8068 | 81.96% |

The row of `src/rt/` is not the runtime of an Anti program. It holds the
files that antic, anti and the unit tests compile, 18 of the 42 C files and
13 headers. See "Where the task and the code differ".

## The twenty files of `src/antic/` with the most lines not reached

| File | Lines | Not reached | Reached |
|---|---|---|---|
| `pattern.c` | 938 | 300 | 68.02% |
| `ast_dump.c` | 984 | 187 | 81.00% |
| `antl_tree.c` | 1650 | 173 | 89.52% |
| `sema_stmt.c` | 2402 | 165 | 93.13% |
| `driver.c` | 1389 | 165 | 88.12% |
| `sema_expr.c` | 1967 | 150 | 92.37% |
| `sema_const.c` | 950 | 145 | 84.74% |
| `llvm_emit.c` | 3067 | 137 | 95.53% |
| `sema_call.c` | 1883 | 123 | 93.47% |
| `antl_verify.c` | 786 | 121 | 84.61% |
| `antl_read.c` | 972 | 120 | 87.65% |
| `sema_generic.c` | 1714 | 115 | 93.29% |
| `sema_copies.c` | 1324 | 106 | 91.99% |
| `antl_read_ir.c` | 983 | 96 | 90.23% |
| `sema.c` | 2231 | 92 | 95.88% |
| `parser_stmt.c` | 679 | 87 | 87.19% |
| `parser_expr.c` | 836 | 85 | 89.83% |
| `parser_decl.c` | 563 | 84 | 85.08% |
| `sema_simd.c` | 420 | 73 | 82.62% |
| `types.c` | 1472 | 65 | 95.58% |

## Functions with no coverage at all

- The checker, the 18 files `sema*.c` with 709 functions: one,
  `copy_failed` at `src/antic/sema_copies.c:92`.
- The parser, the 7 files `pars*.c` with 119 functions: none.
- Lowering, the 17 files `lower*.c` with 437 functions: none.

The other six of `src/antic/`: `field_owner` at `antl_tree.c:369`,
`find_crt_dir` at `driver_link.c:35`, `unexpected` at `llvm_emit.c:148`,
`lanes` at `memcheck.c:161`, and `posix_class` and `lookaround_name` at
`pattern.c:425` and `pattern.c:623`. The four of `src/anti/`:
`record_named` and `enum_named` at `bindapi.c:152` and `bindapi.c:158`,
`default_sysroot` at `main.c:221` and `http_absent` at `repo.c:192`.

`llvm-cov export` gave the functions, read by a script under
`build/cov-scratch/`. The counts of the files agree with
`build/coverage/summary.txt`.

## Where the task and the code differ

- The task asks for a summary of `src/rt/`. The flags it names reach
  antic, anti and the unit tests, so 24 C files of `src/rt/` are in no
  measured program: `backtrace`, `call`, `cast`, `check`, `errno`, `hooks`,
  `init`, `io`, `license`, `license_stub`, `lock`, `mem`, `object`,
  `patterns`, `platform_entry`, `platform_stdio`, `platform_windows`,
  `reflect`, `registry`, `snapshot`, `start`, `sync`, `threads` and
  `trace`. The runtime that an Anti program links is not instrumented, so
  the program tests add nothing to that row.
- The task has `./c coverage` set `LLVM_PROFILE_FILE`. ctest sets it, for
  every test of the tree, from a file of `TEST_INCLUDE_FILES` that the
  configure step writes. A part run by hand with `ctest --preset coverage
  -I` then collects as well.
- The task has the flags compile "antic, anti and the C unit tests". They
  stand on those targets and not in `CMAKE_C_FLAGS`, where the presets
  `asan` and `ubsan` keep theirs. The objects of `tests/abi/` take
  `CMAKE_C_FLAGS`, and an Anti program that links them has no profile
  runtime.
- llvm-cov names a file of the summary from `src/`, the directory all
  files share: `antic/lexer.c`, not `src/antic/lexer.c`.
- The task names `CLAUDE.md` as the document to change.
  `docs/decisions.md` holds the entry on `./c`, so it took the mode too.
- The gates name `tests/known-failures.txt`. It does not exist yet, so the
  suite expects 0 failures.

## Counts and checks

- Tests: 1703 at `6de755a7` and 1704 after, by `ctest --preset host -N`.
  The new one is `coverage_report`.
- Host suite: 1704 of 1704 passed, 0 expected failures, in
  `build/drive/logs/cov-suite-host.log`. Two skip, the
  `mimalloc_environment` tests. It took 349 s.
- ASan: 1703 of 1703 passed, in three parts,
  `build/drive/logs/cov-suite-asan-1.log` to `-3.log`.
- UBSan: 1703 of 1703 passed, in two parts,
  `build/drive/logs/cov-suite-ubsan-1.log` and `-2.log`.
- The coverage tree built with zero warnings, in
  `build/drive/logs/cov-part-1.log`. The other three trees compiled no C
  file again.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- Red before green: `coverage_report` failed without the script, in
  `build/drive/logs/cov-red.log`.
- In two parts the coverage suite took 160 s and 312 s, in
  `build/drive/logs/cov-part-1.log` and `cov-part-2.log`, and gave the
  same lines.
- The docs-style checker reports `0 error(s), 0 warning(s)` on
  `CLAUDE.md`, `docs/decisions.md` and this report.
- Defects found: none. No id from T001 on is taken.

## Not done

- Neither VM built the coverage tree. `coverage_report` stands on a Mac
  alone. The pinned clang carries the profile runtime of Windows for the
  msvc triple alone. A program of a Windows host compiles for the gnu
  triple, so the tree may not link there.
- An antic that crashes or is killed writes no profile, so a test of such
  an exit adds no line.
- The build runs antic on the standard library and leaves
  `build/coverage/default.profraw`, which no report reads.

## Rules the documents do not state

None. The step ran no Anti program of its own.

## Proof

`git log --oneline -3` after the push of the coverage build, before the
commit of this report:

```text
d6c44aa1 Add the coverage build and ./c coverage
32ba909c Set LLVM_PROFILE_FILE for the program alone in the profile tests
6de755a7 Report the pin of 23.1.1-anti.8
```

`git rev-parse HEAD origin/main` after that push:

```text
d6c44aa17ecf72ac78ce2c33eca22fee1b2a51cb
d6c44aa17ecf72ac78ce2c33eca22fee1b2a51cb
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
