---
title: "The build tool"
description: "What the anti command does for a project: manifest and lock file, repositories and resolution, dev and release builds, checks, docs, bindings, libraries for C, licences and symbols."
summary: "What `anti` does and how a project uses it, without its code: the manifest and lock file, repositories and resolution, the module cache, dev and release builds, `anti check`, `anti fmt`, `anti doc`, `anti bind`, libraries for C, `anti license`, `anti symbols` and `anti sdk`."
date: 2026-09-15T04:05:22+02:00
lastmod: 2026-10-18T21:30:00+02:00
draft: false
weight: 240
tags: [compilers, programming-languages]
keywords: [build tool, package manifest, lock file, dependency resolution, package repository, incremental build, code formatter, licence notice]
---

## Previously

[Chapter 23, The runtime archive]({{% relref "/programming/writing-a-compiler/23-runtime-archive" %}}), describes what antic reads beside its own executable. The chapter covers the runtime library that the build compiles for every target and the archive that llvm-ar writes. Beside those stand the sysroot of each target and the licence notice of a program. The native libraries and the generated shims are named there as the work of later chapters.

## Tool scope

The command `anti` is the user-facing tool of Anti, and antic stays the bare compiler. The command `anti build` resolves the dependencies of a project, fetches their library files and compiles the project. The compiler antic keeps working without a manifest, with every library file on its command line or under a search root. Nothing is fetched in that mode.

The tool is declarative. It reads the manifest `anti.toml` and runs the steps that the file describes. Scripting, custom build steps and plugins have no place in it, and anything outside the manifest is not a build concern of `anti`.

The tool links the driver of antic and calls it in its own process. It calls it once per module in dev mode and once for the whole program in release mode. It starts no second compiler. The formatter and the documentation generator read the tokens and the syntax tree that antic reads, so neither can disagree with the language. The licence of `anti` is MIT. This chapter describes what `anti` does and shows none of its code.

## Commands

The table holds every command that `anti --help` lists, and the test `build_tool_page` compares the two.

| Command | Effect |
|---|---|
| `anti new <name>` | Create a project with the default layout and a starter `anti.toml` |
| `anti build [--release] [--target <t>\|all] [--offline] [--strip-docs] [--cpu <level>] [--memory-checks] [--lto full\|thin\|none]` | Resolve, fetch, compile and link, in dev mode by default |
| `anti build --lib static [--bundle-runtime]` | Static library and header, then print the link line |
| `anti build --lib shared [--soname]` | Shared library and header, with an import library on Windows |
| `anti run [--release]` | Build for the host, then run the program |
| `anti test [--release] [--memory-checks] <file.anti>...` | Compile each module with its `tests` and `fixtures` blocks, write a runner and run it |
| `anti check [--warn-undocumented] [--targets all]` | Front end, doc blocks, doc warnings and formatting |
| `anti fmt [--check]` | Format source files in place, or list the files that differ |
| `anti doc [--dev] [--private] [--markdown] [<file.antl>...]` | Write user docs or developer docs |
| `anti bind <api.json>\|--clang <header>` | Write a binding module and, when needed, a shim |
| `anti bind --header <file.antl>` | Write a C header from a library file without the source |
| `anti license [--project [--notice]] [--from <binary>] [--from-archive <library>]` | Print or write licence and attribution text |
| `anti symbols inventory --conf <config.toml>` | Fold the symbols archives of a deployment into one |
| `anti symbols check --conf <config.toml>` | Report per binary whether its symbols are present, stale or missing |
| `anti symbols resolve <trace.txt> --symbols <symbols.zip>` | Print a raw trace with the function and the line of each frame |
| `anti sdk export` | Pack the stubs of Apple's SDK into a bundle, on a Mac |
| `anti sdk import <bundle>` | Unpack that bundle into the sysroot of Anti on any host |

More than one command takes `--runtime <dir>`, `--work <dir>` and `-I <dir>`. They name the package, a directory of scratch files and a search root. The options `--profile-generate` and `--profile-use <file>` of `anti build` and `anti run` are the two steps of a build with a profile.

