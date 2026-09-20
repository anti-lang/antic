# The release script

`./r` in the root makes a release. It is a link to `tools/release.sh`, one file
of shell, and it performs the eleven steps of
`docs/work-order-release-script.md`. This session built it, its test, and the
two files it reads: `tools/version` and `CHANGELOG.md`.

## What each step does

1. Preflight. The host is the Mac, the version is no tag here and none on
   origin, `CHANGELOG.md` holds its entry, `main` is committed and pushed, `gh`
   is logged in, both VMs answer, and `build/CMakeCache.txt` names the four
   downloads. It runs on every call, because it is a gate and not an output.
2. The suite. `git checkout-index` writes the commit to `build/dist/export`,
   which is configured with those paths, built and run. The two sanitizer
   presets follow in the same export.
3. The packages. `tools/pack-anti.cmake` writes the six. Each is read back:
   `tools/check-cpu.cmake` against `tools/cpu-levels`, `tools/check-libc.cmake`
   on both programs of both Linux packages, and `antic --version` of the macOS
   x86_64 package under Rosetta.
4. The symbols. `llvm-objdump --syms` of every shipped program goes into
   `build/dist/symbols/`. A package that carries an archive of its own stops
   the run.
5. The VMs. The tracked files go over as a tar. Each VM configures, builds and
   runs the full suite. It installs the package of its host with the installer
   of its shell, prints the version of both programs and uninstalls. Linux
   compiles and runs a program too.
6. The digests. `SHA256SUMS` over the twelve files, signed with the key that
   `RELEASE_KEY` names. Without the key the run prints the two signing commands
   and stops. A signature made by hand afterwards is taken when it verifies.
7. The tag and the release. A signed tag, a draft release with the changelog
   entry as its body, the fourteen assets one at a time, then published.
8. The matrix. One run of `test.yml` on the tag, watched to the end. A failure
   leaves the release as a pre-release.
9. The site. `downloads/index.toml` in the checkout that `ANTI_SITE` names.
10. The check from outside. The installer from anti-lang.com writes a fresh
    directory. Both programs print the version. A program compiles and runs for
    this host, and links for every target whose sysroot the install carries.
11. The report, committed and pushed as the last commit of the release.

Every step writes a stamp in `build/dist/state/`, and a rerun starts at the
first one that is missing. The state names the commit it ran on, and a run on
another commit starts over. A dry run writes under `build/dist/dry-run/`.

## The files

- `tools/release.sh` and the link `r`.
- `tools/version`, the one place the version is written. `CMakeLists.txt` reads
  it before `project()`, and `tools/pack-anti.cmake` reads it for the name of
  every package. `antic_version` compares what antic prints with
  `PROJECT_VERSION`, so a second spelling of the version fails the suite.
- `CHANGELOG.md`, with the entry of 0.1.0.
- `tools/check-cpu.cmake`, which reads a package against `tools/cpu-levels` and
  refuses one whose levels differ in either direction.
- `ANTI_BASE` in `tools/install.sh` and `tools/install.ps1`. The PowerShell
  installer copies rather than fetches when the base names a directory.

## The test

`release_dry_run` runs `./r --dry-run` over a copy of the staged tree with the
version bumped to 99.0.0. The copy gets a history of one commit and a bare
origin, so the preflight sees a pushed `main`. `cmake`, `ctest`, `gh`, `ssh` and
`scp` are stood in for on the PATH by scripts that write what the real ones
write. One packs six small archives, with a real x86_64 program in the macOS
package for the check under Rosetta.

The test reads the stamps of steps 2 to 5, the six packages, the six symbols
archives, the five logs, the plan printed for steps 6 to 10 and the absence of a
signature. It drives three refusals of the preflight: a version that is a tag,
an uncommitted change and a missing entry in the changelog. A run with
`build_symbols` taken out of the driver fails it. `tools/check-cpu.cmake` is
checked for real in the same test, on a package tree with every level and on one
without `armv8.2` of `linux-arm64`.

## What the dry runs found

Three defects, each on a machine or in a build that no earlier run reached.

1. `cpu_check_refuses` failed in the export of step 2 and passed in `build/` of
   the same commit. `tests/CMakeLists.txt` read `${ANTIC_LLVM_MC}` 52 lines
   above the `find_program` that writes it. A build directory that an earlier
   run filled holds the cache entry before the first line, so the value is right
   there. A fresh one takes an empty string, and the test ran antic with no
   assembler. The search moved to the top of the file. The test `find_order`
   refuses a read of a `find_program` variable above its call. It reads
   `CMakeLists.txt` and `tests/CMakeLists.txt` in the order CMake does. It found
   one more name on its first run, `ANTIC_LLVM_AR`, which the top-level file
   finds before `add_subdirectory(tests)` and is sound.
2. The Linux VM did not build `tests/unit/test_cpu.c`. `setenv` and `unsetenv`
   are POSIX and stand outside the C11 library, so musl and glibc hide them
   under `-std=c11` while the headers of Apple declare them anyway. The file
   defines `_POSIX_C_SOURCE`, as `src/process.c` does. The pinned clang with the
   musl sysroot reproduces it on the Mac in one command.
3. `cpu_level_armv8.0` failed on the Linux VM. The library it read is the one
   that build compiled for its own host, at the optimisation of that build. A
   Debug build writes a call where a release writes the load-store exclusive
   loop. The Mac builds no host library, so all three of its targets are cross
   builds, and the test passed there. The test now skips the library of the host
   target on a host that builds one.

