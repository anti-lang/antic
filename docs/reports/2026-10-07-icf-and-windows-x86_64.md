# Folding identical code, and windows-x86_64 under emulation

Three follow-ups of `docs/reports/2026-10-06-dead-code.md`. windows-x86_64 now folds
identical code in the safe form (`45c9a8ed`). Every windows-x86_64 program crashed
because of a defect of antic, now fixed (`67e25899`). Release `23.1.1-anti.7` of
`anti-lang/llvm-tools` is built and packed for Eddie to publish.

## Folding: measurement

The five programs of the dead-code report, linked in release mode through wrappers in
`build/icf/rt`: `--icf=safe` for ld.lld and ld64.lld, `/OPT:SAFEICF` in place of
`/OPT:NOICF` for lld-link, and `-addrsig` for llc. Each spelling is the one `--help` of
the pinned lld lists. The ELF and COFF runtime objects carry `.llvm_addrsig`, 37 of 37,
and the Mach-O ones carry none, since clang writes none for Mach-O by default. A macOS
runtime compiled with `-faddrsig` changed no Mach-O size by more than 16 bytes. The
change in size, after the fix of windows-x86_64 below:

| Program | macos-arm64 | macos-x86_64 | linux-x86_64 | linux-arm64 | windows-x86_64 | windows-arm64 |
|---|---:|---:|---:|---:|---:|---:|
| hello | 0 | 0 | -0.06% | -0.05% | -1.54% | 0 |
| builder | 0 | 0 | -0.04% | -0.05% | -1.07% | 0 |
| objects | 0 | 0 | 0 | -0.04% | -1.10% | 0 |
| map_work | 0 | 0 | -0.02% | -0.04% | -1.61% | 0 |
| mixed_work | -0.002% | -0.38% | -0.33% | -0.22% | -1.72% | -0.82% |

| windows-x86_64 | hello | builder | objects | map_work | mixed_work |
|---|---:|---:|---:|---:|---:|
| Bytes before | 66,560 | 95,744 | 92,672 | 190,464 | 985,088 |
| Bytes after | 65,536 | 94,720 | 91,648 | 187,392 | 968,192 |

Every program printed its old line, folded or not. macos-arm64 ran on the Mac, and
macos-x86_64 under Rosetta at `v1`, which refuses mixed_work. Both Linux targets ran on
anti-linux and both Windows targets on anti-windows.

## Folding: the choice

Only windows-x86_64 reached 1 percent, on all five programs. lld-link of windows-x86_64
passes `/OPT:SAFEICF`, and llc writes the address-significance table there.
windows-arm64, a link of `link.exe`, which has no safe form, and the ELF and Mach-O links
keep what they had. `llvm_safe_folding` of `src/antic/llvm_target.c` names the target
once. The `[provisional]` entry of `/OPT:NOICF` in `docs/decisions.md` now holds this
choice with the measurement. No test pins it. `unit_link` takes the new option on its
four lld-link lines of windows-x86_64.

`distinct_addresses_<target>` takes the edge of the guarantee in release mode. Anti gives
a constant no address and antic interns identical literals into one, so the data a
program compares by address are the descriptors. The program compares two functions
with one body through the hash of their addresses, which opt cannot decide while it
compiles, and the descriptors of two classes with one body through `is` and
`reflect.describe`. It runs on every target its host runs, and fails with `--icf=all`
on both macOS targets.

## windows-x86_64 under emulation

- Control: a C hello world, compiled by the pinned clang, was linked by lld-link with the
  arguments of antic. It printed its line under the emulation of anti-windows.
- `fn main() -> int { return 7; }` exited with 7. The runtime's start, the processor
  check, the arguments and the environment ran from a C `main` linked against
  `anti_rt.lib`. hello world ended with `0xC0000005` in `ucrtbase.dll`, an ARM64X image
  whose x64 view the event log named at offset 0xfc9a0.