## Project layout

A project is the directory that holds `anti.toml`. Four directories below it have default names.

| Directory | Holds |
|---|---|
| `src/` | Anti source files, one module per file, in directories that mirror the module paths |
| `test/` | Further sources, which `anti check`, `anti fmt` and `anti doc` read beside `src/` |
| `build/<target>/<mode>/` | The library files of the project under `lib/`, the cached objects of dev mode under `obj/`, and the object and the link of the program |
| `dist/<target>/<mode>/` | The program with `NOTICE.txt`, the library for C with its header, or the `.antl` files of a library project |

The mode `<mode>` is `dev` or `release`. A target directory has one of six names: `linux-x86_64`, `linux-arm64`, `macos-x86_64`, `macos-arm64`, `windows-x86_64` and `windows-arm64`. The runtime archive uses the same names under `lib/`. The directory `build/` is disposable, and `dist/` holds what a user runs or links against. The table `[layout]` in `anti.toml` overrides any of the four names.

The command `anti new com.example.hello` writes `anti.toml`, `src/com/example/hello.anti` with a `main` that prints `hello`, and an empty `test/`, in the directory `hello`.

## Manifest

The manifest `anti.toml` is a TOML file, a format in which a hash symbol starts a comment[^1]. The tool reads it with the TOML parser of the runtime, which the runtime configuration shares: tables, strings, integers, booleans, arrays and inline tables.

```toml
[package]
name = "com.niese.anti.geometry"
version = "0.3.0"
antic = "0.2"
license = "MIT"
license_text = "LICENSE"
attribution = ["Copyright 2026 Example"]

[layout]
src = "src"
test = "test"
build = "build"
dist = "dist"

[targets]
default = ["macos-arm64"]
all = ["linux-x86_64", "linux-arm64", "macos-x86_64", "macos-arm64", "windows-x86_64", "windows-arm64"]

[repositories]
ff = "https://anti.foundingfuture.com/repo"

[dependencies]
"com.foundingfuture.anti.tree" = { version = "1.2.4", repo = "ff" }
"com.niese.anti.matrix" = { path = "../libs" }
"com.niese.anti.noise" = { path = "../noise" }
```

A package is a set of modules named by a module path, the path of its root. Every module of the package has a path under that root, and each module compiles to its own `.antl` file. The table `[package]` holds these fields.

- `name` is the module path of the package, with segments of lowercase ASCII letters, digits and `_`.
- `version` is a version of one to three parts of digits, as `1.2.0`.
- `antic` is the lowest compiler version the project builds with, and `anti build` refuses an older compiler.
- `license`, `license_text` and `attribution` belong to the licences, described below.

The list `default` under `[targets]` names the targets of `anti build` without `--target`, the host when the list is absent. The list `all` names the targets of `anti build --target all`, all six when it is absent. The table `[repositories]` maps an alias to a URL prefix, and the alias is local to the manifest. The table `[dependencies]` maps a package name to an inline table. Its keys are quoted, because TOML turns a dotted bare key into a table for each part before the last[^1]. The tables `[inject]` and `[inject.test]` name the provider of an injectable interface. A key the tool does not know is not reported.

A version constraint has one of three forms.

| Form | Meaning |
|---|---|
| `"1.2.4"` | `>= 1.2.4` and `< 2.0.0` |
| `"=1.2.4"` | exactly `1.2.4` |
| `">=1.2.4"` | `1.2.4` or newer, with no upper bound |

The form `1.2.4+` does not exist. In Semantic Versioning a plus sign after the patch version starts build metadata[^2]. The first form admits every version below the next major version. Semantic Versioning increments the major version for every incompatible change of the public API[^2].

## Dependencies

A dependency table has one of three shapes.

| Shape | Source |
|---|---|
| `{ version = "1.2.4", repo = "ff" }` | The repository with that alias |
| `{ version = "1.2.4" }` | Each repository of the manifest in order, refused when more than one holds the package |
| `{ path = "../libs" }` | A directory on disk: an Anti project, which is built first and gives the library files of its `dist/`, or a directory of library files |

