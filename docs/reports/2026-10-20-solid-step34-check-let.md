# Step 34, rules 18 and 19, twelfth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The eleventh
session, `docs/reports/2026-10-20-solid-step34-patch-call.md`, left the
next file of the list of `docs/audit/front-end.md` under "Functions past
a threshold": `check_let` and `check_for` of `sema_stmt.c`. This session
took them. The logs stand in `build/drive/logs/` under the prefix
`s34l-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `check_let` and `check_for` of `sema_stmt.c` (front-end) | `195d9a5e` | `check_let` held the marks on the declared symbol beside the check of the value, and `check_for` the element type of the `over` forms beside the range and the bindings. |

The finding was confirmed by its count: `check_let` stood at 149 lines
and `check_for` at 146 at `24bd64c0`, as at the audit. The two splits
are the ones the report names. `mark_let_symbol` sets the type, the
held value, the pointer into fields, the atomic mark, the closure and
the place of its own of the local a `let` declares, 52 lines, and
`check_let` stands at 103. `for_over_element` checks the value after
`in` and gives the element of a slice, an array, a collection or an
iterator, 56 lines, and `check_for` stands at 96. No message, no order
of the checks and no mark changes.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
664 tests whose names hold `let`, `for`, `iter`, `atomic`, `closure` or
`guard`, with `emit_identity` and `link_identity`, passed first,
`s34l-quick.log`.

`979d0fa4` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `parse_group` of `pattern.c` and `sema_check_binary` of
  `sema_operator.c`. Then the parameter lists of
  `sema_check_field_inits`, `sema_check` and `sema_doc_warnings`.
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

- The three builds report no warnings: `s34l-build1.log`,
  `s34l-asan-build.log` and `s34l-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 328 s and `-I 2001,5194` 313 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34l-host1.log`, `s34l-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  505 s: `s34l-asan1.log` to `s34l-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass: `s34l-ubsan1.log` to `s34l-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34l-docs1.log` and `s34l-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
979d0fa4 Record the split of check_let and check_for of step 34 in the summary
195d9a5e Split the marks of the symbol out of check_let and the element out of check_for
24bd64c0 Report the eleventh session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
979d0fa42db811cb593d8ef32632debc3c455031
979d0fa42db811cb593d8ef32632debc3c455031
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `parse_group` of `pattern.c`.
