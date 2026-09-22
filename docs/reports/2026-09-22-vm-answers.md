# The answers to the VM check

The five answers to `docs/reports/2026-09-22-vm-check.md` are built. Both VMs
then ran the suite from a fresh export of `b44e7dc`.

## Counts

| Host | Run | Suite | ASan | UBSan |
|---|---|---|---|---|
| Mac | `b44e7dc` | 628 of 628 | 627 of 627 | 627 of 627 |
| Linux VM | `b44e7dc` | 539 of 539 | 538 of 538 | 538 of 538 |
| Windows VM | `b44e7dc` | 523 of 523 | none | none |

ctest counts a skipped test as passed. The skips are those of the VM check:
three on Linux, eight on Windows and none on the Mac. Windows gained 13
tests: the ten `clib_*`, the two `pdb_names` and `std_serialize_float`.

## What changed

1. `d852329`: every Windows object carries an `S_LPROC32` record per
   function in `.debug$S`, with `-g` and without it, closed by an `S_END`.
   The entry on COFF debug information is corrected. The record names the
   function by its COFF symbol, which is `[provisional]`. `pdb_names` finds
   every function of `stack_trace.anti` in the PDB of both Windows targets on
   every host. `trace_stack` names every Anti frame of a release trace on
   Windows, and `std_backtrace` passes there too.
2. `97684f6`: every Windows link of antic and of the packer passes
   `/pdbsourcepath:.`. `trace_stack_g` reads `stack_trace.anti:14` on
   Windows, and `strips_debug` checks the flag on every Windows command line.
3. `b44e7dc`: all ten `clib_*` tests run on Windows. The pinned clang
   compiles each C program against the xwin sysroot and links it with lld,
   and the C++17 checks use `-x c++ -fsyntax-only`. `loader.c` loads with
   `LoadLibrary` and `GetProcAddress` beside `dlopen`. `f09bcf6` fixes
   `roundtrip.c`, which read `flags.visible` in the argument list of the
   call that sets it. clang for windows-arm64 read it first.
4. `02f4765`: the entry on the Windows stack walk is settled.
5. `6a01490`: `serialize` writes a float with the fewest digits that read
   back as the same value, from the digits of `rt/text.c`, and its integers
   without `printf`. `deserialize` reads a float with a new reader in
   `rt/text.c`, which rounds the exact value of the text once to the width of
   the field. An `f32` or an `f16` no longer rounds twice through an `f64`,
   and a number has any count of digits. `float_digits` refuses `printf`,
   `scanf` and `strtod` in `rt/object.c`, and a float reader of the C library
   in `rt/registry.c`, where `deserialize` reads. The unit test
   `test_float_read` checks hard cases, every text the writer gives and the
   exact half-way points of all three widths. `std_serialize_float` takes the
   hard cases of `float_text` through a round trip. The `f16` of
   `std_serialize` now reads `0.099975586`.

## Findings

- `clib_bundle` passes on Windows without a change. lld-link refuses the
  link of two bundled libraries, but it names three symbols of the library
  objects, `anti_rt_registry`, `anti_rt_backtrace_default` and
  `anti_rt_trampolines`, each defined in `geo.obj` and in `other.obj`. It
  names no member of the second runtime. The runtime members stand in each
  archive beside the library object, and lld-link takes them from the first
  archive and never pulls the second copy. The Mac joins the runtime into
  the library object and reports 127 duplicates, runtime functions among
  them. The expectation of the test is unchanged.
- `link.exe` does not know `/pdbsourcepath`. On every `--linker platform`
  link it prints warning LNK4044, an unrecognized option, and ignores it.
- `/pdbsourcepath:.` changes relative names only. A `-g` PDB linked on the
  Mac still holds each absolute path it was given. They are the object file,
  the PDB and `/OUT:` in the command line that lld-link records. A relative runtime
  directory stayed relative.
- The ASan and UBSan build directories of the Mac held runtime archives of
  the layout before the CPU levels, from 2026-09-20, which `float_digits`
  read. They are deleted. A fresh build has none.

## Questions

1. `clib_bundle` on Windows passes on three duplicates of antic's own
   symbols and on no runtime symbol. Shall it check for a runtime symbol
   there? Shall a bundled runtime be one object on Windows, as it is on the
   Mac and Linux?
2. Shall `--linker platform` keep passing `/pdbsourcepath:.` to `link.exe`,
   which warns on every link, or leave it out there?
3. Shall a Windows link pass relative paths of the object, the output and
   the PDB? No build path then reaches the command line or the module names
   of the PDB.
