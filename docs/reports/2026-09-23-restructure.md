# Restructure

The step `restructure` of `drive-prep.sh`, with two decisions of Eddie's.
The files that tools require in the root and the release link `r` stay at
the top. The key guard reads the package.

## What moved

`git mv` moved every file, so each keeps its history.

| Before | After |
|---|---|
| `src/` | `src/antic/` |
| `tools/anti/` | `src/anti/` |
| `rt/` | `src/rt/` |
| `std/` | `src/std/` |
| `libs/` | `src/native/` |
| `keys/` | `tools/keys/`, with the ignored `private/` moved beside it |

The presets are `host`, `asan` and `ubsan`, each in `build/<preset>`. The
pinned downloads lie in `build/deps/`, which `tools/deps-dir.cmake` names
once. The raylib source moved from the build tree to `build/deps/raylib`.
`anti bind` finds the pinned clang at `deps/clang` beside its own tree.
`.gitignore` covers `/build/` and `tools/keys/private/`. The paths follow in
the CMake files, the tests, the packer, `tools/release.sh`, both installers,
the workflow, `CLAUDE.md`, `README.md` and `docs/`. The installed package
layout is unchanged. No identity manifest was regenerated: `emit_identity`
and `link_identity` pass against the manifests as they stood.

## The layout rule

`CLAUDE.md` gains "Repository layout". The test `repo_layout` reads `git
ls-files`. It fails on a top-level entry outside the rule, on a tracked path
under `build/`, and on a directory directly under `src/`, `tests/` or
`docs/` that its lists do not hold. A checked negative case names each of
them. `docs/decisions.md` gains the section "Repository layout", with the
native libraries built in this repository.

## What failed and how it was fixed

- `release_key` refused any key file under `tools/`. Eddie decided on the
  replacement: `package_keys` fails on a key file or anything of
  `tools/keys/` in a package the packer wrote, and `release_key` fails when
  the packer's copy list names a file under `tools/keys/`. A copy list with
  `keys/release.pem` added fails as it should.
- The first host run failed 7 tests on missed paths: `unit` (the sources
  that `test_link.c` reads), `release_pins` (the pattern of the download
  directory), `raw_output` (`ENCODING NONE` in the new test), `fmt_canonical`
  (a doc comment to rewrap), `macos_sysroot` and `glibc_sysroot` (the copy of
  `deps-dir.cmake` beside the script) and `release_dry_run`, which clones
  the committed tree and passed after the commit.

## Provisional entries

- `tools/deps-dir.cmake` names `build/deps` once.
- `repo_layout` reads the tree from the disk when the root is no git work
  tree, since step 2 of `./r` runs the suite in an export.

## For Eddie

- The packer compiles `anti` for Linux from `src/antic/` and `src/anti/`
  without `src/rt/json.c`, `symbols.c` and `toml.c` or `-I src/rt`, which
  the CMake build adds. Only a Linux package takes that path. It was so
  before the move and is unchanged.
- The old trees `build-asan/` and `build-ubsan/` now lie in `build/stale/`.
  The files of the old tree at the root of `build/` stay there.

## Verification

A fresh clone in `build/fresh/antic` fetched clang, the LLVM tools, the six
sysroots and raylib once into its `build/deps/`, then configured, built and
ran every preset. The logs are in `build/fresh/logs/`, the development tree's
in `build/drive/logs/rs-*.log`.

| Suite | Result |
|---|---|
| host | 100% tests passed out of 788 |
| asan | 100% tests passed out of 787 |
| ubsan | 100% tests passed out of 787 |

The only build warning is the linker's `-lto_library` note, which
`docs/notes/linker.md` records.

## Proof

At the push of the code, before this report:

```
1ac6185 Count repo_layout in the suite totals
6fb5ffd Move the sources under src/ and the build trees under build/
9f3a635 Report the repository layout step as blocked
$ git status --short
?? docs/reports/2026-09-23-restructure.md
$ git rev-parse HEAD origin/main
1ac61859c4daad6c8619253fc789b960f26f9bbe
1ac61859c4daad6c8619253fc789b960f26f9bbe
```
