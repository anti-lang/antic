# Work order: the release script

`./r` in the root of `antic` performs a release. It starts from a pushed `main` and ends at a published GitHub release, a signed download and an updated site. It refuses at the first step that is not in order. It is the only way a release is made. The script is one file, `tools/release.sh` with `./r` as its link. It is shell, with CMake for what CMake already does, and it mirrors the shape of `llvm-tools/scripts/release.sh`.

Two steps stay manual because they hold a secret: the signing passphrase and the `gh` login. Everything else is the script's. A step that fails stops the script with the step's name. What earlier steps produced stays in place, so a rerun resumes rather than restarts.

You do not stop to ask. Where a step is unclear, take the smallest option that keeps a release reproducible and refusable. Record it in the report and continue.

## Inputs

- `tools/version` holds the version, `0.1.0` and the like, and is the only place it is written. `./r` refuses when the version is already a tag.
- `CHANGELOG.md` holds an entry for the version, above the previous one, in the docs style. `./r` refuses when the entry is missing.
- `main` is checked out, without uncommitted changes, and pushed. `./r` refuses otherwise.
- The runtime archive's downloads are in place: the LLVM tools and clang at the pins, the six sysroots, raylib, from `build/`. `./r` runs the download steps when they are missing.

## Steps, in order

1. Preflight: version not tagged, changelog entry present, tree committed and pushed, `gh auth status` succeeds. `~/.ssh/config` names `anti-linux` and `anti-windows` and both answer.
2. Full suite on the Mac in a fresh export of the tree, then ASan and UBSan. Any failure stops.
3. Packages for six hosts by `tools/pack-anti.cmake`. Each holds antic, the runtime archive with a runtime per level, the standard library, the Linux sysroots, the installers, the licences and `VERSION`. The macOS x86_64 package is checked under Rosetta. `tools/check-libc.cmake` runs on both Linux packages. `tools/check-cpu.cmake` confirms each package's runtime levels match `tools/cpu-levels`.
4. Symbols: the shipped antic and anti are built the way `anti build --release` builds a program. Each package's binaries have their `-symbols.zip` in `build/dist/symbols/`, never inside the package.
5. VM checks: export the tree to `anti-linux` and `anti-windows`. Install each package with its installer, run the full suite on each, run the installer's own check, uninstall. Both must pass.
6. Digests and signature: `SHA256SUMS` over the six packages and the symbols archives, signed with the release key by `openssl pkeyutl -sign`. The key is read from `RELEASE_KEY`, which names an encrypted PEM. The passphrase is asked once. Without the key the script prints the two signing commands and stops before step 7.
7. Tag and release: `git tag -s v<version>` and push the tag. `gh release create v<version>` with the changelog entry as the body. Upload the six packages, the symbols archives, `SHA256SUMS` and `SHA256SUMS.sig`.
8. The runner matrix: `gh workflow run test.yml --ref v<version>` and wait for it. This is the one workflow run per release that the CI rule allows. A failure stops the script after the release exists. The report says so and the release is marked as a pre-release until the run is green.
9. The site: write `downloads/index.toml` for anti-lang.com with the version, the six package names, digests and URLs, and the key's fingerprint. Push it to the site's repository. The site's build publishes it. Nothing binary goes to the site.
10. Verification from outside: a fresh directory, `curl` of the installer from anti-lang.com, install. `anti --version` prints the version, `antic --version` prints the version and the LLVM pin. A hello program compiles and runs for the host and links for the other five.
11. Report: `docs/reports/<date>-release-<version>.md` with the outcome of every step, the counts, the digests and the run id of the matrix. It notes anything that was resumed. Commit and push the report. It is the last commit of the release.

## Options

- `./r --dry-run` performs steps 1 to 5 and prints what 6 to 10 would do, uploading nothing.
- `./r --resume` starts at the first step whose output is missing. A rerun after a failure does that by default.
- `./r --skip-vms` for a machine without the VMs. It marks the release as a pre-release, since step 5 did not run.

## Rules

- Every artefact carries the version and the build id, and `anti license --from` on every shipped binary prints both.
- A published version is never rebuilt. A second run of `./r` with the same version refuses at step 1.
- `./r` never touches the working tree except to write `build/dist/` and the report.
- The script has a test, `release_dry_run`. It runs `./r --dry-run` on a temporary copy of the tree with the version bumped and checks each step's output exists.

## Report

`docs/reports/<date>-release-script.md` for the session that builds `./r`. It holds what each step does, what was tried on the VMs, and the output of the first `--dry-run`.
