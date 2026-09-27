# The target of Anti's binary distribution

The step records Eddie's eight decisions on what Anti delivers, and builds
nothing. Status: BLOCKED on one entry that contradicts the rule of host
tools, which the last section names.

## Where the target stands

- `docs/decisions.md` holds the eight rules in the new section "Binary
  distribution", each with the entries it replaces.
- `docs/tooling.md` holds the same target in its new section "Binary
  distribution", with the installer's two commands and the two commands of
  `anti sdk`.
- `CLAUDE.md` holds the rules of the two toolchains, the six packages and the
  host tools. They stand in its new section "Distribution".
- `docs/distribution.md` now agrees. The packages carry the LLVM tools, the
  glibc sysroots and the import libraries of mingw-w64. A package is accepted
  with the network off.

## The entries it replaces

- In "Libraries and runtime": the published package that left the LLVM tools
  to the installer, and the two sysroots a package could not hold. Also the
  installer's steps for xwin, for the stubs of the Command Line Tools, and
  for CMake from Kitware.
- The provisional entry that the package held no glibc sysroot.
- The four entries of xwin and its pins, and the check of the LLVM tools by
  the installers.
- The fallback of antic on an lld of the search path and on `LIB`, and the
  join of `--bundle-runtime` with Apple's `ld -r`.
- Edited to agree: the sysroots of the runtime archive, the sysroot of the
  clib tests and the one-liner with `| bash`. So were the local headers of a
  release, the key of the installers and the openssl check.

## The gap

What the packages and the installers do today that differs from the target,
in the order they are best built. Items 27 to 36 of "First sessions" in
`CLAUDE.md` hold the same list.

1. The LLVM tools into the package. The installers download them today.
2. antic and anti take their tools from the package alone. antic falls back on
   an lld of the search path and on `LIB` of an MSVC environment. Two of its
   messages name `tools/get-sysroot.cmake`.
3. The two glibc sysroots, with X11, OpenGL and the glibc runtime, into every
   package. No package holds them today.
4. The exact upstream source packages and versions of every copyleft part in
   `licenses/`. Today it holds the licence texts alone.
5. The headers of the native libraries into the runtime archive.
6. The Windows sysroot of mingw-w64, linking `ucrtbase.dll`. The lld link of
   today names `msvcrt.lib`, `libvcruntime.lib`, `ucrt.lib` and
   `legacy_stdio_definitions.lib` of xwin or of the Build Tools. The runtime,
   the native libraries and the tests compile against headers that agree.
7. xwin leaves the scripts, the pins and the installers.
8. The installers download the package alone, with `| bash`. Nothing a user
   runs needs CMake. antic and anti never run it. The installers run it only
   for `tools/get-sysroot.cmake`, for xwin and for the stubs of the Command
   Line Tools. The target is that the installer installs no CMake.
   The downloads page shows the manual install with its checks.
9. `--bundle-runtime` on Mach-O without Apple's `ld -r`, which needs a design:
   ld64.lld 23.1.1 has no relocatable output.
10. The test of a release with the network off on each VM.

## Question for Eddie

`anti bind --clang` runs a clang that is not in the package. In a development
tree it takes the pinned one, and otherwise the first `clang` on `PATH`.
`docs/decisions.md` records that with "The installers ship no clang. A user
needs one for `--clang` alone", and `docs/tooling-addendum.md` says the same.
The new rule says anti uses only what is in the package, with
`antic --linker platform` as the one exception, and that no C compiler ships.
Both cannot hold. Is `anti bind --clang` a second exception, a user's explicit
request like `--linker platform`? Or does `--clang` leave what a user runs, so
that bindings are made in a development tree alone? Those entries stay
unchanged until the answer.
