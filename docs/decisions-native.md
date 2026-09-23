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

## raylib and miniaudio

- raylib is the release that `tools/raylib-pin` names, 6.0, the pin of the bindings.
  `ANTIC_RAYLIB_DIR` names the extracted release, and `src/native/raylib.cmake` builds it.
- [provisional] miniaudio 0.11.24 is pinned in `tools/miniaudio-pin` by version and by
  the SHA-256 of the source archive of the tag. GitHub publishes no digest for a source
  archive, so the digest is the one of the archive at its first download, as for raylib.
  `src/native/get-miniaudio.cmake` downloads it into `build/deps/miniaudio/` and checks
  the digest. Reason: 0.11.24 is the version raylib 6.0 bundles in
  `src/external/miniaudio.h`, so a program carries one version of miniaudio. The test
  `miniaudio_pin` checks the pin, refuses a copy of it in the script or the recipe, and
  compares the version with the one raylib bundles.
- The glibc 2.35 sysroot is extended with the X11 and OpenGL development packages of the
  original jammy release, for amd64 and arm64. They are pinned by digest in
  `tools/sysroot-pins` as libc6-dev is. Eddie decided this. The packages are those of
  X11, Xrandr, Xinerama, Xcursor, Xi, Xext, Xrender, Xfixes, xorgproto and the GL headers
  and libraries. Each development package comes with its library, 22 packages per
  processor. Each digest is the
  SHA-256 that the Packages index of the jammy release pocket lists. The keys start with
  `MEDIA_`, and `MEDIA_PACKAGES` lists them.
- [provisional] The GL packages are libgl-dev, libgl1, libglx-dev, libglx0 and libglvnd0.
  Reason: the smallest set that holds `GL/gl.h`, `GL/glext.h`, `KHR/khrplatform.h`,
  `GL/glx.h` and `libGL.so` with every library that `libGL.so.1` names.
- [provisional] `src/native/get-media-sysroot.cmake` unpacks the packages over
  `sysroot/linux-<cpu>-glibc`, and `tools/get-sysroot.cmake` stays as it was. It
  installs the glibc sysroot first when the tree holds none. A stamp in the tree names
  the digests it was unpacked from. Reason: the script belongs to the recipes that read
  the packages, and `tools/get-sysroot.cmake` and its test `glibc_sysroot` lay outside
  the fence. The fold may move the packages into `glibc_sysroot` of that script.
- [provisional] Every absolute symbolic link of the extended sysroot becomes the relative
  link to the same file inside the tree. Reason: libc6 names the loader
  `/lib64/ld-linux-x86-64.so.2` by an absolute link, which lies outside the sysroot on
  the host, and lld then refuses the `libc.so` script that names it. The same fix
  belongs in `glibc_sysroot` of `tools/get-sysroot.cmake` at the fold.
- [provisional] `ANTIC_GLIBC_SYSROOT_DIR` names the directory of the glibc sysroots, by
  default `build/deps/sysroot` of the checkout. Reason: `ANTIC_SYSROOT_DIR` of a
  worktree is the one of the main checkout, which this lane does not write.
- raylib builds its desktop back end over GLFW, with X11 on Linux and no Wayland. Eddie
  decided this. The configuration is `-DPLATFORM_DESKTOP_GLFW
  -DGRAPHICS_API_OPENGL_33`, the default of raylib's own Makefile, with raylib's
  `config.h` unchanged, `-D_GLFW_X11` on Linux, `-DGL_SILENCE_DEPRECATION` on macOS and
  `-D_CRT_SECURE_NO_WARNINGS -DUNICODE` on Windows, all with `-std=c99 -O2` at the default
  level of the target.
- miniaudio compiles against the C library alone. Eddie decided this. It is
  `miniaudio.c` of the release with no definition of ours: every back end stays in, and
  miniaudio loads the one it uses at run time.
