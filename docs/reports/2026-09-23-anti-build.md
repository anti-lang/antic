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

Added to `docs/decisions.md` under "The build command". They are the module that
links a dev build and the objects of the standard library. Then the depth of a
chain of path dependencies and the cache key. Then the package name `anti new`
takes and the archive a release writes. Eddie settled three of them in his
answers. The name of a deliverable, the library project and the lock file
against a dependency of a path carry no tag.

## What the answers changed

Eddie answered the three open items of this report.

- The build id is the SHA-256 of what the program executes, not of every line of
  assembly. `src/debug.c` records the byte range of everything `-g` appends, and
  `build_id` of `src/driver.c` digests the assembly around those ranges. A `-g`
  link and a plain link of one program now carry one id, which is what
  `anti symbols` needs to match a trace to its archive. The `S_LPROC32` records
  of COFF and the end label they read stand in every build, so neither is a
  range and no id of a plain build moved. `program_build_id` compares the two
  ids of one program, and `anti_build` reads the id out of the program, the
  debug link and the map of a release archive.
- The `https://` fetch gets no automated test yet. It waits for Mbed TLS in
  `src/native/`. The manual check stands under "Gates" below.
- The three entries Eddie named lost their `[provisional]` tag.

## Gates

```text
$ git log --oneline -3
fcb5411 Record the build id rule and settle three entries of the build command
6cf687d Leave what -g added out of the build id
9dc3650 Report the anti build step

$ git status --short

$ git rev-parse HEAD origin/main
fcb54112c8a0ccd2e9b898e4fa26abf2acfa4c81
fcb54112c8a0ccd2e9b898e4fa26abf2acfa4c81
```

- Host build: zero warnings.
- Host suite: 772 of 772 passed, none skipped.
- ASan suite: 771 of 771 passed.
- UBSan suite: 771 of 771 passed.
- The docs-style checker reports nothing on every file the session touched.

### The fetch over HTTP, by hand

The `https://` fetch has no test in the suite until Mbed TLS stands in
`src/native/`. It is one curl request, and the same code runs for a
`http://127.0.0.1` repository, which the rules allow. The check below was run on
the development Mac against a server of the standard library of Python, with the
library file of `tests/anti-build/units` published under the layout of
"Repositories" in `docs/tooling.md`.

```text
$ python3 -m http.server 8788 --directory <repo> &
$ cat anti.toml
[repositories]
local = "http://127.0.0.1:8788"

[dependencies]
"com.example.units" = { version = "1.2.0", repo = "local" }

$ XDG_CACHE_HOME=<cache> anti build --runtime <runtime> --llvm-mc <llvm-mc>
$ ./dist/macos-arm64/dev/consumer
consumer
$ echo $?
8
$ ls <cache>/anti/pkg/com.example.units/1.2.0/
com.example.units.antl
```

The index and the library file were fetched into the cache. The digest of the
index was checked against the file, and `anti.lock` recorded the repository by
URL. The program linked against the fetched module and ran.

## Open

- `anti add`, `anti fetch`, `anti clean` and `anti publish` of the command table
  are not built, and neither is `anti symbols`.
- The `https://` fetch has no automated test until Mbed TLS stands in
  `src/native/`. The manual check under "Gates" is its evidence.
- A library for C is one call over the whole program in both modes. `--lib`
  with no `--release` therefore differs from a release build in the assertions,
  the checks and `-g` alone. `docs/tooling-addendum.md` gives the two modes for
  a program and says nothing of a library.
