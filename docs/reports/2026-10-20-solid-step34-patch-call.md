# Step 34, rules 18 and 19, eleventh session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The tenth session,
`docs/reports/2026-10-19-solid-step34-copies.md`, left the next file of
the list of `docs/audit/front-end.md` under "Functions past a
threshold": `patch_call` of `sema_pattern.c`. This session took it. The
logs stand in `build/drive/logs/` under the prefix `s34k-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `patch_call` of `sema_pattern.c` (front-end) | `2353847f` | One body held the check of the five arguments, the `[]byte` form and the pattern form with the group and the fit of a literal. |

The finding was confirmed by its count: `patch_call` stood at 166 lines
at `07fde00e`, as at the audit. The checked arguments now travel in
`struct patch_args`. `check_patch_args` checks them, `patch_bytes_call`
writes the `[]byte` form, `patch_literal_group` the group and the fit
of a pattern literal, and `patch_pattern_call` the pattern form.
`patch_call` stands at 16 lines and keeps the count check and the
choice of the form. No message, no order of the checks and no argument
of a call changes.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
163 tests whose names hold `patch`, `regex`, `pattern`, `byte` or
`match`, with `emit_identity` and `link_identity`, passed first,
`s34k-quick.log`.

`94236407` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `check_let` and `check_for` of `sema_stmt.c`, `parse_group` of
  `pattern.c` and `sema_check_binary` of `sema_operator.c`. Then the
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

- The three builds report no warnings: `s34k-build1.log`,
  `s34k-asan-build.log` and `s34k-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 344 s and `-I 2001,5194` 298 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34k-host1.log`, `s34k-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`, 249 s, 165 s, 176 s and 501 s. All 1150, 1119, 1457
  and 1497 pass: `s34k-asan1.log` to `s34k-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250`, `-I 2251,5190` and
  `-I 5191,5192`, 236 s, 138 s, 354 s and a few seconds. All 1150,
  1119, 2948 and 2 pass: `s34k-ubsan1.log` to `s34k-ubsan4.log`. The
  third part stopped two tests short of the end by a slip of its range,
  and the fourth ran those two.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34k-docs1.log` and `s34k-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
94236407 Record the split of patch_call of step 34 in the summary
2353847f Split the argument check and the two forms out of patch_call
07fde00e Report the step answers-language
$ git status --short
$ git rev-parse HEAD origin/main
94236407486dd22c1ee0480cda514bf56b3c3400
94236407486dd22c1ee0480cda514bf56b3c3400
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `check_let` and `check_for` of `sema_stmt.c`.
