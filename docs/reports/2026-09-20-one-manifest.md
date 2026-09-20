# The one manifest of a release

Item 18 of the first sessions, both follow-ups of
`docs/reports/2026-09-20-release-script.md`. A release now writes its
twelve files into one directory under one `SHA256SUMS`. The two
installers read the signature of that manifest before they trust a line
of it.

## One directory, one manifest

The packages stood in `build/dist/packages` and the symbols archives in
`build/dist/symbols`, and step 6 wrote a second manifest over both in
`build/dist`. The download area carried the packer's manifest, which named
six files, and the signature covered the other one.

- Step 4 writes the six archives beside the packages and adds a line per
  archive to the `SHA256SUMS` the packer wrote. The step is idempotent: a
  line it wrote before is replaced rather than doubled.
- Step 6 adds no digest. It checks that the manifest names the twelve
  files of the release and nothing more, and signs it in place into
  `SHA256SUMS.sig`.
- Step 6 then runs the checks of `tools/publish.cmake` with `CHECK_ONLY`
  over the directory that goes up. A leftover file, a digest that moved or
  a signature of another manifest stops the release before the tag rather
  than after twelve uploads.
- `tools/pack-anti.cmake` removes its `work/` directory under `DEST`. It
  stood in the directory that is published.
- Step 7 uploads the twelve files, the manifest and its signature, which
  is the same fourteen as before.

## The signature, on every host

Both installers download `SHA256SUMS.sig` beside the manifest and check it
with openssl against the public key they carry, before they read the line
of the package. A signature that is missing or that covers another
manifest stops the install, and so does a missing openssl.

The last point differs from the LLVM tools, where a missing openssl
warns and the install goes on. `tools/llvm-pin` of the
package carries the digest of the tools, so the signature there reads a
number the machine already has. The manifest of a release is the only
thing that says which bytes are the package. Whoever serves the package
serves the manifest beside it, and a digest checked against an unsigned
manifest proves nothing.

`install.ps1` gained `Find-AntiOpenssl` and `Test-AntiSignature`, and the
LLVM block now calls both rather than holding its own copy.

## ANTI_STAGING, and why it exists

Step 5 installs each package on its VM from a directory of that machine,
and step 6 signs after it. The manifest of that staging area therefore has
no signature, and an installer that refuses one refuses the check the
release makes of itself.

`ANTI_STAGING=yes` takes a manifest without a signature, and never one
whose signature is wrong. Only step 5 sets it. The same VM run first
installs without it and reads the refusal. `install.sh` on Linux and
`install.ps1` on Windows therefore run the refusal for real in every
release, and step 5 fails when either prints no such line.

The entry in `docs/decisions.md` is `[provisional]`. The other way is to
sign before the VM checks. That gives those checks a signed manifest, and
moves the passphrase ahead of an hour of machine time. A dry run would
then install on no VM at all, since it signs nothing.

## The tests

- `manifest_signature` makes an EC key and signs a manifest with it. It
  runs a copy of `install.sh` that carries the public half, against a
  staging area on disk. It reads four cases.
  - A signature that verifies installs.
  - A signature of another manifest is refused.
  - No signature is refused.
  - No signature with `ANTI_STAGING` installs with a warning.

  The private key of the release is in no repository. A test that
  installs a signed manifest therefore brings a key of its own. It reads
  both installers for the three names of the rule as well, since only
  the shell half runs on a Mac.
- `publish_manifest` drives the checks of `tools/publish.cmake`. A whole
  area passes, and five directories fail.
  - A missing signature.
  - A signature of another manifest.
  - A file the manifest does not name.
  - A file it names and the directory lacks.
  - A digest that moved.

  `KEY` names the public key, which is how the test signs at all.
- `release_dry_run` reads the twelve files in `packages/`, the twelve
  lines of the one manifest, and refuses a file beside them that the
  manifest does not name.

482 tests pass on the Mac, 481 under ASan and 481 under UBSan.

## The dry run

`./r --dry-run` on `2fbe5a0`, with both VMs. It ran steps 1 to 5 and printed
a plan for the rest.

