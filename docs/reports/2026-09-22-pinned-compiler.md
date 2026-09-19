# The pinned compiler

antic, the runtime and the libraries of `libs/` now build with the clang of release
`23.1.1-anti.2` of `anti-lang/llvm-tools`, and macos-x86_64 is a host again. The
llvm-tools report of the same date covers the archives.

## Pins

- `tools/llvm-pin` names `@VERSION@-anti.2` and the tools of all six hosts, macos-x86_64
  among them.
- `tools/clang-pin` is new. It names the same release and the clang archive of each
  host. `tests/run_release_pins.cmake` holds both pins to one tag.
- `tools/sysroot-pins` loses the Alpine `compiler-rt` package. The Linux sysroots take
  `libclang_rt.builtins.a` from the pinned clang.

## Changes

- `tools/pinned-compiler.cmake`, included before `project()` by `CMakeLists.txt` and
  `libs/CMakeLists.txt`, runs `tools/get-llvm.cmake` and `tools/get-clang.cmake` and
  caches `CMAKE_C_COMPILER` as `build/clang/bin/clang`. Both scripts share
  `tools/fetch-release.cmake`, which leaves an installed archive alone.
- `-DANTIC_SYSTEM_COMPILER=ON` is for a reader. `tools/pack-anti.cmake` refuses a build
  made with it, and a `CLANG` that is not the pinned version.
- The tests that compile C and C++ take the compiler of the build. The test
  `pinned_compiler` checks the cache and refuses a test script that names `cc`.
- The workflow configures with Ninja and adds the runner `macos-15-intel`.
  `docs/decisions.md` reverses the entry that dropped macos-x86_64 and records why.

## Tests

A fresh clone set up by the steps of the README downloaded clang and the LLVM tools of
`23.1.1-anti.2`, checked each against its pin and the signature, and cached
`build/clang/bin/clang` as `CMAKE_C_COMPILER`. It fetched all six sysroots, and both
Windows trees matched their pinned digests. The full suite passed 408 of 408, and ASan
and UBSan passed 407 of 407 each. The LLVM step of both installers took the tools of
`23.1.1-anti.2` on the Mac and both VMs for all six hosts. A copy with another key
refused them.

## Corrections

- The build steps of the README fetched four sysroots and left out both Windows
  targets. They fetch all six again, and the first fresh clone failed `package_keys`
  until then.
- `tools/get-sysroot.cmake` hashed no file of a Windows tree under a relative `DEST`,
  the form the README passes. It makes `DEST` absolute now, and the test
  `sysroot_digest` checks it.
- The owner confirmed that every antic produces binaries for all six targets. The
  provisional entry that left the Apple stubs to a Mac gave way to it. `CLAUDE.md` now
  holds that a provisional entry binds until the owner reviews it.

## Not done

- A Linux or Windows host fetches no Apple stubs yet, so it links no macOS program.
- A sanitizer build on Linux or Windows takes `-DANTIC_SYSTEM_COMPILER=ON` until
  `23.1.1-anti.3` carries their sanitizer runtimes.
- No macos-x86_64 package is published. That needs a release of Anti on anti-lang.com.
- `std/` has no C sources yet, so the pinned clang compiles none there.
