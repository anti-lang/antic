---
title: "The tool"
description: "anti.toml, dependencies and the lock file, anti build and its cache, anti check, anti fmt, anti doc, anti license and anti symbols."
summary: "The manifest `anti.toml`, dependencies and `anti.lock`, `anti build` and its cache, `anti check`, `anti fmt`, `anti doc`, `anti license` and `anti symbols`."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T22:00:00+02:00
draft: true
weight: 180
tags: [compilers, programming-languages]
keywords: [build tool, manifest, lock file, dependency resolution, build cache, formatter, documentation generator, licence notice, symbols]
---

`anti` is the tool of a project. It reads the manifest `anti.toml`, resolves and fetches the dependencies, and compiles through the driver of antic, which is linked into it. Every command of this chapter was run on macos-arm64 with Anti 0.2.0, and the page shows its output as the tool printed it. A path of the machine is shortened to `/home/ann`.

Chapter 1 holds `anti new`, a first `anti build` and `anti run`. Chapter 17 holds `anti bind`, and chapter 19 `anti test`.

## anti.toml

The manifest is the file `anti.toml` in the root of a project. It is TOML: tables, strings, integers, booleans, arrays and inline tables. `anti new com.example.hello` writes the smallest one.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"
```

The tool reads these tables.

| Table | Key | Value |
|---|---|---|
| `[package]` | `name` | the package name, a module path. It is the root of every module of the package |
| | `version` | the version, `major.minor.patch` |
| | `antic` | the lowest compiler version the project builds with |
| | `license` | the SPDX identifier of the licence |
| | `license_text` | the file that holds the licence text |
| | `attribution` | one line the notice of a program carries |
| `[layout]` | `src`, `test`, `build`, `dist` | the four directories, with those names by default |
| `[targets]` | `default` | the targets of `anti build` without `--target`. The host by default |
| | `all` | the targets of `anti build --target all`. All six by default |
| `[repositories]` | an alias | the URL prefix of a repository |
| `[dependencies]` | a package name, quoted | an inline table with `version`, `repo` or `path` |
| `[inject]`, `[inject.test]` | the path of an interface | its provider, as chapter 15 describes |

A source file is one module. Its module path is its path under `src/` with dots for the slashes, so `src/com/example/hello.anti` is `com.example.hello`. The module with `fn main` is the program, and a project without one is a library project.

The manifest below is that of the library of this chapter.

```toml
[package]
name = "com.example.units"
version = "1.2.0"
license = "MIT"
```

Its one module is `src/com/example/units.anti`.

```anti
//! name: units
//! Conversions between metres and feet.

/// The feet in one metre.
pub const FEET_PER_METRE: f64 = 3.28084;

/// The length `metres` in feet.
///
/// ```anti
/// let f = units.feet(2.0);
/// ```
pub fn feet(metres: f64) -> f64
{
	return metres * FEET_PER_METRE;
}

/// The length `feet` in metres.
pub fn metres(feet: f64) -> f64
{
	return feet / FEET_PER_METRE;
}
```

`anti build` refuses a manifest it cannot follow, with one line.

| Manifest | Message |
|---|---|
| `antic = "9.0"` | `anti: ./anti.toml asks for antic 9.0 and this one is 0.2.0` |
| `default = ["amiga-68k"]` | `anti: anti.toml: [targets] names amiga-68k, which is no target of Anti` |
| `src = "source"`, with no such directory | `anti: ./source holds no .anti file, so this project has no module` |

A key the tool does not know is not reported. `colour = "red"` under `[package]` builds as if it were not there.

Where it lives: `src/anti/manifest.c` reads the manifest with the TOML parser of `src/rt/toml.c`, which the runtime configuration shares.

## Dependencies and the lock file

A dependency is a package of library files. A library file, `<module>.antl`, holds the public interface of one module, its checked generics and its IR. A project that depends on a package never sees its source.

`[dependencies]` maps a package name to an inline table of one of three shapes.

| Shape | Source |
|---|---|
| `{ version = "1.2.0", repo = "local" }` | the repository with that alias |
| `{ version = "1.2.0" }` | each repository of the manifest in order |
| `{ path = "../units" }` | a project on disk, which is built first, or a directory of library files |

A version constraint has three forms.

| Form | Meaning |
|---|---|
| `"1.2.0"` | 1.2.0 or newer, below 2.0.0 |
| `"=1.2.0"` | exactly 1.2.0 |
| `">=1.2.0"` | 1.2.0 or newer, with no upper bound |

### Path dependencies

The program of this chapter is `com.example.hello`. Its manifest names the library by its directory.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"

[dependencies]
"com.example.units" = { path = "../units" }
```

