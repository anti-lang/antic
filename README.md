# Anti

Anti is a compiled, statically typed language. It has C layout, classes with
interfaces as inline sub-objects, errors as return values and structured
parallelism. `antic` is its compiler, written in C11 from scratch with no lexer
or parser generator. It emits GAS-style assembly text for six targets: Linux,
macOS and Windows, each on x86_64 and ARM64.

This repository holds the language. It has the compiler, the runtime, the
standard library, the build tool, the runtime archive and the documents that
specify all of it.

## Building

The build needs CMake 3.21 or newer and a C11 compiler. It also needs the
pinned LLVM tools, the sysroots of the six targets and the pinned raylib
source. Three scripts install them into the build tree:

```bash
cmake -P tools/get-llvm.cmake
cmake -P tools/get-sysroot.cmake
cmake -P tools/get-raylib.cmake
cmake -S . -B build
cmake --build build -j8
ctest --test-dir build -j8
```

`tools/llvm-pin`, `tools/sysroot-pins` and `tools/raylib-pin` hold the versions
and the SHA-256 digest of every archive. Nothing installs into a system
location.

`CMakePresets.json` carries two more configurations. `cmake --preset asan`
builds antic under AddressSanitizer and `cmake --preset ubsan` under both
AddressSanitizer and UndefinedBehaviorSanitizer. Each runs the full suite from
its own build directory.

## Installing

Anti 0.1.0 installs with one command on every host. The installers and the
packages are served from anti-lang.com, and `docs/distribution.md` describes the
package of each host and the checks it passes.

## Documents

| File | Contents |
|---|---|
| `docs/decisions.md` | Every settled decision, one entry each. The authority |
| `docs/anti-object-model.md` | Structs, enums, classes, interfaces and errors |
| `docs/anti-language-additions.md` | The features decided after the object model |
| `docs/tooling.md` | The `anti` build tool |
| `docs/distribution.md` | Packages, installers and the runtime archive |
| `docs/libraries-for-c.md` | Anti libraries that C programs link |
| `docs/notes/` | The choices inside each compiler pass |
| `docs/reports/` | What each session did |

## Licence

MIT for antic, the `anti` tool and the repository. The runtime in `rt/` and the
standard library in `std/` are 0BSD, each with its own `LICENSE`. Bundled
components keep their licences in `LICENSES/`.

## The book

"Writing a compiler" builds a compiler for a subset of this language, chapter by
chapter, at
[foundingfuture.com](https://foundingfuture.com/programming/writing-a-compiler/).
Its source is in `FoundingFuture/book-writing-a-compiler`. This repository is the
finished language and does not follow the book's order.
