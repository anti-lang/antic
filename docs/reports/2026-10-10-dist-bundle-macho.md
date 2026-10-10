# The bundled runtime of Mach-O is an archive

The step `bundle-macho` of `docs/work-order-distribution.md`, on 2026-10-10.
It builds decision 6 of that work order: `--bundle-runtime` for the two macOS
targets writes an archive with a marker member, and Apple's `ld -r` left the
code. The commit is `725d07f0`.

## What the step built

- `bundle` of `src/antic/driver_library.c` hands the members of a bundled
  macOS library to llvm-ar instead of to a join. The archive holds the
  library object, `anti_rt_license_stub.o`, every member of `libanti_rt.a`
  except `start.o` and `license.o`, the marker `<name>.bundle.o` and
  `<name>.package.o`. ELF still joins with `ld.lld -r` and COFF with
  `coff_join`.
- `link_relocatable_command` of `src/antic/linker.c` lost its Mach-O form,
  `ld -r -keep_private_externs -arch`. `driver_run` asks for a linker for a
  bundle of ELF alone, so a bundled macOS library needs llvm-ar, opt and
  llc and no sysroot.
- `llvm_emit_bundle_marker` of `src/antic/llvm_emit.c` writes the two lines
  of the marker object. `roots` of the same file writes the reference of
  the library object, a private constant `@anti.bundle` that `llvm.used`
  keeps. The option `bundle` of the emitter is set for a bundled Mach-O
  static library alone, so the text of every other build is unchanged.
- Every host now writes a bundled library of both macOS targets. Before
  the step a Mac alone did, since the join ran `ld` of the search path.

The work order names `driver_library.c` alone. The reference of the library
object and the text of the marker needed the two functions of
`llvm_emit.c`, which stand beside `llvm_emit_package`.

## The marker

Decision 6 has the marker define `anti_rt_bundle_<package>` and has two
bundles define the marker twice. With that one name the markers of two
packages share no symbol and nothing is a duplicate. The marker therefore
defines a second symbol, `anti_rt_bundle`, the same in every bundle. The
name of the package makes the link load the marker of each bundle, and the
fixed name is the duplicate that ld64.lld reports:

```text
ld64.lld: error: duplicate symbol: _anti_rt_bundle
>>> defined in libgeo.a(geo.bundle.o)
>>> defined in libother.a(other.bundle.o)
```

Both symbols are hidden. ld64.lld reports `anti_rt_registry` and the four
other tables of each library object as well. No function of the runtime is
a duplicate on Mach-O, since the link takes each runtime member from the
first archive. `docs/notes/linker.md` holds the choices.

One `[provisional]` entry under "Build tool and distribution" in
`docs/decisions.md`: the marker member is `<name>.bundle.o` and defines the
two hidden symbols, with `_` for each byte of the package name that is no
letter and no digit. The entry on `--bundle-runtime` above it records the
Mach-O form and its reason, and the two entries that named Apple's `ld -r`
say that the step closed them. `docs/libraries-for-c.md` says the same in
"Runtime" and in its test table.

## Tests

Each was written first and failed before the change, `own_tools` with
`antic: cannot run ld` and the two `twice` tests with an archive of one
joined object. Log: `build/drive/logs/bm-red2.log`.

- `clib_bundle_macos-arm64` and `clib_bundle_macos-x86_64` run on every
  host. The pinned clang compiles the C programs against the sysroot of the
  runtime archive, and ld64.lld links them, as antic links an Anti program.
  A Mac runs them, macos-x86_64 under Rosetta at `v1`.
- `clib_bundle_twice_macos-arm64` and `clib_bundle_twice_macos-x86_64` are
  the new test. Each reads the members of the archive and the symbols of
  the marker. It then links two bundled libraries into one C program and
  expects the duplicate `anti_rt_bundle` with both marker members named.
- `own_tools` builds a bundled macOS library with a search path that holds
  no `ld`.
