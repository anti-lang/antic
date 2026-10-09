# Linking

Choices made for linking antic and the programs it builds.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- On macOS every link of the CMake build prints `ld: warning: ignoring -lto_library '<root>/build/deps/clang/lib/libLTO.dylib', file does not exist`. The pinned clang driver links antic, `anti` and the helpers with Apple's ld64. The driver adds `-lto_library` to every link unless `-fuse-ld=lld` names the linker, in `clang/lib/Driver/ToolChains/Darwin.cpp`. No other driver flag turns it off. `-fuse-ld=lld` fails against the SDK of Xcode, whose `libSystem.tbd` names the target `arm64e.x1` that the pinned lld rejects. The packer passes `--ld-path` to `ld64.lld`, which gets the flag too and ignores it without a warning. The clang archive of `anti-lang/llvm-tools` holds no `libLTO.dylib`, and the build never ships one. ld64 loads that library only for a bitcode input. No build of antic makes one, so the warning stays and does no harm. antic itself never prints it, because `src/antic/linker.c` calls `ld64.lld` and `ld -r` directly.

## The tools and the sysroots of a link

- Every linker is the flavour of lld in `bin/` of the runtime archive, which is
  `bin/` of the package beside antic. `link_facts` asks `driver_archive_tool`
  for it before the compile and refuses a missing one by its path. `--linker
  platform` is the one link that runs a program of the host.
- Every target links against `sysroot/<target>` of the archive, the Windows
  targets included, and a missing sysroot is refused with the line that it is
  the package's. Every lld-link command carries `/lldignoreenv`, so the library
  directories of `LIB`, which an MSVC environment sets, reach no link. On a
  Windows host lld-link also finds the installed Visual Studio and Windows SDK
  through the setup configuration and the registry, under `/lldignoreenv` too,
  and the Windows VM read `ucrt.lib` from Windows Kits with `LIB` unset. So
  every lld-link command names `crt` and `sdk` of the sysroot as `/vctoolsdir`
  and `/winsdkdir`, which ends the detection, and a library the sysroot lacks
  stays missing. `own_tools` checks both with stand-in archives of links.
