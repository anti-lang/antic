# The release check of 0.2.0

The step `release-check` of `docs/work-order-distribution.md`, on 2026-10-10.
It is the last step of the work order and builds nothing new. It packs the
package of each host from the tree, installs it on its machine from a
staging area, runs the check of step 10 of `./r` by hand with the network
off, and sets the version to 0.2.0. The commits are `f30c59f4` and
`887a3b19`. `./r --dry-run` and `./r` are Eddie's.

## What the step changed

- `tools/version` holds 0.2.0. `CHANGELOG.md` holds the entry of 0.2.0
  above the one of 0.1.0, in the same form, written from the 304 reports
  since the tag `v0.1.0` of 2026-09-21. `README.md`, `CLAUDE.md` and
  `docs/distribution.md` name the new version where they named 0.1.0.
- The version stands in the output of every program. The notice names the
  runtime and the standard library with it, every module carries it as the
  constant `anti.lang.package.version`, and a plugin records it as the
  version of what it provides. The suite at 0.2.0 failed `std_license`,
  `licenses`, `dump_opt_devirt_plugin`, `emit_identity` and
  `link_identity_macos-arm64` on that alone, in
  `build/drive/logs/rc-host-ctest-1.log`. Their expected files, the
  manifest of `emit_identity` and the digest of `link_identity_macos-arm64`
  are written again from the Mac at 0.2.0. Nothing but the version moved
  in them: the tree differs from `c804e289`, whose suite passed at 0.1.0
  with the old files, in the version and in the files that spell it.
- The header of `tests/run_emit_identity.cmake` said that a new version
  leaves the manifest as it is. The constant makes that untrue, and the
  header says now why the manifest moves. The gate that the two identity
  tests are unchanged holds for every step that leaves the version alone.
  This step cannot keep it, and the pins carry the version on purpose.
- `docs/decisions.md` lost the sentence on the gap list under "Binary
  distribution", and `CLAUDE.md` gained item 38 of "First sessions".

## Corrections of the work order

Step 10 of `docs/work-order-release-script.md` said that `antic --version`
prints the version and the LLVM pin. The script compares the line with
`antic <version>`, and decision 1 of `docs/work-order-distribution.md`
settles the one line, so the step says that now. No sentence of
`docs/work-order-distribution.md` needed a correction.

## The packages

`tools/pack-anti.cmake` packed the six hosts from the tree at `f30c59f4`,
one call per host, in 3:13 to 3:59 each. The logs are
`build/drive/logs/rc-pack-<host>.log`. The LLVM tools of the other five
hosts came from `build/deps/llvm-tools/<host>` at the pin 23.1.1. The
first call named `DEST` by a relative path and failed in `cmake -E tar`,
which the packer runs from its work directory. `./r` gives absolute paths,
so a release is not affected.

| Package | Bytes |
|---|---|
| `anti-0.2.0-macos-arm64.tar.xz` | 156,784,408 |
| `anti-0.2.0-macos-x86_64.tar.xz` | 171,920,260 |
| `anti-0.2.0-linux-arm64.tar.xz` | 151,619,964 |
| `anti-0.2.0-linux-x86_64.tar.xz` | 164,262,468 |
| `anti-0.2.0-windows-arm64.tar.xz` | 143,820,996 |
| `anti-0.2.0-windows-x86_64.tar.xz` | 158,479,144 |

The macos-arm64 package installed on the Mac takes 865 MB.

## The check with the network off

Each machine installed the package of its host with the installer from a
staging area in the layout of step 5 of `./r`. The installer first refused
the manifest without a signature, then installed with `ANTI_STAGING=yes`
and printed `antic 0.2.0` and `anti 0.2.0`. The network went off, the
probe `curl` of `https://github.com` failed, and the check of step 10 ran
by hand: `anti --version`, `antic --version`, a hello program compiled and
run for the host, and a link of it for each of the other five targets,
read with `llvm-readobj` of the package. The uninstaller then removed the
install, the network came back and the probe succeeded. The three logs
are `build/drive/logs/rc-mac-check.log`, `rc-vm-linux-check.log` and
`rc-vm-windows-check.log`, the install of the Mac is in
`rc-mac-phase-install.log`, and the scripts stand in `build/release-check/`.