Two decisions came out of the same runs. A static musl program carries the
debug sections of the sysroot's own objects. Their strings name `__libc_malloc`
and no source of antic. A section header therefore tells no release build from
a debug one. Step 4 reads the symbol table instead. A Windows program has none.
lld-link writes the symbols to a PDB and the packer asks for none. The archive
of a Windows host holds the map of the sections.

## The VMs

Both answered. The Linux VM keeps its downloads in `~/anti` and its tree in
`~/antic-check`, and the script writes the same options that
`~/vm-linux-build.sh` there uses. The Windows VM answers to `cmd` rather than to
a shell, so the script makes its directory with the commands of `cmd` and sends
a command file with CRLF endings, which `vcvarsall.bat arm64` opens. The
uninstallers travel with the installers, because a package carries neither.

## The first dry run

`./r --dry-run` on `2930e2a`, after the three fixes above. It ran steps 1 to 5
and printed a plan for the rest. Every count is of the export of that commit,
and the two VMs ran the suite of the same tree.

```text
r: Anti 0.1.0, a dry run. Nothing is signed, tagged or uploaded.
r: step 1, preflight
  0.1.0 is no tag here and none on origin
  CHANGELOG.md holds the entry of 0.1.0
  main is committed and pushed at 2930e2a
  gh is logged in
  anti-linux answers
  anti-windows answers
  the state is of d5efaa4, so the steps run again
  the downloads of build/ are in place
r: step 2, the suite of the Mac, then ASan and UBSan
  the commit is exported to build/dist/dry-run/export
  the Mac: 100% tests passed out of 480
  asan: 100% tests passed out of 479
  ubsan: 100% tests passed out of 479
r: step 3, the six packages
  anti-0.1.0-macos-arm64.tar.xz, 06be375b813c, levels of tools/cpu-levels
  anti-0.1.0-macos-x86_64.tar.xz, dc9c8c9436dd, levels of tools/cpu-levels
  anti-0.1.0-linux-x86_64.tar.xz, 964755ed30d4, levels of tools/cpu-levels
  anti-0.1.0-linux-arm64.tar.xz, be0c3f6d2a3f, levels of tools/cpu-levels
  anti-0.1.0-windows-x86_64.tar.xz, c9e07e1ef7b7, levels of tools/cpu-levels
  anti-0.1.0-windows-arm64.tar.xz, 367ea93c1b8a, levels of tools/cpu-levels
  linux-x86_64: both programs link the pinned sysroot
  linux-arm64: both programs link the pinned sysroot
  macos-x86_64 under Rosetta: antic 0.1.0
r: step 4, the symbols of the twelve programs
  anti-0.1.0-macos-arm64-symbols.zip, 00e568fa63c4
  anti-0.1.0-macos-x86_64-symbols.zip, aaa9e53c03d5
  anti-0.1.0-linux-x86_64-symbols.zip, 5a7b7b87a27d
  anti-0.1.0-linux-arm64-symbols.zip, 29bc49348f5f
  windows-x86_64: antic.exe has no symbol table, so its sections go in
  windows-x86_64: anti.exe has no symbol table, so its sections go in
  anti-0.1.0-windows-x86_64-symbols.zip, e6f3ae05eb3b
  windows-arm64: antic.exe has no symbol table, so its sections go in
  windows-arm64: anti.exe has no symbol table, so its sections go in
  anti-0.1.0-windows-arm64-symbols.zip, fcf1ca34070d
r: step 5, the two VMs
  anti-linux: 100% tests passed, 0 tests failed out of 419
  anti-linux: the package installs, compiles a program and uninstalls
  anti-windows: 100% tests passed out of 399
  anti-windows: the package installs and uninstalls
r: step 6, the digests and the signature
  would write SHA256SUMS of 12 files in build/dist/dry-run
  would sign it with $RELEASE_KEY into SHA256SUMS.sig
r: step 7, the tag and the release
  would tag v0.1.0 on 2930e2a and push it
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

The index of step 9 stands in `build/dist/dry-run/index.toml`. Its head:

```toml
# The published packages of Anti. The release script of
# antic writes the entries, and the build of the site
# publishes them.

[anti]
version = "0.1.0"
released = "2026-09-20"
base = "https://anti-lang.com/downloads/resources/anti/0.1.0"
release = "https://github.com/anti-lang/antic/releases/download/v0.1.0"
key_fingerprint = "7e64c56e26a42946823a66aa1f30bf686b6b5dbd0dc0e2c165a080540ffc3eca"

[anti.macos-arm64]
file = "anti-0.1.0-macos-arm64.tar.xz"
sha256 = "06be375b813c3c90889677f7c57f617b04f45218354daf8b518303d758e9551e"
url = "https://github.com/anti-lang/antic/releases/download/v0.1.0/anti-0.1.0-macos-arm64.tar.xz"

```

## Questions

1. Where is the site's repository? Step 9 writes `downloads/index.toml` into the
   checkout that `ANTI_SITE` names, and refuses without it. The name and the
   address of that repository are written nowhere here.
2. A Windows program carries no symbol table, so its archive holds the map of
   the sections. A PDB per Windows host would hold the functions, and it changes
   what a release build emits. That is a build configuration, so the answer is
   yours.
3. `RELEASE_KEY` names the encrypted private key. The one of
   `anti-lang/llvm-tools` lies in `keys/private/release-key.enc.pem` of that
   repository. Should `./r` default to it?
4. Step 8 runs the matrix after the release exists, so a red matrix leaves a
   published pre-release rather than nothing. Is that the order you want?
5. Step 5 installs on each VM with `ANTI_BASE` pointing at a directory of that
   machine. The two installers gained that variable for it. A release checks the
   installer of each shell, the package and the uninstaller. The download from
   anti-lang.com is checked by step 10, on the Mac alone.
