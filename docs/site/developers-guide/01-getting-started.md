---
title: "Getting started"
description: "Installing the package of Anti, the first program, anti build and anti run, dev and release builds and where the files go."
summary: "The package per platform and its one-command install, antic and anti, a first program, `anti build` and `anti run`, the dev and the release mode and the directories an install and a build write."
date: 2026-10-11T08:10:24+02:00
lastmod: 2026-10-11T15:30:00+02:00
draft: true
weight: 10
tags: [compilers, programming-languages]
keywords: [install, package, antic, anti, hello world, dev build, release build]
---

Anti installs as one package per platform, with one command and no other download. The package holds the compiler `antic`, the tool `anti` and everything a build needs for all six targets. The chapter covers the install and a first program. It names every file an install and a build write.

## The package

A release of Anti has six packages, one per platform, named `anti-<version>-<target>.tar.xz`. The targets are `macos-arm64`, `macos-x86_64`, `linux-arm64`, `linux-x86_64`, `windows-arm64` and `windows-x86_64`.

Each package is complete on its own. It builds programs for all six targets with no network and no C compiler on the machine.

| Path in the package | Contents |
|---|---|
| `bin/` | `antic`, `anti` and the pinned LLVM tools, `opt`, `llc`, `lld` and `llvm-ar` among them |
| `std/` | the standard library, one library file per module |
| `lib/<target>/<level>/` | the runtime of each target and processor level, as objects and as bitcode |
| `lib/<target>/` | the native libraries of each target |
| `include/` | the headers of the native libraries |
| `sysroot/` | the C library and the system libraries of each target |
| `licenses/` | the licence text of each component and `sources.txt`, the record of its upstream source |
| `LICENSE`, `VERSION` | the licence of Anti and its version |

The frameworks of Apple are the one thing a package lacks, since their licence allows no redistribution. A program that names one with `link framework` needs the stubs that `anti sdk export` packs on a Mac and `anti sdk import` unpacks on any host.

### Install with one command

On macOS and Linux:

```sh
curl -fsSL https://anti-lang.com/install.sh | bash
```

On Windows, in PowerShell:

```powershell
irm https://anti-lang.com/install.ps1 | iex
```

The installer runs these steps and nothing else.

1. It downloads the package of the host and `SHA256SUMS` from the GitHub release of the version.
2. It downloads `SHA256SUMS.sig` from anti-lang.com and checks it against the public key the installer carries.
3. It checks the SHA-256 of the package against the manifest.
4. It unpacks the package into the data directory of the user.
5. It runs `antic --version` and `anti --version` of the package as the check of the install.
6. It copies the two programs into the bin directory of the user and offers to add that directory to the `PATH`.

A missing or wrong signature stops the install before anything is unpacked. The installer runs no CMake, compiles nothing and writes nothing outside the profile of the user.

| Setting | Effect |
|---|---|
| `--arm`, `--intel` | take the package of that processor, which a machine with emulation runs |
| `ANTI_VERSION` | install that version. The default is the newest release |
| `ANTI_HOME` | install everything under one directory, the two programs included |
| `ANTI_REPLACE`, `ANTI_PATH` | answer the two questions of the installer with `yes` or `no` |

An option follows `bash -s --`, as in `curl -fsSL https://anti-lang.com/install.sh | bash -s -- --intel`.

### Install by hand

The same steps as commands, for version 0.2.0 on macos-arm64. The two `openssl` lines are the signature check of the installer.

```sh
package=anti-0.2.0-macos-arm64.tar.xz
release=https://github.com/anti-lang/antic/releases/download/v0.2.0
curl -fsSLO $release/$package
curl -fsSLO $release/SHA256SUMS
curl -fsSLO https://anti-lang.com/downloads/anti/0.2.0/SHA256SUMS.sig
curl -fsSLO https://anti-lang.com/keys/release.pem
openssl dgst -sha256 -binary -out SHA256SUMS.sha256 SHA256SUMS
openssl pkeyutl -verify -pubin -inkey release.pem -in SHA256SUMS.sha256 -sigfile SHA256SUMS.sig
shasum -a 256 --ignore-missing -c SHA256SUMS
mkdir -p ~/.local/share ~/.local/bin
tar -xJf $package -C ~/.local/share
cp ~/.local/share/anti/bin/antic ~/.local/share/anti/bin/anti ~/.local/bin/
antic --version
anti --version
```