- [provisional] Both Linux libraries compile for `x86_64-linux-gnu` and
  `aarch64-linux-gnu` against the glibc sysroot, and they lie in `lib/linux-<cpu>/`
  beside the level directories of the musl anti_rt, as the other libraries of that
  target. Reason: "Runtime archive" in `docs/decisions.md` links a program that imports
  either of them against glibc.
- [provisional] Both macOS libraries compile against the Apple SDK that
  `tools/macos-sdk-pin` names, through `tools/macos-sdk.cmake`. A host without that SDK
  builds neither for macOS and says so at configure. Reason: the zig stubs of the macOS
  sysroot carry no framework headers, and raylib's Cocoa back end and miniaudio's Core
  Audio back end include them.
- [provisional] The Windows flags of every native library put clang's own headers first.
  The headers of the MSVC CRT follow, the order clang-cl searches. Reason: the CRT holds its own
  `immintrin.h`, whose `__m256i` is the union of MSVC. The AVX2 constants of
  `stb_image_resize2.h` in raylib were then initialised byte by byte from 64-bit values,
  which clang truncated. PCRE2 builds and passes its tests under the new order.
- [provisional] raylib compiles with `-Wall -Werror`, `-Wno-missing-braces` and
  `-Wno-unused-function`. Reason: raylib's Makefile turns off the first, and `rtextures.c`
  and `rtext.c` silence the second with a pragma for `__GNUC__`, which clang for MSVC does
  not define. `-Wextra` and `-Wpedantic` raise hundreds of findings in the bundled stb
  and GLFW sources. miniaudio takes the warnings of anti_rt, `-Wall -Wextra -Wpedantic
  -Werror`, which raise nothing on any target.
- [provisional] raylib compiles with `-fno-strict-aliasing`, as its Makefile does, and
  `-fwrapv-pointer`. Reason: `stb_vorbis.c` checks a bound by comparing a pointer after
  an addition that may overflow, and clang folds that comparison to false without it.
- [provisional] `rglfw.c` compiles for macOS as Objective-C with
  `-fno-objc-msgsend-class-selector-stubs`. Reason: the pinned clang calls a class
  method through a stub `objc_msgSendClass$<selector>$<class>` that the linker writes,
  and ld64.lld 23.1.1 writes none, so the link fails. The calls then go through
  `objc_msgSend`.
- [provisional] raylib keeps the copy of miniaudio that `raudio.c` compiles. Reason: the
  smallest option, raylib as released. A program that calls raylib's audio and
  `anti.miniaudio` at once pulls both objects and defines the `ma_` functions twice.
  The report asks how to settle it.
- The names are `libraylib.a` and `libminiaudio.a`, or `raylib.lib` and `miniaudio.lib`
  on Windows, in `lib/<target>/`. The headers stay in the source trees and are not yet
  part of the runtime archive, as for PCRE2. The build copies the licence of each to
  `licenses/raylib.txt` and `licenses/miniaudio.txt` of the runtime tree.
- The licence texts of raylib (zlib) and miniaudio (MIT-0, or public domain) must be
  added to `LICENSES/` of the repository. The fence of this step did not include it.
  So must the copyright files of the 22 X11 and GL packages if the glibc sysroot ever
  ships in the runtime archive. The script copies them to `licenses/` of the sysroot.

### The system libraries of each library

A program links these beyond the C library, which antic links for every program. The
libraries named "at run time" are loaded by the library itself and need no link.

| Library | Linux, glibc | macOS | Windows |
|---|---|---|---|
| raylib | `X11`, `m`, `pthread`, `dl` | the frameworks `Cocoa` and `IOKit` | `gdi32`, `user32`, `shell32`, `winmm` |
| raylib, at run time | libGL, libGLX and the X11 extensions, through GLFW | `OpenGL`, through GLFW | `opengl32.dll`, through GLFW |
| miniaudio | `m`, `pthread`, `dl` | none | none |
| miniaudio, at run time | ALSA, PulseAudio or JACK | `CoreAudio`, `AudioToolbox` | WASAPI, DirectSound or WinMM |

