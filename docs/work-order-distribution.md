# Work order: the binary distribution

The target of what Anti delivers stands under "Binary distribution" in
`docs/decisions.md`, recorded on 2026-09-27. Items 27 to 37 of "First
sessions" in `CLAUDE.md` and the gap list of
`docs/reports/2026-09-27-distribution-target.md` name what the packages
and the installers still do differently. This work order turns that list
into steps for `drive-dist.sh`, one headless session each. Every decision
in it is taken. A sentence of it that contradicts the code as it is, an
existing test or an entry of `docs/decisions.md` is a mistake of this
document, not a decision. The session keeps the documented behaviour,
corrects the sentence in the same commit and says so in its report.

The one aim: from any one of the six packages, a user builds programs for
all six targets with the network off. The installer downloads that package
and nothing else.

## What stands today, file by file

Checked against the tree at `feab21fb` on 2026-10-08.

- The runtime archive that the CMake build writes, `build/host/runtime`,
  holds `bin/` with the twelve pinned LLVM tools and `llvm-version`,
  `lib/<target>/` with the runtimes per level and the native libraries,
  `std/`, `licenses/` with one text per component, and `sysroot/` with
  eight sysroots: musl and glibc for both Linux targets, Zig's stubs for
  both macOS targets, and the trees of xwin for both Windows targets. The
  Windows trees are 630 MB and 857 MB, since xwin fetches the whole SDK.
- `tools/pack-anti.cmake` lays the package out as `bin/` with antic and
  anti alone, `lib/`, `std/`, `licenses/`, `sysroot/` with the two musl
  sysroots and `usr/` of the two macOS ones, and `tools/` with
  `get-sysroot.cmake`, `sysroot-pins`, `zig-stubs-pin`, `cmake-pin`,
  `cmake-version`, `llvm-version`, `llvm-pin` and `package-api`. It copies
  no LLVM tool, no glibc sysroot and no Windows sysroot.
- `tools/install.sh` and `tools/install.ps1` download the package and
  `SHA256SUMS` from the GitHub release and `SHA256SUMS.sig` from
  anti-lang.com, check them with openssl, unpack, and then install the
  LLVM tools from `anti-lang/llvm-tools` and the sysroot of the host with
  CMake and `tools/get-sysroot.cmake`. `ANTI_MICROSOFT` lets xwin fetch
  the Microsoft CRT and SDK. `install.sh` is 452 lines, `install.ps1` 413.
- `tools/get-sysroot.cmake` installs the sysroots from `tools/sysroot-pins`.
  musl comes from Alpine, glibc 2.35 with the kernel headers and the X11 and
  OpenGL packages from Ubuntu 22.04, and the macOS stubs from Zig. The
  Windows trees come through xwin under `ACCEPT_LICENSE=yes`.
- `src/antic/linker.c` links a Windows program against `msvcrt.lib`,
  `ucrt.lib`, `libvcruntime.lib` and `legacy_stdio_definitions.lib` of the
  xwin tree, and a program of `--profile-generate` against `libcmt.lib`,
  since compiler-rt builds its profile runtime of Windows against the
  static C runtime. `/NOENTRY` is given where the entry is antic's own,
  from `src/rt/start.c`.
- `src/antic/driver_link.c` names `tools/get-sysroot.cmake` in two
  messages, at lines 108 and 168 of `feab21fb`. The entries that fell back
  on an lld of the search path and on `LIB` are replaced in
  `docs/decisions.md`. Whether the code still falls back is for the step
  `own-tools` to check with `grep` and a test.
- `--bundle-runtime` joins the library object and the runtime members into
  one relocatable object: `ld.lld -r` on ELF, `coff_join` of
  `src/antic/coff.c` on COFF, and Apple's `ld -r` on Mach-O, which is the
  one host tool left in a build. ld64.lld 23.1.1 writes no relocatable
  object.
- `anti license --from <executable>` is the one form of five that
  `docs/distribution.md` describes under "License command". `anti build`
  writes `NOTICE.txt` from the notice of the binary.