A `version` constraint may accompany `path`, and `anti` checks it against the version in the headers of the library files. The files of a path whose package header names the package of the key belong to it, each as the module its header names. A table that names both a repository and a path is refused.

The package carries the standard library as library files, and antic finds them without a manifest entry. The one module of it that binds a native library is `anti.regex` over PCRE2. The table `[dependencies]` names only packages fetched from a repository or read from a path.

## Lock file

The command `anti build` writes the file `anti.lock` next to `anti.toml`. It records the name and version of each resolved package, the repository URL or the path, the packages it needs and the SHA-256 digest of each `.antl` file of the package. A build with a lock file uses the locked versions and refuses a digest mismatch. An application commits `anti.lock`, and a library may omit it.

## Repositories

A repository is a URL prefix that serves static files, and it needs no server code. A Hugo site can serve one from its `static/` directory. The URL starts with `https://` or `file://`. The scheme `http://` is refused except for `127.0.0.1`, `localhost` and `[::1]`, where the resolver tests run against a local server without certificates.

```text
<prefix>/<name>/index.toml
<prefix>/<name>/<version>/<module>.antl
```

A repository has no inventory of its packages. The file `<prefix>/<name>/index.toml` exists or returns 404, and 404 means that the repository does not have the package. The index lists every published version with the modules of each version and the digest of each module.

```toml
name = "com.foundingfuture.anti.tree"
revision = 7

[[version]]
version = "1.2.3"
yanked = true
modules = [{ path = "com.foundingfuture.anti.tree", sha256 = "5b0c2d1e7a94f3c6b8d0e1f2a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6" }]
dependencies = []

[[version]]
version = "1.2.4"
modules = [
    { path = "com.foundingfuture.anti.tree", sha256 = "9f3a7c1d2e4b5a6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7" },
    { path = "com.foundingfuture.anti.tree.balance", sha256 = "0a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f9" },
]
dependencies = [{ name = "com.niese.anti.geometry", version = "2.0", repo = "https://anti.foundingfuture.com/repo" }]
```

The index follows four rules.

- A version is never removed. A yanked version keeps its entry with `yanked = true`, lock files that name it still resolve, and new resolutions skip it.
- The list `modules` names every `.antl` file of the version with its digest, so the resolver knows what to fetch before it downloads a library file.
- A `dependencies` entry names its repository by URL and never by alias.
- The number `revision` is the readable stamp of the file for the people who write it. The tool does not read it.

A repository on a subdomain, such as `https://anti.foundingfuture.com/repo`, can move to another host without a change to any lock file. `anti` has no command that publishes. The author of a package copies the library files of `dist/` under the prefix and writes the index.

## Module cache

Downloaded index files and library files stay in the cache of the user, shared by all projects. It is `~/.cache/anti` on macOS and Linux unless `XDG_CACHE_HOME` names another directory, and `%LOCALAPPDATA%\anti\cache` on Windows.

```text
index/<sha256-of-prefix>/<name>/index.toml
index/<sha256-of-prefix>/<name>/index.toml.checked
index/<sha256-of-prefix>/<name>/index.toml.etag
pkg/<name>/<version>/<module>.antl
```

An `https://` fetch runs the `curl` of the system, which macOS, Linux and Windows carry. A redirect of the server is followed when it leads to `https://` or to `http://` of the machine itself, and refused otherwise.

An entity tag, or ETag, is an opaque validator that tells representations of one resource apart[^3]. The tool stores the ETag of each index file and sends it in `If-None-Match` at the next check. A server whose tag still matches answers `304 Not Modified`, a response without a body[^3]. The file `index.toml.checked` is the stamp of the last check, and the check runs at most once per hour per index file. The option `--offline` never contacts a repository and fails when a needed file is not in the cache.

A cached library file is never fetched again. The tool verifies its digest from the index at download time and again before every build. The digest is the whole check of a downloaded file.

## Resolution

The command `anti build` resolves the dependency graph in four steps.

