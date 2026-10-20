# Step 34, rules 18 and 19, fourteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The thirteenth
session, `docs/reports/2026-10-20-solid-step34-parse-group.md`, left the
next file of the list of `docs/audit/front-end.md` under "Functions past
a threshold": `sema_check_binary` of `sema_operator.c`. This session took
it. The logs stand in `build/drive/logs/` under the prefix `s34n-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `sema_check_binary` of `sema_operator.c` (front-end) | `21d1fd07` | `sema_check_binary` held the logical operators, the comparisons and the arithmetic in one switch, and the last two repeated one prefix of checks. |

The finding was confirmed by its count: `sema_check_binary` stood at 193
lines at `da2dfe07`, against 185 at the audit, and the comparison and the
arithmetic arms both checked a type parameter, a simd struct and an
operator function in the same order. The split is the one the report
names. `shared_binary` holds that prefix once. `check_logical`,
`check_comparison` and `check_arithmetic` hold the three arms, and
`default_equality` holds the default `==` of a type of parts with its
three messages. `sema_check_binary` is now a dispatch of 20 lines. No
check moved before or after another, and every message stays as it was.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
924 tests whose names hold an operator, simd, flags, a comparison, the
values, hashing or `none`, with `emit_identity` and `link_identity`,
passed first, `s34n-quick.log`.

`b483a044` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold": the
  parameter lists of `sema_check_field_inits`, `sema_check` and
  `sema_doc_warnings`.
- `docs/audit/back-end.md` under "The functions past a threshold":
  `driver_build_c_library` and `bundle` of `driver_library.c`,
  `driver_interface` of `driver.c`, `driver_load_libraries` of
  `driver_search.c` and `frames_kept` of `llvm_emit_facts.c`.
- `docs/audit/middle.md`: the four parameter lists of `lower_owning.c`.
- `docs/audit/library-file.md`: `read_signature` and `antl_read_ir` of
  `antl_read_ir.c`, `read_declaration` and `read_extern` of
  `antl_tree.c`, and the parameters of `antl_read`.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after, 14 of them tests. No new feature,
option or command. `emit_identity` and `link_identity_macos-arm64`, the
one target of the Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34n-build2.log`,
  `s34n-asan-build.log` and `s34n-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 336 s and `-I 2001,5194` 314 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34n-host1.log`, `s34n-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  500 s: `s34n-asan1.log` to `s34n-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 350 s: `s34n-ubsan1.log` to
  `s34n-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34n-docs1.log` and `s34n-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
b483a044 Record the split of sema_check_binary of step 34 in the summary
21d1fd07 Split the comparisons and the arithmetic out of sema_check_binary
da2dfe07 Report the thirteenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
b483a04485ec47fccc558187a853d2bc6958ad5d
b483a04485ec47fccc558187a853d2bc6958ad5d
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with the parameter lists of `sema_check_field_inits`, `sema_check` and
   `sema_doc_warnings`.
