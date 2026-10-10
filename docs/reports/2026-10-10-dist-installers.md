# The installers download the package alone

The step `installers` of `docs/work-order-distribution.md`, on 2026-10-10.
It builds decision 7 of that work order: the installers download the
package of the host, `SHA256SUMS` and `SHA256SUMS.sig`, check both, unpack,
and run nothing of the package but its two programs. The commit is
`f8128624`.

## What the step built

- `tools/install.sh` and `tools/install.ps1` check the signature and the
  digest as before, unpack the package into the directories of the
  platform, run `antic --version` and `anti --version` from `bin/` of the
  unpacked package and stop when a program does not run or prints another
  version than the package. They then put the two programs on the path.
  The install of CMake, the step that took the stubs of the Command Line
  Tools for a package without Zig's, and the check of `tools/package-api`
  left `install.sh`. The check of `tools/package-api` left `install.ps1`.
  The one-liner is `| bash`, as the decision says, and the script is still
  POSIX sh. `install.sh` went from 386 lines to 334, `install.ps1` from 330
  to 329.
- `tools/pack-anti.cmake` writes no `tools/` into a package. The six files
  that travelled there for the installer, `get-sysroot.cmake`,
  `sysroot-pins`, `zig-stubs-pin`, `cmake-pin`, `cmake-version` and
  `package-api`, were 34,781 bytes before compression.
- `tools/package-api`, `tools/cmake-pin` and `tools/cmake-version` left the
  tree with the tests `package_api` and `cmake_pin` and the script
  `tests/run_pin.cmake`. `install.sh` alone read them.
- `tools/downloads.html.in` shows the manual install for both shells, with
  the checks of the installers as commands: the two `openssl` lines over
  `SHA256SUMS.sig` and the key, `shasum` or `Get-FileHash` over the
  package, the unpacking into the directories of the platform and the two
  programs run from there. `tools/release.sh` fills the two new marks
  `@DOWNLOAD@` and `@KEY@` from the download prefix of the release and the
  URL of the key, so the page spells neither address.
- Item 34 of "First sessions" in `CLAUDE.md` reads "Done", and the
  "Distribution" section says what the installers do.

## Corrections of the work order

The step says that step 5 of `./r` and `docs/work-order-release-script.md`
lose their lines about the LLVM tools and the sysroot. Step 5 of `./r` had
none left: the steps `tools` and `mingw` took them. What was wrong was step
3 of the release work order, which named the installers among the contents
of a package and the Linux sysroots alone. It now names the sysroots of all
six targets, no installers and no `tools/`. The sentence of the work order
says so.

`tools/release.sh` changed in `write_page` alone, for the two marks of the
page. Step 5 of the script was right as it stood.

## What it tested

- `installer_alone`, `tests/run_installer_alone.cmake`, is new. It reads
  both installers for `cmake`, `get-sysroot`, `package-api`,
  `xcode-select`, `CommandLineTools` and `ANTI_MICROSOFT` in either case,
  and for `--version`. It then runs `install.sh` against a package of the
  two programs alone, as scripts that print the version, with a `cmake` and
  an `xcode-select` first on the PATH that leave a file and fail when they
  are called. The log has to hold `antic 99.0.0` and `anti 99.0.0`, and a
  package whose programs print `98.0.0` is refused with both versions
  named. It failed first on the words in the old installers,
  `build/drive/logs/installers-red-1.log`.
- `installer_github` and `manifest_signature` failed first because the old
  `install.sh` demanded `tools/package-api` of the package, and pass with
  fake packages of the two programs alone. `installer_options` lost its
  check of the stubs step.
- `package_keys` refuses a package with any entry under `anti/tools/`. It
  failed first because the packer still copied the pins,
  `build/drive/logs/installers-red-package.log`.
- `release_dry_run` reads the page the dry run writes for the commands of
  the manual install, for every mark filled and for no `cmake`,
  `sysroot-pin`, `get-sysroot` or `package-api`. It failed first on the
  page without the commands, `build/drive/logs/installers-red-dryrun.log`.
  The stand-in packer of the test writes no `tools/` either. The real
  `./r --dry-run` is Eddie's and did not run.
