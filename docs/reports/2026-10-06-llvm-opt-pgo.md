# Step pgo of the optimization facts

The step `pgo` of `docs/work-order-llvm-optimization.md`, choice D7 under condition C3.
The first run stopped because `23.1.1-anti.5` had no `llvm-profdata` and no profile
runtime, as `docs/reports/2026-10-05-llvm-opt-pgo.md` says. `23.1.1-anti.6` holds both.

## What the step built

- `tools/llvm-pin` and `tools/clang-pin` name `23.1.1-anti.6`. `SHA256SUMS` and
  `SHA256SUMS.sig` of the release were downloaded first. The signature verifies against
  `tools/keys/release.pem` with `openssl pkeyutl -verify`, as
  `tools/fetch-release.cmake` does. Each of the 12 lines equals the table of
  `../llvm-tools/docs/reports/2026-10-05-llvm-tools-profile.md`, and each digest line
  of the two pins equals its line.
- `llvm-profdata` joins `ANTIC_LLVM_TOOLS` of `tools/pinned-tools.cmake`, the version
  check and the copy of `tools/check-llvm.cmake`, and the header of
  `tools/get-llvm.cmake`. `pinned_tools` failed on the anti.5 tree with
  `.../bin/llvm-profdata is missing` (`build/drive/logs/pgo-pinned-red.log`).
- `CMakeLists.txt` copies the profile runtime of each target from `lib/clang/23/lib/`
  of the clang archive into `lib/<target>/` of the runtime archive, beside the runtime
  of AddressSanitizer. The eight directories hold it, glibc and musl apart.
- `build/deps/llvm` and `build/deps/clang` were removed. `cmake --preset host`
  downloaded both anti.6 archives and verified each against the pin, `SHA256SUMS` and
  the signature (`pgo-configure-host.log`). The configure on anti-linux did the same
  (`pgo-linux-build.log`).
- `antic --profile-generate` runs `opt` with `-pgo-kind=pgo-instr-gen-pipeline
  -profile-file=default_%m.profraw`. `--profile-use <file>` runs it with
  `-pgo-kind=pgo-instr-use-pipeline -profile-file=<file>`. The link of an
  instrumented program takes the profile runtime after `libanti_rt`. An ELF link adds
  `-u __llvm_profile_runtime`, and a Windows link adds `/NODEFAULTLIB:libcmt.lib`.
  `anti build` and `anti run` pass both options to the release call of a program.
- `docs/decisions.md` line 21, `docs/distribution.md`, `docs/tooling-addendum.md`,
  `docs/notes/hosts-and-harness.md` and `docs/toolchain-later.md` name the eight
  tools, the profile runtime and the new tag.

## Tests

- `unit_link`, `profile_links`: the link line of macos-arm64, linux-arm64 of musl,
  linux-x86_64 of glibc and windows-arm64. It failed to build before the field existed
  (`pgo-unit-red.log`).
- `profile_guided`, `tests/run_profile.cmake`: the refusals, then `mixed_work` built
  with `--profile-generate`, run under `LLVM_PROFILE_FILE` and without it, merged by
  `llvm-profdata` and built with `--profile-use`. That build prints nothing, so opt
  matched every function, and `mixed_work.main` carries `function_entry_count` 1. Both
  programs print `mixed_work.expected`. It failed first (`pgo-profile-red.log`).
- `anti_build` ends with the same cycle through `anti build`, and with the refusals of
  `anti` (`pgo-anti-red.log` before the change).
- Cross links by hand on the Mac of an instrumented `map_work` for linux-arm64 and
  windows-x86_64 succeeded. No Windows host ran an instrumented program.

`emit_identity` and `link_identity_macos-arm64` pass unchanged, so nothing is re-pinned.
The default text does not change.

| Host | Suite | Passed | Failed | Logs |
|---|---|---|---|---|
| Mac | host | 1620 of 1620 | 0 | `pgo-ctest-host.log` |
| Mac | ASan | 1619 of 1619 | 0 | `pgo-ctest-asan.log`, `pgo-ctest-asan-emit.log` |
| Mac | UBSan | 1619 of 1619 | 0 | `pgo-ctest-ubsan-1.log`, `-2.log`, `-emit.log` |
| anti-linux | host | 1324 of 1328, 2 skipped | 2 | `pgo-linux-suite-1.log`, `-2.log` |

The ASan suite ran past the limit of one call while another session ran suites on the
Mac. It was stopped with 1618 passed and none failed. `emit_identity`, the one test
left, then passed alone through `ctest --preset asan`. UBSan ran in two parts and
`emit_identity` alone, 1619 distinct tests in all. On anti-linux `profile_guided`
passes (`pgo-linux-step-tests.log`), with `release_dry_run` and `macos_sdk` skipped as
always. `program_mixed_work_lto_full` and `_thin` fail there: lld warns that the
runtime as bitcode is `aarch64-unknown-linux-musl` and the program
`aarch64-unknown-linux-gnu`. They fail the same way at `e76f6abf` with anti.5
(`pgo-linux-base-lto.log`), so the step `runtime-lto` left them, and this step does not
touch them. The Linux build printed 67 warnings, all in raylib's own sources.

## Measurement

`tests/bench/ablate/run.py <program> -- --profile-use=<program>.profdata` on
macos-arm64, 15 runs each. Each profile comes from one run of the program built with
`--profile-generate` (`build/pgo/bench/profiles.sh`). The tables are
`pgo-bench-<program>.md`.

| Program | Default | With its profile | To default | Object, default | Object, profile |
|---|---:|---:|---:|---:|---:|
| scalar_loop | 294.3 ms | 294.9 ms | 1.00 | 1,896 B | 2,024 B |
| objects | 147.2 ms | 73.2 ms | 0.50 | 13,448 B | 14,000 B |
| builder | 275.1 ms | 244.1 ms | 0.89 | 16,440 B | 16,704 B |
| simd_loop | 135.5 ms | 134.7 ms | 0.99 | 2,016 B | 2,144 B |
| map_work | 167.4 ms | 169.2 ms | 1.01 | 110,240 B | 106,944 B |
| mixed_work | 177.0 ms | 178.9 ms | 1.01 | 695,344 B | 694,256 B |

The C twins ran at 293.9, 108.0, 52.2, 133.8 and 153.3 ms. With its profile `objects`
runs in 0.68 of its twin. The release compile changed by 7 percent or less on each program,
2513 ms to 2612 ms on `mixed_work`.

## Decisions

The entry of the options in `docs/decisions.md`, and these `[provisional]` entries:

- An instrumented program writes `default_%m.profraw` in its working directory, and
  `LLVM_PROFILE_FILE` names another file.
- Both options are refused with `--dev` and `--lto`, and together.
  `--profile-generate` is refused with `-S`, `-c`, `--lib` and `--linker platform`.
  `anti` refuses either without `--release` or with `--lib`.
- `--profile-use` reads the indexed profile alone and names the merge for a raw one.
- One profile runtime per target in `lib/<target>/`, for every level.
- A Windows link of `--profile-generate` passes `/NODEFAULTLIB:libcmt.lib`.

## State

After the push, before this report's commit:

```text
$ git log --oneline -3
7c074297 Add --profile-generate and --profile-use to antic and anti build
d0f15c39 Pin 23.1.1-anti.6 of the LLVM tools and clang
e76f6abf Report step measure of the optimization facts
$ git status --short
$ git rev-parse HEAD origin/main
7c074297eb3c13e58457db5963b94b9dec44c54e
7c074297eb3c13e58457db5963b94b9dec44c54e
```

Every log stands in `build/drive/logs/`.
