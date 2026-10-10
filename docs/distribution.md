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
  `anti-0.2.0-macos-arm64.tar.xz`. The target names are the six of `--target`.
- A release holds the six packages and the six symbols archives. `SHA256SUMS` names
  all twelve, and `shasum -c` reads it. It is the manifest the release script signed.
  `SHA256SUMS.sig` stands on anti-lang.com, never here. Both installers read the
  signature against the key they carry before they trust a line of the manifest. A
  signature that is missing or wrong stops the install.
- The newest version is the tag of the newest release, which the latest-release API
  names. `ANTI_VERSION` names another one, and `ANTI_BASE` points an installer at a
  staging area of a release instead, in the layout `<base>/anti/<version>/<file>` that
  the packer writes.
- Each installer downloads the package of its host and `SHA256SUMS` from the release
  and `SHA256SUMS.sig` from the site, checks the signature with the key it carries and
  the digest against the manifest, unpacks the package into the directories of the
  platform, runs `antic --version` and `anti --version` of the package as the check of
  the install and puts the two programs on the path. It downloads nothing else and runs
  nothing else. The package carries the LLVM tools and the sysroot of every target, and
  no script or pin for an installer, since the step `installers` of
  `docs/work-order-distribution.md`. The test `installer_alone` holds the rule.

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
page, `SHA256SUMS.sig` of the version and `tools/keys/release.pem`. It then reads the
signature and the key back over HTTPS, and checks that the signature covers the
manifest of the GitHub release. The page names every asset of the release by URL and
carries its digest. It shows the manual install as commands for both shells, with the
checks of the installers: the two `openssl` lines over `SHA256SUMS.sig` and the key,
`shasum` or `Get-FileHash` over the package, and the two programs run from the unpacked
package. The signature of a version keeps its own directory, so the
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

Every package carries the LLVM tools of its host in `bin/` beside antic and anti:
llvm-mc, lld under its four names, llvm-ar, llvm-objdump, llvm-readobj, opt, llc and
llvm-profdata, with `llvm-version`. They are not served from the download area on their
own. The repository `anti-lang/llvm-tools` builds them
from the pinned LLVM source and publishes one archive per host as an asset of a GitHub
release, tagged `<version>-anti.<build>` as in `23.1.1-anti.7`. Beside the archives stand `SHA256SUMS` and its signature
`SHA256SUMS.sig`. The recipe, the hosts and the checks of each build are in that
repository. Each release holds the tools and clang of each of the six hosts. antic takes
the tools of its host from `bin/` of the archive above its own, by the rule of
`runtime_archive` in `src/antic/userdirs.c`, and a build of antic takes clang as well. The clang archive also
carries `libunwind.a` for the two glibc targets, which the build copies into
`lib/<target>/` of the runtime archive. The runtime of AddressSanitizer calls its
`_Unwind_Backtrace` and `_Unwind_GetIP` in a Linux program of `--memory-checks`. It
carries the profile runtime of compiler-rt for every target as well, which the build
copies into `lib/<target>/` and a program of `--profile-generate` links.

antic writes LLVM IR text, and opt and llc turn it into the object that lld links. Since
the step `switch` of `docs/work-order-llvm-back-end.md` they are the one back end, and no
build runs llvm-mc. llvm-mc leaves the set when a separate decision says so.

`23.1.1-anti.4` added opt and llc, for the LLVM back end. Each tools archive about
doubled, 177,317,004 bytes over the six hosts:

| Host | Tools archive, anti.3 | Tools archive, anti.4 | Growth |
|---|---|---|---|
| linux-x86_64 | 26,271,712 | 58,190,076 | 31,918,364 |
| linux-arm64 | 23,635,156 | 51,820,800 | 28,185,644 |
| macos-arm64 | 25,034,028 | 54,392,536 | 29,358,508 |
| macos-x86_64 | 28,161,820 | 61,934,492 | 33,772,672 |
| windows-x86_64 | 25,795,084 | 54,878,132 | 29,083,048 |
| windows-arm64 | 22,577,824 | 47,576,592 | 24,998,768 |

