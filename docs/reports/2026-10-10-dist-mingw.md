# The Windows sysroots from mingw-w64

Step `mingw` of `docs/work-order-distribution.md`, 2026-10-09 and 2026-10-10. The
Windows sysroots are the headers and the import libraries of mingw-w64. Every Windows
program links against `ucrtbase.dll` and nothing of Microsoft, and every package carries
both sysroots.

## What the step built

- `tools/sysroot-pins` pins the source release mingw-w64 v14.0.0 by URL and digest.
  `tools/get-sysroot.cmake` lays `sysroot/windows-<cpu>/` out from it: `include/` with
  the headers of `crt/`, `include/` and `ddk/include/` of the release and `_mingw.h`
  written for the UCRT and Windows 10, and `lib/` with the import libraries of
  ucrtbase, ntdll, kernel32, user32, gdi32, shell32, winmm, dbghelp, bcrypt and ws2_32,
  which llvm-dlltool, the pinned llvm-ar under that name, writes from the `.def` files
  after the pinned clang preprocesses each `.def.in` for the processor, beside
  `clang_rt.builtins.lib` of the pinned clang. The licence is `licenses/mingw-w64.txt`,
  and `licenses/sources.txt` names the release. The code of xwin, of the Build Tools
  and of the splat script left the script with the two tests that read them, and
  `tools/pack-anti.cmake` copies both sysroots as the runtime archive holds them.
- The C of a Windows target compiles for the gnu triple, since `_mingw.h` defines
  `__attribute__` away for a compiler without `__GNUC__` and twenty sources of the
  runtime then fail in the intrinsics of clang. `tools/windows-compile.cmake` holds the
  one definition of the triple, the compile options and the link options, which the
  runtime, the native libraries, the C of the tests, the packer and `anti bind
  --clang` take. The bitcode of the runtime is written again under the msvc triple by
  clang reading its own IR, so the LTO link of a release build warns on nothing.
- antic links every Windows program with lld-link in its mingw mode, which ties the
  `.pdata$f` and `.xdata$f` sections of the gnu objects to their functions and looks
  for no Visual Studio of the machine, drops `msvcrt.lib`, `libcmt.lib`, `oldnames.lib`
  and `uuid.lib` by name, and names `clang_rt.builtins.lib`, `ucrtbase.lib`, `ntdll.lib`
  and `kernel32.lib` after the runtime. A DLL for C asks for its import library, which
  the mode writes on request alone.
- The runtime defines what the static libraries of Microsoft gave. `DllMainCRTStartup`,
  the tables of the initialisers, the directory of thread-local storage, `_fltused`,
  `__chkstk` with `___chkstk_ms`, the stack cookie, set from `rand_s` at start, with its
  check, and `__main` stand in `src/rt/platform_windows.c`. `mainCRTStartup` stands in
  `src/rt/platform_entry.c`, an object only the link of a program pulls in. `atexit`
  and the printf and scanf families stand weak in `src/rt/platform_stdio.c`. The two
  objects join the bitcode archives as objects. The runtime also defines the
  fourteen functions of mingw-w64's own library that raylib, miniaudio and the C of the
  tests reach: `_assert`, `hypotf`, `opendir`, `readdir`, `closedir` and the nine
  behind `fpclassify`, `isnan` and `signbit`. The test `rt_names` lists every name.
- The installers lay out no Windows sysroot any more. `install.ps1` runs no CMake.
- Item 32 of "First sessions" in `CLAUDE.md` reads Done.

## Corrections of the work order

Decision 4 said that `__chkstk` comes from `clang_rt.builtins.lib` of the pinned clang
and `__C_specific_handler` from `ntdll.dll`. The builtins of the pinned clang hold no
`__chkstk` for either processor, and the `.def` files of mingw-w64 give
`__C_specific_handler` in kernel32 on x86_64 and in ucrtbase on both. The runtime
defines the probe. Decision 4 also named `-gcodeview` for the C of the gnu triple:
alone it adds the records of every function and the build information with the paths of
the machine to each object, which `dead_code` and `no_paths` refuse, so no `-g` option
is given, as before. The work order says so at each of the three places.

## What it tested

Test first. `windows_sysroot` lays the two sysroots out from a stand-in release and
failed on the old script, `build/drive/logs/mingw-windows-sysroot-red.log`. `unit_link`
pins the new lld-link commands, `own_tools` checks that a library the sysroot lacks
stays missing with `LIB` set, `package_keys` requires both sysroots in the package and
links a program for both Windows targets from it, and `rt_names` lists the names of the
C runtime part. On anti-windows the step's six programs run through the suite: the
hello and every `program_*` test, the stack trace in `trace_stack`, `trace_copies` and
`std_backtrace`, raylib in `raylib_link_windows-arm64` and `media_audio_run`, the plugin
host in `plugin_host` and `plugin_versions`, `--memory-checks` in
`memory_checks_windows-x86_64`, which links and skips its run under the x64 emulation
as before, and `--profile-generate` in `profile_guided` and the four
`profile_lto_<mode>_<target>` tests, which run an instrumented program of both Windows
targets. `--profile-generate` links without `libcmt.lib`, so no target refuses it.