- `anti sdk export` and `anti sdk import` exist in `src/anti/sdk.c`.
- The pinned clang of `build/deps/clang` carries `clang_rt.builtins.lib`
  for `x86_64-pc-windows-msvc` and `aarch64-pc-windows-msvc`, and the
  ASan and profile runtimes for x86_64 Windows.
- `./r`, `tools/release.sh`, runs the eleven steps of
  `docs/work-order-release-script.md`. Step 5 installs each package on a VM
  with its installer, which today reaches the network for the LLVM tools
  and the sysroot.

## The package after this work order

```text
anti/
  bin/        antic, anti, and the LLVM tools of the host: llvm-mc, lld,
              ld.lld, ld64.lld, lld-link, llvm-ar, llvm-objdump,
              llvm-readobj, llvm-profdata, opt, llc, and llvm-version
  lib/<t>/    the runtime of every target per level, the native libraries,
              the bitcode of full LTO, the profile runtime, libunwind
  include/    the headers of the native libraries, one directory each
  std/        the standard library
  sysroot/    linux-x86_64, linux-arm64, their -glibc twins, macos-arm64,
              macos-x86_64, windows-x86_64, windows-arm64
  licenses/   one text per component, and sources.txt
  LICENSE
  VERSION
```

No `tools/` directory. Nothing in the package is a script that an installer
runs. The tools of `bin/` are the ones the runtime archive holds, copied as
they are. The `sysroot/` directory holds what the runtime archive holds,
copied as it is, once the Windows trees come from mingw-w64.

## Decisions taken in this work order

Each of these settles a point the gap list left open. Eddie decided them on
2026-10-08 by accepting this work order.

1. **The tools travel in `bin/`**, beside antic and anti, not in a
   directory of their own. antic finds them beside itself, as it finds them
   in `build/host/runtime/bin` today. `llvm-version` travels with them, so a
   package names the pin of its tools. `antic --version` prints the one line
   `antic <version>` as before, which the test `antic_version` and steps 3 and
   10 of `./r` read.
2. **The record of copyleft sources is `licenses/sources.txt`**, one line
   per component of the package whose licence asks for source. A line
   holds the component, the version, and the URL of the exact upstream
   source package. glibc and the kernel headers are the two that need it. The file
   lists every component of `sysroot-pins` all the same, since the line
   costs nothing and a reader then has one list. The CMake build writes it
   from `tools/sysroot-pins` and the pins of the native libraries, as it
   writes the licence texts, so nothing is typed twice.
3. **The headers go into `include/<library>/`** of the runtime archive and
   the package: `pcre2`, `raylib`, `miniaudio`, `mbedtls` and `sqlite3`.
   `lib/cacert.pem` goes in beside them if the build of Mbed TLS carries
   one, and the step records where it comes from. The headers serve
   `anti bind --clang` from a package and a C program that links an Anti
   library and the native library it uses.
4. **The Windows sysroot is mingw-w64**, pinned by version and digest of
   its source release in `tools/sysroot-pins`. It holds the headers of
   `mingw-w64-headers` and import libraries that `llvm-dlltool`, which is
   `llvm-ar` under that name, writes from the `.def` files of
   `mingw-w64-crt` for `ucrtbase.dll`, `kernel32.dll`, `ntdll.dll`,
   `user32.dll`, `dbghelp.dll` and every other DLL the runtime, raylib,
   miniaudio and the tests name. The build writes them for both Windows
   targets on every host. The compiler helpers come from
   `clang_rt.builtins.lib` of the pinned clang for the target, which goes
   into `lib/` of the sysroot as the builtins of the Linux targets do. The
   stack probe `__chkstk` is not among them: the builtins of the pinned
   clang hold none, so the runtime defines it, with the entry point, the
   directory of thread-local storage, `_fltused` and the printf family,
   which the static libraries of Microsoft gave. The SEH entry point
   `__C_specific_handler` comes from `kernel32.dll` on x86_64 and from
   `ucrtbase.dll` on both processors, as the `.def` files of mingw-w64
   have it. The step `mingw` corrected the two sentences that named the
   builtins and `ntdll.dll`. The C of the gnu triples compiles with no
   `-g` option, as before: `-gcodeview` alone adds the records of every
   function and the build information with the paths of the machine to an
   object, which the tests `dead_code` and `no_paths` refuse, and the step
   corrected that word as well.
   The triples stay `x86_64-pc-windows-msvc` and `aarch64-pc-windows-msvc`,
   so CodeView, the PDB and lld-link stay as they are. The runtime, the
   native libraries and the tests compile against the mingw-w64 headers
   under those triples. If a header of mingw-w64 refuses the msvc triple,
   the C of the runtime and the native libraries compiles under
   `x86_64-w64-windows-gnu` and `aarch64-w64-windows-gnu` with `-gcodeview`,
   and the text of antic keeps the msvc triple. Both are COFF of the same
   calling convention and link together. The step records which of the two
   held.
