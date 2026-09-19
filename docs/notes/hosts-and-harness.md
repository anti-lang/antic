# Hosts and test scripts

`docs/vm-setup.md` gives the images, packages and SSH setup. This note holds what no other
file does.

## Reaching the machines

- Linux ARM64 VM: `eddie@192.168.60.131`. Windows 11 ARM64 VM: `eddie@192.168.60.132`.
- Pass `-o BatchMode=yes -o UserKnownHostsFile=<llvm-tools>/build/vm_known_hosts` to every
  `ssh` and `scp`. Change no SSH configuration. Use `ssh -n` inside a script, since `ssh`
  otherwise eats the input of the script.
- Linux holds the tools in `~/anti` and a tree in `~/antic-check`. Windows holds the tools in
  `%USERPROFILE%\anti3` and a tree in `%USERPROFILE%\antic-check`, built with Ninja.
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
| Mac | 439 | ASan, UBSan | macos-x86_64 under Rosetta, `emit_identity` with `WRITE=yes`, `anti sdk export` |
| Linux VM | 385 | ASan, UBSan | glibc sysroot, linux-arm64 programs |
| Windows VM | 368 | none | windows-arm64 programs, the Win32 expected files |

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

## For the next session

- The toolchain is frozen at `23.1.1-anti.3`. A toolchain change goes to
  `docs/toolchain-later.md` as one line.
- Both VMs ran the suite after item 13 of "First sessions". Linux passed 385 of 385 at
  `03d1064`. Windows passed 367 at `03d1064` and skipped `sysroot_digest`, which a
  Windows host always skips. `emit_identity` passed on both.
- On Windows, `%USERPROFILE%\main-suite.cmd` extracts `%USERPROFILE%\tree.tar` into the
  tree, then configures, builds and runs the suite into `main-*.log` there.
- The next session starts at item 14 of "First sessions" in `CLAUDE.md`.
- A signed enum on Windows turns a comparison with a `size_t`, or a conversion to an
  unsigned operand, into an error that the Mac never shows. Convert the enum first.
