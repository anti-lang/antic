# Decisions of the native libraries

The decisions of the sessions that build `src/native/`. A later step folds them into
`docs/decisions.md`. An entry marked `[provisional]` was taken where the documents are
silent and is binding until Eddie reviews it.

## PCRE2

- PCRE2 10.48 is pinned in `tools/pcre2-pin` by version and by the SHA-256 of
  `pcre2-10.48.tar.gz`, the digest that the release page of `PCRE2Project/pcre2`
  publishes. `src/native/get-pcre2.cmake` downloads the archive over HTTPS into
  `build/deps/pcre2/`, checks the digest and unpacks it there. An archive already on
  disk with the pinned digest is not downloaded again, so a configure without a network
  works once the source is there. The test `pcre2_pin` checks the pin and refuses a copy
  of the version or the digest in the script or the recipe.
- The library is the 8-bit build with UTF support and JIT off, as "Runtime archive" in
  `docs/decisions.md` settles. `src/native/pcre2.cmake` compiles the 31 sources of the
  list in `NON-AUTOTOOLS-BUILD` of the release with the pinned clang, and llvm-ar writes
  `lib/<target>/libpcre2-8.a`, or `lib/<target>/pcre2-8.lib` on Windows, in the runtime
  tree. Every target with a sysroot is built, at the default level of the target.
- [provisional] `src/native/` is a subdirectory of the build of antic, which the
  top-level `CMakeLists.txt` adds after `tests/`, and no project of its own run once per
  target. Reason: one configure then builds all six targets with the sysroots and the
  flags that the cross builds of anti_rt use, and the tests link through the antic of
  the same tree. The CMake block quoted under "Libraries of later chapters" in
  `docs/site/runtime-archive/index.md` shows the former form and changes when this entry
  is folded.
- [provisional] The triple and the flags of each target stand in `antic_native_target`
  of `src/native/CMakeLists.txt`, a second copy of the arguments that the top-level file
  passes to `anti_cross_runtime`. Reason: the step could change no other line of the
  top-level file. The fold makes both builds read one function.
- [provisional] The download script is `src/native/get-pcre2.cmake`, beside its recipe,
  and not in `tools/` beside `tools/get-raylib.cmake`. Reason: `tools/` holds the pin,
  and the script belongs to the one recipe that runs it.
- [provisional] The library keeps the name PCRE2 gives its 8-bit library, `pcre2-8`, as
  `libpcre2-8.a` and `pcre2-8.lib`, the forms of `libanti_rt.a` and `anti_rt.lib`.
  Reason: a C project that links the published file finds it under its usual name.
- [provisional] The configuration is `config.h.generic` of the release with four
  definitions: `HAVE_CONFIG_H`, `PCRE2_CODE_UNIT_WIDTH=8`, `SUPPORT_UNICODE` and
  `PCRE2_STATIC`. `SUPPORT_JIT` is never defined. The limits of matching, nesting and
  names stay the defaults of the release. Reason: the smallest configuration that gives
  the decided build.
- [provisional] PCRE2 compiles with the warnings of anti_rt, `-Wall -Wextra -Wpedantic
  -Werror`, and `-Wno-overlength-strings`. Reason: the table of messages in
  `pcre2_error.c` is one literal of 5,686 bytes, above the 4,095 that C11 guarantees, and
  clang, which compiles every target, takes it. No other warning is raised on any of the
  six targets.
- [provisional] `pcre2.h` stays in `build/<preset>/native/pcre2/include/` and is not yet
  part of the runtime archive. Reason: nothing in the archive reads it before
  `anti.regex` binds it, and the directory of headers in the archive is not decided.
- [provisional] The build copies `LICENCE.md` of the release to `licenses/pcre2.txt` of
  the runtime tree, the directory of one licence file per component that "Licences that
  travel" describes.
- PCRE2's licence text must be added to `LICENSES/` of the repository, which the fence of
  this step did not include. The text is `LICENCE.md` of the pinned release, whose SPDX
  identifier is `BSD-3-Clause WITH PCRE2-exception`.
- [provisional] The tests of the native libraries are registered in
  `src/native/CMakeLists.txt` through `src/native/pcre2.cmake`, and their scripts and
  sources lie in `tests/`: `tests/run_native_link.cmake`, `tests/run_pcre2_pin.cmake`
  and `tests/abi/pcre2_probe.c` with `tests/abi/pcre2_match.anti`. Reason: the tests
  exist only where the libraries are built, and `tests/CMakeLists.txt` stayed outside
  the fence.
- `pcre2_link_<target>` links `pcre2_match.anti` with the C probe and the library for
  each of the six targets through antic and lld, as a program of that target links. A
  missing symbol fails the link. `pcre2_match` runs the program of the host. It checks
  that the library reports UTF support and no JIT, that a group starts and ends at the
  offsets PCRE2 names, that `.` takes the two bytes of `ß` as one character, that a
  caseless `ö` matches `Ö`, and that `a(b` fails to compile at offset 3.

## Lines of other documents that change at the fold

- `docs/decisions.md`, "CPU levels": "Nothing in `src/native/` builds yet, so the
  refusal has no site until the first library arrives." PCRE2 now builds. The refusal
  still has no site, since no module imports a native library.
- `docs/decisions.md`, the open item on where the native libraries are published: "which
  nothing builds yet" no longer holds.
- `CLAUDE.md`, item 16 of "First sessions": the native libraries in `src/native/`, which
  nothing builds yet. PCRE2 is the first one built.