- `libX11` is linked, since `rcore.c` calls it for the clipboard. GLFW loads the rest of
  X11. Since glibc 2.34 `pthread` and `dl` lie in `libc.so.6`, and the two names stay on
  the link line as raylib's Makefile gives them, for an older loader.
- On macOS `Cocoa` and `IOKit` resolve every symbol. The table of `src/anti/bindtype.c`
  names `Cocoa`, `CoreVideo`, `IOKit` and `OpenGL` for raylib, and the test links all
  four. It names `AudioToolbox`, `CoreAudio` and `CoreFoundation` for miniaudio, which
  links without them.
- On Windows the probe names the four libraries with `#pragma comment(lib, ...)`, which
  reaches lld-link as a `/DEFAULTLIB` directive. `kernel32.lib` comes with the C runtime.

### Tests

- [provisional] The tests are registered in `src/native/raylib.cmake`,
  `src/native/miniaudio.cmake` and `src/native/media.cmake`, as for PCRE2. The probes are
  `tests/abi/raylib_probe.c` and `tests/abi/miniaudio_probe.c`, and the Anti halves
  `tests/abi/raylib_link.anti` and `tests/abi/miniaudio_link.anti`.
- On Linux, `raylib_link_<target>` and `miniaudio_link_<target>` link the probe with its
  own `main`, compiled against the extended sysroot, with the pinned clang and lld
  against glibc. Eddie decided this: linking an Anti program against glibc is not this
  step's work. `tests/run_glibc_link.cmake` names every start file and library, checks
  that the program names the loader of glibc and `libc.so.6`, and on a host of that
  target runs it.
- On macOS and Windows the same tests link the Anti half with the probe and the library
  through antic and lld, as `pcre2_link_<target>` does. `tests/run_native_link.cmake`
  takes the options of antic, so the macOS link passes `--framework` for raylib.
  `raylib_run` and `miniaudio_run` run the program of the host.
- The raylib probe resizes a solid red image through the SIMD path of
  `stb_image_resize2.h`, formats and reads a text, packs a colour, and takes the address
  of `InitWindow`, `InitAudioDevice`, `DrawText` and `LoadModel`, so GLFW, rlgl and
  raylib's miniaudio must link. The miniaudio probe compares the version with the
  header, reads a square wave, counts the playback devices of the null back end, and
  takes the address of `ma_device_init` and `ma_decoder_init_file`.
- `media_sysroot` checks that every package the step names is pinned for both
  processors with a digest. It then runs a copy of `get-media-sysroot.cmake` on stand-in
  packages. The files land, an absolute link becomes relative and the licences are
  copied. A second run changes nothing, and a package of another digest is refused.

## Lines of other documents that change at the fold

- `docs/decisions.md`, "CPU levels": "Nothing in `src/native/` builds yet, so the
  refusal has no site until the first library arrives." PCRE2 now builds. The refusal
  still has no site, since no module imports a native library.
- `docs/decisions.md`, the open item on where the native libraries are published: "which
  nothing builds yet" no longer holds.
- `CLAUDE.md`, item 16 of "First sessions": the native libraries in `src/native/`, which
  nothing builds yet. PCRE2 is the first one built.
- `docs/decisions.md`, "Runtime archive": the glibc sysroot is "three packages of the
  jammy release pocket per processor". It is now those three and the 22 packages of X11
  and OpenGL. The same section leaves the glibc sysroots out "until the link mode
  against glibc exists". `src/native/media.cmake` installs them now for the two media
  libraries.
- `docs/decisions.md`, the entry on the frameworks of a binding: raylib links with
  `Cocoa` and `IOKit` alone, and miniaudio with none.
- `CLAUDE.md`, item 16 of "First sessions": raylib and miniaudio build as well.