5. **`--memory-checks` and `--profile-generate` on Windows keep the
   runtimes of the pinned clang**, `clang_rt.asan_dynamic` and
   `clang_rt.profile`, as they are built. They are the sanitizer's and
   compiler-rt's, not Microsoft's. The step records which DLLs a program of
   `--memory-checks` needs on the machine that runs it, and
   `docs/distribution.md` says so. If the profile runtime cannot link
   without `libcmt.lib`, `--profile-generate` for a Windows target is
   refused with a message that names the reason, and the entry under
   "Compiler behaviour" on profiles says so. A refusal is a documented
   state, a silent dependency on Microsoft's libraries is not.
6. **`--bundle-runtime` on Mach-O writes an archive, not one object.** The
   archive holds the library object, every runtime member except the one of
   `src/rt/start.c`, and one marker member that defines
   `anti_rt_bundle_<package>`, which the library object references. Two
   bundled libraries in one program then define the marker twice, and ld64
   reports a duplicate symbol, as the one object does on ELF and COFF. The
   entry on `--bundle-runtime` in `docs/decisions.md` records the Mach-O
   form and its reason: ld64.lld 23.1.1 writes no relocatable object, and
   a Mach-O joiner of our own is out of proportion to the feature. Apple's
   `ld -r` leaves the code.
7. **The installers download the package alone.** `install.sh` and
   `install.ps1` download the package of the host and `SHA256SUMS` from the
   GitHub release and `SHA256SUMS.sig` from anti-lang.com, check the
   signature with the key they carry, check the digest, unpack into the
   install directories already decided, and put `bin/` on the path. They
   run no CMake, install no sysroot and know no `ANTI_MICROSOFT`. The
   check of an install is `anti --version` and `antic --version`, both from
   the package. The downloads page shows the same as commands.
8. **The network-off test is step 5 of `./r`.** On each VM the installer
   installs the package from the staging area. Then the VM's network goes
   off for the check. A raylib program and a plugin host for the VM's own
   target build and run, and a hello program cross-builds for every other
   target. The network comes back for the suite, which fetches nothing but
   is long. `docs/vm-setup.md` says how each VM turns its network off and
   on from a script.
9. **The order of the steps is the order below.** The LLVM tools first,
   since every later test of a package needs them. mingw-w64 before the
   installers, since the installers lose xwin with it.

## Steps

Each step is one headless session. Each ends with the three suites on the
Mac, the suite on both VMs where the step says so, a report
`docs/reports/<date>-dist-<step>.md`, and a push. A step that changes
`tools/pack-anti.cmake`, `tools/release.sh` or an installer keeps the test
`release_dry_run` passing, which runs `./r --dry-run` over stand-ins inside
the host suite. The real `./r --dry-run` is Eddie's. It needs a pushed
`main` and runs longer than one call of a session allows, so no step runs
it. Eddie runs it after the step `release-check`, before `./r`.

