# Toolchain work for later

The toolchain is frozen at `23.1.1-anti.3` of `anti-lang/llvm-tools`. Each line below is
toolchain work that came up after the freeze. It waits until a need makes it a release.

- The shared sanitizer runtimes of Linux, for a plugin under ASan loaded into an
  unsanitized host. They link against GCC's `crtbeginS.o`, `libgcc_s` and `libstdc++`,
  which the glibc sysroot does not hold.
- ASan for windows-arm64, once a release of compiler-rt builds it for Windows on arm64.
  23.1.1 builds it for Windows on x86 alone.
- `install.ps1` verifies `SHA256SUMS.sig` with openssl, and refuses the install without
  one. Windows PowerShell 5.1 can read a P-256 signature on its own, through `ECDsaCng`
  and a key blob built by hand from the PEM. Only a Windows host can test it.
- The newest SDK of the Command Line Tools that antic takes, `APPLE_SDK_NEWEST_MAJOR` in
  `src/applesdk.h`, follows the ld64.lld of the pin. A new LLVM pin checks whether it reads
  the stubs of SDK 27 and later.
