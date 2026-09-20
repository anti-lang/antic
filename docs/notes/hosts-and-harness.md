# Hosts and test scripts

`docs/vm-setup.md` gives the images, packages and SSH setup. This note holds what no other
file does.

## Reaching the machines

- The Linux ARM64 VM is `anti-linux` and the Windows 11 ARM64 VM is `anti-windows`, both
  in `~/.ssh/config` on the Mac with the user and the address. Name them and nothing else
  in a command or a document, so an address that changes is one edit in one file.
  `docs/vm-setup.md` gives the entry.
- Pass `-o BatchMode=yes` to every `ssh` and `scp`, so a missing key fails rather than
  asking. Use `ssh -n` inside a script, since `ssh` otherwise eats the input of the
  script.
- Both VMs hold their tools in the data directory of the user, under a name of their own:
  `~/.local/share/anti-vm` on Linux and `%LOCALAPPDATA%\anti-vm` on Windows. The tree each
  run replaces is `~/antic-check` and `%USERPROFILE%\antic-check`, built with Ninja on
  Windows. Nothing of a VM stands outside the profile of its user, so a reset is those
  directories. `C:\anti` held the Windows tools once and is gone. A directory there is
  outside the profile, and no backup, sync or reset of the account reaches it.
- Send a tree as `git ls-files -z | xargs -0 tar cf tree.tar` and extract it with
  `tar -xmf`. Without `-m` the Mac's file times return, and Ninja keeps newer objects of an
  older source. That cost four false failures on 2026-09-19.
- On Windows, write the commands into a `.cmd` file with CRLF endings, copy it over and run
  `ssh -n ... "cmd /c %USERPROFILE%\x.cmd"`. The file calls `vcvarsall.bat arm64` first.
- A program built by `run_program.cmake` has no `.exe` suffix. `cmd` runs a copy named
  `x.exe`.

## Which host runs what

| Host | Suite | Sanitizers | Only there |
|---|---|---|---|
| Mac | 486 | ASan, UBSan, 485 each | macos-x86_64 under Rosetta, `emit_identity` with `WRITE=yes`, `anti sdk export`, lldb |
| Linux VM | 419 | ASan, UBSan, 407 each on 2026-09-19 | glibc sysroot, linux-arm64 programs, gdb |
| Windows VM | 399 | none | windows-arm64 programs, the Win32 expected files |

The counts of the Mac and of the two VMs are of 2026-09-20, from the release
script's own step 5. A host runs fewer tests than the Mac because a test that
needs something it lacks skips, `release_dry_run` and `sysroot_digest` among
them.

`debug_info` runs the debugger of the host, lldb on the Mac and gdb on Linux, and takes
neither from the other. No host here debugs a Windows program.

A sanitizer preset names no path of one machine. It takes the sysroot and the raylib
source of the default build, from the cache of `build/`, when its own default is not on
disk. That is the `antic_shared_path` macro of `CMakeLists.txt`. The presets held the
Mac's `build/sysroot` before, and `cmake --preset asan` on the Linux VM, whose downloads
live in `~/.local/share/anti-vm`, then built a runtime with no sysroot and failed to link
102 programs.

No machine here runs linux-x86_64 or windows-x86_64 programs. Only the CI runners do,
when started by hand.

## Quirks

- `execute_process` with `OUTPUT_VARIABLE` drops the CR of each CRLF and every NUL on every
  host. A lone CR stays. `ENCODING NONE` changes nothing there. `OUTPUT_FILE` keeps all.
- On Windows CMake decodes captured output through the console code page unless
  `ENCODING NONE` is given, which turned UTF-8 into CP437.
- The C runtime of Windows answers `raise` from its own signal table. Its default ends the
  process with code 3. It resets a handler before it calls it. It adds
  its own console handler at the first `signal` of SIGINT or SIGBREAK. Windows calls console
  handlers newest first.
- `git mv` stages at once. A later `git add X && git commit` commits the whole index. Check
  `git diff --cached --stat` before every commit.
- zsh does not split `$OPTS` into words. Run such scripts with `bash`.
- `pkill -f` matches the `ssh` session that runs it.
- `tools/docs-style/check_docs.py` reads `CMakeLists.txt` as Markdown. Every comment is a
  heading to it, and code joins its sentences. Compare its count with that of HEAD.
- To test a staged tree before a commit, run `git checkout-index -a --prefix=<dir>/` and
  configure `<dir>` with the `ANTIC_*_DIR` values of `build/CMakeCache.txt`. A full run
  takes a minute on the Mac.
