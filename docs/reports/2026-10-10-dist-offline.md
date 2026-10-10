# The test of a release with the network off

The step `offline` of `docs/work-order-distribution.md`, on 2026-10-10. It
builds decision 8 of that work order: step 5 of `./r` checks a fresh install
on each VM while the network of the VM is off. The commits are `6354d50d`
and `9c720c2c`.

## What the step built

- `tools/check-offline.cmake` builds and runs a raylib program and a plugin
  host for the host and links a hello program for each of the other five
  targets. It reads the format of each with llvm-readobj of the package.
  Every tool runs by its path with a `PATH` of the bin directory of the
  install alone.
- Step 5 of `tools/release.sh` is three phases per VM. `vm_install` sends
  the tree and the package and installs it. `vm_offline` turns the network
  off, runs the check and the uninstaller and turns the network on.
  `vm_suite` builds the tree and runs its tests. The script of a VM takes
  the phase as its argument.
- The network of a VM goes off by one rule of its firewall. The rule
  refuses every packet that leaves for another address than the one of the
  Mac, so the ssh session stays up. anti-linux takes a table of nftables
  under `sudo`. anti-windows takes a rule of the firewall of Windows that
  blocks the ranges around that address. `docs/vm-setup.md` shows the two
  commands of each VM and a probe, under "The network of a VM, off and on".
- The probe is `curl` of `https://github.com`. It has to fail before the
  check and to succeed after it. Every exit of `./r` turns the network of
  a VM on that the run turned off.

## What it tested

Each test failed before its code stood. `package_keys` runs
`tools/check-offline.cmake` on the package it packs and reads the refusal
of a directory without antic. `release_dry_run` gained four checks:

- `docs/vm-setup.md` shows the six commands of `tools/release.sh`.
- The stand-in of ssh keeps the state of the network of each VM. The
  install has to run with it on, the check with it off and the suite with
  it on, in that order.
- A check that fails leaves the network on, runs no suite and leaves no
  stamp. A command that turns nothing off stops the step before the check.
- No script of a VM names a log without a directory.

## The real step 5

`./r` did not run, since a session runs neither it nor its dry run. A
script under `build/` read the functions of step 5 out of
`tools/release.sh` as they stand and called them against a staging area
with two packages that `tools/pack-anti.cmake` built from `9c720c2c`:
`anti-0.1.0-linux-arm64.tar.xz` of 151,599,688 bytes and
`anti-0.1.0-windows-arm64.tar.xz` of 143,810,704 bytes. The tree both VMs
took equals `9c720c2c` file by file.

anti-linux ran `vm_install`, `vm_offline` and `vm_suite` in one call of
321 s. Its log, `build/drive/logs/offline-step5-vm-linux.log`, without six
warnings of tar about attributes of macOS:

```text
the installer refuses a manifest without a signature
antic 0.1.0
anti 0.1.0
hello
the package of linux-arm64 is installed
curl: (28) Resolving timed out after 10001 milliseconds
a raylib program of linux-arm64 builds and runs
a plugin host of linux-arm64 loads its plugin
a hello program links for linux-x86_64: elf64-x86-64
a hello program links for macos-arm64: Mach-O arm64
a hello program links for macos-x86_64: Mach-O 64-bit x86-64
a hello program links for windows-x86_64: COFF-x86-64
a hello program links for windows-arm64: COFF-ARM64
the install builds for linux-arm64 and links for the other five targets
the uninstaller refuses a directory without the marker
the install of linux-arm64 is checked and removed
100% tests passed, 0 tests failed out of 1627
```

anti-windows ran `vm_install` and `vm_offline` in 45 s. Its log,
`offline-step5-vm-windows.log`, without its empty lines:

```text
the installer refuses a manifest without a signature
antic 0.1.0
anti 0.1.0
the package of windows-arm64 is installed
Ok.
curl: (28) Resolving timed out after 10012 milliseconds
a raylib program of windows-arm64 builds and runs
a plugin host of windows-arm64 loads its plugin
a hello program links for linux-x86_64: elf64-x86-64
a hello program links for linux-arm64: elf64-littleaarch64
a hello program links for macos-arm64: Mach-O arm64
a hello program links for macos-x86_64: Mach-O 64-bit x86-64
a hello program links for windows-x86_64: COFF-x86-64
the install builds for windows-arm64 and links for the other five targets
the uninstaller refuses a directory without the marker
the install of windows-arm64 is checked and removed
Deleted 1 rule(s).
Ok.
Windows IP Configuration
Successfully flushed the DNS Resolver Cache.
```

