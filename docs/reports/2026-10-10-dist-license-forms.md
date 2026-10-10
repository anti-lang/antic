# The other four forms of `anti license`

The step `license-forms` of `docs/work-order-distribution.md`, on 2026-10-10.
It builds item 37 of "First sessions": the plain form, `--project`,
`--project --notice` and `--from-archive` of "License command" in
`docs/distribution.md`, and a `NOTICE.txt` that comes from the project. The
commits are `dfa2a909`, `2b65557f`, `d10ed993` and `ec470c18`.

## What the step built

- Every form prints the lines of the notice of a binary: `package <name>
  <version> <identifier>`, `attribution`, each text once after `text for
  <names>`, and the `source` line of the record after it. antic writes them
  for both tools through `antic_notice_lines`.
- The plain form prints `LICENSE` of the package for antic and `anti`, then
  the runtime and every other text of `licenses/`. A version comes from
  `sources.txt`. The CMake build now writes `LICENSE` into the root of the
  runtime archive of the tree, where a package has it.
- The identifier of a component stands once, in `notice_component_license`
  of `src/antic/notice.c`. The notice of a binary reads `0BSD` and `MIT`
  there too. A text with the notices of more than one licence is
  `LicenseRef-<name>`.
- `--project` builds the project and prints its notice for one target. It
  names the runtime, musl and mimalloc where the link took them, every
  package of `anti.lock`, the bundled modules the project imports and the
  project last. The link reports through `links_musl` of the options of the
  driver whether it took musl.
- `--project --notice` writes that notice as `NOTICE.txt` beside whatever
  the build wrote. `anti build` writes the same file beside a program and a
  shared library, from the project and no longer from the binary.
- `--from-archive` walks an archive of the GNU, the BSD or the COFF form to
  the member `<name>.package.o` and prints the fields of its package header.
  `src/anti/licensing.c` holds the three forms in 426 lines.

## What it tested

`license_plain`, `license_project`, `license_project_notice` and
`license_from_archive` check one form each, from `tests/run_license_forms.cmake`.
The project is `tests/anti-build/notices`, which links `shapes` and `tones`,
two packages with a licence, a text and attributions. `shapes` is the static
archive of `--lib static`, built for one target of each archive form and once
with `--bundle-runtime`. `package_keys` runs the plain form of an unpacked
package with `PATH` of its `bin/` alone. Each test failed first, on the usage
text of `anti`.

## What the tests found

- `anti build` passed the licence fields of the manifest to no call that
  links. The notice of a program named its own package without licence, and
  the header copy of a static library held no licence at all. Every linking
  call takes the header now, and `license_notice` checks it. That is
  `dfa2a909`.
- A member may repeat the first bytes of a library file. The reader of an
  archive read a header at every such place, into one memory pool. It now
  gives a member up after eight places. That is `d10ed993`.
- `file(WRITE)` of CMake wrote CR and LF into the malformed archives of the
  test on anti-windows. That is `ec470c18`.

## Sizes

| What | Size |
|---|---|
| `anti license` over the runtime archive of the Mac | 214132 bytes, 39 packages, 35 `source` lines |
| `NOTICE.txt` of `notices` for linux-arm64, with musl and mimalloc | 9256 bytes |
| `NOTICE.txt` of `notices` for macos-arm64 | 1723 bytes |
| A package | unchanged, it held `LICENSE` and `licenses/` already |

## Gates

| Machine | Suite | Result | Log |
|---|---|---|---|
| Mac | `host` | 1701 of 1701 pass, 2 skipped, 379 s | `build/drive/logs/lf-final-host.log` |
| Mac | `asan` | 1700 of 1700 pass, 2 skipped, 521 s | `lf-final-asan.log` |
| Mac | `ubsan` | 1700 of 1700 pass, 2 skipped, 453 s | `lf-final-ubsan.log` |
| anti-linux | the suite, 1631 tests | 850 and 792 in two parts of `ctest -I`, all pass, 5 skipped, 226 s and 58 s | `lf-vm-linux-ctest-1.log`, `-2.log` |
| anti-windows | the suite, 1619 tests | 1660 runs in eight parts, all pass, 14 skipped, 1202 s | `lf-vm-win-ctest-1.log` to `-8.log` |

The Mac ran `ec470c18`. Both VMs ran `d10ed993` with the test file of
`ec470c18`, whose digest was compared on each, and anti-linux ran the five
licence tests again with it, in `lf-vm-linux-license.log`. A part of a suite
counts the fixtures it needs again, so the parts sum to more than the tests.
No build had a warning of our own code. `emit_identity` and
`link_identity_macos-arm64` pass, and no file under `tests/emit-identity` or
`tests/link-identity` changed. `release_dry_run` passes, and the step changed
no packer, release script or installer. The docs-style checker reports nothing
on the documents the step touched. No sentence of the work order needed a
correction.

The ASan suite took 521 s of its 540. `package_keys` sets that time with
519 s, and `emit_identity` follows with 479 s.

## Provisional entries

Seven, under "Build tool and distribution" in `docs/decisions.md`:

- Every form prints the lines of the notice of a binary.
- The plain form: antic and `anti` first, `LICENSE` in the root of the
  archive, the version of the record or `-`.
- The table of identifiers, and `LicenseRef-<name>` for every other text.
- `--project` is a build, and the link reports whether it took musl.
- The order and the sources of the notice of a project. A package of the
  lock that no module imports is named.
- `--project` prints one target and refuses `--target all` and `--lib
  static`. `--project --notice` writes beside whatever the build wrote.
- `--from-archive` finds the member by its name and prints its package.

## Questions

- The table gives glibc `LGPL-2.1-or-later` and the kernel headers
  `GPL-2.0-only`, the licence each text names first. Is that the identifier
  wanted, or `LicenseRef` for both?
- The table gives miniaudio `Unlicense OR MIT-0` and Mbed TLS `Apache-2.0 OR
  GPL-2.0-or-later`, as their texts say. "Obligations of a shipped program"
  names `MIT-0` and `Apache 2.0`.
- A program that imports `anti.regex` links PCRE2, and neither notice names
  it. Which package header should carry the licence of PCRE2?
- `license_text` of a manifest may name a file outside the project, and its
  text reaches the notice. Should the build refuse such a path?
- Should `anti build --lib static` write `NOTICE.txt` as well, now that the
  file comes from the project?

## State

The four commits of the step are pushed. This report follows them in a
commit of its own.

```text
$ git log --oneline -3
ec470c18 Write the malformed archives of license_from_archive with line feeds
d10ed993 Bound the search for a package header in a static archive
2b65557f Build the other four forms of anti license
$ git status --short
$ git rev-parse HEAD origin/main
ec470c18898f538f2cf25a8b3f3e4b5beb06c53b
ec470c18898f538f2cf25a8b3f3e4b5beb06c53b
```
