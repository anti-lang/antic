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

With the archives installed from the local build, 407 of 407 pass on the Mac, and
ASan and UBSan pass 406 of 406 each. Every cache names `build/clang/bin/clang`.

## Not done

- The release of `23.1.1-anti.2` waits for the signature of the owner. Until it is
  published, a fresh checkout cannot configure, so these commits stay unpushed.
- The configure of a fresh checkout that downloads both archives, and the installer
  runs on the VMs and the Mac, wait for the release.
- No macos-x86_64 package is published. That needs a release of Anti on anti-lang.com.
- `std/` has no C sources yet, so the pinned clang compiles none there.
