# The answers of 2026-11-01

The session of 2026-11-01 built the five answers that Eddie gave to the
questions of `docs/reports/2026-10-28-raylib.md`. "Answers of 2026-11-01"
in `docs/decisions.md` records them.

## What changed

- `ea67a055` records the answers and names them in the entry on the
  feature freeze.
- `dfb31de4` builds Q1. The table of the system libraries of raylib and
  the probes of the native tests name `GL`, `m`, `pthread`, `dl`, `rt`
  and `X11` on Linux. The entry on the link lines of the binding is no
  longer provisional. `raylib_module_link_<target>` reads `libGL.so.1`.
- `72a39afd` builds Q5. `anti bind --clang` without an archive says
  "found no runtime archive in <dir above anti>, above the directory of
  anti, or in <data directory>, the data directory of the user.
  `--runtime <dir>` names one." `user_runtime_places` gives both places
  to the search and the message.
- `9de4f10f` builds Q3. An empty `anti.toml` is a manifest. A project
  without `[package]` is the package of its root module. A manifest with
  `[dependencies]` and no `[package]` is refused.
- `c4ef58f9` builds Q2. The notice of a binary and `NOTICE.txt` name
  PCRE2, raylib and miniaudio where the link takes them. Each takes its
  text and its version from `licenses/` of the archive. The link reports
  them through `links_natives`, as it reports musl.
- `e76c19ae` lets `anti bind --clang` take more than one header into one
  module, which Q4 needs. The headers before the last go to clang with
  `-include`.
- `09e32f31` builds Q4. `anti.raylib` binds `raylib.h`, `rlgl.h`,
  `raymath.h` and `rcamera.h` in the order of `rcore.c`, under raylib's
  configuration and `RAYMATH_IMPLEMENTATION`. Its 921 functions all
  resolve through the export of `libraylib.a`, and no shim is written.

## Tests

- `anti_bind_message_clang_no_runtime`, rewritten for the new message.
- `anti_build_message_manifest_dependencies_without_package`,
  `anti_build_no_package_program` and `anti_build_no_package_library`.
  `anti_build_raylib` builds from an empty manifest.
- `license_notice_native`: a program of `anti.regex`, one of
  `anti.raylib` and one of neither.
- `anti_bind_headers`: two headers of `tests/bind/` into one module.
- `std_raylib_math`, `std_raylib_math_dev` and
  `std_raylib_math_memory_checks`, `raylib_math_link_<target>` for the
  six targets, and `raylib_module_symbols`. `anti_bind_raylib` and
  `anti_bind_raylib_shipped` bind the four headers as the build does.

Two results of `std_raylib_math` differ from
`tests/abi/abi_raymath.expected` in the last place. `libraylib.a`
compiles with the default of clang, which fuses a multiply and an add.
A C build of raymath with `-ffp-contract=on` gives the same values.

## Suites

The host suite ran whole once per code commit, as `ctest -j14`: 925 s,
935 s, 930 s, 911 s and 872 s, the last with 6795 tests and the two skips
of `mimalloc_environment`. Each sanitizer suite ran its 6793 tests in
the six parts of `asan`: `asan` 248 s, 194 s, 251 s, 187 s, 451 s and
419 s, `ubsan` 276 s, 213 s, 260 s, 218 s, 494 s and 486 s. Every test
passed. The Windows form of the case of Q5 was not run, since the VMs
are down.

## The acceptance check

A copy of `pythagoras-tree` at `204489d` in
`build/s-answers/pythagoras`, made with `git archive`, ran its
`build.sh` with `ANTI_TREE` at this tree. The 18 tests of
`tree.geometry` pass, and `build/tree` links and loads Cocoa,
CoreVideo, IOKit and OpenGL. A second copy with an empty `anti.toml`
builds with `anti build` in dev and release mode and needs no change of
its sources. Its program is `dist/macos-arm64/<mode>/main`, named after
`tree.main`, and its notice names raylib and miniaudio.

## Provisional entries

- `anti bind --clang` takes more than one header, under "The bind
  command".
- The order and the definitions of the binding of `anti.raylib`, under
  "raylib and miniaudio".
- A project without `[package]` is the package of its root module, and a
  `[package]` that names another field still needs `name`, under "The
  build command".
- The package, the version and the order of a native library in a
  notice, beside the entries on `anti license`.
- The notice of a binary names the same libraries as `NOTICE.txt`, in
  the same place.

## Questions for Eddie

1. A program without `[package]` takes the last segment of the module
   that holds `main`, as antic does without `-o`. The tree's program is
   then `main`. Keep that, or take the name of the project directory?
2. raylib compiles with clang's default contraction, and antic never
   fuses. Should the native libraries take `-ffp-contract=off`?