`23.1.1-anti.5` added `libunwind.a`, 137,622 bytes for x86_64 and 145,794 for ARM64,
which grew each clang archive by 57,788 to 74,916 bytes. Its tools archives differ from
those of anti.4 in `VERSION` alone. The sizes are in bytes, as shipped, from the reports
of `anti-lang/llvm-tools` of 2026-10-02 and 2026-10-03.

`23.1.1-anti.6` added `llvm-profdata` to each tools archive and the profile runtime of
compiler-rt to each clang archive, for `--profile-generate` and `--profile-use`. The
runtime of one target weighs 151,522 to 228,104 bytes. Every other file is that of
anti.5. The sizes are in bytes, as shipped, from the report of `anti-lang/llvm-tools`
of 2026-10-05:

| Host | Tools, anti.5 | Tools, anti.6 | Growth | Clang, anti.5 | Clang, anti.6 | Growth |
|---|---|---|---|---|---|---|
| linux-x86_64 | 58,207,088 | 59,504,208 | 1,297,120 | 31,964,636 | 32,123,632 | 158,996 |
| linux-arm64 | 51,819,892 | 53,010,396 | 1,190,504 | 28,936,004 | 29,099,632 | 163,628 |
| macos-arm64 | 54,411,340 | 56,111,556 | 1,700,216 | 30,882,556 | 31,063,076 | 180,520 |
| macos-x86_64 | 61,933,048 | 63,882,332 | 1,949,284 | 34,196,960 | 34,367,592 | 170,632 |
| windows-x86_64 | 54,855,680 | 56,460,784 | 1,605,104 | 30,522,700 | 30,704,892 | 182,192 |
| windows-arm64 | 47,578,316 | 48,903,092 | 1,324,776 | 27,164,136 | 27,337,428 | 173,292 |

`23.1.1-anti.7` gives every static musl tool of both Linux hosts a thread stack of 8 MiB
in its `PT_GNU_STACK` header, where musl gave each thread its small default. The ThinLTO
link of `mixed_work` crashed `ld.lld` of anti.6 on linux-arm64 in a worker thread. Each
of those binaries differs from its anti.6 copy in that one field. Besides `VERSION`,
every other file of every archive is that of anti.6.

`tools/llvm-pin` names the tag, the address of the release, the name of an asset and
the digest of the archive of each of the six hosts. `tools/clang-pin` names the archives
of clang in the same release. `tools/get-llvm.cmake` takes the
archive of the host into `build/deps/llvm`, and checks the digest of the pin, the line
of `SHA256SUMS` and the signature. A toolchain bump is a new release there and a new
pin here. Every package carries the LLVM tools of its platform, as "Binary
distribution" in `docs/decisions.md` decides, so an installer downloads none, and the
`tools/` of a package holds neither `llvm-pin` nor `llvm-version`. The packer copies
`bin/` of the runtime archive into the package of the machine's own host. For each
other host, `tools/get-llvm.cmake -DHOST=<host>` lays the archive of that host out the
same way under `build/deps/llvm-tools/<host>`, checked against the pin, the manifest
and the signature as the host's own. Step 1 of `./r` fills those five directories and
step 3 hands the directory to the packer as `TOOLS`. The macos-arm64 package that the
test `package_keys` builds from the tree grew from 20,533,160 bytes to 125,271,420
with the tools on 2026-10-09, since the four names of lld are four copies of 73 MB
that xz compresses one by one.

The public key that checks `SHA256SUMS.sig` with `openssl pkeyutl -verify` lives in the
installers, which anti-lang.com serves, and in `tools/keys/release.pem` of the repository,
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
| `raylib`, `pcre2`, `mbedtls`, `miniaudio` | The static library per target, with headers | The CMake build in `src/native/` |
| `musl` | The Linux sysroot per processor | `tools/get-sysroot.cmake` |
| `mimalloc` | The static library per Linux target and processor level, the C allocator of every program of musl | The CMake build in `src/native/` |