Its source imports the module and calls it. `anti run` builds the library project first and prints `2 m is 6.56 ft`.

```sh
anti run
```

`anti build` writes `anti.lock` beside the manifest on its first run.

```toml
# Written by `anti build`. Commit it for an application.
version = 1

[[package]]
name = "com.example.units"
version = "1.2.0"
path = "./../units/dist/macos-arm64/dev"
modules = [
    { path = "com.example.units", sha256 = "4ab76bae31c4e96324107e36ab2ad4e7332f61d23d9a54fe7b0b8a1f4d752276" },
]
```

The lock file names every package of the graph with its version, its source and the SHA-256 of each library file. A later build takes the locked versions and compares each digest before it compiles.

### Repositories

A repository is a URL prefix over static files, and no server code stands behind it. The URL starts with `https://` or `file://`. `http://` is taken for `127.0.0.1` and `localhost` alone.

```text
<prefix>/<name>/index.toml
<prefix>/<name>/<version>/<module>.antl
```

`index.toml` lists every version of a package with the digest of each module and the dependencies of the version.

```toml
name = "com.example.units"
revision = 1

[[version]]
version = "1.2.0"
modules = [{ path = "com.example.units", sha256 = "4ab76bae31c4e96324107e36ab2ad4e7332f61d23d9a54fe7b0b8a1f4d752276" }]
dependencies = []
```

A version with `yanked = true` keeps its entry. A lock file that names it still resolves, and a new resolution passes over it.

The program takes the library from a repository with this manifest.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"

[repositories]
local = "file:///home/ann/repo"

[dependencies]
"com.example.units" = { version = "1.2.0", repo = "local" }
```

`anti build` then writes this lock file.

```toml
# Written by `anti build`. Commit it for an application.
version = 1

[[package]]
name = "com.example.units"
version = "1.2.0"
repo = "file:///home/ann/repo"
modules = [
    { path = "com.example.units", sha256 = "4ab76bae31c4e96324107e36ab2ad4e7332f61d23d9a54fe7b0b8a1f4d752276" },
]
```

### Resolution and the cache

A build resolves in four steps.

1. It reads the manifest. When a lock file stands and every locked entry meets the manifest, it takes the lock file as it is.
2. Otherwise it reads the index of each dependency and walks the dependencies of the versions until the graph is closed. It picks the highest version that is not yanked and meets every constraint on the package, and writes `anti.lock`.
3. It fetches every library file that is not in the cache and compares its digest with the index.
4. It hands the sources of the project and every library file of the graph to the compiler.

Fetched files go to the cache of the user, which chapter 1 places: `~/.cache/anti` on macOS and Linux unless `XDG_CACHE_HOME` names another, and `%LOCALAPPDATA%\anti\cache` on Windows. After the build above it holds three files.

```text
index/<sha256 of the prefix>/com.example.units/index.toml
index/<sha256 of the prefix>/com.example.units/index.toml.checked
pkg/com.example.units/1.2.0/com.example.units.antl
```

The file `index.toml.checked` is the stamp of the last check. The tool reads an index from its repository again at most once per hour. A library file in the cache is never fetched again. An `https://` fetch runs the `curl` of the system.

`anti build --offline` contacts no repository and fails when a file it needs is not in the cache.