`tools`. The LLVM tools into the package. `tools/pack-anti.cmake` copies
`bin/` of the runtime archive into `bin/` of the package, beside antic and
anti, and `VERSION` into the root. The runtime archive holds the tools of
the machine alone, so for every other host the packer takes
`build/deps/llvm-tools/<host>/bin`, which `tools/get-llvm.cmake
-DHOST=<host>` lays out the same way and step 1 of `./r` fills. antic and
anti find the tools beside themselves. A test unpacks a package built from the tree and runs `antic
--version`, `anti --version` and a hello program for the host with the
package alone on the path and `PATH` otherwise empty of LLVM tools. The
installers stop installing the LLVM tools, and `tools/llvm-pin` and
`llvm-version` leave the package's `tools/`. `docs/distribution.md` under
"The LLVM tools" and the "Binary distribution" entry on the six packages
say that the tools travel inside. Done when the test passes on the Mac and
the suites pass.

`own-tools`. antic and anti use the package alone. `grep` finds every
place where antic or anti reads `PATH`, `LIB` or another variable of the
environment to find a tool, a library or a sysroot, and every message that
names `tools/get-sysroot.cmake`. The fallbacks go, except `--linker
platform` and `anti bind --clang`, which the "Binary distribution" entry on
host tools keeps. A message that named `get-sysroot.cmake` names the
package and `anti sdk import` instead. A test runs antic with an empty
`PATH` and no `LIB` and links a program for every target that needs no
framework. The package holds no Windows sysroot until the step `mingw`, so
the test stands in the Windows sysroots of the runtime archive at the place
the package will hold them. Done when the test passes on the Mac and both
VMs.

`glibc`. The two glibc sysroots into every package. `tools/pack-anti.cmake`
copies `sysroot/linux-x86_64-glibc` and `sysroot/linux-arm64-glibc` of the
runtime archive, with their X11 and OpenGL development files and the glibc
runtime. A test links a program of `link linux` and a raylib program for
both Linux targets from an unpacked package on the Mac. The packages grow
by about 100 MB each, which the report records per host. Done when the
test passes and the suites pass.

`sources`. `licenses/sources.txt`, as decision 2 says. The CMake build
writes it into the runtime archive beside the licence texts, from
`tools/sysroot-pins`, `tools/raylib-pin`, `tools/mbedtls-pin`,
`tools/miniaudio-pin` and the pins of PCRE2, SQLite, musl and mimalloc.
The packer copies it. `anti license` prints the line of a component after
its text. A test reads the file of a package and checks that glibc and the
kernel headers have a line with a URL that names the version of
`sysroot-pins`. `docs/distribution.md` under "Obligations of a shipped
program" names the file. Done when the test passes.

`headers`. The headers of the native libraries into `include/<library>/`
of the runtime archive and the package, as decision 3 says, and
`lib/cacert.pem` if the build of Mbed TLS carries one. `tests/repo_layout`
is not touched, since `include/` is a directory of the archive and not of
the tree. A test runs `anti bind --clang` on `include/raylib/raylib.h` of
an unpacked package with the pinned clang and compares the module it writes
with `anti.raylib` as the `anti` of the tree writes it from `raylib.h` of the
pinned raylib source, the module that the test `anti_bind_raylib` compiles.
The tree holds no file of `anti.raylib`, since "Bindings" in
`docs/decisions.md` has `anti bind` write it. Done when the test passes and item 16 and
item 31 of "First sessions" in `CLAUDE.md` read "Done".

`mingw`. The Windows sysroot of mingw-w64, as decision 4 says. In order:
pin the source release in `tools/sysroot-pins` with its digest.
`tools/get-sysroot.cmake` unpacks the headers and writes the import
libraries with `llvm-dlltool` for both Windows targets, and copies
`clang_rt.builtins.lib` of each target from the pinned clang into the
sysroot. `src/antic/linker.c` links a Windows program against the import
libraries of `ucrtbase.dll`, `ntdll.dll`, `kernel32.dll` and the DLLs the
program names, and `clang_rt.builtins.lib`, and no longer against
`msvcrt.lib`, `ucrt.lib`, `libvcruntime.lib` or
`legacy_stdio_definitions.lib`. The runtime, the native libraries and the
tests compile against the mingw-w64 headers. Test first on the Windows VM:
a hello program, a program with a stack trace, a raylib program, a plugin
host with a plugin, a program of `--memory-checks` and one of
`--profile-generate`, each built against the new sysroot, run and give what
they give today. Then the suite on the Windows VM, and the cross-build of
both Windows targets from the Mac and from anti-linux. Decision 5 says what
holds for the two runtimes of compiler-rt. The report records the size of
the two Windows sysroots before and after, which of the two triples of
decision 4 held for the C of the runtime, and the DLLs a program of
`--memory-checks` needs. Done when the suite passes on all three machines
with the new sysroot and the xwin trees are no longer read anywhere.