The runtime archive of a package holds, beside the runtime of each target and level in
`lib/<target>/<level>/`, its bitcode of full LTO in `bitcode/full/`. A release build
links the program and that bitcode through the LTO of lld by default, which Eddie decided
on 2026-10-07. The bitcode of ThinLTO, `bitcode/thin/`, stays in the build tree, so
`--lto thin` from a package names the archive it lacks. The bitcode of full LTO adds 11.5
MB to a package before compression and 0.82 MB after xz.

The headers of PCRE2, SQLite, Mbed TLS, miniaudio and raylib stand in
`include/<library>/` of the runtime archive and of every package, one directory each,
which is the directory a C compile names with `-I`. They serve `anti bind --clang` from a
package and a C program that links an Anti library and the native library it uses. The
build of Mbed TLS carries no CA bundle, so no `lib/cacert.pem` stands beside them yet.

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

Apple's SDK is never served here, since its licence allows no redistribution. A
program that names a framework takes its stubs from the Command Line Tools on a Mac. On
another host it takes them from a Mac the user owns, with `anti sdk export` there and
`anti sdk import` on that host. Zig's stubs of libSystem are ours to serve, and every
package carries them. The Windows targets link against the import libraries of the
mingw-w64 project and `ucrtbase.dll`, never against Microsoft's CRT and SDK, which may
not be redistributed. Every package carries those import libraries, and the two glibc
sysroots with their X11 and OpenGL packages. The Windows sysroots went in with the step
`mingw` of `docs/work-order-distribution.md`: `include/` with the headers of mingw-w64
and `lib/` with the import libraries that llvm-dlltool wrote from its `.def` files and
`clang_rt.builtins.lib` of the pinned clang, 18 MB for the two before compression. A
program of `--memory-checks` for windows-x86_64 runs with `clang_rt.asan_dynamic.dll`
of the runtime archive beside it, as the pinned clang built it, and that DLL needs
`VCRUNTIME140.dll` and the API sets of the UCRT, which the Visual C++ Redistributable
installs on the machine that runs the program. The glibc sysroots went in with the step
`glibc` of `docs/work-order-distribution.md`, copied as the runtime archive holds them,
with the runtime of both Linux targets against glibc in `lib/linux-<cpu>-glibc/`. A
program of `link linux`, a raylib program and a plugin host then link for Linux from any
host. The two add about 120 MB to a package before compression and 17 MB after xz.

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
`tools/keys/release.pem`.

### The release script

`./r` in the root of the repository makes a release. It reads the version from
`tools/version` and its entry from `CHANGELOG.md`. It runs the suite of the Mac
and the two sanitizer suites in an export of the commit, packs the six hosts,
writes the symbols archives beside them and checks the packages on both VMs. Each VM
installs the package of its host, builds with that install while its network is off
and then runs the suite. The
packages and the archives stand in `build/dist/packages` under the one `SHA256SUMS`
that the packer wrote, which it then signs in place with the release key. It tags
the commit and uploads the assets to a GitHub release. It uploads thirteen files and no signature. It
publishes the text of anti-lang.com and installs the result from outside. Anti uses no
CI, so no runner checks a release. `./r --dry-run` performs the
first five steps and prints what the rest would do. On a version that is a tag already
it warns where a real run refuses.

The site takes the two installers, the downloads page, the signature of the manifest
and the public key. That page names the version and the six packages, with the digest
and the release URL of each. It carries the fingerprint of the public key. Step 9
rsyncs the four files and reads the signature and the key back. Nothing binary reaches
the site.

`docs/work-order-release-script.md` holds the steps, and
`docs/decisions.md` holds the decisions under "The release script".

### After publishing a package

A package is accepted when a machine of that host installs it and links for every
target, with the network off after the download. The checks that a build on the
development Mac cannot make:

1. `curl -fsSL https://anti-lang.com/install.sh | bash` on the target machine, or
   `irm https://anti-lang.com/install.ps1 | iex` on Windows.