| Situation | Message |
|---|---|
| `--offline`, empty cache | `anti: --offline and no cached com.example.units of com.example.units 1.2.0` |
| the fetched file differs from the index | `anti: com.example.units of com.example.units 1.2.0 has the digest 6108... and the index names 4ab7...` |
| `version = "2.0.0"`, and the repository has 1.2.0 | `anti: file:///home/ann/repo has no version of com.example.units that satisfies every constraint on it` |
| no `repo`, and no repository of the manifest has the package | `anti: no repository of the manifest has a version of com.example.units that satisfies every constraint on it` |
| `path = "../nowhere"` | `anti: ./../nowhere holds no library file of the package com.example.units` |
| `version = "=2.0.0"` beside a path that holds 1.2.0 | `anti: ./../../units/dist/macos-arm64/dev holds com.example.units 1.2.0, which no constraint on the package takes` |

The two digests of the second row are shortened here. The tool prints all 64 digits of each.

Where it lives: `src/anti/deps.c` resolves the graph and writes the lock file, and `src/anti/repo.c` fetches into the cache.

## anti build and its cache

`anti build` reads `anti.toml` of the current directory, resolves, fetches and compiles. It prints nothing when it succeeds. `anti run` does the same for the host and runs the program.

| Option | Effect |
|---|---|
| `--release` | release mode. Dev mode is the default of the tool |
| `--target <t>`, `--target all` | build for that target, or for the list `all` of the manifest |
| `--cpu <level>` | the processor level, which chapter 20 lists |
| `--offline` | contact no repository |
| `--lib static`, `--lib shared` | write a library for C with its header, as chapter 17 describes |
| `--bundle-runtime`, `--soname` | the two options of a library for C |
| `--memory-checks` | build with the memory checks of chapter 12 |
| `--strip-docs` | leave the doc text out of the library files |
| `--lto full`, `thin`, `none` | the link time optimization of a release build |
| `--profile-generate`, `--profile-use <file>` | the two steps of a build with a profile, which chapter 21 describes |
| `--runtime <dir>` | the package to build with |

### A dev build

Dev mode compiles one module at a time. A dev build of `com.example.hello` writes these files.

```text
build/macos-arm64/dev/lib/com/example/hello.antl
build/macos-arm64/dev/lib/com/example/hello.antl.key
build/macos-arm64/dev/obj/anti/io.o
build/macos-arm64/dev/obj/anti/io.o.key
build/macos-arm64/dev/obj/anti/lang.o
build/macos-arm64/dev/obj/anti/lang.o.key
build/macos-arm64/dev/obj/anti/mem.o
build/macos-arm64/dev/obj/anti/mem.o.key
build/macos-arm64/dev/obj/anti/text.o
build/macos-arm64/dev/obj/anti/text.o.key
build/macos-arm64/dev/obj/com/example/units.o
build/macos-arm64/dev/obj/com/example/units.o.key
build/macos-arm64/dev/hello.o
build/macos-arm64/dev/hello
dist/macos-arm64/dev/hello
dist/macos-arm64/dev/NOTICE.txt
```

The build writes a library file per module of the project, and an object per module the program reaches. That covers the modules of the project, of the dependencies and of the standard library below them. The module with `main` is compiled from its source and linked in the same call.

Every cached output has a key file beside it. The key is one line.

```text
4ab76bae31c4e96324107e36ab2ad4e7332f61d23d9a54fe7b0b8a1f4d752276 0.2.0 macos-arm64 armv8.5 g
```

| Part | Contents |
|---|---|
| The digest | the SHA-256 of the input: the source for a library file, the library file for an object |
| `0.2.0` | the version of antic |
| `macos-arm64`, `armv8.5` | the target and the processor level |
| `g` or `no-g` | whether the output carries the lines of `-g`. A dev object does, a library file does not |
| `strict`, `memory-checks` | present when warnings are errors, and when the object carries the memory checks |

