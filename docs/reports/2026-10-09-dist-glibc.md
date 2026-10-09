# The glibc sysroots into every package

Step `glibc` of `docs/work-order-distribution.md`, 2026-10-09. Every package carries the
two glibc sysroots and the glibc runtime of both Linux targets.

## What the step built

- `tools/pack-anti.cmake` copies `sysroot/linux-x86_64-glibc` and
  `sysroot/linux-arm64-glibc` of the runtime archive as they are, with the X11 and OpenGL
  development files. It copies `lib/linux-<cpu>-glibc/` beside the runtime of the six
  targets: the runtime per level with its bitcode of full LTO, libunwind and the ASan and
  profile runtimes. The bitcode of ThinLTO stays out, as it does for the other targets.
- "Binary distribution" in `docs/decisions.md`, `docs/distribution.md` under "What the
  components will hold" and item 29 of `CLAUDE.md` say so.

## What it tested

Test first. `package_keys` failed on the old packer, since the archive lacked
`linux-x86_64-glibc/lib/x86_64-linux-gnu/libc.so.6`, `build/drive/logs/glibc-red.log`.

`package_keys`, `tests/run_package.cmake`, now requires the libc, `Scrt1.o`, `libX11.so`,
`libGL.so`, `Xlib.h` and `gl.h` of both glibc sysroots and the glibc runtimes. From the
unpacked package, with its `bin/` alone on the `PATH`, it does this for both Linux targets:

- `anti build --target` of `tests/anti-build/window`, a program of `link linux`;
- a raylib program: the packed `anti bind` binds `tests/bind/raylib_api.json` as
  `game.raylib`, `antic -c` compiles it into a library file, and antic links the program
  with `lib/<target>/libraylib.a` of the package and the `link linux` names of the binding;
- the packed `llvm-readobj` finds `PT_INTERP`, `libX11.so.6`, `libGL.so.1` and `libc.so.6`
  in both programs;
- a Linux host runs both programs of its own target. No display serves them, so the
  raylib program calls `IsMouseButtonPressed`, which pulls in rcore and GLFW and opens no
  window.

By hand, `build/drive/logs/glibc-hand-mac.log` and `glibc-hand-linux.log`: both
programs linked from the unpacked macos-arm64 package on the Mac, with the package's `bin/`
alone on the `PATH`. The linux-arm64 pair ran on anti-linux and printed `no display`,
`no context` and `no click`, exit 0.

## Sizes

Each package packed on the Mac from `build/host/runtime` before and after the change,
`build/drive/logs/glibc-pack-before-<host>.log` and `glibc-pack-after-<host>.log`.

| Package | Before, bytes | After, bytes | Growth, bytes |
|---|---:|---:|---:|
| macos-arm64 | 125,273,968 | 142,227,872 | 16,953,904 |
| macos-x86_64 | 140,443,076 | 157,387,600 | 16,944,524 |
| linux-arm64 | 120,130,820 | 137,051,372 | 16,920,552 |
| linux-x86_64 | 132,745,420 | 149,655,972 | 16,910,552 |
| windows-arm64 | 112,316,828 | 129,285,852 | 16,969,024 |
| windows-x86_64 | 126,945,448 | 143,901,776 | 16,956,328 |

Unpacked, `du -sk` gives 47,988 KiB and 53,372 KiB for the two sysroots and 9,192 KiB
and 8,936 KiB for the two runtimes, 119,488 KiB in all. The work order expected about 100 MB per package. After xz the
growth is 17 MB.

## Decisions

No `[provisional]` entry. The step's sentence names the source of each copy, and the
bitcode of ThinLTO follows the entry of 2026-10-07.

Observed and left alone: a whole-program compile imports library files and not sources,
so the raylib program compiles its binding with `antic -c` first. `anti build` has no
line that links a static library such as `libraylib.a`. `antic -c -I . game/raylib.anti`
with a relative root named the module `raylib`, where absolute paths gave `game.raylib`.

## Gates

| Suite | Result | Time and log |
|---|---|---|
| Mac host | 1693 of 1693, `sysroot_build_tools` and the two `mimalloc_environment` skipped | 369 s, `build/drive/logs/glibc-host-suite.log` |
| Mac ASan | 1692 of 1692, the same three skipped | 520 s, `glibc-asan-suite.log` |
| Mac UBSan | 1692 of 1692, the same three skipped | 452 s, `glibc-ubsan-suite.log` |
| anti-linux | 1623 of 1623 in three parts of `ctest -I`, six skipped | 44 s, 252 s and 11 s, `glibc-vm-linux-suite-1.log` to `-3.log` |
| anti-windows | 1611 of 1611 in five parts, fourteen skipped | 155 s, 403 s, 110 s, 29 s and 330 s, `glibc-vm-windows-suite-1b.log`, `-2b.log`, `-3.log` to `-5.log` |

`package_keys` took 342 s on the Mac host suite, 252 s on anti-linux and 330 s alone on
anti-windows. The first Windows part 2 ran without `vcvarsall.bat`, and `deps_dir` found
no Ninja, `glibc-vm-windows-suite-2.log`. Parts 1 and 2 ran again in its environment as
`test.cmd` sets it. The VMs took the six changed files with `tar -xmf`, after a digest of
every tracked file showed that the rest equalled `HEAD`. `emit_identity` and
`link_identity_macos-arm64` passed unchanged in the three Mac suites.

`tools/pack-anti.cmake` changed. `release_dry_run` passed in the host suite, and its
stand-in packer does not read the real one. The real `./r --dry-run` is Eddie's and did
not run here.

## State

Before the commit of this report, which follows it and is pushed with it.

```text
$ git log --oneline -3
0b55d4f2 Put the two glibc sysroots and the glibc runtime into every package
a43b0438 Report the step own-tools of the binary distribution
968cf018 Name the sysroot as the toolchain of every lld-link
$ git status --short
?? docs/reports/2026-10-09-dist-glibc.md
$ git rev-parse HEAD origin/main
0b55d4f276d9fc7e0557dfa1c532844af5bf6c61
a43b04389e5e44c230635b3bd1f8914ef0e53492
```