1. Read `anti.toml`. When `anti.lock` exists and every locked entry satisfies the manifest, use the lock file and skip to step 3.
2. Read the index of each dependency from its repository, or from each repository of the manifest in order. A path dependency reads the headers of its library files instead. Walk the dependencies of the versions until the graph is closed. Pick the highest version that is not yanked and satisfies every constraint on the package, and refuse when no version does. Write `anti.lock`.
3. Download every `.antl` file that is not in the cache and verify its digest.
4. Compile the project sources against every `.antl` file of the graph.

Step 2 costs one request per package per repository. With a warm cache and the 304 answer, that request transfers no file.

## Build modes

Dev mode rebuilds only the modules that changed, and release mode optimises the whole program. Both write `build/<target>/<mode>/` and copy what a user runs into `dist/<target>/<mode>/`.

### Dev mode

Dev mode is the default of `anti build`, `anti run` and `anti test`. It compiles with `-g` and with the checks of dev mode on.

- The tool compiles every module of the project into its library file under `lib/`, and every module the program reaches into its object under `obj/`. That covers the modules of the project, of the dependencies and of the standard library below them. The module with `main` is compiled from its source and linked in the same call.
- Every cached output has a key file beside it. Its first line holds the SHA-256 digest of the input, the version of antic, the target and the processor level. Marks follow for the lines of `-g`, for warnings as errors and for the memory checks. The input is the source for a library file and the library file for an object. The lines after it name the package fields, the dependencies, the injections and the digest of every library file the input reaches through its imports.
- A build compiles an output again when it is missing or its key differs from the one the build would write. The module with `main` is the one module every build compiles again, and its compile carries the link.
- A call into another module is a reference to its mangled symbol, and no function is inlined across modules.

The key of an object is over the library file and not over the source. A comment added to a source writes the library file again with the same bytes, and the object stays.

### Release mode

The option `--release` of `anti build`, `anti run` and `anti test` selects it. The build writes the library file of each module of the project, cached by its key as in dev mode, and then compiles the whole program in one call of the driver. That call loads every library file and optimises the whole program, as [chapter 10]({{% relref "/programming/writing-a-compiler/10-optimizer" %}}) describes. It caches no object. The link runs through full link-time optimisation, and `--lto thin` and `--lto none` choose the other two forms.

Release mode compiles without `-g` and with the checks of dev mode off. The program of `dist/` carries no symbol data. Beside it stands `<program>-symbols.zip`, which holds the same link with its debug sections kept and the map of the functions of the program. The command `anti symbols` reads it.

### Targets

The option `--target <t>` writes `build/<t>/` and `dist/<t>/` beside those of the host, and the cache of each target is its own. The command `anti build --target all` builds each target of the list `all`. Library files are target-independent, and lld links every target from any host against the sysroots of the package. Running a program needs a machine of its target. The option `--cpu <level>` picks the processor level of the target.

## Package header options

Each module of a project compiles into its own `.antl` file. The file holds these parts in this order.

1. The format version stamp, which antic checks before it reads further.
2. The package header, with name, version and the dependency list with name, constraint and repository URL. The licence fields `license`, `license_text` and `attribution` follow.
3. The module path, its imports, the libraries its `link` lines name and the `//!` text of the module.
4. The public interface: its types, its items with the `///` text of each, and the checked tree of every generic.
5. The lowered, unoptimised IR of every function body.

A library file never stores a `//#` note or an ordinary comment. [Chapter 9]({{% relref "/programming/writing-a-compiler/09-modules-and-library-files" %}}) describes the format. The tool hands the fields of the manifest to the driver as the options of antic below would.

| Manifest | Option of antic |
|---|---|
| `name` | `--package-name <name>` |
| `version` | `--package-version <version>` |
| each entry of `[dependencies]` | `--dependency <name>,<constraint>,<url>` |
| `license` | `--license <spdx>` |
| `license_text` | `--license-text <file>` |
| each line of `attribution` | `--attribution <line>` |

Without these options antic writes the module path as the name, the version `0.0.0` and empty licence fields. The option `--license-text` names a file whose content goes into the header, so the header never refers to the source tree. The option `--strip-docs` of `anti build` passes `--strip-docs` to antic, which then leaves the doc text out of the interface.