Linux spells the digest check `sha256sum --ignore-missing -c SHA256SUMS`. The last two commands print `antic 0.2.0` and `anti 0.2.0`.

The packages and `SHA256SUMS` stand on GitHub, and the signature and the key on anti-lang.com. A forged release therefore needs both hosts.

## antic and anti

`antic` is the compiler. It takes one source file and writes one executable, and it reads no project file.

```sh
antic [options] <file.anti> [<library.antl>...] [<object.o> <archive.a>...]
```

| Option | Effect |
|---|---|
| `-o <file>` | the output. Without it the output is the input without `.anti` |
| `--dev` | dev mode. Release mode is the default of `antic` |
| `--target <name>` | compile and link for that target. The host is the default |
| `--cpu <level>` | the processor level. Chapter 20 lists the levels |
| `-S` | stop after writing the assembly text |
| `-c` | write the library file `<file>.antl` of the module and stop |
| `-I <dir>` | a search root: module `a.b` is `a/b.anti` or `a/b.antl` under it |
| `-g` | write the source positions and keep the debug sections |

`antic --help` prints the full list. An `import` resolves in one of three places: a library file on the command line, a file under a search root, or `std/` of the package.

`anti` is the tool for a project. It reads the manifest `anti.toml` and resolves the dependencies. It compiles through the driver of antic, which is linked into `anti`, and starts no `antic` process.

| Command | Effect |
|---|---|
| `anti new <name>` | write a starter project |
| `anti build` | resolve, fetch and compile the project |
| `anti run` | build for the host, then run the program |
| `anti test` | compile and run the `tests` blocks of the given files |
| `anti check` | run the front end, the doc examples, the doc warnings and the formatting check |
| `anti fmt` | write every source in the canonical form |
| `anti doc` | write the documentation pages of the modules |
| `anti bind` | write a binding module from a C header |
| `anti license` | print the licence notice of the package, a project or a binary |
| `anti symbols` | collect and read the symbols archives of a deployed program |
| `anti sdk export`, `anti sdk import` | pack and unpack the stubs of Apple's SDK |

Chapter 18 describes each command. `anti --help` prints the options of all of them.

## Hello

The first program is one file, `hello.anti`.

```anti
//! name: first
import anti.io;

fn main() -> int
{
	io.println("hello");
	return 0;
}
```

```text output
hello
```

`import anti.io;` makes the module `anti.io` reachable under its last segment, `io`. `main` returns an `int`, which becomes the exit status of the program. A `//` comment runs to the end of its line.

### Hello with antic alone

```sh
antic hello.anti
./hello
```

The first command writes two files beside the source: the executable `hello` and the object `hello.o`. The build is a release build, so the program and the runtime are linked through LTO. The second command prints `hello`.

`antic --target linux-x86_64 -o hello-linux hello.anti` writes the same program for Linux on any host. The package holds the runtime and the sysroot of every target, so a cross-build needs no other install.

### Hello as a project

`anti new` takes a package name, which is a module path of at least two segments. The directory takes the last segment.

```sh
anti new com.example.hello
cd hello
anti run
```

`anti new` writes the manifest `anti.toml`, the source `src/com/example/hello.anti` and the empty directory `test/`. The source is the program above with a doc comment of the module on top. The manifest names the package and its version.

```toml
[package]
name = "com.example.hello"
version = "0.1.0"
```

A name of one segment is refused: `` anti: `hello` is one segment, and a package name is a module path of at least two, as com.example.hello ``.

## anti build and anti run

`anti build` reads `anti.toml` of the current directory and builds the project for the host in dev mode. `anti run` does the same and then runs the program.

A dev build of `com.example.hello` on macos-arm64 writes these files.