A build compiles an output again when it is missing or its key differs from the one the build would write. It removes the key before the compile and writes it after, so a build that stops halfway leaves no key to trust.

The key of an object is over the library file and not over the source. A comment added to a source writes the library file again with the same bytes, and the object stays.

What two builds of `com.example.hello` wrote again, read from the times of the files:

| Build | Files written again |
|---|---|
| after a change of `hello.anti` | `hello.antl` and its key, `hello.o`, `hello`, and the two files of `dist/` |
| with no change | `hello.o`, `hello`, and the two files of `dist/` |

The objects of `anti.io`, `anti.lang`, `anti.mem`, `anti.text` and `com.example.units` stayed in both builds. The module with `main` is the one module every build compiles again, and its compile carries the link.

### A release build

`anti build --release` compiles the whole program in one call of the compiler, as chapter 2 describes, and caches no object.

```text
build/macos-arm64/release/lib/com/example/hello.antl
build/macos-arm64/release/lib/com/example/hello.antl.key
build/macos-arm64/release/hello.o
build/macos-arm64/release/hello
build/macos-arm64/release/hello.debug
build/macos-arm64/release/hello.debug.o
build/macos-arm64/release/hello.debug.lto/0.arm64.lto.o
build/macos-arm64/release/hello.map
dist/macos-arm64/release/hello
dist/macos-arm64/release/hello-symbols.zip
dist/macos-arm64/release/NOTICE.txt
```

The program of `dist/` carries no symbol data. `hello-symbols.zip` holds `hello.debug`, the same link with its debug sections kept, and `hello.map`, the map of the functions of the program. "anti symbols" below reads both.

| Program | Bytes |
|---|---:|
| `dist/macos-arm64/dev/hello` | 119,760 |
| `dist/macos-arm64/release/hello` | 90,208 |

### Targets and libraries

`--target <t>` writes `build/<t>/` and `dist/<t>/` beside those of the host, and the cache of each target is its own. A manifest with `default = ["macos-arm64", "linux-x86_64"]` makes a plain `anti build` write both.

```text
dist/linux-x86_64/dev/NOTICE.txt
dist/linux-x86_64/dev/probe
dist/macos-arm64/dev/NOTICE.txt
dist/macos-arm64/dev/probe
```

A library project has no `main`. Its build writes the library file of each module into `dist/`, which is what a path dependency and a repository take.

```text
build/macos-arm64/dev/lib/com/example/units.antl
build/macos-arm64/dev/lib/com/example/units.antl.key
dist/macos-arm64/dev/com/example/units.antl
```

A library file holds no machine code. One file serves all six targets, and the build of the program that uses it writes the object.

Where it lives: `src/anti/build.c` holds `anti build`, `anti run` and `anti new`, with the key in `cache_key`. `src/anti/units.c` collects the modules and the link facts of a program.

## anti check

`anti check` runs four classes of checks over the sources of a project and writes no program. It prints one line per class.

```sh
anti check
```

```text
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 1 block
anti check: doc warnings: 0
anti check: formatting: 1 file
```

| Class | What it does |
|---|---|
| Front end | the lexer, the parser and the checker on every source, with every warning as an error |
| Doc blocks | compiles each `anti` block of a doc comment |
| Doc warnings | reports what a doc comment gets wrong. With `--warn-undocumented` it reports every `pub` item without a `///` comment |
| Formatting | compares each file with what `anti fmt` writes |

The classes run in this order. The first class that fails ends the run, and the exit status is 1. The doc warnings fail nothing.

| Option | Effect |
|---|---|
| `--targets all` | run the front end once per target. The first line then reads `1 file, 6 targets` |
| `--warn-undocumented` | add the warning for a `pub` item without a doc comment |
| `<file.anti>...` | check those files. Without one, every source under the directories of `[layout]` |
| `--work <dir>` | the directory of the modules it writes for the doc blocks. `build/check` in the example |

With `--warn-undocumented`, a run over a `pub` function without a doc comment that is not in the canonical form prints:

