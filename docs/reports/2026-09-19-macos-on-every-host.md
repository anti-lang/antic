# macOS on every host

antic now takes clang and the LLVM tools from `23.1.1-anti.3`. Every host links a program
for all six targets, macOS included, as `docs/decisions.md` requires. The Linux and
Windows VMs built and ran antic with the pinned clang for the first time. They found
defects that the Mac could not show.

## Pins

- Both pins name `23.1.1-anti.3`. Its clang archives carry sanitizer runtimes for Linux
  and Windows, so the sanitizer builds of five hosts run with the pinned clang. A
  sanitizer build on windows-arm64 takes `-DANTIC_SYSTEM_COMPILER=ON`.
- `tools/sysroot-pins` names glibc 2.35 of Ubuntu 22.04, the packages that llvm-tools
  builds the Linux runtimes against. `get-sysroot.cmake` installs them as
  `linux-<cpu>-glibc` for the link mode against glibc.
- `tools/zig-stubs-pin` names Zig 0.16.0 and the APSL text of SPDX 3.29.0.

## macOS on every host

- Every host links a macOS program that names no framework against the stubs of libSystem
  that Zig generates, the Mac as well. Both macOS runtime libraries compile against Zig's
  headers on every host. Every package carries both macOS sysroots of Zig's.
- A program that names a framework, with `antic --framework <name>`, takes Apple's SDK
  from `sdk/` of the sysroot, or on a Mac from the Command Line Tools. `APPLE_SDK` of
  `get-sysroot.cmake` fills `sdk/` from a copy of the SDK. `anti sdk export` on a Mac packs
  a bundle that `anti sdk import` installs on any host. Without an SDK the link names the
  frameworks and where a Mac keeps the SDK. antic never fetches it.
- `anti` starts in `tools/anti/` with those two commands and ships in every package.

## Defects the VMs found

- `memcpy` took a null pointer for an empty copy at four sites. glibc declares its
  pointers non-null, so UBSan on Linux stopped the build, and macOS declares nothing.
- `program_abi_raymath` kept an expected file per host. The difference was a fused multiply
  and add that clang emits on arm64. The reference C of the tests now fuses none, and one
  expected file serves every target.
- On Windows, clang needed `_CRT_SECURE_NO_WARNINGS`, an enum is signed, and the driver
  looked for `ld64.lld` without `.exe`, so no target linked with lld from Windows.
- Four calls passed two arguments that emit code. clang for Windows evaluates arguments
  right to left, so antic built there emitted another program. Each now computes them
  in order, and a design note beside `select_reg` states the rule.

## Results

| Host | Suite | ASan | UBSan |
|---|---|---|---|
| Mac | 414 of 414 | 413 of 413 | 413 of 413 |
| Linux VM | 364 of 364 | 363 of 363 | 363 of 363 |
| Windows VM | 343 of 349 | none | none |

- `return42` for macos-arm64 links to the same bytes on the Mac, the Linux VM and the
  Windows VM, which `link_identity_macos-arm64` checks against the digest of the Mac.
- With a copy of SDK 26.5 as `APPLE_SDK`, a program that calls CoreFoundation links on
  both VMs. The executables of all three hosts are identical, and they run on the Mac.
  `anti sdk import` installs the bundle of the Mac on both VMs with one digest.

## Findings

- No Zig release since 0.13.0 ships a stub per macOS version. 0.16.0 ships one
  `libSystem.tbd`, whose targets name `arm64e-macos` as Apple's own stubs do. ld64.lld
  links arm64 programs against it, so no stub came from `tapi stubify`.
- The stubs of SDK 26.5 take 158 MB per macOS sysroot as regular files. The bundle is
  5.4 MB. Zig's headers take 7.4 MB.

## Questions

- Six tests fail on the Windows VM alone. `program_args` and `std_text` read UTF-8 output
  as the console code page, and `program_abi_wchar` expects a 32-bit `wchar_t`.
  `std_error`, `std_log` and `std_signals` need a look. Do they come before the IR pass?

## Not done

- The six Windows failures above.
- ASan on real x86_64 hardware, on the first release day.