| File | Contents |
|---|---|
| `anti.lock` | the resolved version of every dependency |
| `build/macos-arm64/dev/lib/com/example/hello.antl` | the library file of the module |
| `build/macos-arm64/dev/obj/anti/io.o` | the dev object of `anti.io`, one per module the program reaches |
| `build/macos-arm64/dev/hello.o` | the object of the module with `main` |
| `build/macos-arm64/dev/hello` | the linked program |
| `dist/macos-arm64/dev/hello` | the program, copied for delivery |
| `dist/macos-arm64/dev/NOTICE.txt` | the licence notice of what the program links |

The library file and each object under `obj/` have a file with the suffix `.key` beside them. The key is one line: the SHA-256 of the input, the version of antic, the target, the processor level and whether the build writes debug lines. `strict` follows in a build that refuses warnings and `memory-checks` in a build with `--memory-checks`. A build compiles a module again only when its key changes.

`build/` is disposable, and `dist/` holds what a user runs. Chapter 18 describes the manifest, the lock file and the cache.

`anti build --release` writes `build/macos-arm64/release/` and `dist/macos-arm64/release/` in the same way. Its `dist/` also holds `hello-symbols.zip`, the symbols archive of the program. `--target <name>` builds for another target and `--target all` for all six.

## Dev and release

A build has one of two modes. `anti build` and `anti run` take dev mode unless `--release` is given. `antic` takes release mode unless `--dev` is given.

| | Dev | Release |
|---|---|---|
| Compiles | one module per object, and only the changed ones again | the whole program at once |
| Checks | overflow, bounds, division, shifts and conversions stop the program with a message | dropped |
| `assert` | checked | dropped |
| Optimization | `llc -O1`, no inlining | `opt` at O3 and LTO with the runtime |
| A warning | is printed | is an error |
| Debug lines under `anti build` | on, as `-g` | off, with a symbols archive beside the program |

The program below multiplies past the range of an `int`. One factor comes from the command line, so the compiler cannot compute the product.

```anti
//! name: grow
import anti.io;

fn grow(n: int) -> int
{
	return n * 4611686018427387904;
}

fn main(args: []str) -> int
{
	if grow(args.len + 1) < 0 {
		io.println("the product wrapped");
	}
	return 0;
}
```

```text output
the product wrapped
```

The output above is the release build. `args.len` is 1 without arguments, and 2 times 2 to the power 62 is one past the largest `int`. The release build wraps to the least `int`, which is negative.

A dev build of the same file stops at the multiplication. It prints `grow.anti:6: overflow in *: left 2, right 4611686018427387904` to the standard error and aborts. The text stands in the dev module as a constant.

```llvm excerpt=grow target=linux-x86_64 mode=dev
overflow in *\00"
```

The release module holds no such text and no call of `anti_rt_check_failed`. `--checks` keeps the checks in a release build, and `--no-checks` drops them from a dev build. Chapter 2 shows the code of both modes for one function, and chapter 3 lists every check.

## File locations

An install follows the conventions of the platform and writes inside the profile of the user alone.

| | macOS and Linux | Windows |
|---|---|---|
| `antic` and `anti` | `$XDG_BIN_HOME`, or `~/.local/bin` | `%LOCALAPPDATA%\Programs\anti\bin` |
| The package | `$XDG_DATA_HOME/anti`, or `~/.local/share/anti` | `%LOCALAPPDATA%\anti` |
| The cache of fetched library files | `$XDG_CACHE_HOME/anti`, or `~/.cache/anti` | `%LOCALAPPDATA%\anti\cache` |

`antic` and `anti` find the package in two places, in this order. The first is the directory above the `bin/` they stand in, which is the package itself before it is installed. The second is the data directory of the table. `--runtime <dir>` names another.

A build writes inside the project alone.

| Path | Written by | Contents |
|---|---|---|
| `anti.lock` | `anti build` | the resolved dependencies |
| `build/<target>/<mode>/` | `anti build` | library files, objects and the linked program |
| `dist/<target>/<mode>/` | `anti build` | the program, `NOTICE.txt` and in release mode the symbols archive |
| `<output>` and `<output>.o` | `antic` | the executable and its object, beside each other |

Where it lives: `tools/install.sh` and `tools/install.ps1` are the installers. `tools/pack-anti.cmake` lays a package out. `src/antic/userdirs.c` holds the directories and `src/antic/main.c` the options of antic. `src/anti/build.c` holds `anti build`, `anti run` and `anti new`.
