# Step 34, rules 18 and 19, seventeenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The sixteenth
session, `docs/reports/2026-10-20-solid-step34-driver-library.md`, left
`driver_interface` of `driver.c` first on its list. This session took
it. The logs stand in `build/drive/logs/` under the prefix `s34q-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `driver_interface` of `driver.c` (back-end) | `c7ac2465` | One function took a library file on one branch and a source on the other. |

The finding was confirmed by its code at `21aa9fd3`. `driver_interface`
had handed its body to `interface_of` since `461dba24`, and
`interface_of` still ran to 120 lines, with the library file as a
branch of 40. That branch is now `library_interface`, and
`interface_of` runs to 84 lines and holds the source alone. The messages
and the interface handed to `anti doc` stay as they were.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
47 tests of `anti doc`, the doc pages, `emit_identity` and
`link_identity` passed first, `s34q-quick.log`.

`32fd761a` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/back-end.md` under "The functions past a threshold":
  `driver_load_libraries` of `driver_search.c` and `frames_kept` of
  `llvm_emit_facts.c`.
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

- The three builds report no warnings: `s34q-build.log`,
  `s34q-asan-build.log` and `s34q-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 360 s and `-I 2001,5194` 300 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34q-host1.log`, `s34q-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  505 s: `s34q-asan1.log` to `s34q-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 381 s: `s34q-ubsan1.log` to
  `s34q-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34q-docs1.log` and `s34q-docs2.log`.

## Not run on the target

The split moves no code of a platform. `anti doc` of a library file
runs the same on every host, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
32fd761a Record the split of driver.c of step 34 in the summary
c7ac2465 Split the library file of interface_of in driver.c into its own function
21aa9fd3 Report the sixteenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
32fd761adb673fa87a913fd9144f03df2c7f19f5
32fd761adb673fa87a913fd9144f03df2c7f19f5
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `driver_load_libraries` of `driver_search.c`.