- Cause: antic compiled Windows with llc's relocation model `static`. On x86_64 that
  writes the address of the string as `IMAGE_REL_AMD64_ADDR32`, 32 absolute bits.
  lld-link puts the image at 0x140000000 and kept the low half without a warning, so
  `fwrite` read outside the image. clang uses `pic` for all six triples. The cause is
  ours, and a real x86_64 Windows takes the same image base.
- Fix: every target takes the model clang passes, `pic` on all six, and
  `llvm_datalayout_pin` compares it with `clang -###`. `windows_addresses` refuses an
  `ADDR32` relocation in the object of hello world for windows-x86_64 and runs the
  program on a Windows host. Both failed before the fix. The settled entry that named
  `static` became a `[provisional]` one in `docs/decisions.md`.
- After the fix all five programs printed their lines for windows-x86_64 under
  emulation, at the default level `v3`.

Re-pinned: the LLVM text of both Windows targets gains the module flag `PIC Level`. That
rewrites 13 files of `tests/dump/`, the module flags of `unit_llvm_emit` and 442 lines
of `tests/emit-identity/programs.sha256`, each by that flag alone. The only
`link_identity_<target>` is that of macos-arm64, which neither change touches, so its
value `5267da9c…f583` stands.

## Suites

At `45c9a8ed`:

| Host | Suite | Result | Time |
|---|---|---|---:|
| Mac | host | 1640 of 1640 | 143 s |
| Mac | asan | 1639 of 1639 | 474 s |
| Mac | ubsan | 1639 of 1639 | 324 s |
| anti-linux | host | 1346 of 1348, 2 skipped | 148 s |
| anti-windows | host | 1325 of 1335, 9 skipped | 2223 s |

The three Mac builds print no warning. The build of anti-linux prints the warnings of
raylib's `stb_truetype.h`, which builds with raylib's flags. On anti-linux
`repo_layout` first refused three logs of mine in the top level of the export, and
passed once they moved out. On anti-windows `windows_addresses` and
`distinct_addresses_windows-x86_64` ran their programs under emulation and passed.
`anti_build` failed: its second dev build could not write `dist/windows-arm64/dev/app.exe`
right after the test had run that program. It passed alone and failed again within three
repeats. The run of 2026-09-27 in `build/scratch/win-run5.log` shows the same failure,
before either change, so it is an older intermittent defect and is not fixed here.

## 23.1.1-anti.7 of anti-lang/llvm-tools

Commits `e9e1888` and `4c76c39` there, not pushed. Every static musl binary links with
`-z stack-size=8388608`, and the step `check-stack` refuses one whose `PT_GNU_STACK`
asks for less. With the thin runtime bitcode of linux-arm64 compiled again with the
musl triple of before `45e8bcee`, the anti.6 `ld.lld` ended `mixed_work --lto thin`
with signal 11 three times of three. The anti.7 one linked it three times, and the
program printed `615670597359`. `diff -r` against anti.6 shows `VERSION` and the 18 Linux
binaries alone, each one byte apart. Its report is
`docs/reports/2026-10-07-llvm-tools-thread-stack.md` of that repository.

Once Eddie has run `./r` there, `tools/llvm-pin` and `tools/clang-pin` of this
repository take the tag `23.1.1-anti.7` and the twelve digests that report lists, and
the suites run on the Mac and both VMs. The pins here are unchanged.

## Questions for Eddie

1. A run on real x86_64 Windows, the `windows-2025` runner of
   `.github/workflows/test.yml`, would show that windows-x86_64 programs run outside the
   emulation, and would check the x86_64 ASan runtime. Hosted minutes are limited, so
   the run is yours to decide.
2. The relocation model of Windows was a settled entry, `static`. It is now `pic` and
   `[provisional]`, for your review.
3. `anti_build` fails now and then on anti-windows, when the copy into `dist/` meets a
   program that exited a moment before. It needs a session of its own.