```text
r: Anti 0.1.0, a dry run. Nothing is signed, tagged or uploaded.
r: step 1, preflight
  0.1.0 is no tag here and none on origin
  CHANGELOG.md holds the entry of 0.1.0
  main is committed and pushed at 2fbe5a0
  gh is logged in
  anti-linux answers
  anti-windows answers
  the state is of 2930e2a, so the steps run again
  the downloads of build/ are in place
r: step 2, the suite of the Mac, then ASan and UBSan
  the commit is exported to build/dist/dry-run/export
  the Mac: 100% tests passed out of 482
  asan: 100% tests passed out of 481
  ubsan: 100% tests passed out of 481
r: step 3, the six packages
  anti-0.1.0-macos-arm64.tar.xz, 63da2410251b, levels of tools/cpu-levels
  anti-0.1.0-macos-x86_64.tar.xz, 3dfb58df6bd2, levels of tools/cpu-levels
  anti-0.1.0-linux-x86_64.tar.xz, e8d90aeb37a8, levels of tools/cpu-levels
  anti-0.1.0-linux-arm64.tar.xz, db4aa56e8db0, levels of tools/cpu-levels
  anti-0.1.0-windows-x86_64.tar.xz, 55138b1cd06e, levels of tools/cpu-levels
  anti-0.1.0-windows-arm64.tar.xz, 591fcd9aacb9, levels of tools/cpu-levels
  linux-x86_64: both programs link the pinned sysroot
  linux-arm64: both programs link the pinned sysroot
  macos-x86_64 under Rosetta: antic 0.1.0
r: step 4, the symbols of the twelve programs
  anti-0.1.0-macos-arm64-symbols.zip, 13ea4915e7b6
  anti-0.1.0-macos-x86_64-symbols.zip, 809910829926
  anti-0.1.0-linux-x86_64-symbols.zip, c8409ef4f044
  anti-0.1.0-linux-arm64-symbols.zip, 511321a0a364
  windows-x86_64: antic.exe has no symbol table, so its sections go in
  windows-x86_64: anti.exe has no symbol table, so its sections go in
  anti-0.1.0-windows-x86_64-symbols.zip, 4840bcbf0419
  windows-arm64: antic.exe has no symbol table, so its sections go in
  windows-arm64: anti.exe has no symbol table, so its sections go in
  anti-0.1.0-windows-arm64-symbols.zip, d326d497cb51
  SHA256SUMS of build/dist/dry-run/packages names 12 files
r: step 5, the two VMs
  anti-linux: 100% tests passed, 0 tests failed out of 421
  anti-linux: the package installs, compiles a program and uninstalls
  anti-windows: 100% tests passed out of 401
  anti-windows: the package installs and uninstalls
r: step 6, the signature of the manifest
  would sign build/dist/dry-run/packages/SHA256SUMS with $RELEASE_KEY into SHA256SUMS.sig
r: step 7, the tag and the release
  would tag v0.1.0 on 2fbe5a0 and push it
  would create the release v0.1.0 of anti-lang/antic
  would upload 14 files, the body from the entry of CHANGELOG.md
r: step 8, the runner matrix
  would run the workflow test.yml on v0.1.0 and wait for it
r: step 9, the site
  would write index.toml of downloads/ and push it to $ANTI_SITE
  the file it would write stands in build/dist/dry-run/index.toml
r: step 10, the check from outside
  would install 0.1.0 from https://anti-lang.com into build/dist/dry-run/verify
  would compile a program for this host and link one for the other five
r: step 11, the report
  would write docs/reports/2026-09-20-release-0.1.0.md and push it as the last commit
r: the dry run of 0.1.0 is done, and its files stand in build/dist/dry-run
```

The directory of the release holds thirteen entries, the twelve files and
their manifest:

```text
anti-0.1.0-linux-arm64-symbols.zip     anti-0.1.0-macos-x86_64-symbols.zip
anti-0.1.0-linux-arm64.tar.xz          anti-0.1.0-macos-x86_64.tar.xz
anti-0.1.0-linux-x86_64-symbols.zip    anti-0.1.0-windows-arm64-symbols.zip
anti-0.1.0-linux-x86_64.tar.xz         anti-0.1.0-windows-arm64.tar.xz
anti-0.1.0-macos-arm64-symbols.zip     anti-0.1.0-windows-x86_64-symbols.zip
anti-0.1.0-macos-arm64.tar.xz          anti-0.1.0-windows-x86_64.tar.xz
SHA256SUMS
```

Both VMs printed the refusal of the unsigned manifest before they
installed, which is what step 5 now reads:

```text
the installer refuses a manifest without a signature
```

## Questions

1. `ANTI_STAGING` is the provisional entry above. Sign before the VM
   checks instead, and let a dry run skip the install on the VMs?
2. A missing openssl now stops an install. `install.ps1` looks for the
   openssl of Git for Windows, as it already did for the LLVM tools. A
   Windows machine without Git therefore refuses the install, with a
   message that names it. Windows PowerShell 5.1 can verify a P-256
   signature without openssl, through `ECDsaCng` and a key blob built by
   hand from the PEM. That is about forty lines which only that host can
   test. Worth it?
3. `docs/decisions.md` says the symbols archive of a Windows host holds
   the PDB of each program beside the map of its sections, written by
   lld-link with `/DEBUG`. Step 4 writes the map alone, and the packer
   passes no `/DEBUG`. The decision is not implemented, and it is not part
   of this item.
