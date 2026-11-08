# The two checks of dsymutil and the fix of T212 and T213

Eddie's answers stand as "Answers of 2026-11-08" in `docs/decisions.md`.
The first builds both checks of a macOS release build after dsymutil. The
second carries Q2 of "Answers of 2026-11-07, sixth set" to `--lto none`,
whose defect the first check found.

## Commits

- `284df013` records the two answers.
- `dedce9d7` fixes T213. A release program of `--lto none` names its object
  by its absolute path below the build directory and passes `-oso_prefix`.
- `eecd710a` fixes T212 with the two checks.

## T212, the warnings of dsymutil

dsymutil 23.1.1 exits with 0 for an object of the debug map it cannot open.
The text, reproduced for an object that is missing, that it may not read,
that is no object and that is truncated:

```text
warning: (arm64) <object> unable to open object file: <reason>
warning: no debug symbols in executable (-arch arm64)
```

antic reads the standard error of dsymutil through the new
`platform_capture_errors` of `src/antic/platform.c` and writes it on as
dsymutil wrote it. The first such warning refuses the build:

```text
antic: dsymutil could not open <object>, an object of the debug map of <program>
```

Then `dsym_has_lines` of `src/antic/driver_link.c` reads the file and the
line of `anti.rt.main` from the DWARF of the dSYM with
`anti_rt_macho_object_line`, the reader of `anti symbols resolve`, through
the symbol and the start that the debug map of the link names. A dSYM
without them refuses the build:

```text
antic: dsymutil wrote no line table for the code of <program>
```

`<program>` is the program in the build directory,
`./build/<target>/release/app`. `src/rt/symbols.c` moved from the sources
of anti into `antic_core`, so no second DWARF reader came. The freeze lets
the two messages in because they fix T212, and no option came. Neither check
runs for ELF or COFF.

## T213, the object of `--lto none`

The link passed `-oso_prefix` only with LTO. Under `--lto none` the debug map
named `<project>/./build/<target>/release/app.o`, and dsymutil looked for it
behind `--oso-prepend-path`. Since `f2c8d50f` every such program shipped an
archive without lines, and its bytes depended on the directory. Check 1
refused the release build of `--lto none` in `anti_build`. `link_facts`
now resolves the object to its absolute path, whose links are resolved as
those of the root are, and `macos()` of `src/antic/linker.c` passes
`-oso_prefix` for it.

## Tests

- New, each failed first: `anti_build_message_dsymutil_object_not_opened`
  and `anti_build_message_dsym_without_lines` exited with 0.
  `symbols_moved_frame_without_build_lto_none` resolved no line, and
  `symbols_same_bytes_lto_none_macos-arm64` and `_macos-x86_64` differed in
  39 and 8 bytes. `unit_link` checks the command of `--lto none`.
- The two message cases run a stand-in archive whose `dsymutil` is a script
  around the real one. The first removes the LTO object. The second runs
  without `--oso-prepend-path` and drops the standard error.
- Fault counts, measured on the Mac. The pipe of dsymutil is run 3, dsymutil
  run 4 and llvm-strip run 5. The reads of the check are opens 36 and 37,
  in the new cases `fault_link_unread_for_lines` and
  `fault_dsym_unread_for_lines`, beside `fault_dsymutil_no_pipe`. The opens
  of the archive moved up by two on macOS alone, to 44 to 50. The table
  keeps the old counts for the other hosts.
- The host suite ran in three parts before each code commit, and every test
  passed. It took 378 s, 265 s and 305 s with 6969 tests, then 378 s, 273 s
  and 337 s with 6974. Before the fix of T213 the run with the checks alone
  failed `anti_build` and nothing else.
- Both sanitizer suites ran in six parts before the push, 6972 tests each,
  and every test passed. `asan` took 257 s, 188 s, 258 s, 196 s, 436 s and
  476 s. `ubsan` took 261 s, 192 s, 256 s, 209 s, 457 s and 510 s.

## Linux and Windows

The VMs were not touched, so no suite of either host ran. The capture of
the standard error on Windows, through `start` of the platform layer, is
compiled on no host here and is unverified. Both hosts stay on anti.8,
which has no dsymutil, so neither check runs there yet.