```text
src/com/example/units.anti:22:8: warning: the pub item `yards` has no `///` comment [undocumented]
src/com/example/units.anti:22:1: error: the line is not the one `anti fmt` writes
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 1 block
anti check: doc warnings: 1
anti check: formatting: 1 file, 1 finding
```

A doc block that does not compile ends the run at the second class. The block below gives a `float` to an `int`.

```text
build/check/check/docs/com_example_units_1.anti:5:14: error: expected `int`, found `float`
anti: the `anti` block of `miles` at src/com/example/units.anti:27 failed, and its module is build/check/check/docs/com_example_units_1.anti
anti check: front end: 1 file, 1 target, 0 warnings
anti check: doc blocks: 2 blocks, 1 failed
```

An error of the front end ends the run at the first class.

```text
src/com/example/units.anti:34:9: error: the operands of `/` have the types `float` and `str`
anti check: front end: 1 file, 1 target, 0 warnings, 1 failed
```

A doc block compiles in one of two contexts. A block of a `///` comment becomes a module of its own that imports the module of the comment, so it reaches the public items alone. A block of a `//#` note is compiled inside a copy of the module, where the private items are in reach. A block without `fn main` is wrapped in a function. The example `let f = units.feet(2.0);` of `feet` is therefore checked against the real function.

Where it lives: `src/anti/check.c`. The front end is `antic --front-end --warnings-as-errors`, called through the driver.

## anti fmt

`anti fmt` writes every source in the canonical form. It prints the path of each file it changed. `anti fmt --check` writes nothing, prints the files that differ and exits with 1 when one does.

The canonical form:

- one tab per level of nesting
- the brace of an item body on a line of its own
- the brace of a statement block on the line of its statement
- no parentheses around a whole condition
- one statement per line
- a doc comment wrapped at 80 columns
- the names of a direct import in sorted order

The line breaks of the author stand where no rule moves them. A source before `anti fmt`:

```anti
import anti.text.{equal, Builder};
import anti.io;

/// Whether `n` is small. The doc comment of this function is longer than eighty columns, so the formatter wraps it.
fn small(n: int) -> bool {
    if (n < 10) { return true; }
    let a = 1; let b = 2;
    return (a + b) > n;
}
```

The same source after it:

```anti
import anti.text.{Builder, equal};
import anti.io;

/// Whether `n` is small. The doc comment of this function is longer than eighty
/// columns, so the formatter wraps it.
fn small(n: int) -> bool
{
	if n < 10 { return true; }
	let a = 1;
	let b = 2;
	return (a + b) > n;
}
```

The formatter sorted the two names of the import list and left the order of the two import lines. It dropped the parentheses around the condition of `if` and kept those around `a + b`, which are no whole condition. The block `{ return true; }` stays on its line, since the author wrote it there.

`anti fmt` works on the tokens of a file and not on its syntax tree. It uses the lexer of the compiler, so it formats a file that does not parse. An ordinary comment stays where it stands between two tokens.

A second run changes nothing: `anti fmt` prints no path and `anti fmt --check` exits with 0. The sources of the standard library and of the tests of Anti stand in this form.

Where it lives: `src/anti/fmt.c`.

## anti doc

`anti doc` writes one page per module and an index of them, from the doc comments. A `//!` comment at the top of a file documents the module, and a `///` comment documents the item that follows it.

```sh
anti doc -o docs
```

```text
doc: 1 module into docs
```