2. Turn the network off.
3. Build and run a raylib program and a plugin host for the host itself.
4. Cross-build a program for every other target.
5. Run a foreign Linux one under `qemu-x86_64` or `qemu-aarch64` when the machine has it.

Step 4 is the one that found the missing zlib in our own lld. The musl objects of Alpine
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
| `anti license --from <executable>` | Reads `anti_licenses` out of the binary by its marker and prints it without the markers and the build id, with the line of `licenses/sources.txt` after the text of each component the record names |
| `anti license --from-archive lib<name>.a` | Reads the licence fields from the copy of the `.antl` package header in a static archive, so a C project can produce its notice |

The runtime archive holds a `licenses/` directory with one file per component, written
by the CMake build. Beside them stands `sources.txt`, the record of the upstream source
of every pinned component. `anti license` reads both. Nothing is typed twice.

All five forms are built. Each prints the lines of a notice. A package has the line
`package <name> <version> <identifier>` and its `attribution` lines. Each distinct text
follows once, after the line `text for <names>` with the packages it covers. The line
`source <name> <version> <url>` of `sources.txt` follows the text of a component that
the record names.

The plain form reads the package it runs from: `LICENSE` in its root for `antic` and
`anti`, then `licenses/` with `sources.txt`. The version of a component is the one of the
record, and `-` where the record names none. The identifier is the SPDX expression that
the text of the component names. A text that holds the notices of more than one
licence, as the copyright file of a package of Ubuntu does, has `LicenseRef-<name>`.

`--project` builds the project as `anti build` does and takes its options. The notice
names the runtime, then musl and mimalloc where the program links them, every package of
`anti.lock`, the package of every bundled module the project imports, and the package of
the project last. Without `--notice` it prints the notice of one target, the one of
`--target` or the host. With `--notice` it writes `NOTICE.txt` for every target of the
build, beside whatever the build wrote. `anti build` writes the same file beside a
program and a shared library, from the project and not from the notice of the binary.

`--from-archive` reads the member `<name>.package.o` of the archive, which holds the copy
of the package header. It prints the package of the library with its identifier, its
attributions and its text.

## Obligations of a shipped program

Code emitted by `antic` is the user's code translated. The LLVM licence exception says
the same for the objects llc writes. The user's obligations toward Anti are whatever the Anti
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
| musl, in every program of musl | MIT | Include the copyright and permission notice with the binary |
| mimalloc, in every program of musl | MIT | Include the copyright and permission notice with the binary |
| CA bundle | MPL 2.0 | None. The file is loaded from the archive, not shipped |

A program that imports `anti.raylib` and `anti.miniaudio` owes nobody anything. A
program that imports `anti.net` or `anti.regex` owes an attribution, and `anti_licenses`
plus `NOTICE.txt` supply it. Every program of musl carries the notices of musl and
mimalloc in `anti_licenses` and `NOTICE.txt`, and a program of the glibc mode or of any
other target carries neither. This section is a statement of how the licences read, not legal advice.

The package itself carries glibc and the kernel headers in its glibc sysroots, whose
licences ask for the source. `licenses/sources.txt` of the package records, beside the
licence texts, the upstream source of every pinned component: the name of its licence
text, its version and the URL of the exact source package. For glibc, the kernel headers
and the X11 and OpenGL packages that is the `.dsc` of the Ubuntu source package, for
musl, Zig and each native library the release archive. The CMake build writes the file
from the pins, so nothing is typed twice. `anti license --from` prints the line of a
component after its text, as `source <name> <version> <url>`, and `NOTICE.txt` holds the
same, so a program of musl names the source of musl and mimalloc with their notices.
The plain form of `anti license` prints every line of the record, each after the text of
its component.

## Licence choices for Anti itself

- The thread-pool runtime and the standard library: 0BSD, with a `LICENSE` file in
  `src/rt/` and in `src/std/`. Use without attribution, so the common case embeds one short
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