- The MSVC branch of `tests/binary_stdio.h` has never compiled. No host here builds with
  `cl`.
- The two debuggers read the debug information of `-g` differently. gdb gives a position to
  every frame of a backtrace. Apple's lldb gives one to the frame that stops and names the
  function alone below it, because the compile unit describes no function of its own yet.
  Neither reaches `main` by its Anti name: it shares its address with the symbol of the
  runtime entry, so lldb prints `app.main` and gdb `anti.rt[main]` for the same frame.
- gdb writes the last segment of a dotted symbol in brackets. `com.example.step.step` prints
  as `com.example.step[step]`, and a test that matches a function name leaves the character
  before the segment open.

## Test hooks

- `ANTI_DEV_CPU` compiles the processor simulation into `rt/cpu.c`. With it,
  `anti_cpu_level` reads the level from the environment variable `ANTI_CPU_LEVEL`
  instead of the processor, so a test on this machine sees the refusal a lower machine
  gets. The unit tests are the only build that defines it. The runtime of the archive
  is compiled without it, so a shipped program reads no variable of its own and the
  one-environment-variable rule holds.

## What a fresh build and the VMs catch

- A build directory here has been configured more than once. That hides a read
  of a `find_program` variable above the call that writes it, since the first
  run alone takes an empty string. The release script configures a fresh export
  of the commit, which is the only build that sees it. `find_order` refuses the
  order now.
- `setenv` and `unsetenv` are POSIX and outside the C11 library. The headers of
  Apple declare them under `-std=c11` and musl and glibc do not, so a file that
  uses them without `_POSIX_C_SOURCE` compiles on the Mac alone.
- On a host that is not the Mac, `runtime/lib/<host target>/` holds the library
  that build compiled, at its own optimisation. Every other target is a cross
  build. A Debug build writes a call where a release writes the exclusive loop,
  so a test of a level's instructions skips the host's own library.
- The Windows VM answers ssh with `cmd`, not with a shell. A command with `;`
  or `&&` in it reaches the program as arguments. Send a `.cmd` file, or call
  `cmd /c` with one command.

## For the next session

- The toolchain is frozen at `23.1.1-anti.3`. A toolchain change goes to
  `docs/toolchain-later.md` as one line.
- Both VMs ran the suite after the libc check. Linux passed 408 of 408, and both
  sanitizer suites 407 of 407 with `cmake --preset asan` and `ubsan` and no other option.
  Windows passed 388 of 388 and skipped `sysroot_digest`, which a Windows host always
  skips. `emit_identity` passed on both.
- The `std_toml` segmentation fault of `d32dd31` is fixed. It was never the error path:
  antic supplied the out pointer of a `catch` binding over stack storage it never zeroed,
  and the `=` in `Document.read` tore down whatever the frame held. `docs/decisions.md`
  holds it under "Object model", and `program_out_slot` pins it on every host.
- `table_unset` reads the two labels with `table[._]run:` and `table[._]main:`. Each host
  spells them its own way: `table.run:` on ELF, `_table.run:` on Mach-O and
  `_A5table_run:` on COFF, which writes the last segment of a dotted name after an `_`.
  The pattern held the dotted form alone, so the match was empty on Windows and the test
  reported that release mode checks a table in a dispatch.
- Windows has no debugger test. The CodeView line table of a `-g` build was read instead,
  with `build\runtime\bin\llvm-readobj.exe --codeview` on the object of the two sources
  that `tests/run_debug.cmake` writes. It names `com/example/step.anti` and `app.anti` in
  its `FileChecksums`, and one `FunctionLineTable` per function with the statement lines:
  1, 3 and 4 for `step`, 3 and 5 for `helper`, 8, 10 and 11 for `main`. A debugger check
  on Windows, with lldb of the tools or WinDbg, is the one debug test not run on any host.
- antic takes the search roots and the sources of `-I` with forward slashes on Windows. A
  root spelled with backslashes matches no source, and every module path is then the whole
  path, which is not a lowercase identifier.
- On Windows, `%USERPROFILE%\main-suite.cmd` extracts `%USERPROFILE%\tree.tar` into the
  tree, then configures, builds and runs the suite into `main-*.log` there.
- The next session starts at item 14 of "First sessions" in `CLAUDE.md`.
- A signed enum on Windows turns a comparison with a `size_t`, or a conversion to an
  unsigned operand, into an error that the Mac never shows. Convert the enum first.