`vm_suite` did not run on anti-windows. Its one call of `ctest -j4` takes
about 19 minutes, and a call of a session ends at 10. The same build and
the same tests ran by hand instead, the tests in eight parts of
`ctest -j4 -I`. Both VMs were left on the network, with no rule and no
install.

## What the real run found

- The scripts of a VM wrote their logs into the tree they extracted. That
  tree has no history, so `repo_layout` reads its directory, and it failed
  on anti-linux on seven logs. The logs now stand in `anti-release`. The
  script before this step wrote two of them before its suite ran. It would
  then have failed the same test, which no run here showed.
- Under `cmd /c` the phase reached the command file of Windows with a
  closing quote. ssh now runs the file by its path.
- A Windows raylib program links `gdi32.lib`, `user32.lib`, `shell32.lib`
  and `winmm.lib`, which the check names by their paths in the sysroot.
- `package_keys` read the refusal of the check as one line, and CMake
  wraps it where the path ends. It failed on anti-linux for that.

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1697 of 1697 pass, 2 skipped, 351 s | `build/drive/logs/offline-host-suite-2.log` |
| Mac | `asan` | 1696 of 1696 pass, 2 skipped, 474 s | `offline-asan-suite-2.log` |
| Mac | `ubsan` | 1696 of 1696 pass, 2 skipped, 422 s | `offline-ubsan-suite-2.log` |
| anti-linux | the suite, 1627 tests | 1627 of 1627 pass, 5 skipped, 276 s | `offline-step5-linux.log` |
| anti-windows | the suite, 1615 tests | 1654 runs in eight parts, all pass, 14 skipped, 1138 s | `offline-step5-windows-ctest-1.log` to `-8.log` |

All five ran the tree of `9c720c2c`. A part of a suite counts the fixtures
it needs again, so the parts of anti-windows sum to more than its tests.
The builds of the three Mac trees and of both VMs had no warning of our
own code. `emit_identity` and `link_identity_macos-arm64` pass, and no file
under `tests/emit-identity`, `tests/link-identity` or `src/` changed.
`release_dry_run` passes. The step changed `tools/release.sh`, so the real
`./r --dry-run` waits for Eddie. The docs-style checker reports nothing on
the documents the step touched. No sentence of the work order needed a
correction.

## Provisional entries

Three, under "The release script" in `docs/decisions.md`:

- The network of a VM goes off by one rule of its firewall, with the probe
  on both sides of the check.
- The check is `tools/check-offline.cmake`, one script for all three
  systems, with the sources of its programs from the tree.
- Step 5 removes the install while the network is off, so the suite runs
  on a VM that holds no install.

## Questions

- The language has `link framework` and `link linux` and no line for
  Windows. A raylib program of Anti alone names four import libraries by
  their paths. Is a `link windows` line wanted?
- Two packs of the same tree gave two digests for `linux-arm64`,
  `2b8bb2a4` and `a62fa841`. Should a package be reproducible?
- The script of anti-linux prints no name of a failing test, where the one
  of anti-windows does. Step 5 extracts the tree over the old one, so a
  file that left the repository stays on the VM.

## State

The two commits of the step are pushed. This report follows them in a
commit of its own, with two lines of `docs/vm-setup.md` on the suites.

```text
$ git log --oneline -3
9c720c2c Keep the logs of a VM out of the tree it tests
6354d50d Check an install with the network of the VM off
a7bf1ab9 Report the step bundle-macho of the binary distribution
$ git status --short
 M docs/vm-setup.md
?? docs/reports/2026-10-10-dist-offline.md
$ git rev-parse HEAD origin/main
9c720c2c974ba85f1639281b9efe60424fef1299
9c720c2c974ba85f1639281b9efe60424fef1299
```
