# Work order: the release script

`./r` in the root of `antic` performs a release. It starts from a pushed `main` and ends at a published GitHub release, a signed download and an updated site. It refuses at the first step that is not in order. It is the only way a release is made. The script is one file, `tools/release.sh` with `./r` as its link. It is shell, with CMake for what CMake already does, and it mirrors the shape of `llvm-tools/scripts/release.sh`.

Two steps stay manual because they hold a secret: the signing passphrase and the `gh` login. Everything else is the script's. A step that fails stops the script with the step's name. What earlier steps produced stays in place, so a rerun resumes rather than restarts.

You do not stop to ask. Where a step is unclear, take the smallest option that keeps a release reproducible and refusable. Record it in the report and continue.

## Inputs

- `tools/version` holds the version, `0.1.0` and the like, and is the only place it is written. `./r` refuses when the version is already a tag.
- `CHANGELOG.md` holds an entry for the version, above the previous one, in the docs style. `./r` refuses when the entry is missing.
- `main` is checked out, without uncommitted changes, and pushed. `./r` refuses otherwise.
- The runtime archive's downloads are in place: the LLVM tools and clang at the pins, the six sysroots, raylib, from `build/`. `./r` runs the download steps when they are missing.
- `tools/keys/private/release-key.pem` holds the private key that signs `SHA256SUMS`. `.gitignore` excludes the directory, and a run stops before the tag when the file is missing.
- `ANTI_SITE` holds the webroot of anti-lang.com as `<host>:<path>`, today `triton.niese.net:/var/www/anti-lang.com/webroot`. Step 9 rsyncs the text of the site there over ssh. The preflight refuses a run without the variable, or with a host that does not answer.

## Steps, in order

1. Preflight: version not tagged, changelog entry present, tree committed and pushed, `gh auth status` succeeds. `~/.ssh/config` names `anti-linux` and `anti-windows` and both answer. `ANTI_SITE` names a webroot and its host answers.
2. Full suite on the Mac in a fresh export of the tree, then ASan and UBSan. Any failure stops.
3. Packages for six hosts by `tools/pack-anti.cmake`. Each holds antic, the runtime archive with a runtime per level, the standard library, the Linux sysroots, the installers, the licences and `VERSION`. The macOS x86_64 package is checked under Rosetta. `tools/check-libc.cmake` runs on both Linux packages. `tools/check-cpu.cmake` confirms each package's runtime levels match `tools/cpu-levels`.
4. Symbols: the shipped antic and anti are built the way `anti build --release` builds a program. Each package's binaries have their `-symbols.zip` beside the packages in `build/dist/packages/`, never inside the package. A Windows binary has no symbol table, so its archive holds the map of its sections and the PDB the packer left in `build/dist/symbols/`. The CodeView record of the executable is read against that PDB first. The step adds the line of each archive to the `SHA256SUMS` the packer wrote, so the directory of the release carries one manifest of twelve files.
5. VM checks: export the tree to `anti-linux` and `anti-windows`. Install each package with its installer, run the full suite on each, run the installer's own check, uninstall. Both must pass. The manifest travels without the signature that step 6 writes, so each machine first reads the installer's refusal of an unsigned manifest and then installs with `ANTI_STAGING=yes`.
6. The signature: the `SHA256SUMS` of steps 3 and 4 names the twelve files of the release, and this step signs it in place with the release key by `openssl pkeyutl -sign`. The key is read from `tools/keys/private/release-key.pem`, and no environment variable names it. It is plaintext on the offline machine that cuts releases, and an encrypted PKCS#8 PEM signs as readily, with its passphrase asked once. Before it signs, `git ls-files` must call the file untracked and `git check-ignore` must call it ignored, and either answer otherwise stops the run naming the file. The preflight makes the same check when the file is there. Without the key the script prints the two signing commands and stops before step 7. It ends with the checks of `tools/publish.cmake` over the directory that goes up.
7. Tag and release: `git tag -a v<version>` and push the tag. The tag is annotated and unsigned, as in llvm-tools, because `SHA256SUMS.sig` is the signature of a release. `gh release create v<version>` with the changelog entry as the body. Upload thirteen files: the six packages, the six symbols archives and `SHA256SUMS`. `SHA256SUMS.sig` stays off the release, because a signature beside the binaries it covers protects nothing. Step 9 publishes it on the other host.
8. The runner matrix, which a release leaves out. `./r --matrix` runs it: `gh workflow run test.yml --ref v<version>` and wait for it. It is then the one workflow run per release that the CI rule allows. A failure stops the script after the release exists. The report says so, and the release is marked as a pre-release until the run is green.
9. The site: rsync the text of anti-lang.com into the webroot that `ANTI_SITE` names. That is `tools/install.sh`, `tools/install.ps1`, the downloads page, `SHA256SUMS.sig` of this version and `tools/keys/release.pem`. `tools/downloads.html.in` holds the page, which names the version, the six packages with their digests and the URLs of the release assets, the URL of the signature and the key's fingerprint. `tools/site-base` names the site and the two paths under it. The signature goes to `downloads/anti/<version>/SHA256SUMS.sig`, one directory per version. The step then reads the signature and the key back over HTTPS and checks that the signature covers the manifest of the release. Nothing binary goes to the site, which has no git clone and no build step.
10. Verification from outside: a fresh directory, `curl` of the installer from anti-lang.com, install. The installer takes the package and the manifest from the GitHub release of the tag and the signature from anti-lang.com, as a user's install does. `anti --version` prints the version, `antic --version` prints the version and the LLVM pin. A hello program compiles and runs for the host and links for the other five.
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
