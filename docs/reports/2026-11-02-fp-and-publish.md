# The answers of 2026-11-02 on the runtime flag and publish

The session built the three answers that Eddie gave to the questions of
`docs/reports/2026-11-01-raylib-answers-2.md`. They stand as "Answers of
2026-11-02" in `docs/decisions.md`, and the entry on the feature freeze
names the two changes they let in.

## What changed

- `798e5889` records Q1 to Q3 and the line of the feature freeze.
- `8d4084fb` builds Q1. `ANTIC_FP_OPTIONS` of the top-level
  `CMakeLists.txt` defines `-ffp-contract=off` once, in place of
  `ANTIC_NATIVE_FP_OPTIONS` of `src/native/CMakeLists.txt`. The cross
  build of anti_rt takes it at every level of every target, the glibc
  mode included, in its objects, its licence stub and its bitcode of both
  LTO modes, and so does the runtime of the fault tests. The native
  libraries and the reference C of the tests take it from the same
  variable. The step that writes the bitcode of a Windows target again
  for the msvc triple reads IR and compiles no C, so it takes no flag.
  The runtime archive was built again in `host`, `asan` and `ubsan`.
- Q2 changes no code. The entry of the native flag under "Libraries and
  runtime" names the scope that stays. mimalloc keeps the flag, and the
  copy of PCRE2 inside antic does not take it.
- `60f117d0` builds Q3. `manifest_read` of `src/anti/manifest.c` refuses
  a `[package]` that names `publish` without `name`, in the manifest of a
  project and in that of a path dependency. `docs/tooling.md` and the
  entries of "The build command" name the refusal.

## Tests

- `runtime_fp_contract` reads the 2825 recorded compiles of the runtime:
  anti_rt, its stub, its bitcode of `full` and `thin`, the glue of
  `anti.regex` and both runtimes of the fault tests. It refuses a compile
  without the flag, and a target or level of the eight runtime targets
  without a compile. Before the flag it failed for the compiles of
  anti_rt, its stub and both bitcodes at all 24 levels of the eight
  targets, and for the runtime of the fault tests. The glue already took
  the flag through the native flags.
- `native_fp_contract` shares the runner, renamed from
  `run_native_fp_contract.cmake` to `run_fp_contract.cmake`, which now
  also takes `LEVELS`.
- `names_once` refuses `-ffp-contract=` outside a comment in any CMake
  file of the build but the top-level `CMakeLists.txt`. The runners of
  `tests/runners/` are left out, since they spell what they check. It
  failed for the two spellings of `src/native/CMakeLists.txt` and
  `tests/CMakeLists.txt` before the change.
- `anti_build_message_manifest_publish_without_name` holds the refusal
  of `[package]` with `publish = "ff"` alone. Before the change the build
  went on and stopped at the runtime archive.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and
`-I 4301,6900` with `-j14`: 452 s, 278 s and 328 s before `8d4084fb`,
484 s, 333 s and 328 s before `60f117d0`. Every test passed, 6805 in all
with the two skips of `mimalloc_environment`. Each sanitizer suite ran
its 6803 tests in the six parts of `asan`: `asan` 317 s, 242 s, 251 s,
200 s, 445 s and 443 s, `ubsan` 253 s, 192 s, 247 s, 201 s, 463 s and
455 s. Every test passed. The VMs are down, so no suite ran on Linux or
Windows.

## Provisional entries

None.

## Questions for Eddie

1. The refusal reads the key `publish` of `[package]` as a string, the
   form of `docs/distribution.md`. A `publish` written as an array or a
   table without `name` is not refused, and nothing reads such a value.
   Should every form be refused, or a `publish` that is no string?