The command writes `docs/index.html` and `docs/com.example.units.html`. The page of the module:

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>com.example.units</title>
<main class="doc">
<h1>com.example.units</h1>
<p>Conversions between metres and feet.</p>
<section class="item" id="FEET_PER_METRE">
<h2>FEET_PER_METRE</h2>
<pre class="signature">pub const FEET_PER_METRE: float = 3.28084</pre>
<p>The feet in one metre.</p>
</section>
<section class="item" id="feet">
<h2>feet</h2>
<pre class="signature">pub fn feet(metres: float) -&gt; float</pre>
<p>The length <a href="#metres"><code>metres</code></a> in feet.</p>
<pre class="code">let f = units.feet(2.0);
</pre>
</section>
<section class="item" id="metres">
<h2>metres</h2>
<pre class="signature">pub fn metres(feet: float) -&gt; float</pre>
<p>The length <a href="#feet"><code>feet</code></a> in metres.</p>
</section>
</main>
```

The page is plain HTML with eight class names and no styling: `doc`, `index`, `item`, `signature`, `code`, `fields`, `members` and `internals`. A site gives them a style of its own.

Three facts of the page follow from how the tool reads a module.

- A signature is written from the checked interface. `f64` reads `float`, its other name.
- A code span that names an item of the module becomes a link to it. The parameter `metres` of `feet` links to the function `metres`, since the tool matches the name alone.
- An `anti` block of a doc comment becomes a `pre` of class `code`.

| Option | Effect |
|---|---|
| `-o <dir>` | the directory of the pages |
| `--markdown` | write Markdown, with each signature and each example as an `anti` block |
| `--private` | keep the items that are not `pub` |
| `--dev` | the developer's pages: the private items and the `//#` notes of the source |
| `<file.anti>`, `<file.antl>` | document those modules. Without one, the modules of the project |

The user pages come from the public interface alone, which a library file carries. `anti doc units.antl` writes the same bytes as `anti doc` over the source, so a package without source has its documentation. `--private` and `--dev` need the source.

```sh
anti doc --dev -o docs dist/macos-arm64/dev/com/example/units.antl
```

```text
anti: dist/macos-arm64/dev/com/example/units.antl: --dev needs the source
```

A module with three `pub` items and two private ones gives 3 sections without an option and 5 with `--private` or `--dev`.

`anti build --strip-docs` leaves the doc text out of the library files, and a page from such a file holds the signatures alone.

Where it lives: `src/anti/doc.c`, and `src/antic/docpage.c` for the page of one module.

## anti license

`anti license` prints licence notices. A notice is one line per package, with its version and its licence, and then each licence text once.

| Form | What it prints |
|---|---|
| `anti license` | the licence of antic and anti and of every component of the package: 4,016 lines for Anti 0.2.0 on macos-arm64 |
| `anti license --project` | the packages the project links, from `anti.lock` and the bundled modules it imports. It builds the project first |
| `anti license --project --notice` | writes that notice as `NOTICE.txt` in `dist/` and prints nothing |
| `anti license --from <binary>` | the notice a program or a shared library of Anti carries inside it |
| `anti license --from-archive <library>` | the licence fields of the package header in a static library of `--lib static` |

The first lines of the plain form:

```text
package antic 0.2.0 MIT
package anti 0.2.0 MIT
package anti.rt 0.2.0 0BSD
package apsl - APSL-2.0
package compiler-rt - Apache-2.0 WITH LLVM-exception
package glibc 2.35-0ubuntu3 LGPL-2.1-or-later
```

`anti license --project` in `com.example.hello` prints 19 lines. The first five:

```text
package anti.rt 0.2.0 0BSD
package com.example.units 1.2.0 MIT
package anti 0.2.0 0BSD
package com.example.hello 0.1.0 
text for anti.rt anti
```

The text of the BSD Zero Clause License follows. `com.example.units` names MIT in its manifest and no `license_text`, so the notice holds its line and no text for it. `com.example.hello` names no licence, and its line ends after the version.

`anti build` writes the same notice as `NOTICE.txt` beside every program and every shared library. The program carries it as well, in the data `anti_licenses`, and `anti license --from dist/macos-arm64/dev/hello` prints the same 19 lines from the binary alone.

```sh
anti license --from-archive libgeometry.a
```

```text
package geometry 0.0.0 
```

The archive is the library of chapter 17, built by antic without a package version, so its header names `0.0.0` and no licence.

