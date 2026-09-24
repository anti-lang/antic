# The glibc link mode and plugins on Linux and Windows

Item 26 of "First sessions" in `CLAUDE.md`, and R60 of the provisional review.

## What changed

- `link linux "Name";` at module level names a library of the glibc sysroot. The
  checker takes the rules of `link framework`, the library file records the names
  and its format version is 54. `anti build` and `anti test` pass them as
  `--linux-lib`. `anti bind` writes the Linux link lines of raylib and miniaudio.
- A Linux program that reaches such a line, or that can load a plugin, links with
  ld.lld as a PIE against `sysroot/linux-<cpu>-glibc`. Its runtime is built against
  the headers of glibc 2.35 in `lib/linux-<cpu>-glibc/<level>/`. Every other Linux
  program stays static against musl.
- `tools/sysroot-pins` adds `libx11-6`, `libx11-dev`, `libgl1` and `libgl-dev` of
  jammy. `tools/get-sysroot.cmake` copies the builtins of the pinned clang into the
  glibc sysroot and writes every symbolic link as a copy of its file.
- A plugin on ELF reaches the names of its host through the GOT. A Windows host
  writes `<program>.def` and links `<program>.lib`. The `.def` file lists the names of
  its object and of the runtime members its link takes, which `coff_archive_exports`
  finds. A Windows plugin links that import library, reaches data of the host through
  `__imp_` entries in code, and lists the places in its data in `anti_rt_imports`.
  The loader of `src/rt/platform_windows.c` fills them.
- R60 states what is built and has no tag. The overview, `docs/notes/plugins.md`,
  the README, `docs/vm-setup.md` and `CLAUDE.md` follow.

## Tests

- `linux_modes` builds `anti-build/app` (musl) and `anti-build/window` (glibc, X11,
  GL) for both Linux targets in both modes. It reads the interpreter and the needed
  libraries, and runs both on the Linux VM.
- `plugin_host` and `plugin_versions` run on macOS, Linux and Windows. On Windows each
  host takes a library linked against its own import library.
- `glibc_sysroot` covers the new packages, both kinds of link and the builtins.
- `errors/link_linux*`, and unit tests of the three new link command lines.

## Failures and fixes

- The Windows link of a host pulled every runtime member through its exports, and
  `call.o` needs `anti_rt_trampolines`, which only some programs write. The exports
  are now the closure of the members the link takes.
- lld on the Windows VM could not open the relative link `libm.so`. The installer now
  copies every link.
- The Windows VM did not build antic at `db447ea`. The three `mach_add` calls of
  `arm64.c` failed `-Wsign-conversion` (fixed in `dd77ad1`, and the same call in
  `x86_64.c`).
- The Linux VM fails 14 tests at this commit and the same 14 at `db447ea`, before this
  work. The Windows VM fails 11 and cannot compile `test_toml.c` and `test_zip.c`, all
  outside the paths this step changed. Neither is fixed here.
  Logs: `build/drive/logs/vm-linux-final.log`, `vm-linux-base.log`,
  `vm-win-final.log`.

## Suites

Host 842 of 842 (`build/drive/logs/host-full3.log`), ASan 841 of 841
(`asan-test.log`), UBSan 841 of 841 (`ubsan-test.log`). Zero compiler warnings. The
linker prints only the `-lto_library` warning that `docs/notes/linker.md` explains.

## Provisional entries added

- The form `link linux "Name";`, `--linux-lib`, and the tables of `anti bind`.
- A sysroot and a runtime of their own per processor for the glibc mode, and libm
  in every program of the mode.
- The X11 and GL packages, and every link of the sysroot written as a copy.
- The published package still holds no glibc sysroot and no glibc runtime.
- A Windows plugin imports from the one executable whose import library it linked.
- A Windows host exports the runtime members its link takes, and no more.

## Questions

- Should the published package carry the two glibc sysroots and runtimes? Without
  them an installed Anti cannot link a Linux program that loads a plugin. The same
  holds for one that reaches a `link linux` line.
- The two unit test files and the 11 failing tests of the Windows VM need a session
  of their own, and so do the 14 of the Linux VM.
