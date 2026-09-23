# raylib and miniaudio in src/native

The step builds raylib and miniaudio in `src/native/` for all six targets. The first
attempt stopped on Linux, see `2026-09-23-native-raylib-miniaudio.md`. Eddie then
decided three things. The glibc 2.35 sysroot takes the X11 and OpenGL development
packages of jammy, pinned by digest. raylib builds against X11 and not Wayland.
miniaudio builds against the C library alone. The Linux libraries are tested with a C
program that the pinned clang links against the extended sysroot. Linking an Anti program
against glibc stays for another step.

## What was done

- `tools/sysroot-pins` pins 22 packages per processor as `MEDIA_` lines, with the digests
  of the Packages index of the jammy release pocket. `src/native/get-media-sysroot.cmake`
  unpacks them over `sysroot/linux-<cpu>-glibc` and installs glibc first when it is
  missing.
- `tools/miniaudio-pin` pins miniaudio 0.11.24, the version raylib 6.0 bundles, and
  `src/native/get-miniaudio.cmake` downloads it. raylib uses `tools/raylib-pin`.
- `src/native/media.cmake` holds the targets and the flags the two libraries share.
  `src/native/miniaudio.cmake` and `src/native/raylib.cmake` build `libminiaudio.a` and
  `libraylib.a`, or `miniaudio.lib` and `raylib.lib`, in `lib/<target>/` for all six
  targets on this Mac.
- The tests `raylib_link_<target>` and `miniaudio_link_<target>` link a C probe for every
  target. On Linux the link runs through `tests/run_glibc_link.cmake`, and elsewhere
  through antic. `raylib_run` and `miniaudio_run` run on the host. `media_sysroot` and
  `miniaudio_pin` check the pins and the script. The system libraries of each library on
  each target are in `docs/decisions-native.md`.

## What failed and how it was fixed

- On Windows the AVX2 constants of `stb_image_resize2.h` were truncated, a silent
  miscompile that `-Wconstant-conversion` reported. The MSVC CRT's `immintrin.h` shadowed
  clang's. The native Windows flags now search clang's headers first.
- The link on linux-x86_64 could not find `/lib64/ld-linux-x86-64.so.2`. libc6 names it
  by an absolute link, which leaves the sysroot. The script makes such links relative.
- raylib on Linux needs `libX11`, since `rcore.c` calls it for the clipboard.
- The macOS link lacked every framework. antic takes them from `--framework` and not from
  `link framework` in the source, so `tests/run_native_link.cmake` now takes options.
- ld64.lld 23.1.1 writes no `objc_msgSendClass$` stub, so `rglfw.c` compiles with
  `-fno-objc-msgsend-class-selector-stubs`.
- `stb_vorbis.c` raised `-Wtautological-compare` on an overflow check. It compiles with
  `-fwrapv-pointer`, which keeps the check.
- `fmt_canonical` refused the two new `.anti` files. `anti fmt` rewrote them.

## Gates

- The build has zero warnings on the host, ASan and UBSan trees.
- Host suite: 812 of 812 passed. ASan: 811 of 811. UBSan: 811 of 811. The sanitizer
  trees leave out `no_paths` as before.
- The docs-style checker reports nothing on every touched file. It reads
  `src/native/CMakeLists.txt` as Markdown and flags its comment lines as headings, which
  it did before this step. A copy named `.cmake` passes.
- Logs: `build/native-media-work/logs/` of the worktree, `build.log`, `ctest-host.log`,
  `ctest-asan.log` and `ctest-ubsan.log`. The driver's `build/drive/` stayed untouched.

## Provisional entries added

All in `docs/decisions-native.md`, "raylib and miniaudio": the miniaudio pin and its
version, the five GL packages, the script in `src/native/` and not in
`tools/get-sysroot.cmake`, relative links, `ANTIC_GLIBC_SYSROOT_DIR`, the Linux libraries
in `lib/linux-<cpu>/`, the pinned Apple SDK for macOS, the Windows header order, the
raylib warnings, `-fno-strict-aliasing` and `-fwrapv-pointer`, the Objective-C stub flag,
raylib's own copy of miniaudio, and where the tests are registered.

## Questions for Eddie

1. `raudio.c` compiles raylib's own copy of miniaudio with external linkage. A program
   that calls raylib's audio and `anti.miniaudio` at once defines every `ma_` function
   twice. Options: raylib without `raudio.c`, or `raudio.c` built with `-Dminiaudio_c`
   so that it calls the separate library, which the shared version makes possible. The
   third option is to leave it.
2. `LICENSES/` needs the raylib and miniaudio texts, as it needs PCRE2's. Which step adds
   them?
3. The absolute-link fix and the X11 packages could move into `glibc_sysroot` of
   `tools/get-sysroot.cmake` at the fold. Should they?
