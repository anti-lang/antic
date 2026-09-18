# LLVM tools from anti-lang/llvm-tools

The LLVM tools of every host now come from release `23.1.1-anti.1` of
`anti-lang/llvm-tools`. That repository builds them from the pinned LLVM source, and
its `docs/reports/2026-09-21-llvm-tools.md` reports the build. antic downloads one
archive per host and no longer holds a recipe, an upstream pin or a packer for LLVM.

## Changes

- `tools/llvm-pin` names the tag, the release, the asset name and the digests of the
  five hosts. `tests/run_llvm_pin.cmake` checks it, that it names no macos-x86_64, and
  that no script spells the version or the release.
- `tools/get-llvm.cmake` downloads the asset of the host into `build/llvm`. It checks
  the pinned digest, the line of `SHA256SUMS` and `SHA256SUMS.sig` with
  `openssl pkeyutl -verify` against `tools/llvm-tools-key.pem`, then runs
  `tools/check-llvm.cmake`.
- `CMakeLists.txt` names `build/llvm` once, as `ANTIC_LLVM_DIR`. The presets and the
  workflow no longer spell the path of the tools.
- `tools/install.sh` and `tools/install.ps1` take the LLVM tools from the release that
  the pin of the package names. Each installer carries the public release key and
  checks the tools with it. `keys/release.pem` holds the same key for
  `tools/get-llvm.cmake`. `tools/pack-anti.cmake` packs the pin and no key, and takes
  `ANTIC` to pack one host from an antic already built.
- `tools/build-llvm.cmake`, `tools/llvm-upstream`, `tools/pack-llvm.cmake` and
  `tools/zlib-pin` left. `docs/decisions.md` and `docs/distribution.md` refer to the
  new repository.

## Tests

- 406 of 406 pass on the development Mac with the downloaded tools, among them the
  12 cross links and the 120 assembly tests of the six targets. ASan and UBSan pass
  405 of 405 each. `release_key` holds both installers to `keys/release.pem`, and
  `package_keys` packs this host and refuses an archive that carries a key.
- `get-llvm.cmake` ran on the Mac with OpenSSL 3 and with the LibreSSL of macOS, and on
  the Windows ARM64 VM with the openssl of Git. The LLVM block of `install.sh` ran on
  the Linux VM and on the Mac, and on the Mac without openssl it warns and installs.
  The block of `install.ps1` ran on the Windows VM for arm64 and x86_64. A wrong key
  is refused by `get-llvm.cmake` and `install.sh` on the Mac and by `install.ps1` on
  the Windows VM. A wrong digest is refused by `get-llvm.cmake`.
- PowerShell 5.1 with `Stop` turns the stderr of a native command into an error, so
  the check reads the exit code of openssl with `Continue`.

## Decisions of the owner

- GPG leaves. openssl signs and verifies, and a Mac warns only where it has no
  openssl. The release key is ECDSA P-256, since the LibreSSL of macOS has no Ed25519.
- macos-x86_64 is built and published and not supported. The pin names no digest for
  it, and `docs/decisions.md` says so.
- A separate release key of `release@anti-lang.com` signs the releases, and its
  private half stays off the development machine.
- The installers, which anti-lang.com serves, carry the public key. No package
  carries a key, since whoever could replace a package could replace a key in it.
- The tag is `<version>-anti.<build>`. Release `23.1.1-1` is deleted and the same six
  archives are published again as `23.1.1-anti.1`.
- `tools/package-api` stays at 1. The installer takes the tools from the release when
  the package holds `tools/llvm-pin`, and from the package otherwise, as 0.1.0 does.
