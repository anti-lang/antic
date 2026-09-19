# Toolchain work for later

The toolchain is frozen at `23.1.1-anti.3` of `anti-lang/llvm-tools`. Each line below is
toolchain work that came up after the freeze. It waits until a need makes it a release.

- The shared sanitizer runtimes of Linux, for a plugin under ASan loaded into an
  unsanitized host. They link against GCC's `crtbeginS.o`, `libgcc_s` and `libstdc++`,
  which the glibc sysroot does not hold.