## Check command

The command `anti check` runs everything that writes no program. It checks four classes in order, prints one line per class and stops after the first class that fails.

1. The front end on every file of `src/` and `test/`: lexer, parser and checker, without IR, with every warning as an error. With `--targets all` it runs once per target.
2. Every fenced `anti` block in a doc comment, compiled as the section on doc blocks describes.
3. The doc warnings: markup outside the subset, a name in backticks that resolves nowhere, a `//#` note on a `pub` item without a `///` comment, and a doc comment that documents nothing. With `--warn-undocumented` every `pub` item without a `///` comment is also reported. The doc warnings fail nothing.
4. The formatting, which compares each file with what `anti fmt` writes.

The check passes `--doc-warnings` to antic, which then reports the doc warnings. Without the option antic is silent about documentation. The command `anti fmt --check` remains for a pre-commit hook that checks only the layout.

## Formatter

The command `anti fmt` rewrites the files it is given, or every `.anti` file under `src/` and `test/`, into one canonical form and prints the path of each file it changed. The command `anti fmt --check` writes nothing, lists the files that differ and exits with 1 when one does. Every Anti listing of this book uses that form.

- Indentation is one tab per level. A wrapped line gets one tab more, and nothing is aligned past the indentation.
- The brace of an item body stands on its own line.
- The brace of a statement block stands on the line of its statement. `} else {` and `} while` each come to one line.
- Parentheses around a whole condition are dropped, and each statement takes its own line.
- Doc comments are wrapped again at 80 columns, where a tab counts four.
- The names of a direct import stand in sorted order.

The line breaks of the author stand where no rule moves them, and no rule reflows an expression. The formatter works on the tokens of the lexer of antic and not on the syntax tree. It therefore formats a file that does not parse. An ordinary comment keeps its position between two tokens.

## Documentation

### Doc comment markers

Comments run from `//` to the end of the line or from `/*` to `*/`, and block comments do not nest. Four markers attach documentation to the syntax tree, and each has a line form and a block form that give the same text.

| Line form | Block form | Attaches to | Audience |
|---|---|---|---|
| `///` | `/** */` | The next item or field | Whoever can see the item |
| `//!` | `/*! */` | The module | The module's user |
| `//#` | `/*# */` | The next item | Developers of the library |
| `//#!` | `/*#! */` | The module | Developers of the library |

Visibility decides the audience of `///`. On a `pub` item it documents the contract for the user of the library, and on a private item it documents the item for its developer. A `//#` note records an implementation detail, such as the invariant of a struct or the reason for an algorithm. The `//!` text is the user's guide to the module, and `//#!` is the developer's guide. [Chapter 4]({{% relref "/programming/writing-a-compiler/04-lexer" %}}) defines both forms in the lexer.

A block of one line holds its text between the opener and `*/`. Over more than one line the text starts on the line after the opener and ends on the line before `*/`. Text on a delimiter line is a lexical error. A fenced code block inside a block comment must not contain `*/`, so such an item takes the line form.

### Doc markup

Doc text is a fixed subset of Markdown, and `anti doc --markdown` passes it through unchanged.

- Paragraphs, separated by a blank line
- Fenced code blocks with a language tag, where `anti check` compiles `anti` blocks and skips `text` blocks
- Inline code in backticks, where a name that resolves in the module becomes a link
- Unordered lists with `-`
- Links as `[text](url)`

Headings, emphasis, tables, images, numbered lists and HTML are outside the subset. The check warns on each of them.

### Doc block compilation

The check compiles an `anti` block in one of two contexts. A block in `///`, `/** */`, `//!` or `/*! */` compiles as a separate module that imports the module of the comment. Its names are qualified as a user writes them, so an example that uses a private item fails the check. A block in `//#`, `/*# */`, `//#!` or `/*#! */` compiles inside a copy of the module and may use private items unqualified. A block without `fn main` is wrapped in a function. Blocks go through the front end only and never run.

### Documentation generator

