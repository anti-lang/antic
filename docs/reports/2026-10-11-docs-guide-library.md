# Chapters 16 to 18 of the developer's guide

The step of the documentation driver that follows `guide-parallel`, on
2026-10-11, on the branch `docs`. It writes chapters 16, 17 and 18 of
`docs/site/developers-guide/` in full. It changes no code under `src/` and
no file under `tests/`. The task text gives the step no id, so this report
takes the name `guide-library`.

## What the step wrote

- `16-collections.md` has ten feature sections with the five parts of the
  guide. Seven are on the collections: List and Deque, Ring and Grid, the
  two maps, the three sets, the sorted ones, Pool, and Tree with
  PriorityQueue. Three are on walking, on hashing with its seed and on
  order with equality. The last section holds one table of the fourteen
  plain collections with their layout, growth and costs. A second table
  holds the shared operations.
- `17-c-interop.md` has six: `extern fn` and the C types, the ABI of a
  call, `anti bind`, `link`, libraries for C and plugins across the
  boundary. A call to C and a call from C stand in the LLVM module and in
  the machine code of linux-x86_64.
- `18-the-tool.md` follows the form of chapters 1 and 2, as the plan of
  the guide set for a chapter on tools. It holds the manifest, the
  dependencies with the lock file, a repository and the cache, and the
  commands `build`, `check`, `fmt`, `doc`, `license` and `symbols`.

## The examples it added

The three pages hold 23 `anti` blocks that compile, 12 of them programs
that run against their output. The compiler refuses 50 more blocks with
the error each names, and the pages hold 21 excerpts.

- Chapter 16: `lists`, `growth`, `rings`, `maps`, `sets`, `sorted`,
  `pools`, `trees`, `walks`, `hashes`, `orders`. A `Counting` allocator
  prints each allocation, so the growth of every layout is output.
- Chapter 17: `cee`, `tocee`, `geometry`, `bound`, `display`, `plugs`.
- Chapter 18: `units`, and a source before and after `anti fmt`.

The one time is that of `map_work` of `tests/bench` beside its C twin,
127.9 ms against 137.8 ms, at commit `1fb19e61` on the development Mac.
`tests/bench` has no program for another collection, so their costs are
counts.

Some facts were built and run by hand with the pinned tools. They are a
C program against `libgeometry.a` on macos-arm64, and for linux-x86_64
the three library forms, a plugin with its host and the two link modes.
Every command of chapter 18 ran over two projects and a `file://`
repository.

## What was wrong in antic

None of these is changed. The guide states each as it is.

- `clear` of a `List` removes from the front and closes the gap each
  time. A list of `n` moves `n (n - 1) / 2` elements, and one of 200,000
  took 2.35 s in a release build by hand. A loop of `pop` moves none.
- The bounds message of `l[5]` in a dev build names the file of the
  program with the line of `list.anti`: `com/example/trapper.anti:222`.
- `count` of a collection is a public field, and a write to it compiles.
- `for x in &p` over a `Pool` raises the version of every element reached.
- `extern fn puts(s: str)` compiles and passes two words.
- `antic` alone ignores a `link linux` line of its own input. The library
  links with `--linux-lib` or through `anti build`.
- `anti build` gives no message for an unknown key of `[package]`, a
  version of two parts or a package name of one segment, and builds.
- `anti doc` links a code span to an item of the same name, also when
  the span names a parameter.

## What was wrong in older documents

- `docs/libraries-for-c.md` says the header maps the `c_` types back to
  C names. It writes `int32_t` for `c_int` and `int8_t` for `c_char`, and
  a C name for `c_long`, `c_ulong` and `c_wchar` alone. An `export const`
  is `#define LIMIT ((int32_t)8)`, with a cast.
- `docs/tooling.md` places the cache in `~/.anti/cache`. It is
  `$XDG_CACHE_HOME/anti` or `~/.cache/anti`, with `index/` and `pkg/`.
- The same document describes ETag and `If-None-Match`. `src/anti/repo.c`
  has neither and reads an index again at most once per hour, by a stamp.
- It names a `sha256` file per version, `anti add` and `anti publish`. A
  repository without that file serves a build, and the tool has neither
  command.

## Answers from the code

- A collection is 96 bytes of `Collection<T>`, of which 72 record the
  place of the last change of the size, in every build. `push` copies 64
  of them per call.
- Growth is by doubling: a list from 8 elements, a `HashMap` from 16
  slots at half load, a B-tree block by its nodes of 11 elements. A pool
  adds blocks of 8, 16, 32 slots and copies none.
- `x.hash()` carries no seed. A table mixes the seed in at each lookup,
  three mixes for an `int` key. A `HashMap` takes its walk order from a
  random number of its own.
- Eleven C types are other names of sized types, `c_char` of `i8`. Three
  are types of their own.
- A variadic call passes its arguments on the stack on macos-arm64 and
  in registers on linux-arm64.
- A program that calls `plugin.load` or injects an interface skips
  release devirtualisation, by `whole.c`. `--closed` gives it back.
- The key of a cached output is the digest of its input, the version,
  the target, the level and three marks. The module with `main` is
  compiled by every build.
- `anti fmt` works on tokens and formats a file that does not parse.
- `anti doc` writes the same bytes from a library file and from source.

## Open points for Eddie

- `check-web-content` reports the part headings, "Where it lives:" and
  the word "holds", as in every chapter, and lists of three facts.
- `guide_examples` took 228 s in the suite.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac, in 355 s with `ctest --preset host -j14`. The sanitizer suites did not
run, since the step changed no file under `tests/`. The docs-style checker
reports nothing on the three pages and on this report. `git grep` finds no
marker of unfinished text in the three pages. This is the state before the
commit of this report:

```text
$ git log --oneline -3
b5583223 Drop two contrast frames and the semicolons of a list in chapters 16 to 18
bf116640 Write chapter 18 of the developer's guide, the tool
ca382e21 Write chapter 17 of the developer's guide, C interop
$ git status --short
$ git rev-parse HEAD origin/docs
b55832238079a3c7b7412c046474320e73f6e20c
b55832238079a3c7b7412c046474320e73f6e20c
```