- The Mac installed under `ANTI_HOME` in `build/release-check/mac-install`,
  as step 10 does. Its network went off per process: `sandbox-exec` with
  `(deny network*)` ran the two programs, the programs they built and the
  probe, which ended with status 6 of curl. The probe outside the sandbox
  succeeded after the check.
- anti-linux installed into `~/.local/bin` and `~/.local/share/anti`. The
  table of nftables of `docs/vm-setup.md` turned its network off, and the
  probe ended with status 28. Its five links read `elf64-x86-64`,
  `Mach-O arm64`, `Mach-O 64-bit x86-64`, `COFF-x86-64` and `COFF-ARM64`.
- anti-windows installed into `%LOCALAPPDATA%\Programs\anti\bin` and
  `%LOCALAPPDATA%\anti`. The rule of its firewall turned the network off,
  and the probe ended with status 28. The network came back with the rule
  removed and the cache of the DNS client flushed.

Both VMs stand as the step found them, on the network, with no install and
no rule.

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1701 of 1701 pass, 2 skipped, 350 s | `build/drive/logs/rc-host-ctest-2.log` |
| Mac | `asan` | 1700 of 1700 pass, 2 skipped, 476 s | `rc-asan-ctest.log` |
| Mac | `ubsan` | 1700 of 1700 pass, 2 skipped, 427 s | `rc-ubsan-ctest.log` |
| anti-linux | the suite, 1631 tests | 850 and 792 in two parts of `ctest -I`, all pass, 5 skipped, 303 s and 58 s | `rc-vm-linux-suite-1.log`, `-2.log` |
| anti-windows | the suite, 1619 tests | 1660 runs in ten parts, all pass, 14 skipped, 1300 s | `rc-vm-windows-suite-1.log` to `-10.log` |

The Mac ran `887a3b19`, and both VMs ran the tree of `f30c59f4`, which
differs from it in `CLAUDE.md`, `docs/decisions.md` and
`docs/work-order-release-script.md` alone. A part of a suite counts the
fixtures it needs again, so the parts of a machine sum to more than its
tests. The fourth part of anti-windows took 768 s, of which `deps_dir`
took 402 s. No build had a warning of our own code. The skips on the Mac
are the two `mimalloc_environment` tests. `release_dry_run` passes with
the version 0.2.0. `emit_identity` and `link_identity_macos-arm64` pass
against the pins written at 0.2.0. The docs-style checker reports nothing
on the documents the step touched.

The ASan suite took 476 s. `package_keys` sets its time with 475 s, and
`emit_identity` follows with 462 s.

## Provisional entries

One, under "The release script" in `docs/decisions.md`. The check of step
10 by hand on the Mac runs with the network off per process, through the
sandbox of macOS. A rule of the firewall of the Mac would end the session
that gives it.

## Questions

- The manifest of `emit_identity` carries the version of the standard
  library through `anti.lang.package.version`, so every release writes it
  again. Should the digest read that constant as `VERSION` too, as it reads
  the ident line, so the manifest survives a version?
- The suite of anti-windows takes 1300 s in parts, past the 10 minutes of
  one call of a session, and its fourth part alone took 768 s. `deps_dir`
  takes 402 s there. Is a faster `deps_dir` on Windows wanted?
- `ctest --preset asan` and `ctest --preset ubsan` run one test at a time
  without `-j`, since the presets name no jobs. `./c` passes `-j`. Should
  the presets carry the number of jobs?

## State

The two commits of the step are pushed. This report follows them in a
commit of its own.

```text
$ git log --oneline -3
887a3b19 Record the release check of 0.2.0 in the documents
f30c59f4 Set the version to 0.2.0 and write its changelog entry
c804e289 Report the step license-forms of the binary distribution
$ git status --short
?? docs/reports/2026-10-10-dist-release-check.md
$ git rev-parse HEAD origin/main
887a3b192bd08233334c9b65fababb1ff6444cfd
887a3b192bd08233334c9b65fababb1ff6444cfd
```
