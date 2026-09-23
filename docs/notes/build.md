# The build command

The choices inside `anti build`, `anti run` and `anti new`. The decisions they
follow are under "The build command" in `docs/decisions.md`, and the
specification is "Commands", "Project layout", "Manifest", "Lock file",
"Dependencies", "Repositories", "Cache" and "Resolution" in `docs/tooling.md`
with "Build modes and caching" in `docs/tooling-addendum.md`.

## Three files and one order

`src/anti/manifest.c` reads `anti.toml`, `src/anti/repo.c` fetches from a
repository into the cache of the user, `src/anti/deps.c` resolves the graph
and writes `anti.lock`, and `src/anti/build.c` compiles. A build reads the
manifest, checks the compiler version it asks for, resolves the graph, reads
the modules under the source directory and then builds each target.

The reader of the manifest is `src/rt/toml.c`, the one parser that `anti.toml`, the
logger and the runtime configuration share. It gained inline tables for this
step. A dependency is written `{ version = "1.2.4", repo = "ff" }`, and the
modules of a version of an index are an array of them.

## Where a file lands

Everything the compiler writes lands under `build/<target>/<mode>/`, and the
tool copies the deliverable into `dist/<target>/<mode>/`. The split of
`docs/tooling.md` then holds: `build/` is disposable and `dist/` is what a user
runs or links against. antic writes the assembly and the object beside the
output it is given, so a link straight into `dist/` would put them there.

The library file of each module stands under `build/<target>/<mode>/lib/`, as
the module path spells it, and the object under `obj/`. A library project, one
whose modules declare no `main`, copies each library file into `dist/` instead,
which is what a dependency of a path reads.

## The cache of a module

The key of an output is the SHA-256 of its input, the version of antic, the
target and the processor level, with a mark for `-g`. It stands in a file
beside the output, under the name of the output and `.key`. A compile drops the
key first and writes it after, so a run that stops halfway leaves no key that
the next build would trust.

The key of a library file is over the source, and the key of an object is over
the library file. A change that does not reach the interface or the IR of a
module therefore leaves its object alone. A comment or a line break rewrites
the library file to the same bytes, and the object is not written again.

## The link of a dev build

A dev build compiles one module into its own object, so the link needs an
object of every module it reaches. That is the modules of the project, the
modules of the dependency graph and the modules of the standard library below
them. `driver_libraries` of the compiler walks the imports of a seed and gives
all three, and the build writes an object of each. `anti test` links the same
way.

antic stops a dev build of a library file at its object and never links one.
The module that carries `main` is therefore compiled from its source, and it
links in the same call. It is the one module of a dev build that every build
compiles again, and the link it carries is the relink of that build.

## Resolution

The lock file answers the manifest when every dependency of a repository is in
it. Each stands there with a version its constraint takes and the repository it
names. A
dependency of a path never answers, because the files under it are the
developer's own and carry no version. A project with one therefore walks. That
costs an index read per package of a repository, and an index is checked
against its repository at most once an hour.

The walk keeps one requirement per constraint and one pick per package. A round
resolves every package that has no pick. It resolves one whose pick no longer
satisfies every constraint on it as well. The walk ends when a round changes
nothing. A graph that still moves after sixty-four rounds holds a pair of
constraints that pull at each other. That is an error rather than a loop.

A dependency of a path whose directory holds a manifest is a project. The build
builds it for the host first, in the mode of the build that names it. It then
reads the library files under its `dist/`. The chain stops after eight, so a
cycle of path dependencies reports rather than running out of stack.

## The symbols archive

A release binary carries no symbol data. The build therefore links the program
a second time with the debug sections kept, and writes
`<program>-symbols.zip` beside the binary in `dist/`. `src/anti/zip.c` writes
the archive. Its entries are stored and never compressed, so the tool needs no
library. Every field that would carry a clock or a machine is fixed.

`src/anti/symmap.c` writes the map: one line per function with the range of
its addresses, its name and, where the debug information gives them, its file
and line. It reads the program with the readers of `src/rt/symbols.c`, the ones
`anti.lang.StackTrace.symbolize` reads a running program with. The map is
therefore what a trace of that program would name. Those readers looked up one
address at a time, and `anti_elf_functions` and `anti_macho_functions` walk a
whole symbol table for this.

The binary, the debug link and the map all carry one build id, which is what
ties a frame of a trace to this archive. The digest of an id leaves out what
`-g` added, so a `-g` link carries the id of the plain link beside it.
"Build ids" in `docs/decisions.md` holds the rule and `src/antic/debug.c` records the
ranges. Both links place every function at the same address, so the one map
answers for either of them.

A Windows program carries no symbol table, because lld-link writes the symbols
to a PDB. The archive holds that PDB, and the map says so.
