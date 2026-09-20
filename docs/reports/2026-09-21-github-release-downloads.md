# The binaries move to the GitHub release

Eddie decided on 2026-09-20 that the binaries of a release live on the GitHub
release of its tag alone. anti-lang.com serves text: the two installers, a
downloads page and the public key. The reason is that the
binaries and the key that checks them must sit on two hosts. A mirror of the
packages beside the key defeats that. This session made the change and ran
`./r --dry-run`. Nothing was signed, tagged or uploaded.

## What the audit found

Both installers took four files from `https://anti-lang.com/downloads/resources`:
the version from `anti/latest`, the package, `SHA256SUMS` and `SHA256SUMS.sig`.
Only the LLVM tools came from a GitHub release. The site, which serves the key,
therefore served the packages as well, and the webroot on triton still holds the
six packages of 0.1.0 under `downloads/resources/anti/0.1.0/`.

Step 9 wrote `downloads/index.toml` into a git clone that `ANTI_SITE` named and
pushed it. The site is no git clone and has no build step, so that step could
not have run. It also published neither installer.

## What changed

- `tools/release-base` names the repository, the download prefix of a release
  and the latest-release API. `tools/download-base` became `tools/site-base` and
  names the site.
- Both installers default to the release area and resolve the newest version
  from the API. `ANTI_BASE` stays the override for a staging area. An installer
  that gets a base without `ANTI_VERSION` stops, because a staging area names
  no newest version. `ANTI_GITHUB` and `ANTI_GITHUB_API` point at a
  fake release, which is how the new test reaches no server.
- The signature check is unchanged and now covers this route. No line of the
  manifest is read before `openssl pkeyutl -verify` accepts `SHA256SUMS.sig`
  against the carried key. That is how the LLVM tools are read too.
- Step 9 rsyncs `tools/install.sh`, `tools/install.ps1` and the downloads page
  over ssh to the webroot. It then reads `keys/release.pem` of the site against
  the one of the checkout. The page comes from `tools/downloads.html.in`. It
  names every asset by its release URL with its digest. The mode is written as
  `--chmod=u=rw,g=r,o=`, which openrsync takes. The webroot is setgid, so a file
  keeps the group of the server. openrsync refuses `--chmod=F640`.
- The preflight refuses a run whose `ANTI_SITE` is unset, whose value is not
  `<host>:<path>`, or whose host does not answer. It ran after the tag before,
  which would have published binaries that nothing pointed at.
- The test `installer_github` stands a fake release on disk. It signs the
  manifest with a key of its own. It then runs a copy of the installer that
  carries the public half. The checks are the API route, the named-version
  route, a manifest the signature does not cover, and the staging area. A
  staging area without a version is refused. So is an installer that still
  names `downloads/resources`.
- `docs/decisions.md`, `docs/distribution.md`, `docs/work-order-release-script.md`,
  `docs/vm-setup.md`, `CLAUDE.md` and `CHANGELOG.md` carry the model.

## The dry run

`ANTI_SITE=triton.niese.net:/var/www/anti-lang.com/webroot ./r --dry-run` on
`10382a8`, steps 1 to 5 for real and a plan for 6 to 11.

| Step | Outcome |
|---|---|
| 2 | Mac 494, ASan 493, UBSan 493, in an export of the commit |
| 3 | Six packages, each checked against `tools/cpu-levels`, macos-x86_64 under Rosetta |
| 4 | Six symbols archives, one `SHA256SUMS` of twelve files |
| 5 | anti-linux 431 tests, anti-windows 411 tests, each installs and uninstalls |
| 7 | Would tag `v0.1.0` and upload 14 files to `anti-lang/antic` |
| 9 | Would rsync the two installers and `downloads/index.html`, and read the key |

Step 5 is the evidence that the installer change works on Linux and on Windows.
Each machine first read the refusal of the unsigned manifest and then installed
through `ANTI_BASE` with `ANTI_STAGING=yes`, which is the staging route.

## Questions

1. The webroot still serves the six packages of 0.1.0 and an unsigned
   `SHA256SUMS`, and a mirror of the LLVM tools under
   `downloads/resources/llvm/23.1.1/`. Eddie decided to leave both for now.
   Nothing was removed.
2. The entry on the native libraries of `libs/` still publishes them under
   `downloads/resources/<library>/<version>/`, which no longer exists. The open
   section of `docs/decisions.md` holds the question.
3. `RELEASE_KEY` and `ANTI_SITE` are exported in Eddie's shell and not in the
   one that runs the commands of a session. The dry run passed `ANTI_SITE` on
   the command line. A real release needs `RELEASE_KEY` the same way.
