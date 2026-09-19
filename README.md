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

The build needs CMake 3.21 or newer and openssl. The pinned clang compiles it, and
the configure step installs that clang into `build/clang` and the pinned LLVM tools
into `build/llvm`. The Linux sysroots take the builtins of the pinned clang, so
`tools/get-clang.cmake` runs before `tools/get-sysroot.cmake`:

```bash
cmake -P tools/get-clang.cmake
cmake -P tools/get-llvm.cmake
cmake -DDEST=build/sysroot -DLLVM_BIN=build/llvm/bin -DACCEPT_LICENSE=yes \
      -DTARGETS="linux-x86_64;linux-arm64;macos-arm64;macos-x86_64;windows-x86_64;windows-arm64" \
      -P tools/get-sysroot.cmake
cmake -DDEST=build/raylib -P tools/get-raylib.cmake
cmake -S . -B build
cmake --build build -j8
ctest --test-dir build -j8
```

`-DACCEPT_LICENSE=yes` accepts the terms of the Microsoft CRT and Windows SDK, which xwin
downloads for the two Windows targets. antic links programs for all six targets on every
host, so the build needs the sysroots of all six.

On Windows the configure step takes `-G Ninja`, because the Visual Studio generator
takes the compiler of its own toolset. A reader who only wants antic from source
passes `-DANTIC_SYSTEM_COMPILER=ON`, and the compiler of the machine builds it.

`tools/clang-pin`, `tools/llvm-pin`, `tools/sysroot-pins` and `tools/raylib-pin` hold
the versions and the SHA-256 digest of every archive. Nothing installs into a system
location. clang and the LLVM tools come from the releases of `anti-lang/llvm-tools`.
openssl checks their signature against `keys/release.pem`, and macOS, Linux and Git
for Windows carry it.

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