Where it lives: `src/anti/licensing.c`, and `src/antic/notice.c` for the notice a build writes into a program.

## anti symbols

A release program carries no symbol data, and a trace it prints is a list of addresses. `anti symbols` keeps the symbols archives of a deployment together and turns such a trace into names and lines. It has three forms.

| Form | What it does |
|---|---|
| `anti symbols inventory --conf <config.toml> [--from <dir>] [--out <symbols.zip>]` | finds the binaries of a deployment and folds their archives into one |
| `anti symbols check --conf <config.toml> [--symbols <symbols.zip>]...` | reports per binary whether its symbols are present, stale or missing |
| `anti symbols resolve <trace.txt> --symbols <symbols.zip>...` | prints a raw trace with the function and the line of each frame |

The commands start from the runtime configuration of chapter 15. They find the program in the directory of the configuration, and the libraries that its `plugins` key and its `[injections]` table name. Every binary is known by its build id, the 64 hex digits its notice carries. An archive belongs to a binary when the two ids are equal.

The deployment of the example is one directory with three files: the program `tracer` of `tests/anti-symbols`, its archive `tracer-symbols.zip` and the configuration `tracer.toml`.

```sh
anti symbols inventory --conf tracer.toml --out all.zip
```

```text
present ./tracer a6e65b24adfc2dc0c33d2cf2c685216182a1a39e9b0c119db5796f185c1ec40a
wrote all.zip with 1 of 1 modules
```

`all.zip` holds the symbols of every binary under a directory named after its build id, and an index.

```toml
# The symbols archives of one deployment, by build id. Each entry stands
# under a directory named after its id.

[[module]]
module = 'tracer'
id = 'a6e65b24adfc2dc0c33d2cf2c685216182a1a39e9b0c119db5796f185c1ec40a'
version = '1.2.0'
source = './tracer-symbols.zip'
```

`anti symbols check` prints the same `present` line and exits with 0. With the archive moved away it prints `missing` and exits with 1. `--symbols all.zip` gives it the inventory to read, and the binary is `present` again.

```text
missing ./tracer a6e65b24adfc2dc0c33d2cf2c685216182a1a39e9b0c119db5796f185c1ec40a
```

The program prints the raw trace of `StackTrace.text()`. A `module` line names a binary with its build id and the address it was loaded at. A frame is an address, and the number of its module with the offset into it.

```text
module 0 a6e65b24adfc2dc0c33d2cf2c685216182a1a39e9b0c119db5796f185c1ec40a 0x00000001025a0000 /home/ann/deploy/tracer
module 1 - 0x000000018a9d8000 /usr/lib/dyld
0x00000001025a0714 0+0x714
0x00000001025a5604 0+0x5604
0x000000018a9f84e4 1+0x204e4
```

```sh
anti symbols resolve trace.txt --symbols all.zip
```

```text
module 0 a6e65b24adfc2dc0c33d2cf2c685216182a1a39e9b0c119db5796f185c1ec40a 0x00000001025a0000 /home/ann/deploy/tracer
module 1 - 0x000000018a9d8000 /usr/lib/dyld
0x00000001025a0714 0+0x714 com.example.tracer.main tracer.anti:11
0x00000001025a5604 0+0x5604 main
0x000000018a9f84e4 1+0x204e4
```

The first frame gains its function and its line from the debug link of the archive. The source calls `capture` in `inner`, three calls below `main`. The release build inlined those calls, so the frame is that of `main`, and the line is the one of the call of `capture`. The second frame is the `main` of the C runtime, which the map of the program names and no line covers. The third lies in `dyld`, whose build id no archive holds, and stays raw.

The trace is resolved by its offsets, so it may come from another machine and another run. The address a module was loaded at does not enter.

A shared library and a plugin have no symbols archive written by `anti build`. `resolve` leaves a frame of a Windows program raw, since it reads no PDB.

Where it lives: `src/anti/syms.c`, with `src/anti/symmap.c` for the map and `src/anti/zip.c` for the archives.
