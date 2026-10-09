# antic takes its tools, libraries and sysroots from the package alone

Step `own-tools` of `docs/work-order-distribution.md`, 2026-10-09. The fallbacks on
the search path and on `LIB` are gone, and no message names
`tools/get-sysroot.cmake`.

## What the grep found

`platform_getenv` is the one read of the environment in `src/antic/` and
`src/anti/`. Its callers are the user directories of `src/antic/userdirs.c`, `HOME`,
the XDG roots and `LOCALAPPDATA`, and the sanitizer options of
`src/anti/memreport.c`. None finds a tool, a library or a sysroot. The fallbacks
stood in the code instead, as bare names for the search path:

- `archive_tool` of `src/antic/driver.c` gave `opt` or `llc` as a bare name when
  `bin/` of the archive lacked it or no archive stood.
- `program` of `src/antic/linker.c` gave the flavour of lld as a bare name when
  `link_facts` found none in `bin/`.
- `src/antic/driver_library.c` named `llvm-ar` bare in two places.
- `link_facts` of `src/antic/driver_link.c` let a Windows link go without a sysroot,
  and `windows_libpaths` then wrote no `/LIBPATH`, so lld-link read `LIB`.
- Two messages of `driver_link.c` named `tools/get-sysroot.cmake`.

The kept exceptions stand: `--linker platform` runs `xcrun`, `ld` and `link.exe`,
`anti bind --clang` takes a clang of the search path, `anti sdk` takes the host's
`tar` and `anti build` fetches with `curl`, each by an entry of `docs/decisions.md`.

## What the step built

- `driver_archive_tool` of `driver_link.c` gives `bin/<name>` of the archive, or
  refuses with the path and the line that the package holds it in `bin/` beside antic.
  `driver_compile_llvm` asks it for opt and llc, the static library for llvm-ar, and
  `link_facts` for the flavour of lld before the compile. `--opt`, `--llc` and
  `--llvm-ar` name a tool elsewhere as before.
- `link_facts` refuses a missing sysroot for every target, the Windows ones included,
  with `needs the sysroot <path> of the package`. The message of a framework without
  Apple's SDK names `anti sdk export` and `anti sdk import` alone.
- Every lld-link command carries `/lldignoreenv`, `/vctoolsdir:<sysroot>/crt` and
  `/winsdkdir:<sysroot>/sdk`. The second and the third came from the Windows VM: lld-link
  finds the Visual Studio and the Windows SDK of a Windows host through the setup
  configuration and the registry, under `/lldignoreenv` too, and read `ucrt.lib` from
  Windows Kits with `LIB` unset, `build/drive/logs/own-tools-probe2.log`. Naming the
  sysroot as that toolchain ends the detection, and the directories lld-link derives,
  `lib/x64` and `Lib/<version>/ucrt/x64`, do not exist in a sysroot that names its
  architectures `x86_64` and `aarch64`.

## What it tested

Test first. `own_tools` was red on the old code, `build/drive/logs/own-tools-red.log`,
and the lld, llvm-ar and the two `LIB` cases linked by hand through the fallbacks
before the change.

- `own_tools`, `tests/run_own_tools.cmake`: stand-in archives of links into the real
  one. With `PATH` holding the real tools, an empty `bin/` is refused for opt, for
  ld.lld, for lld-link and for llvm-ar, and nothing is written. With `LIB` naming the
  real Windows sysroot, an archive without one is refused and names the package. With
  the sysroot's `ucrt/` directory empty and `LIB` naming the real one, `ucrt.lib` stays
  missing. No line with a string literal in `src/antic/*.c` or `src/anti/*.c` names
  `get-sysroot.cmake`.
- `package_keys`: from the unpacked package, with `PATH` empty and `LIB` unset, antic
  links `return42.anti` for all six targets. The package holds no Windows sysroot until
  the step `mingw`, so the Windows sysroots of the runtime archive stand in as links at
  the place the package will hold them, before the host's own hello program, since a
  Windows host links that one against them.
- `unit_link` drops the case without a sysroot and pins the three options.

## Measured

| Measure | Value |
|---|---:|
| `package_keys` on the Mac, host, ASan, UBSan | 288 s, 337 s, 356 s |
| `package_keys` on anti-linux | 216 s |
| `package_keys` on anti-windows | 593 s |
| `own_tools` everywhere | under 2 s |

The six links from the package added about 80 s on the Mac and 320 s on anti-windows,
where the x64 emulation runs the x86_64 tools. A part of `ctest -I` that holds it
stays under ten minutes there with 350 tests.

## Decisions

- `[provisional]` under "Compiler behaviour" in `docs/decisions.md`: the refusals by
  path, the Windows refusal, the three lld-link options and the stand-in of the Windows
  sysroots in `package_keys` until `mingw`.
- The entry on host tools under "Binary distribution" says what this step built.
  `CLAUDE.md` item 28 reads "Done". `docs/notes/linker.md` gains the tools and the
  sysroots of a link, `docs/notes/llvm.md`, `docs/site/runtime-archive/index.md` and
  `docs/vm-setup.md` lose the search path and `LIB`.
- The step's sentence in `docs/work-order-distribution.md` says that the test stands in
  the Windows sysroots until `mingw`. The package layout of the same document holds no
  Windows sysroot before that step.

## Gates

| Suite | Result | Time and log |
|---|---|---|
| Mac host | 1693 of 1693, `sysroot_build_tools` and the two `mimalloc_environment` skipped | 370 s, `build/drive/logs/own-tools-host-suite-3.log` |
| Mac ASan | 1692 of 1692, the same three skipped | 518 s, `own-tools-asan-suite-2.log` |
| Mac UBSan | 1692 of 1692, the same three skipped | 450 s, `own-tools-ubsan-suite-2.log` |
| anti-linux | 1623 of 1623 in three parts of `ctest -I`, six skipped | 38 s, 217 s and 13 s, `own-tools-vm-linux-suite-4.log` to `-6.log` |
| anti-windows | 1611 of 1611 in five parts, fourteen skipped | 145 s, 132 s, 593 s, 53 s and 24 s, `own-tools-vm-windows-suite-4.log` to `-8.log` |

The parts of `ctest -I` count a fixture in each part that needs it, so their sums
exceed the totals. Neither `tools/pack-anti.cmake`, `tools/release.sh`, an installer nor
`tools/downloads.html.in` changed, so `release_dry_run` follows nothing new, and the
real `./r --dry-run` stays Eddie's. `emit_identity` and `link_identity_macos-arm64`
passed unchanged in every suite.

The first run of the Linux suite stopped at the limit of a call after twenty minutes,
`own-tools-vm-linux-suite-1.log` of that run. The Windows VM rebuilt its whole tree at
the same time, and `unit_sema` took 252 s alone on anti-linux, against 27 s once that
build ended. Orphans of that run were killed by hand. The export of the changed files
alone, with `tar -xmf`, kept the second Windows build incremental.

## State

Before the commit of this report, which follows it and is pushed with it.

```text
$ git log --oneline -3
968cf018 Name the sysroot as the toolchain of every lld-link
29ce13a0 Take every tool, library and sysroot of a link from the package alone
09656f24 Leave the dry run of a release to Eddie in the distribution work order
$ git status --short
?? docs/reports/2026-10-09-dist-own-tools.md
$ git rev-parse HEAD origin/main
968cf018cac588954636713d3e16aa8b24e4f17d
09656f249b67bcf30a790d33728adbe871625e0d
```
