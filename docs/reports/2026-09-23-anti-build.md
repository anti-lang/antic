# anti build

The build command of `docs/tooling.md`: the manifest, the lock file, the
repositories and the cache, dev mode with its per-module objects, release mode,
`--target`, `--cpu`, `-g`, `--lib static` and `--lib shared`, the symbols
archive of a release, `anti run` and `anti new`.

## What was built

- `rt/toml.c` reads inline tables. A dependency is written
  `{ version = "1.2.4", repo = "ff" }` and the modules of a version of an index
  are an array of them. `tests/unit/test_toml.c` covers the three forms.
- `tools/anti/manifest.c` reads the whole manifest: `[package]`, `[layout]`,
  `[targets]`, `[repositories]` and `[dependencies]`, with the four refusals a
  bad dependency earns.
- `tools/anti/repo.c` holds the repositories and the cache of the user. A
  `file://` URL is a copy and an `https://` URL is one curl request per file,
  with the entity tag of the earlier fetch. An index is checked at most once an
  hour and a package file is immutable once cached.
- `tools/anti/deps.c` resolves the graph in the order of "Resolution", writes
  `anti.lock` and checks every digest. `tests/unit/test_deps.c` covers the
  version comparison, the three constraint forms, the URLs and the manifest.
- `tools/anti/build.c` builds. Dev mode writes one object per module under
  `build/<target>/dev/obj/`, keyed by the digest of its input, the compiler
  version, the target and the level. Release mode is one call over the whole
  program. `--target`, `--target all`, `--cpu`, `--offline`, `--strip-docs`,
  `--lib static` and `--lib shared` are built, and a project whose modules
  declare no `main` writes the library file of each module into `dist/`.
- `tools/anti/zip.c` and `tools/anti/symmap.c` write the symbols archive of a
  release: the same link with its debug sections kept, the map of the program's
  functions with their files and lines, and the PDB of a Windows link.
  `rt/symbols.c` gained `anti_elf_functions` and `anti_macho_functions`.
- `anti run` builds for the host and runs what it wrote. `anti new <name>`
  writes the default layout and refuses a name of one segment.
- `tests/anti-build` holds a two-module project, a library project and a
  consumer of it. `anti_build` and `anti_build_deps` cover both modes, the
  cache, the `-g` rule, `--target`, `--cpu`, the lock file, a dependency of a
  path, one of a `file://` repository, `--offline`, a yanked version, the two
  libraries for C, the symbols archive, `anti run` and `anti new`.

## What failed and how it was fixed

- The first dev build passed a module's own library file to its own compile.
  antic refuses a module that stands twice on one command line. The object
  compile now drops its input from the list it is given.
- The link of a dev build had no object of `anti.io`, so it failed on an
  undefined symbol. `driver_libraries` walks the imports of the project and the
  graph, and the build writes an object of every module below them, as
  `anti test` does.
- The first link read the library file of the module with `main`. antic stops a
  dev build of a library file at its object and never links one, so that module
  is compiled from its source.
- `driver_libraries` puts its list in the memory pool, and the build freed it.
  The run ended in an abort with no message.
- `copy_file` writes a plain file, so the program in `dist/` had no execute
  bit. `copy_program` of `tools/anti/files.c` carries the mode over.
- The fixture stood under `tests/build/`, which `.gitignore` matches with its
  `build/` line. It is `tests/anti-build/`, beside `tests/anti-test/`.
- `link_identity_macos-arm64` failed twice, once for `rt/toml.c` and once for
  `rt/symbols.c`. Both changed the runtime library, and the recorded digest was
  written again from a run on this Mac.
- `raw_output` refused `tests/unit/test_toml.c` until it included
  `../binary_stdio.h`.

## Provisional entries

Added to `docs/decisions.md` under "The build command". They are the name of a
deliverable, the library project, the module that links a dev build and the
objects of the standard library. Then the lock file against a dependency of a
path, the depth of a chain of path dependencies and the cache key. Then the
package name `anti new` takes and the two entries of the symbols archive.

## Gates

```text
$ git log --oneline -3
e8a9be7 Record the build command in the decisions, the tooling page and the notes
e26531f Write the symbols archive of a release build
9c1fbf9 Build a project with anti build, anti run and anti new

$ git status --short

$ git rev-parse HEAD origin/main
e8a9be7bc64cb2c1142004b82d14e70db9d7b244
e8a9be7bc64cb2c1142004b82d14e70db9d7b244
```

- Host build: zero warnings.
- Host suite: 772 of 772 passed, none skipped.
- ASan suite: 771 of 771 passed.
- UBSan suite: 771 of 771 passed.
- The docs-style checker reports nothing on every file the session touched.

## Open

- "Symbols tooling" in `docs/anti-language-additions.md` asks that the program
  and its debug link both carry the build id. The id is the digest of the
  assembly antic wrote, and `-g` writes more of it, so the two links carry two
  ids. The map names the id of the program. Both links place every function at
  the same address, so the one map answers for both. A build id that leaves the
  debug directives out would make the rule hold. That is a change to "Build
  ids" in `docs/decisions.md`, which is Eddie's to make.
- The `https://` fetch has no test. It is one curl request, the same code the
  test drives over `file://`, and it was run by hand against a server on
  `127.0.0.1`. A test of it needs a server in the suite, which no test starts
  today.
- `anti add`, `anti fetch`, `anti clean` and `anti publish` of the command table
  are not built, and neither is `anti symbols`.
- A library for C is one call over the whole program in both modes. `--lib`
  with no `--release` therefore differs from a release build in the assertions,
  the checks and `-g` alone. `docs/tooling-addendum.md` gives the two modes for
  a program and says nothing of a library.
