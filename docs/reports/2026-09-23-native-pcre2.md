# PCRE2 in src/native

Step 16 starts with its first library. PCRE2 10.48 now builds for all six targets from
the pinned source with the pinned clang. The build is the 8-bit library with UTF
support and JIT off. The decisions are in `docs/decisions-native.md`.

## Done

- `tools/pcre2-pin` pins PCRE2 10.48 and the SHA-256 of `pcre2-10.48.tar.gz`, taken from
  the release page. The archive downloaded here has the same digest.
- `src/native/get-pcre2.cmake` downloads the archive into `build/deps/pcre2/`, checks the
  digest and unpacks it.
- `src/native/pcre2.cmake` compiles the 31 sources and writes `lib/<target>/libpcre2-8.a`,
  or `pcre2-8.lib` on Windows, in the runtime tree. It also writes `licenses/pcre2.txt`.
- `src/native/CMakeLists.txt` is now a subdirectory of the build of antic, and the
  top-level `CMakeLists.txt` adds it with one line.
- Tests: `pcre2_pin`, `pcre2_link_<target>` for all six targets, and `pcre2_match`,
  which runs on the host. `tests/abi/pcre2_probe.c` does the compile and the match, and
  `tests/abi/pcre2_match.anti` prints the results. I checked that the link test can
  fail: without the library, ld.lld and lld-link both stop on
  `undefined symbol: pcre2_compile_8`.
- The toolchain, the sysroots and raylib come from the main checkout's `build/deps/`,
  passed as `-DANTIC_LLVM_DIR`, `-DANTIC_CLANG_DIR`, `-DANTIC_SYSROOT_DIR` and
  `-DANTIC_RAYLIB_DIR`. Only the PCRE2 source was downloaded into this worktree.

## Failures and fixes

- The first host suite had 6 failures. The details of four of them are in
  `build/drive/logs/test-fail.log`:
  - `raw_output`: every C test file must include `../binary_stdio.h`. The probe now does.
  - `fmt_canonical`: `anti fmt` rewrapped the doc comment of `pcre2_match.anti`.
  - `anti_bind_*`, four tests: anti bind looks for the pinned clang only in
    `build/deps/clang` of its own checkout. That directory is now a symbolic link to the
    main checkout's clang. It sits under `build/` and is not committed.
- `pcre2_error.c` raised `-Woverlength-strings` under `-Wpedantic`. The recipe turns off
  that one warning, and the reason is recorded in the recipe and in the decisions file.

## Gates

- Build: zero warnings from the new targets. `build/drive/logs/build-host3.log`,
  `build-asan.log` and `build-ubsan.log`. The `ld: warning ... libLTO.dylib` lines come
  from the host executables that already existed, and the main checkout's logs have them
  too.
- Host suite: 796 of 796 passed, none skipped, in `build/drive/logs/test-host.log`.
- ASan: 795 of 795 and UBSan: 795 of 795, in `build/drive/logs/test-asan.log` and
  `test-ubsan.log`.
- The docs-style checker reports nothing on `docs/decisions-native.md`, this report or
  `tests/abi/pcre2_probe.c`. It skips `.cmake` files. It reads `CMakeLists.txt` as
  Markdown and flags comment lines as headings and code lines as long sentences, in the
  top-level file as in this one. I fixed the one real finding, a banned phrase.
- The syntax overview and both specifications need no change: `anti.regex` is still not
  built, and they state that.

## Provisional entries added

All of them are in `docs/decisions-native.md`:

- `src/native/` is a subdirectory of the main build.
- The per-target flags are a second copy of those in `anti_cross_runtime`.
- The download script lives in `src/native/`.
- The library is named `pcre2-8`.
- The configuration is the generic `config.h` plus four definitions.
- `-Wno-overlength-strings` is on.
- `pcre2.h` stays in the build tree.
- The licence goes to `licenses/pcre2.txt` of the runtime tree.
- The native tests are registered in `src/native/`.

## Outside the fence

- PCRE2's licence text still has to be added to `LICENSES/`.
- `docs/site/runtime-archive/index.md` quotes the old standalone form of
  `src/native/CMakeLists.txt`.
- Lines of `docs/decisions.md` and item 16 of `CLAUDE.md` still say that nothing in
  `src/native/` builds. `docs/decisions-native.md` lists them for the fold.