A program of `--memory-checks` for windows-x86_64 needs `clang_rt.asan_dynamic.dll` of
the runtime archive beside it. That DLL imports `VCRUNTIME140.dll`, `KERNEL32.dll`,
`api-ms-win-core-synch-l1-2-0.dll` and the API sets `api-ms-win-crt-runtime`,
`-string`, `-utility`, `-stdio` and `-environment-l1-1-0.dll` of the UCRT. The Visual
C++ Redistributable installs them on the machine that runs it. `docs/distribution.md`
says so.

## Sizes

| Sysroot | Before, xwin | After, mingw-w64 |
|---|---|---|
| windows-x86_64 | 644,100 KiB in 5,590 files | 88,448 KiB in 1,718 files |
| windows-arm64 | 877,064 KiB in 5,603 files | 88,540 KiB in 1,718 files |

The import libraries are 2.4 MiB per target and the headers 82 MiB. The `.idl` files,
the change logs and the makefiles of the release stay out.

## Decisions

Seven `[provisional]` entries under "Binary distribution" in `docs/decisions.md`: the
layout of a Windows sysroot and its ten import libraries, the gnu triple with the
compile options and the bitcode written again under the msvc triple, the static part of
the C runtime in the runtime with the names it carries and the fourteen functions of
mingw-w64's library, the lld-link command in its mingw mode with the four libraries
dropped by name, the link of antic and anti of a Windows host by the packer, and the
DLLs a program of `--memory-checks` needs. Rule 22 and rule 25 of
`docs/c-guidelines.md` each gained one sentence for the runtime's part, which Eddie
reviews with the entries. `src/antic/coff.c` accepts a COMDAT that its section symbol
alone names, which the gnu objects use for their unwind data, so `--bundle-runtime`
joins them as before. The `clib` tests compile their C with clang and link it with
lld-link against the sysroot. `binary_stdio.h` of the tests puts its constructor into
`.CRT$XCU`, since a constructor of the gnu triple lands in `.ctors`, which no start of
ours runs.

A C program compiled against the headers of the package may reach a function of
mingw-w64's own library outside the fourteen. It then gets an undefined symbol from the
link. That is a gap for Eddie: the library of mingw-w64 built into the sysroot, or the
list grown as the need shows.

## Gates

| Suite | Result | Time and log |
|---|---|---|
| Mac host | 1693 of 1693, the two `mimalloc_environment` tests skipped | 342 s, `build/drive/logs/mingw-host-suite-5.log` |
| Mac ASan | 846 and 857 in two parts of `ctest -I`, the same two skipped | 194 s and 372 s, `mingw-asan-suite-final-1.log` and `-2.log` |
| Mac UBSan | 1692 of 1692, the same two skipped | 419 s, `mingw-ubsan-suite-final.log` |
| anti-linux | 800 and 834 in two parts of `ctest -I`, three skipped | 126 s and 192 s, `build/vm-logs/ctest-1.log` and `-2.log` on the VM |
| anti-windows | 330, 334, 347, 343 and 293 in five parts, thirteen skipped | 199 s, 188 s, 757 s, 65 s and 25 s, `build\ctest-1.log` to `-5.log` on the VM |

The builds had no warning of our own code on any machine. mimalloc prints six of its
own on both VMs, under its own flags, as before. `emit_identity` and
`link_identity_macos-arm64` passed unchanged in the three Mac suites. The VMs built
the export of the commit, `git archive HEAD`, in a fresh directory each.

`tools/pack-anti.cmake`, `tools/install.sh` and `tools/install.ps1` changed.
`release_dry_run` passed in the host suite, and its stand-in packer does not read the
real one. The real `./r --dry-run` is Eddie's and did not run here.

The logs of the step stand under `build/drive/logs/` with the prefix `mingw-`: the
trial compiles of the runtime under both triples in `mingw-rt-compile-msvc.log` and
`-gnu.log`, the layout of the real sysroots in `mingw-get-sysroot-mac.log`, the host
builds in `mingw-host-build-1.log` to `-16.log` and the targeted runs in
`mingw-host-targeted-1.log` to `-15.log`.

## State

After the push of the commit of the code, before the commit of this report, which
follows it and is pushed with it.

```text
$ git log --oneline -3
3f111e33 Build the Windows sysroots from mingw-w64 and link against ucrtbase.dll
f2dd2a55 Report the step headers of the binary distribution
16464f7d Put the headers of the native libraries into the archive and the package
$ git status --short
 M docs/vm-setup.md
?? docs/reports/2026-10-10-dist-mingw.md
$ git rev-parse HEAD origin/main
3f111e3311a6725715565893ba0c7016a26553ef
3f111e3311a6725715565893ba0c7016a26553ef
```