- The case `bundle` of `tests/run_clib.cmake` now builds and runs
  `roundtrip.c` against the bundled library on every target. Before, it ran
  `notice.c` alone.

Two existing checks changed. The case `bundle` links two bundles and asks
for a duplicate that the runtime library defines. That check holds for ELF
and COFF and no longer runs for a macOS target, where the case `twice`
reads the marker instead. `tests/unit/test_link.c` lost the two lines that
pinned the command of Apple's `ld -r`.

## Sizes

| File, macos-arm64 | Before | After |
|---|---|---|
| `libgeo.a` of `tests/clib` with `--bundle-runtime` | 174,152 bytes | 218,056 bytes |
| The program of `roundtrip.c` linked against it | 174,416 bytes | 58,048 bytes |
| The marker `geo.bundle.o` | none | 616 bytes |

The archive holds 39 members where it held 2, and a program shrank, since
the link takes the runtime members it reaches. `libgeo.a` of macos-x86_64
is 220,192 bytes and its program 32,880 bytes. anti-linux and anti-windows
write the same 218,056 and 58,048 bytes for macos-arm64 as the Mac.

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1697 of 1697 pass, 2 skipped, 347 s | `build/drive/logs/bm-host2.log` |
| Mac | `asan` | 1696 of 1696 pass, 2 skipped, 469 s | `bm-asan2.log` |
| Mac | `ubsan` | 1696 of 1696 pass, 2 skipped, 420 s | `bm-ubsan.log` |
| anti-linux | the suite, 1627 tests | 850 and 788 in two parts of `ctest -I`, 5 skipped, 268 s and 59 s | `bm-vm-linux-ctest-1.log`, `-2.log` |
| anti-windows | 73 tests, not the suite | 73 of 73 pass, 48 s | `bm-vm-win-subset.log` |

The work order names no VM for this step. The whole suite ran on
anti-linux. On anti-windows the tests that the change reaches ran, chosen
with `ctest -R`, and not the suite. They are every `clib_*` test,
`own_tools`, the `unit_*` tests, `emit_identity` and
`link_identity_macos-arm64`. Both VMs ran `be5a205a`, which differs from
`725d07f0` in two numbers of `CLAUDE.md`.

The first ASan run, `bm-asan.log`, failed `own_tools`. The new check asked
for an empty error output. The antic of the ASan build writes a line about
a missing symbolizer there, since the search path of that test is bare.
The check now reads the status and the library, and the second ASan run
passed every test. The second part of a suite counts the fixtures it needs
again, so the parts of anti-linux sum to more than its tests.

The builds of the three Mac trees and of both VMs had no warning of our
own code. `emit_identity` and `link_identity_macos-arm64` pass unchanged,
and no file under `tests/emit-identity` or `tests/link-identity` changed.
`release_dry_run` passes. The step changed no file of the packer, the
release script or the installers, so nothing here waits for the real
`./r --dry-run`. The docs-style checker reports nothing on the documents
the step touched.

## Questions

- The fixed name `anti_rt_bundle` is this session's choice where decision 6
  is silent. A marker that reused a symbol of the runtime would let the old
  check of the case `bundle` hold for Mach-O too. Its price is a member
  that the runtime does not need.
- The Windows VM ran 73 tests. The whole suite there takes eight calls of
  up to seven minutes each, and the step asks for none.

## State

The commit of the step is pushed. This report follows it in a commit of
its own.

```text
$ git log --oneline -3
725d07f0 Write the bundled runtime of Mach-O as an archive
875053da Report the step installers of the binary distribution
f8128624 Make the installers download the package alone
$ git status --short
?? docs/reports/2026-10-10-dist-bundle-macho.md
$ git rev-parse HEAD origin/main
725d07f03f4516a9cfb9b8cfda8f8640e2920e69
725d07f03f4516a9cfb9b8cfda8f8640e2920e69
```
