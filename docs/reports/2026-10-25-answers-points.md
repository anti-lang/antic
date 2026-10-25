# Step points: Q3 to Q15 of the answers of 2026-10-24

The step builds the thirteen points Q3 to Q15 of "Answers of 2026-10-24"
in `docs/decisions.md`, one commit each, with a test where behaviour
changes. Logs stand in `build/drive/logs/answers-points/`.

## Points built

| Point | Commit | What changed |
|---|---|---|
| Q3 | `270cdc4e` | Done by the record: the entry on `anti.log` reads the file at the first line written, as the code does. No change. |
| Q4 | `902c5e95` | `set_name`, `set_level` and `set_format` of `Logger` take a lock that the two loggers hold while they make a line. `std_log_setters` sets the name while two workers log. |
| Q5 | `3e692771` | The code passed the search on already. `repositories_older_then_second` holds it. |
| Q6 | `f4870c22` | `antic_linux_code_options` gives the runtime, the native libraries of Linux and the glue of `anti.regex` both options. The new `native_frames` failed for PCRE2 and the rest on x86_64 before. |
| Q7 | `930cb449` | The work directories of `anti check`, `anti doc` and `anti test` stand in the build directory of `[layout]`. One case per command failed before. |
| Q8 | `b344ced4` | The `ubsan` presets take `-g` and `ASAN_OPTIONS=detect_leaks=1`. The new test `presets` reads them. |
| Q8 | `2540e7d8` | The line of a Windows library names the libraries alone. `unit_link` and `run_clib.cmake` hold it. |
| Q8 | `81cef395` | `deps_dir` reads every download path of the cache, the native sources among them. |
| Q10 | `ee0a994c` | The entry on `patch` keeps `patch_bytes` in `anti.regex`. A document alone. |
| Q11 | `1773f48e` | `presets` reads `ANTIC_FAULTS` of the five configure presets. |
| Q12 | `78d44db0` | The DESIGN comment of `tools/coverage.cmake` says why the pattern replaced `LLVM_PROFILE_FILE`. The entry on `./c coverage` still named the variable. |
| Q13 | `6639f33a` | A copy of a generic refuses `3 <<% -1` in a length as a constant is refused. The back end had refused the length 0 instead. Two refusals of the matrix hold it. |
| Q14 | `8ead80da` | `anti bind --clang` says that it cannot read the version of clang. |
| Q15 | `2384b6ce` | `file:///C:/repo` is refused on macOS and Linux as a path of another system, where it read `C:/repo` relative to the project. |
| Q9 | `596770d5`, `c7dc2a83` | `values_aggregates` is now `values_aggregates_arrays` and `values_aggregates_records_and_reads`. Under UBSan their dev builds took 3 s and 5 s. |
| Q9 | `10fdfa4f` | The headers move to `tests/clib/headers/`, the notice to `tests/programs/`, the code tests of a target to the new `tests/asm/` and `symbols_probe.c` to `tests/trace/`. |

`2e509a5e` records the counts of the suites in `CLAUDE.md`.

## Where the task met the documents

- Q8, "`ANTI_DEPS_DIR` holds the cache paths". If that meant removing
  `ANTIC_CLANG_DIR`, `ANTIC_LLVM_DIR` and `antic_shared_path`, it would
  re-open two settled entries. Eddie decided on 2026-09-27 that the VMs
  keep their setup, which names each directory. The entry on the
  sanitizer presets keeps `antic_shared_path`. The step kept both and
  pins that every download path defaults into `ANTI_DEPS_DIR`.
- Q9: the expected headers first went to `tests/clib/`. The C programs
  there include `"geo.h"`, so they would have compiled against the
  expected file in place of the one antic wrote. The headers stand in
  `tests/clib/headers/`.

## Findings

None of `docs/audit/tests-findings.md` closed, and none recorded. Q8 and
Q9 answer two minor findings of `docs/audit/build-and-tools.md` and
`docs/audit/tests.md`, the three ways to move the downloads and the
directories of more than one kind. `tests/known-failures.txt` holds 26
lines, seven entries, before and after.

## `[provisional]` entries

- The pages of `anti doc` without `-o` follow `[layout]` as its work
  directory does.
- The reading of "`ANTI_DEPS_DIR` holds the cache paths" above.
- The line of a Windows library names the libraries alone, in the order
  of the other targets.
- The one kind of `tests/dump/` is the dump, with `--print-targets`. The
  name `tests/asm/` and the places of the other moved files stand in the
  same entry.

## Not run on the target

- `std_log_setters` on Linux and Windows, where the Mutex is a futex word
  or an `SRWLOCK`.
- `native_frames` read the bytes of the Linux libraries on the Mac. A
  trace through a native library that calls back into Anti, on Linux.
- `anti_build_message_repository_of_a_drive_letter` on Windows, where
  `file:///C:/nowhere` stays a path, and on Linux, where it is refused.
- `clib_static` and `clib_bundle` on a Windows host, whose link line
  holds the libraries alone.
- The release script and `docs/vm-setup.md` keep their four directories
  by the reading of Q8. Nothing of them changed.

## Gates

- The three builds have no warning of our own code: `build16.log`,
  `asan-build.log` and `ubsan-build.log`. The warnings of mimalloc are
  those of its own flags.
- Host, 6744 tests, `ctest --preset host -j14` in three parts:
  `host-1.log`, `host-2.log` and `host-3b.log`. All pass, the five
  expected failures of the Mac among them, and the two
  `mimalloc_environment` tests skip. `host-3.log` stopped at
  `fmt_canonical` for the two split programs, which `c7dc2a83` formats.
- ASan, 6742 tests in six parts, `asan-1.log` to `asan-6.log`: all pass
  with the same skips.
- UBSan, 6742 tests in the same six parts, `ubsan-1.log` to
  `ubsan-6.log`: all pass with the same skips, leak detection on.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reported nothing on every `.md` file of the
  step, `docs-*.log`.

## State

```text
$ git log --oneline -3
2e509a5e Count the suite of the step points of the answers of 2026-10-24
c7dc2a83 Write the two programs of the aggregates in the canonical form
10fdfa4f Hold one kind of test in tests/dump and tests/unit
$ git status --short
$ git rev-parse HEAD origin/main
2e509a5e58dedba1beab09611481c36101f3c66d
2e509a5e58dedba1beab09611481c36101f3c66d
```

The commit of this report follows, then its push. The reply of the
session holds the state after it.
