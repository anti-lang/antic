# Step 34, rules 18 and 19, eighteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The seventeenth
session, `docs/reports/2026-10-20-solid-step34-driver-interface.md`,
left `driver_load_libraries` of `driver_search.c` first on its list.
This session took it. The logs stand in `build/drive/logs/` under the
prefix `s34r-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `driver_load_libraries` of `driver_search.c` (back-end) | `f7649fbb` | The load loop ended in the report of a blocked library file, at a nest of 5. |

The finding was confirmed by its code at `acae04bc`. The report of the
first import that no file provides, or else of the cycle, stood inside
the `while` of the load at a nest of 5. It is now `report_blocked`, at a
nest of 3, and `driver_load_libraries` reaches 4. The messages stay as
they were.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
465 tests of modules, of the messages of antic, `emit_identity` and
`link_identity` passed first, `s34r-quick.log`. `tests/modules/cycle`,
`unit_modules` and the cases of `tests/antic_messages.cmake` hold the
three messages.

`559b3e80` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/back-end.md`: `frames_kept` of `llvm_emit_facts.c`.
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

- The three builds report no warnings: `s34r-build.log`,
  `s34r-asan-build.log` and `s34r-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 354 s and `-I 2001,5194` 286 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34r-host1.log`, `s34r-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  493 s: `s34r-asan1.log` to `s34r-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 368 s: `s34r-ubsan1.log` to
  `s34r-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34r-docs1.log` and `s34r-docs2.log`.

## Not run on the target

The split moves no code of a platform. The load of library files runs
the same on every host, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
559b3e80 Record the split of driver_search.c of step 34 in the summary
f7649fbb Split the report of the blocked library files out of driver_load_libraries
acae04bc Report the seventeenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
559b3e801a2bcee0c6620cd0be53d137fd1d8772
559b3e801a2bcee0c6620cd0be53d137fd1d8772
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `frames_kept` of `llvm_emit_facts.c`.