- `release_key` refuses a packer that copies any file of `tools/`. Its old
  check wanted the list of the files the packer copies, and failed in the
  first host run, `build/drive/logs/installers-host-ctest-1.log`.
- An install from a staging area on both VMs, with the network on, from
  packages the tree packed for linux-arm64 and windows-arm64. Each VM first
  read the installer's refusal of the unsigned manifest, then installed
  with `ANTI_STAGING=yes` into the directories of the platform. The
  installer printed `antic 0.1.0` and `anti 0.1.0` from the package, the
  two programs of the bin directory printed the same, a hello program for
  the host built and ran, the install held no `tools/`, and the
  uninstaller removed it. On anti-windows the digest line of the manual
  install printed `True`. The logs are
  `build/drive/logs/installers-vm-linux-install.log` and
  `installers-vm-windows-install.log`. `shasum` on anti-linux printed
  perl's locale warnings, since the ssh session of the Mac forwards
  `LC_CTYPE=UTF-8`, which is the session and not the installer.

## Sizes

| Item | Size |
|---|---|
| `anti-0.1.0-linux-arm64.tar.xz`, packed from the tree | 151,597,024 bytes |
| `anti-0.1.0-windows-arm64.tar.xz`, packed from the tree | 143,806,616 bytes |
| The install on anti-linux, unpacked | 807 MB |
| The six files of `tools/` that left a package | 34,781 bytes |

## Decisions

Two `[provisional]` entries under "Binary distribution" in
`docs/decisions.md`: the check of an install, which compares the line each
program prints with the version of the package and stops otherwise, and
the three pins that left the tree with their tests. The entries that
pinned CMake at `tools/cmake-version` and that gave `tools/package-api` its
number are replaced, and the entry on `tools/install.ps1` says that it
runs on the Windows VM from a staging area rather than that it has never
run.

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1693 of 1693 pass, 2 skipped, 345 s | `build/drive/logs/installers-host-ctest-2.log` |
| Mac | `asan` | 1009 and 694 in two parts of `ctest -I`, 2 skipped, 437 s and 77 s | `installers-asan-ctest-1.log`, `-2.log` |
| Mac | `ubsan` | 1009 and 694 in two parts, 2 skipped, 411 s and 52 s | `installers-ubsan-ctest-1.log`, `-2.log` |
| anti-linux | the suite, 1623 tests | 850 and 784 in two parts, 5 skipped, 221 s and 58 s | `installers-vm-linux-ctest-1.log`, `-2.log` |
| anti-windows | the suite, 1611 tests | 1650 runs in eight parts of 200, 14 skipped, 110 to 437 s each | `installers-vm-windows-ctest-1.log` to `-8.log` |

The skips on the Mac are the two `mimalloc_environment` tests. The first
host run, `installers-host-ctest-1.log`, failed `release_key` on its old
check of the packer, and the run after the change passed every test. The
first Linux part failed `repo_layout` on two build logs this session had
written into the root of the exported tree, which moved under `build/`,
and the test passed on its own before the second part ran. The second
part of a suite counts the fixtures it needs again, so the parts of a
machine sum to more than its tests. The builds of all three Mac trees and
of both VMs had no warning of our own code. `emit_identity` and
`link_identity_macos-arm64` pass unchanged, and the docs-style checker
reports nothing on the documents the step touched.

## State

The commit of the step is pushed. This report follows it in a commit of
its own.

```text
$ git log --oneline -3
f8128624 Make the installers download the package alone
e47a0279 Report the step no-xwin of the binary distribution
20bcc695 Take the last traces of the SDK fetcher out of the tree
$ git status --short
?? docs/reports/2026-10-10-dist-installers.md
$ git rev-parse HEAD origin/main
f81286248d111532dd4613d3cd10006ab8f0ed23
f81286248d111532dd4613d3cd10006ab8f0ed23
```
