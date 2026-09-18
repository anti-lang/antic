# LLVM tools from anti-lang/llvm-tools

The LLVM tools of every host now come from release `23.1.1-1` of
`anti-lang/llvm-tools`. That repository builds them from the pinned LLVM source, and
its `docs/reports/2026-09-21-llvm-tools.md` reports the build. antic downloads one
archive per host and no longer holds a recipe, an upstream pin or a packer for LLVM.

## Changes

- `tools/llvm-pin` names the tag, the release, the asset name, the signing key and the
  six digests. `tests/run_llvm_pin.cmake` checks it, and that no script spells them.
- `tools/get-llvm.cmake` downloads the asset of the host into `build/llvm`. It checks
  the pinned digest, the line of `SHA256SUMS` and `SHA256SUMS.sig` with gpgv against
  `tools/llvm-tools-key.gpg`, then runs `tools/check-llvm.cmake`.
- `CMakeLists.txt` names `build/llvm` once, as `ANTIC_LLVM_DIR`. The presets and the
  workflow no longer spell the path of the tools.
- `tools/install.sh` and `tools/install.ps1` take the LLVM tools from the release that
  the pin of the package names. `tools/pack-anti.cmake` packs the pin and the key in
  place of the tools.
- `tools/build-llvm.cmake`, `tools/llvm-upstream`, `tools/pack-llvm.cmake` and
  `tools/zlib-pin` left. `docs/decisions.md` and `docs/distribution.md` refer to the
  new repository.

## Tests

- 404 of 404 pass on the development Mac with the downloaded tools, among them the
  12 cross links and the 120 assembly tests of the six targets. ASan and UBSan pass
  403 of 403 each.
- `get-llvm.cmake` ran on the Mac and on the Windows ARM64 VM. The LLVM block of
  `install.sh` ran on the Linux VM with gpgv and on the Mac without it. The block of
  `install.ps1` ran on the Windows VM for arm64 and x86_64. A wrong digest and a wrong
  key are refused.
- The Windows run found two faults, both fixed. The gpgv of Git reads `C:` in a
  keyring path as a URL scheme. PowerShell 5.1 with `Stop` turns the stderr of gpgv
  into an error.

## Questions

- An installer without gpgv relies on the pinned digest alone, as on a Mac without
  GnuPG. Should it refuse instead?
- The work order asked for six archives, including macos-x86_64, which
  `docs/decisions.md` names a target and not a host. The entry now says the release
  carries it. Keep it?
- `tools/package-api` stays at 1. The installer takes the tools from the release when
  the package holds `tools/llvm-pin`, and from the package otherwise, as 0.1.0 does.
- The signing key carries the address `eniese@gmail.com`. Should a key of anti-lang
  sign the releases?
