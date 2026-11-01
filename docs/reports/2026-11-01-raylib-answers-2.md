# The two answers on the program name and the native flags

The session built the two answers that Eddie gave to the questions of
`docs/reports/2026-11-01-raylib-answers.md`. They stand as Q6 and Q7 of
"Answers of 2026-11-01" in `docs/decisions.md`.

## What changed

- `a79059ef` records Q6 and Q7 and names the field and the flag they let
  in, in the entry on the feature freeze.
- `95fb6002` builds Q6. A program takes `program` of `[package]`, else
  the last segment of `name`, else the name of the directory that holds
  `anti.toml`. `name_program` of `src/anti/build.c` holds the rule.
  Every field of `[package]` but `name` stands without `name`, and only
  `[dependencies]` needs it. The refusal of a `[package]` without `name`
  is gone with its test. `docs/tooling.md`, the overview and the page of
  the build tool name the field and the rule.
- `aa2fe952` builds Q7. `ANTIC_NATIVE_FP_OPTIONS` of
  `src/native/CMakeLists.txt` holds `-ffp-contract=off`, and
  `antic_native_target` and `antic_media_target` add it to the flags of a
  target. Every recipe records the command of each compile, which the new
  test reads. The runtime archive was built again in `host`, `asan` and
  `ubsan`.

## Tests

- `anti_build_no_package_program` now expects `internal`, the name of
  its directory, where it expected `app`.
- `anti_build_no_package_named` builds `tree` from `program = "tree"`.
- `anti_build_no_package_version` reads `1.2.3` in the notice of the
  program, which antic writes from `--package-version`.
- `anti_build_no_package_license` builds from `license` and
  `license_text` alone.
- `anti_build_no_package_directory_space` and
  `anti_build_no_package_directory_device` hold the refusal of the
  directories `my tree` and `con`. The second does not run on Windows,
  which makes no such directory.
- `anti_build_message_manifest_program_blank`,
  `anti_build_message_manifest_program_path` and
  `anti_build_message_program_library` hold the refusals of `program`.
- `unit_deps` refuses dependencies without `name` where it refused a
  `[package]` of `version` alone.
- `native_fp_contract` refuses a compile of a native library without
  `-ffp-contract=off`. It read 894 compiles, of six libraries on six
  targets, and failed for every one of them before the flag.
- `std_raylib_math` expects `6.10449028` and `0.820994496`, the values of
  `tests/abi/abi_raymath.expected`, and failed before the flag.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and
`-I 4301,6900` with `-j14`: 424 s, 267 s and 266 s before the second
code commit, 381 s, 269 s and 255 s before the third. The first run
failed `unit_deps` and `host_names`, both tests of the change, and the
commit holds them fixed. The second run failed `source_tree` until the
new runner was added to git. 6803 tests, with the two skips of
`mimalloc_environment`. Each sanitizer suite ran its 6801 tests in the six
parts of `asan`: `asan` 249 s, 194 s, 261 s, 196 s, 438 s and 409 s,
`ubsan` 252 s, 222 s, 259 s, 205 s, 458 s and 424 s. Every test passed.
The VMs are down, so no suite ran on Linux or Windows.

## The acceptance check

pythagoras-tree at `204489d`, copied with `git archive` into directories
`pythagoras-tree` under `build/acceptance/`. With an empty `anti.toml`,
`anti build` wrote `dist/macos-arm64/dev/pythagoras-tree` and
`dist/macos-arm64/release/pythagoras-tree`. With `program = "tree"` and
`version = "1.0.0"` it wrote `tree` in both modes, and the notice names
`tree.main 1.0.0`. `build.sh` with `ANTI_TREE` at this tree passed the 18
tests of `tree.geometry`, and `build/tree` links Cocoa, CoreVideo, IOKit
and OpenGL. The repository of pythagoras-tree is unchanged.

## Provisional entries

- A project whose manifest names no package, with or without
  `[package]`, is the package of its root module, which shares the
  `internal` items of its modules. The entry existed and now covers a
  `[package]` without `name`.
- `antic` and `attribution` stand without `name` as well.
- A directory name or a `program` is a name of the portable file name
  set of POSIX, not first `.` or `-`, not last `.`, and no device name of
  Windows before its first `.`. Any other is refused. On POSIX the
  directory is the one `realpath` gives.
- `program` in a library project is refused, and a library of `--lib`
  keeps its name.

## Questions for Eddie

1. The runtime, `src/rt/` and the glue of `anti.regex` compile with the
   default contraction of clang. The glue takes `-ffp-contract=off` now,
   through the flags it shares with PCRE2, and anti_rt does not. Should
   the runtime take it too?
2. mimalloc is built in `src/native/` and takes the flag, as "every
   native library" reads. The copy of PCRE2 that antic links to check
   patterns builds with the flags of antic and does not. Is that the
   intended scope?
3. `name` is required for a published package, and `anti publish` is not
   built. Should a `[package]` of `publish = "<target>"` without `name`
   be refused now?