The command `anti doc` writes user docs, and `anti doc --dev` writes developer docs.

| Marker | On a public item | On a private item |
|---|---|---|
| `///` | User docs and developer docs | Developer docs |
| `//#` | Developer docs | Developer docs |

User docs need only a library file, as in `anti doc tree.antl`, because the interface holds the doc text. A page built from a library file equals the page built from the source. A library user needs neither the source nor an online copy, and the standard library documents itself from the package. Developer docs need the source, and `--dev` refuses a library file. The option `--private` adds private items to the user docs and needs the source as well.

The output is one HTML page per module with an index page, or Markdown with `--markdown` for a Hugo site. The HTML carries eight class names and no styling: `doc`, `index`, `item`, `signature`, `code`, `fields`, `members` and `internals`. A fenced `anti` block of a doc comment becomes a `pre` of class `code`.

## Bindings

The command `anti bind` writes an ordinary `.anti` module from a C API description. The command `anti bind <api>.json` reads a JSON description in the format of raylib's rlparser. The command `anti bind --clang <header>` runs clang with `-fsyntax-only -Xclang -ast-dump=json` on the header and reads the JSON that clang writes. It uses the pinned clang in a development tree and otherwise the first clang on `PATH`, and it refuses a clang whose major version differs from the pinned one. The package ships no clang, so a user needs one for `--clang` alone, and a binding is generated once and committed.

The module is `anti.<name of the file>` unless `--module <path>` names another, and it goes to `<dir>/<last segment>.anti` under the directory of `-o`. The options `-I` and `-D` reach clang and the shim. The layout of a bound struct is computed by antic, as for every struct, and never read from clang.

A binding uses the C types of Anti, such as `c_int` and `c_char`, so it reads like the header. A typedef of the C library stands for the Anti type of its meaning on every target, so `int64_t` is `i64`. A declaration that has no Anti type, such as one that names `long double`, is left out with a warning that says why, and the command goes on.

A `static inline` function of a C header has no symbol. When the header has such functions, `anti bind` writes `shim_<name>.c` next to the binding, with an exported wrapper of the same name for each. Shim and binding come from one run of the generator, so they always match.

The option `--probe` writes the ABI probe as `probe_<name>.c` and `probe_<name>.anti`. Each prints the size and the alignment of every bound record, and per field the bytes of an object that holds only that field set. The two programs print the same lines when antic lays the records out as C does. [Chapter 18]({{% relref "/programming/writing-a-compiler/18-structs-and-arrays" %}}) runs the probe of `tests/abi` in the test suite.

## Libraries for C

The option `--lib` of `anti build` turns a library project into a library for C. A project that defines `main` is refused, because the C program owns the process. Both forms write `<name>.h` next to the library, where `<name>` is the last segment of the package name, and `anti bind --header <name>.antl` writes the same header from a library file alone.

| Target | `--lib static` | `--lib shared` |
|---|---|---|
| `linux-*` | `lib<name>.a` | `lib<name>.so` |
| `macos-*` | `lib<name>.a` | `lib<name>.dylib` |
| `windows-*` | `<name>.lib` | `<name>.dll` and the import library `<name>.lib` |

A static library holds no runtime. The tool prints the command line that links a C program with the archive and the `libanti_rt.a` of the target and its level. The option `--bundle-runtime` puts the runtime into the archive, and the printed line then names the archive alone. A shared library links the runtime in. The glue of `anti.regex` and PCRE2 are never in the archive. The printed line does not name them yet, so a C program of such a library adds the archives `anti_rt_regex` and `pcre2-8` of `lib/<target>/` of the package after the runtime.

A dev build of the library project `com.example.geo` for macos-arm64 prints this line, with the path of the package in place of `<package>`.

```text
cc main.c ./build/macos-arm64/dev/libgeo.a <package>/lib/macos-arm64/armv8.5/libanti_rt.a
```

With `--bundle-runtime` after `--lib static`, the same build prints `cc main.c ./build/macos-arm64/dev/libgeo.a`.

