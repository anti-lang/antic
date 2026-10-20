# Step 34, rules 18 and 19, sixteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The fifteenth
session, `docs/reports/2026-10-20-solid-step34-parameters.md`, left
`driver_build_c_library` and `bundle` of `driver_library.c` first on its
list. This session took them. The logs stand in `build/drive/logs/`
under the prefix `s34p-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `driver_build_c_library` and `bundle` of `driver_library.c` (back-end) | `122eda50` | One function held the static and the shared library in two branches, and the other extracted the runtime and joined it. |

Both findings were confirmed by their code at `e51a78a4`:
`driver_build_c_library` ran to 195 lines with a shared branch of 125,
and `bundle` to 108. `build_static` and `build_shared` now hold the two
branches. `soname_parts` takes the names of `--soname`, and
`write_exports` writes the export file of macOS and the `.def` file of
Windows. `extract_runtime` and `join_bundle` are the two parts of
`bundle`. `add_pooled` copies a path into the memory pool in the three
places that did it inline, and `append_exports` writes the export lines
of both systems. The longest function of the file is now
`build_shared`, at 67 lines. Every command line and every file the build
writes stays as it was.

The test of the findings is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
252 tests of the libraries, the bundles, the plugins, `--soname`, the
ABI, `emit_identity` and `link_identity` passed first,
`s34p-quick.log`.

`2f324533` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/back-end.md` under "The functions past a threshold":
  `driver_interface` of `driver.c`, `driver_load_libraries` of
  `driver_search.c` and `frames_kept` of `llvm_emit_facts.c`.
- `docs/audit/middle.md`: the four parameter lists of `lower_owning.c`.
- `docs/audit/library-file.md`: `read_signature` and `antl_read_ir` of
  `antl_read_ir.c`, `read_declaration` and `read_extern` of
  `antl_tree.c`, and the parameters of `antl_read`.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34p-build2.log`,
  `s34p-asan-build.log` and `s34p-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 349 s and `-I 2001,5194` 278 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34p-host1.log`, `s34p-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  513 s: `s34p-asan1.log` to `s34p-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 393 s: `s34p-ubsan1.log` to
  `s34p-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34p-docs1.log` and `s34p-docs2.log`.

## Not run on the target

The split moves the code of the libraries of Linux and Windows, which
the Mac builds and links but does not run:

- The `.def` file of a Windows library and of a Windows plugin, now
  written by `write_exports`: `clib_shared` and `plugin_host` run on
  anti-windows.
- The versioned file and the link of `--soname` on Linux, now named by
  `soname_parts`: `anti_option_soname_linux` with the library loaded by
  a C program on anti-linux.
- The joined object of a bundled runtime of ELF and COFF, now written by
  `join_bundle`: `clib_bundle` run on both VMs.

## State at the push

```text
$ git log --oneline -3
2f324533 Record the split of driver_library.c of step 34 in the summary
122eda50 Split driver_build_c_library and bundle of driver_library.c
e51a78a4 Report the fifteenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
2f324533f043550af56e80dc1e0253b577928cc5
2f324533f043550af56e80dc1e0253b577928cc5
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `driver_interface` of `driver.c`.