`no-xwin`. xwin leaves `tools/get-sysroot.cmake`, `tools/sysroot-pins`,
`tools/install.sh`, `tools/install.ps1`, `ANTI_MICROSOFT`, `CMakeLists.txt`
and every document. `grep -ri xwin` over `src/`, `tests/`, `tools/`,
`docs/notes/`, `docs/site/`, `docs/*.md` and `CLAUDE.md` finds nothing but
history in `docs/reports/` and `docs/decisions.md`. Done when the three
suites pass and the grep finds nothing.

`installers`. The installers download the package alone, as decision 7
says. `install.sh` and `install.ps1` lose the LLVM tools, the sysroot, CMake
and xwin, and `tools/` leaves the package in `tools/pack-anti.cmake`.
`tools/downloads.html.in` shows the manual install with the same checks as
commands, and `docs/distribution.md` under "Binary downloads" and "What the
site serves" says what the installer does now. The test `release_dry_run`
follows the installers. Step 5 of `./r` and `docs/work-order-release-script.md`
lose the lines about the LLVM tools and the sysroot. Done when the suites
pass and an install from the staging area on both VMs, with the network on,
gives `anti --version` and a hello program for the host.

`bundle-macho`. `--bundle-runtime` on Mach-O as decision 6 says. Apple's
`ld -r` leaves `src/antic/driver_library.c`, and the marker member and the
archive take its place for the two macOS targets. The tests
`clib_bundle_<target>` of both macOS targets build and run their C program
against the archive, and a new test links two bundled libraries into one C
program and expects the duplicate symbol. The entry on `--bundle-runtime`
in `docs/decisions.md` records the Mach-O form. Done when the three suites
pass and item 35 reads "Done".

`offline`. The network-off test of a release, as decision 8 says.
`docs/vm-setup.md` gains the two commands that turn each VM's network off
and on from the Mac, over the SSH session that stays up. Step 5 of
`tools/release.sh` installs the package from the staging area, turns the
network off, builds and runs a raylib program and a plugin host for the
VM's target, cross-builds a hello program for every other target, turns the
network on, and runs the suite. `release_dry_run` follows. The step runs
the real step 5 once against a package built from the tree, with the
network off on both VMs, and the report holds what each VM printed. Done
when that run passes and `docs/work-order-release-script.md` describes the
step.

`license-forms`. The four forms of `anti license` that "License command"
in `docs/distribution.md` describes beside `--from`: the plain form,
`--project`, `--project --notice` and `--from-archive`. `anti build` writes
`NOTICE.txt` from `anti.lock` and the imported bundled modules, as
`--project --notice` prints it, and no longer from the notice of the
binary. The plain form reads `licenses/` of the package, `sources.txt`
included. Test first, one test per form, with a project that links two
packages and a static archive built with `--lib static`. Done when the
tests pass on the Mac and both VMs and item 37 reads "Done".

`release-check`. The last step builds nothing new. It packs the package of
each host from the tree, installs it on its machine with the installer from
the staging area, and runs the check of step 10 of the release script by
hand on the Mac and both VMs: `anti --version`, `antic --version`, a hello
program for the host and a link for the other five targets, with the network
off. It writes the `CHANGELOG.md` entry of 0.2.0 from the reports since
0.1.0, in the form of the entry of 0.1.0, and sets `tools/version` to 0.2.0.
It does not tag and does not run `./r`. Done when the three checks pass and
`release_dry_run` passes with the new version. Eddie then runs `./r --dry-
run` and `./r`.