The option `--soname` writes the version from `anti.toml` into a shared library. On Linux the file is `lib<name>.so.<major>` with a `lib<name>.so` symlink, and on macOS the compatibility version and the current version carry it. Windows has no equivalent, and without the option a library carries no version. Every library of macOS has the install name `@rpath/<file name>`, with the option and without it, so a program that links one names its directory with `-rpath`. The major version is the compatibility version, and a change to any exported signature or struct requires a new major version. The shared library of `com.example.geo` at version 0.1.0 names these, as llvm-objdump prints them.

```text
dist/macos-arm64/dev/libgeo.dylib:
	@rpath/libgeo.dylib (compatibility version 0.0.0, current version 0.1.0)
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1356.0.0)
```

Chapter 25, Libraries for C, shows how antic builds these outputs.

## Licences

### Licence fields

Three keys of `[package]` name the terms under which a package is distributed.

```toml
[package]
name = "com.foundingfuture.anti.tree"
version = "1.2.4"
license = "MIT"
license_text = "LICENSE"
attribution = ["Copyright 2026 Eddie", "Contains code from Example Corp, BSD-3-Clause"]
```

- `license` is an identifier of the SPDX License List, such as `MIT`, `0BSD` or `Apache-2.0`[^4].
- `license_text` is the path of the file that holds the full text, relative to the root of the project.
- `attribution` holds zero or more lines, copied verbatim.

The library file header carries all three, with the content of the file in place of a path. The library files of the standard library carry the same fields.

### Embedded licence text

The driver knows every module of an executable or a shared library at link time. It emits one read-only data object with the symbol `anti_licenses`, between the marker lines `ANTI_LICENSES_BEGIN` and `ANTI_LICENSES_END`. The line `build <id>` follows the begin marker, with the build id of the binary in 64 hex digits, which `anti symbols` matches against the symbols archives. A line `package <name> <version> <license>` stands for each package, followed by its `attribution` lines. A line `text for <names>` then introduces each distinct licence text once. The runtime is the package `anti.rt` and the standard library the package `anti`, both with the antic version and 0BSD. A program of musl names musl and mimalloc after the runtime. The embedding is unconditional, so the notices travel with the binary.

A release build of the program `com.example.hello` 2.0.0 for macos-arm64, which names `license = "MIT"` and one attribution line and depends on the path of `com.example.geo` 0.1.0 with its licence file, writes this `NOTICE.txt` beside the program. The bytes between the markers of the program are the same, after the `build` line.

```text
package anti.rt 0.2.0 0BSD
package com.example.geo 0.1.0 MIT
package anti 0.2.0 0BSD
package com.example.hello 2.0.0 MIT
attribution Copyright 2026 Example
text for anti.rt anti
BSD Zero Clause License

Copyright (c) 2026 Eddie Niese / Founding Future

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
text for com.example.geo
Permission is granted to use this example for any purpose.
```

A static archive carries no `anti_licenses`. It has no link step, and two archives in one C program would define the symbol twice. It carries a copy of the package header of its library file instead, licence fields included.

The command line of a program stays untouched. The module `anti.license` of the standard library returns the text, and a program that wants to print it does so itself. Nothing in the language depends on it.

### License command

The command `anti license` has five forms.

| Form | Prints |
|---|---|
| `anti license` | The licence of antic and `anti`, then every component of the package with name, version, identifier and full text |
| `anti license --project` | The packages that the project links, from `anti.lock` and the imported bundled modules, with identifiers, attributions and texts. It builds the project first |
| `anti license --project --notice` | The same content, written as `dist/<target>/<mode>/NOTICE.txt` |
| `anti license --from <binary>` | The notice `anti_licenses` of a program or a shared library of Anti, found by its markers, with the upstream source of each component after its text where `licenses/sources.txt` of the package names one |
| `anti license --from-archive <library>` | The licence fields from the package header copy in a static archive, for the notice of a C project |

The package holds a `licenses/` directory with one file per component, which `anti license` reads. Beside them stands `sources.txt`, the record of the upstream source of every pinned component, written from the pins.

### Obligations of a shipped program

