# Linking

Choices made for linking antic and the programs it builds.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- On macOS every link of the CMake build prints `ld: warning: ignoring -lto_library '<root>/build/deps/clang/lib/libLTO.dylib', file does not exist`. The pinned clang driver links antic, `anti` and the helpers with Apple's ld64. The driver adds `-lto_library` to every link unless `-fuse-ld=lld` names the linker, in `clang/lib/Driver/ToolChains/Darwin.cpp`. No other driver flag turns it off. `-fuse-ld=lld` fails against the SDK of Xcode, whose `libSystem.tbd` names the target `arm64e.x1` that the pinned lld rejects. The packer passes `--ld-path` to `ld64.lld`, which gets the flag too and ignores it without a warning. The clang archive of `anti-lang/llvm-tools` holds no `libLTO.dylib`, and the build never ships one. ld64 loads that library only for a bitcode input. No build of antic makes one, so the warning stays and does no harm. antic itself never prints it, because `src/antic/linker.c` calls `ld64.lld` and `ld -r` directly.