## Documentation changes

The step that makes a change makes its documentation change in the same
commit, in the words of the decisions.

- `docs/decisions.md`: the entries under "Binary distribution" gain
  "built in the step <step>" where a step completes them. The entry on
  `--bundle-runtime` gains the Mach-O form. The entries on the installer
  and on the test of a release say what the steps `installers` and
  `offline` made. The gap list sentence under the heading goes once every
  step is done.
- `CLAUDE.md`: items 27 to 37 of "First sessions" read "Done" with the
  report that did them, one by one. The "Distribution" section says what
  the package holds and that the installers download it alone.
- `docs/distribution.md`: six sections describe the state after the step
  that changes them. They are "Binary downloads", "What the site serves",
  "The LLVM tools", "What the components will hold", "Obligations of a
  shipped program" and "License command".
- `docs/work-order-release-script.md`: steps 3, 5 and 10 as the steps
  `installers`, `offline` and `release-check` leave them.
- `docs/vm-setup.md`: the network commands of each VM.
- `docs/notes/linker.md`: the Windows link against mingw-w64 and the
  Mach-O form of `--bundle-runtime`.
- `docs/reports/2026-09-27-distribution-target.md` stays as written. It
  is history.

## Questions an implementer asks

| Question | Answer |
|---|---|
| Where do the LLVM tools go in the package? | `bin/`, beside antic and anti, copied from `bin/` of the runtime archive. Decision 1. |
| Does the package keep a `tools/` directory? | No. The step `installers` removes it. Until then it holds what it holds today. |
| Which sysroots does the package hold? | All eight of the runtime archive: musl and glibc for Linux, Zig's stubs for macOS, mingw-w64 for Windows. The Apple SDK stubs never. |
| What is the record of copyleft sources? | `licenses/sources.txt`, one line per component: name, version, URL. Decision 2. |
| Where do the headers go? | `include/<library>/` of the runtime archive and the package. Decision 3. |
| Which triple compiles the runtime for Windows? | The msvc triple, with the mingw-w64 headers. If a header refuses it, the gnu triple with `-gcodeview` for the C alone. Decision 4. |
| Where do `__chkstk` and `__C_specific_handler` come from? | `clang_rt.builtins.lib` of the pinned clang, and the import library of `ntdll.dll`. Decision 4. |
| What about the ASan and profile runtimes on Windows? | They stay as built. The report lists the DLLs a program needs. `--profile-generate` is refused if it cannot link without `libcmt.lib`. Decision 5. |
| How does `--bundle-runtime` work on Mach-O? | An archive with a marker member, so two bundles are a duplicate symbol. Decision 6. |
| What does the installer do? | Download the package, check the signature and the digest, unpack, put `bin/` on the path. Nothing else. Decision 7. |
| Does the installer need CMake? | No. Nothing a user runs needs CMake. |
| How is the network turned off on a VM? | By the commands the step `offline` adds to `docs/vm-setup.md`, from the Mac over SSH. Decision 8. |
| Which tests get re-pinned? | None. The identity tests read antic's output, which no step changes. |
| Does a step add a directory under `src/`, `tests/` or `docs/`? | No. `include/` is a directory of the runtime archive and of the package, not of the tree. |
| What if a sentence here contradicts the code? | The code, the tests and `docs/decisions.md` win. Correct the sentence in the same commit and say so in the report. |
| When is BLOCKED right? | For a question no document answers. Not for a contradiction, and not for a choice this document makes. |
| Who runs `./r --dry-run` and `./r`? | Eddie, after the step `release-check`. No session runs either: the dry run needs a pushed `main` and runs longer than one call allows. |

## Later options, out of scope

- A Mach-O joiner of our own, if the archive form of `--bundle-runtime`
  proves a problem for a C project.
- Packages per target instead of per host, if the size of a package with
  every sysroot becomes a complaint.
- The sanitizer runtime of Windows built against mingw-w64 in
  `anti-lang/llvm-tools`, so a program of `--memory-checks` depends on no
  DLL but Windows' own.