Code emitted by antic is the user's code translated, and the licence of antic never reaches its output. The LLVM Exceptions cover the object files of llc. When compiling embeds portions of LLVM into an object form, those portions may be redistributed without sections 4(a), 4(b) and 4(d) of the Apache License[^5]. Static linking copies components of the package into an executable, and each brings its licence.

| Component | Licence | Linked into |
|---|---|---|
| Runtime and standard library | 0BSD, which asks for nothing[^6] | Every program |
| musl | MIT | Every Linux program that links statically |
| mimalloc | MIT | Every Linux program that links statically |
| PCRE2 | BSD 3-clause with the PCRE2 exception, whose second condition covers a binary redistribution[^7] | A program that imports `anti.regex` |

The notice of a binary names the runtime, the standard library, musl, mimalloc and the packages of the project, with their texts. It names no PCRE2 yet. The text of PCRE2 stands in `licenses/pcre2.txt` of the package. The table records how the licence texts read and gives no legal advice.

### Licence choices

The runtime in `src/rt/` and the standard library in `src/std/` are 0BSD. The common case of a program therefore embeds one short notice and owes nothing. The compiler antic and the tool `anti` are MIT. The site decides the licence of the book text separately.

## Symbols command

A release program carries no symbol data, and a trace it prints is a list of addresses. The command `anti symbols` keeps the symbols archives of a deployment together and turns such a trace into names and lines.

- `anti symbols inventory --conf <config.toml>` reads the runtime configuration and finds the program beside it, the libraries of its `plugins` directories and those of `[injections]`. It folds the symbols archive of each into one archive keyed by build id, with an index of module, id, version and source. `--from <dir>` names the directory of the archives, which otherwise stand beside each binary, and `--out <symbols.zip>` the archive it writes.
- `anti symbols check --conf <config.toml>` reports per binary whether its symbols are present, stale or missing, and exits with 1 unless every one is present. Each `--symbols <symbols.zip>` names an archive to read.
- `anti symbols resolve <trace.txt> --symbols <symbols.zip>` prints a raw trace with the function and the line of every frame whose build id an archive holds, and leaves every other frame raw.

Every binary is known by the build id its notice carries, and an archive belongs to a binary when the two ids are equal. The trace is resolved by its offsets, so it may come from another machine and another run.

## SDK command

A program for macOS that names a framework with `link framework "Name";` links against the stubs of Apple's SDK, which no package carries. The command `anti sdk export` packs the `.tbd` stubs and the version of the SDK into `apple-sdk-<version>.tar.xz` on a Mac, from the SDK of `--sdk <MacOSX.sdk>` into the directory of `-o`. The command `anti sdk import <bundle>` unpacks that bundle into the sysroot of Anti on any host, or into the directory of `--sysroot <dir>`. Apple's licence governs where the bundle may be used.

## Next

Chapter 25, Libraries for C, builds static and shared libraries from Anti code. It covers the export marker, the signature rule and the generated header. It also covers the runtime in both forms, symbol visibility, memory and threads across the boundary, versioning, and the tests that check them.

## References

[^1]: Tom Preston-Werner and others, *TOML v1.0.0*, sections "Comment" and "Keys", https://toml.io/en/v1.0.0

[^2]: Tom Preston-Werner, *Semantic Versioning 2.0.0*, summary and items 8 and 10, https://semver.org/

[^3]: Internet Engineering Task Force, *RFC 9110, HTTP Semantics*, sections 8.8.3, 13.1.2 and 15.4.5, https://www.rfc-editor.org/rfc/rfc9110.html

[^4]: Linux Foundation, *SPDX License List*, the identifiers of the listed licences, https://spdx.org/licenses/

[^5]: LLVM Project, *LLVM Exceptions to the Apache 2.0 License*, in `LICENSE.txt`, https://llvm.org/LICENSE.txt

[^6]: Open Source Initiative, *Zero-Clause BSD*, https://opensource.org/license/0bsd

[^7]: Open Source Initiative, *The 3-Clause BSD License*, condition 2, https://opensource.org/license/bsd-3-clause
