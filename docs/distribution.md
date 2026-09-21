# Anti distribution

Design of how Anti libraries are published and how licence obligations travel with
programs. Settled on 2026-09-14 and aligned with `docs/decisions.md` on 2026-09-15. The
binaries moved to the GitHub release and the site to text alone on 2026-09-20.
`docs/decisions.md` is the authority, and the details below agree with it, with
`docs/tooling.md` and with `docs/tooling-addendum.md`.

Contents:

- [Binary downloads](#binary-downloads)
- [HTTPS](#https)
- [CA bundle](#ca-bundle)
- [Publication targets](#publication-targets)
- [Transports](#transports)
- [Publish sequence](#publish-sequence)
- [GitHub templates](#github-templates)
- [Licence fields](#licence-fields)
- [Embedded licence text](#embedded-licence-text)
- [License command](#license-command)
- [Obligations of a shipped program](#obligations-of-a-shipped-program)
- [Licence choices for Anti itself](#licence-choices-for-anti-itself)
- [Chapter changes](#chapter-changes)

## Binary downloads

The repository holds no binary, and neither does anti-lang.com. The binaries of a
version are the assets of the GitHub release of its tag, under one layout. The
signature of the manifest is not among them.

```text
https://github.com/anti-lang/antic/releases/download/v<version>/<file>
https://github.com/anti-lang/antic/releases/download/v<version>/SHA256SUMS
```

- `tools/release-base` names the repository, that download prefix and the
  latest-release API `https://api.github.com/repos/anti-lang/antic/releases/latest`.
  Both installers carry the two addresses, because the site serves them and they read
  no file of the repository before they hold a package. The test `installer_github`
  pins their copies against that file.
- `<file>` names the component, the version and the target, as in
  `anti-0.1.0-macos-arm64.tar.xz`. The target names are the six of `--target`.
- A release holds the six packages and the six symbols archives. `SHA256SUMS` names
  all twelve, and `shasum -c` reads it. It is the manifest the release script signed.
  `SHA256SUMS.sig` stands on anti-lang.com, never here. Both installers read the
  signature against the key they carry before they trust a line of the manifest. A
  signature that is missing or wrong stops the install.
- The newest version is the tag of the newest release, which the latest-release API
  names. `ANTI_VERSION` names another one, and `ANTI_BASE` points an installer at a
  staging area of a release instead, in the layout `<base>/anti/<version>/<file>` that
  the packer writes.

A tag is published once and never again, so a path is written once and never again. A
pin file in the repository names the file and its digest, so an older Anti keeps
installing its pinned version.

### What the site serves

anti-lang.com serves text alone, from the webroot that `ANTI_SITE` names. `tools/site-base`
holds its address, today `https://anti-lang.com`, and the two paths below it.

```text
https://anti-lang.com/install.sh
https://anti-lang.com/install.ps1
https://anti-lang.com/downloads/
https://anti-lang.com/downloads/anti/<version>/SHA256SUMS.sig
https://anti-lang.com/keys/release.pem
```

Step 9 of a release rsyncs four files there over ssh: the two installers, the downloads
page, `SHA256SUMS.sig` of the version and `keys/release.pem`. It then reads the
signature and the key back over HTTPS, and checks that the signature covers the
manifest of the GitHub release. The page names every asset of the release by URL and
carries its digest. The signature of a version keeps its own directory, so the
installer of an older version still finds the signature of that version.

### Why the two halves stand apart

The binaries stand on GitHub and the signature that covers them stands on
anti-lang.com. The private key is on neither host.

- Whoever holds the GitHub release can replace a package. The manifest that names its
  digest is then covered by no signature, and both installers stop.
- Whoever holds the site can serve another signature or another key. Neither signs a
  package, because signing needs the private key.
- A forged release therefore needs both hosts at once.

A signature stored beside the binaries it covers leaves the private key as the only
thing between an attacker and a release. That is what this layout removes. Eddie
decided the split on 2026-09-21, after deciding on 2026-09-20 that the binaries leave
the site.

One release key signs everything Anti publishes, `anti-lang/antic` and
`anti-lang/llvm-tools` alike. The public half at `https://anti-lang.com/keys/release.pem`
is the single trust anchor of every download, and its SHA-256 fingerprint over the DER
form is `7e64c56e26a42946823a66aa1f30bf686b6b5dbd0dc0e2c165a080540ffc3eca`. A session
that meets a signature it cannot check does not generate a second key, and does not
move a signature onto the host that serves the binaries.

### The LLVM tools

The five LLVM tools are not served from the download area. The repository
`anti-lang/llvm-tools` builds them from the pinned LLVM source and publishes one archive
per host as an asset of a GitHub release, tagged `<version>-anti.<build>` as in
`23.1.1-anti.1`. Beside the archives stand `SHA256SUMS` and its signature
`SHA256SUMS.sig`. The recipe, the hosts and the checks of each build are in that
repository. Each release holds the tools and clang of each of the six hosts. antic takes
the tools of its host, and a build of antic takes clang as well.

`tools/llvm-pin` names the tag, the address of the release, the name of an asset and
the digest of the archive of each of the six hosts. `tools/clang-pin` names the archives
of clang in the same release. `tools/get-llvm.cmake` takes the
archive of the host into `build/llvm`, and the installers take it into the install
directory. Both check the digest of the pin, the line of `SHA256SUMS` and the
signature. A toolchain bump is a new release there and a new pin here.

The public key that checks `SHA256SUMS.sig` with `openssl pkeyutl -verify` lives in the
installers, which anti-lang.com serves, and in `keys/release.pem` of the repository,
which `tools/get-llvm.cmake` reads. anti-lang.com serves it as `keys/release.pem`. No
package carries a key. A key that travelled with a package could be replaced with it,
which is why the key and the packages stand on two hosts.

### What the components will hold

Every component is a name at the first level, and the kind of artefact is part of that
name rather than a directory above it. A pin names an exact URL, so nothing browses the
tree, and one less level is one less path to get wrong.

| Component | Files | Built by |
|---|---|---|
| `anti` | The runtime archive with antic per target | The release build of chapter 23 |
| `raylib`, `pcre2`, `mbedtls`, `miniaudio` | The static library per target, with headers | The CMake build in `libs/` |
| `musl` | The Linux sysroot per processor | `tools/get-sysroot.cmake` |

A Linux program of a release links the pinned sysroot and never the libc of the machine
that packed it: musl for the static form, glibc 2.35 and the kernel headers of Ubuntu
22.04 for the dynamic one. The shipped antic then runs on any Linux from Ubuntu 22.04 on.
`tools/pack-anti.cmake` reads the versioned symbols of each Linux program it packs with
`tools/check-libc.cmake` and refuses one that names a glibc above 2.35. The test
`linux_libc` reads a program linked by that same recipe, and `package_keys` packs a real
one on a Linux host.

A file we build carries the prefix `anti-`, as in `anti-raylib-6.0-linux-x86_64.tar.xz`.
A mirror of an untouched upstream file keeps its own name, as in `raylib-6.0.tar.gz`. A
variant, should one appear, follows the target in the name.

Two sysroots can never be served here. Apple's SDK is Apple's and the Windows CRT and SDK
are Microsoft's, and neither licence allows redistribution. The CRT stays a step on the
user's machine, which is why `tools/get-sysroot.cmake` asks for `ACCEPT_LICENSE`. Apple's
SDK comes from a Mac the user owns, with `anti sdk export` and `anti sdk import`, for a
program that names a framework. Zig's stubs of libSystem are ours to serve, and every
package carries them.

### Publishing

`tools/publish.cmake` uploads a version directory and checks it.

```sh
cmake -DDIR=<directory> -DREMOTE=<user@host:/path> -P tools/publish.cmake
```

It refuses a directory whose `SHA256SUMS` names a file that is missing, whose digests
disagree, or that holds a file the manifest does not name. It refuses one without
`SHA256SUMS.sig`, or whose signature is of another manifest, because an installer takes
neither. It then sends the files, the signature and the manifest last, and reads the
digests back over ssh. `CHECK_ONLY` runs the checks and sends nothing, which is what step
6 of a release does before it tags. `KEY` names another public key than
`keys/release.pem`.

### The release script

`./r` in the root of the repository makes a release. It reads the version from
`tools/version` and its entry from `CHANGELOG.md`. It runs the suite of the Mac
and the two sanitizer suites in an export of the commit, packs the six hosts,
writes the symbols archives beside them and checks the packages on both VMs. The
packages and the archives stand in `build/dist/packages` under the one `SHA256SUMS`
that the packer wrote, which it then signs in place with the release key. It tags
the commit and uploads the assets to a GitHub release. It uploads thirteen files and no signature. It runs the
runner matrix once, publishes the text of anti-lang.com and installs the result from
outside. `./r --dry-run` performs the
first five steps and prints what the rest would do.

The site takes the two installers, the downloads page, the signature of the manifest
and the public key. That page names the version and the six packages, with the digest
and the release URL of each. It carries the fingerprint of the public key. Step 9
rsyncs the four files and reads the signature and the key back. Nothing binary reaches
the site.

`docs/work-order-release-script.md` holds the eleven steps, and
`docs/decisions.md` holds the decisions under "The release script".

### After publishing a package

A package is accepted when a machine of that host installs it with the installer of the site and links
for every target it claims. The checks that a build on the development Mac cannot make:

1. `curl -fsSL https://anti-lang.com/install.sh | sh` on the target machine.
2. `antic hello.anti` for the host itself, then run the program.
3. `antic --target linux-x86_64` and `--target linux-arm64`, because the package carries
   both Linux sysroots and claims to link them anywhere.
4. Run the foreign one under `qemu-x86_64` or `qemu-aarch64` when the machine has it.

Step 3 is the one that found the missing zlib in our own lld. The musl objects of Alpine
for x86_64 carry `SHF_COMPRESSED` debug sections, and a linker without zlib stops on
every one of them. No check on the Mac saw it, because the LLVM release that the macOS
package carried then had zlib. Every archive of `anti-lang/llvm-tools` links zlib in.

## HTTPS

Every repository `url` is `https://` or `file://`. `http://` is refused, except for
`127.0.0.1` and `localhost`, so the resolver tests can run against a local server
without certificates.

`anti` links Mbed TLS from the runtime archive for its HTTPS client. The index fetch,
the ETag check, the package download and the post-publish verification all use it. The
`anti.net` module in the standard library uses the same library later.

## CA bundle

The trust store is Mozilla's root store, taken as curl's extract `cacert.pem` from
https://curl.se/docs/caextract.html. Licence: MPL 2.0.

- The runtime archive's CMake build downloads the file for a pinned date, verifies the
  SHA-256 curl publishes next to it, and installs it as `lib/cacert.pem`.
- The pin is bumped with every runtime archive release.
- `anti` and the `anti.net` module load the file at runtime from the archive. It is never
  embedded in a user's executable.
- On Linux, `--system-certs` reads `/etc/ssl/certs` instead. macOS Keychain and Windows
  CryptoAPI are not read. Mbed TLS cannot open them without a platform export. That
  export is out of scope.

The runtime archive's licence directory lists Mozilla and the MPL 2.0 for the bundle.

## Publication targets

`anti publish` writes to a publication target. Targets live in the user's
`~/.anti/config.toml`, never in a project, because they name hosts and keys.

```toml
[publish.ff]
url = "https://anti.foundingfuture.com/repo"
transport = "ssh://primus/var/www/anti/repo"

[publish.gh]
url = "https://raw.githubusercontent.com/eddie/anti-packages/main"
transport = "git+ssh://git@github.com/eddie/anti-packages.git"

[publish.local]
url = "file:///Users/eddie/antl-repo"
transport = "file:///Users/eddie/antl-repo"
```

`url` is what users list under `[repositories]`. `transport` is how `anti` writes.
`anti publish --to ff` selects a target. `publish = "ff"` under `[package]` in
`anti.toml` sets the project default.

## Transports

The transport is a URL scheme. `anti` runs system binaries for authentication and never
reads a key itself.

| Scheme | Mechanism |
|---|---|
| `file://` | Writes directly to the path |
| `ssh://host/path` | Runs `ssh` and `rsync`. `~/.ssh/config` supplies user, port, key and jump hosts. Reads the remote index with `ssh host cat` |
| `git+ssh://` and `git+https://` | Clones or fetches into `~/.anti/publish/<sha256-of-url>/`, copies the staged files, commits, runs `git pull --rebase`, pushes. Retries once on a rejected push |
| `cmd:<program>` | Runs the program with the staging directory and the target name as arguments. For S3, FTP or any custom API |

## Publish sequence

`anti publish` runs these steps in order:

1. Refuse when `[package]` lacks `license` or `license_text`, or when the package has
   dependencies and no `anti.lock`.
2. Build the `.antl` files in release mode and compute the `sha256` of each.
3. Read the current `<name>/index.toml` through the transport, not from the cache.
4. Write `build/publish/<name>/<version>/<module>.antl` for every module, `sha256`, and
   the updated `<name>/index.toml` with the modules of the version and `revision`
   increased by one.
5. Upload the version directory, then `sha256`, then `index.toml`. A reader never sees
   an index entry whose files are missing.
6. Fetch `<url>/<name>/index.toml` over HTTPS and confirm the new entry is present. On
   failure, print the URL read and the entry expected. The usual cause is a wrong `url`.

`--dry-run` runs steps 1 to 4 and prints the transport commands without running them.

## GitHub templates

Two template repositories on github.com. Both automate `anti publish`. Neither replaces
it.

`anti-repository`, one per author or organisation:

- Holds `<name>/index.toml` and `<name>/<version>/` directories on `main`.
- Served by `raw.githubusercontent.com`. The raw URL of `main` is the repository users
  list.
- Has no workflow. Publishing is a commit made by a library's workflow.

`anti-library`, one per library:

- The default layout, a starter `anti.toml`, `.gitignore` for `build/` and `dist/`.
- `check.yml` on demand, through a manual trigger: `anti check`, then `anti test` on
  GitHub's runners for Linux x86_64 and ARM64, macOS ARM64 and x86_64, Windows x86_64
  and ARM64. It does not run on every push, because hosted build minutes are limited.
- `release.yml` on a tag `v<version>`: verifies the tag equals `version` in `anti.toml`,
  runs `check.yml`, then `anti publish --to gh`, then attaches the `.antl` and `sha256`
  to the GitHub release.
- Pins the `anti` version it installs.

Setup the template cannot do, stated in its `README.md`:

1. Create a repository from `anti-repository`.
2. Generate a deploy key with write access to it.
3. Store the private half as the secret `ANTI_REPO_KEY` in each library repository.
   The workflow adds it to the runner's ssh agent before `anti publish`.

The two templates are created only after `anti` exists and has published one release by
hand. A template that has never run carries untested steps.

Two libraries releasing at the same moment never touch the same file, because the index
is per package. The `git pull --rebase` in the transport handles the push contention.

## Licence fields

`[package]` in `anti.toml` gains three fields:

```toml
[package]
name = "com.foundingfuture.anti.tree"
version = "1.2.4"
license = "MIT"
license_text = "LICENSE"
attribution = ["Copyright 2026 Eddie", "Contains code from Example Corp, BSD-3-Clause"]
```

- `license` is an SPDX identifier.
- `license_text` is a path to the full text, or an `https://` URL.
- `attribution` is zero or more lines copied verbatim.

The `.antl` header carries all three. A path in `license_text` is replaced by the file's
content at build time. The header never references the source tree. The runtime
archive's `.antl` files carry the same fields, written by the CMake build from the pinned
sources of each bundled library.

## Embedded licence text

The driver knows every module in an executable or a shared library at link time. It
emits one read-only data object, symbol `anti_licenses`, holding:

- The build id of the binary, the digest of its code.
- For every linked package, in dependency order: name, version, SPDX identifier and
  attribution lines.
- Every distinct licence text once, followed by the names of the packages it covers.

The object starts and ends with a fixed marker string so a tool can find it in any Anti
binary. Size is a few kilobytes. The embedding is unconditional. The notices travel with
the binary, which is what Apache 2.0 and BSD 3-clause require.

A static archive carries no `anti_licenses`, because it has no link step and two archives
in one C program would define the symbol twice. It carries a copy of the package header
of its `.antl`, licence fields included.

The program's command line is not touched. The standard library exposes the text as
`license.text() -> str` in the module `anti.license`. The `main` that `anti new` writes
handles `--licenses`:

```anti
import anti.io;
import anti.license;
import anti.text;

fn main(args: []str) -> int
{
    if args.len > 1 && text.equal(args[1], "--licenses") {
        io.print(license.text());
        return 0;
    }
    return 0;
}
```

The author may delete those lines. Nothing in the language depends on them.

## License command

`anti license` has five forms:

| Form | Prints |
|---|---|
| `anti license` | The licence of `antic` and `anti`, then every runtime archive component with name, version, identifier and full text |
| `anti license --project` | The packages the current project links, from `anti.lock` and the imported bundled modules, with identifiers, attributions and texts |
| `anti license --project --notice` | Writes the same content as `dist/<os>-<cpu>/<mode>/NOTICE.txt` |
| `anti license --from <executable>` | Reads `anti_licenses` out of the binary by its marker and prints it |
| `anti license --from-archive lib<name>.a` | Reads the licence fields from the copy of the `.antl` package header in a static archive, so a C project can produce its notice |

The runtime archive holds a `licenses/` directory with one file per component, written
by the CMake build. `anti license` reads it. Nothing is typed twice.

## Obligations of a shipped program

Code emitted by `antic` is the user's code translated. The LLVM licence exception says
the same for llvm-mc output. The user's obligations toward Anti are whatever the Anti
licence states about output, and it states none.

Static linking is where obligations enter. What the runtime archive links into an
executable carries its licence with it:

| Component | Licence | Binary obligation |
|---|---|---|
| Thread-pool runtime and standard library | 0BSD | None |
| raylib | zlib | None. Notice only in source distributions |
| miniaudio | MIT-0 | None |
| Mbed TLS | Apache 2.0 | Ship the licence and notice with the binary |
| PCRE2 | BSD 3-clause | Reproduce the copyright notice with the binary |
| CA bundle | MPL 2.0 | None. The file is loaded from the archive, not shipped |

A program that imports `anti.raylib` and `anti.miniaudio` owes nobody anything. A
program that imports `anti.net` or `anti.regex` owes an attribution, and `anti_licenses`
plus `NOTICE.txt` supply it. This section is a statement of how the licences read, not legal advice.

## Licence choices for Anti itself

- The thread-pool runtime and the standard library: 0BSD, with a `LICENSE` file in
  `rt/` and in `std/`. Use without attribution, so the common case embeds one short
  notice and owes nothing.
- `antic` and `anti`: MIT. The compiler's licence never reaches its output.
- The book text: decided separately on the site.

## Chapter changes

- Chapter 9: the three licence fields in the `.antl` header.
- Chapter 16: the `anti_licenses` data object and its markers in the emitter.
- Chapter 23: the CA bundle, the `licenses/` directory, the obligations table, the
  licence choices for the runtime and standard library.
- The build tool chapter: HTTPS, publication targets, transports, the publish
  sequence, the two GitHub templates, `anti license`, described without the code of
  `anti`.
- The standard library chapters: the `anti.license` module and the `--system-certs`
  option of `anti.net`.
