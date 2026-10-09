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

## The Windows link against mingw-w64

- A Windows sysroot is `include/`, the headers of mingw-w64 with `_mingw.h` written
  for the UCRT, and `lib/`, the import libraries that llvm-dlltool wrote from the
  `.def` files of mingw-w64-crt beside `clang_rt.builtins.lib` of the pinned clang.
  `lib/ucrtbase.lib` is the file that shows the sysroot is complete. The step `mingw`
  of `docs/work-order-distribution.md` built it, and the test `windows_sysroot` lays
  one out from a stand-in release.
- Every lld-link command runs in the mingw mode of lld-link, `/lldmingw`, which ties
  the `.pdata$f` and `.xdata$f` sections of the gnu objects to their function f and
  looks for no Visual Studio of the machine, passes `/lldignoreenv`, drops
  `msvcrt.lib`, `libcmt.lib`, `oldnames.lib` and `uuid.lib` by name with `/NODEFAULTLIB`, gives
  `lib/` of the sysroot as its one `/LIBPATH`, and after the runtime names
  `clang_rt.builtins.lib`, `ucrtbase.lib`, `ntdll.lib` and `kernel32.lib`. Nothing of
  Microsoft is linked. The four names matter because the runtimes of compiler-rt name
  them in their directives, and lld-link refuses a default library it cannot find. A
  directive that names another library, as the probe of raylib names `user32.lib`,
  reaches lld-link and is found in `lib/`.
- ucrtbase.dll holds the C library alone. What the static libraries of Microsoft gave
  beside it, the entry points `mainCRTStartup`, which stands in
  `src/rt/platform_entry.c` so that only the link of a program pulls it in, and
  `DllMainCRTStartup`, the printf family with `atexit`, weak in `src/rt/platform_stdio.c`
  and a member as an object alone, the tables
  of the initialisers that `.CRT$XCU` constructors land in, the directory of
  thread-local storage, `_fltused`, `__chkstk`, the cookie of the stack check,
  `atexit` and the printf family, stands in `src/rt/platform_windows.c`, so every
  program and every C library of a Windows target links `anti_rt.lib`. A plugin
  reaches those names through the import library of its host, and the packer links
  antic and anti of a Windows host with the runtime of the lowest level for them.
- The C of a Windows target compiles for the gnu triple with `-fms-extensions
  -fno-auto-import -nostdinc -D__USE_MINGW_ANSI_STDIO=0`, the headers of clang before
  those of mingw-w64 and no `-g`, from `tools/windows-compile.cmake`, since
  `_mingw.h` defines `__attribute__` away for a compiler without `__GNUC__`. The text of antic keeps the msvc triple.
  The gnu objects call `___chkstk_ms` where the msvc text calls `__chkstk`, and the
  runtime defines both names on x86_64. The bitcode of the runtime is written again
  under the msvc triple by clang reading its own IR, so the LTO link warns on no
  module.
- The runtime also defines `_assert`, `hypotf`, `opendir`, `readdir`, `closedir` and
  the nine functions behind `fpclassify`, `isnan` and `signbit` of `math.h`, which the
  headers of mingw-w64 declare, its own library defines, and raylib, miniaudio and the
  C of the tests reach.
