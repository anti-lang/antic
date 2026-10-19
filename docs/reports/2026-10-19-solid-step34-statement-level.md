# Step 34, rules 18 and 19, fourth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The first three
sessions split `sema_stmt.c`, `llvm_emit.c` and `sema.c`, and the third,
`docs/reports/2026-10-19-solid-step34-sema-declare.md`, left the
functions of rule 18 that the reports judged clearer split. This session
took the first of them, `statement_level` of `parser_stmt.c`, the first
function of the list of `docs/audit/front-end.md` under "Functions past a
threshold". The logs stand in `build/drive/logs/` under the prefix
`s34d-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `statement_level` (front-end) | `a07f6378` | Its cases held the whole of the `for`, `switch` and `select` statements and the statement no keyword opens, 410 lines and a nest of 5. |

The finding was confirmed by its count: `statement_level` stood at 410
lines at `1bf63543`, the count of the front-end report. The four parts
the report names are now `for_statement`, `switch_statement`,
`select_statement` and `expression_statement`, beside `let_or_const` and
`if_let`. A script moved each case whole and took four spaces off its
lines. Each function declares its `struct stmt *s` first, and the
DESIGN comment of each case stands above its function. No line of code
changed. `statement_level` stands at 180 lines, a dispatch over the
keywords whose short cases stay in place, and `parser_stmt.c` at 906.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds every form of the four statements, their refusals included,
in `tests/matrix/`, `tests/errors/` and `tests/dump/`.

`2e75369c` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `primary` and `postfix_links` of `parser_expr.c`, `item_level` and
  `class_item` of `parser_item.c`, `check_expr_inner` of `sema_expr.c`,
  `resolve_type_inner` of `sema.c`, `sema_generic_call` of
  `sema_generic.c`, `xe` and `xs` of `sema_copies.c`, `patch_call` of
  `sema_pattern.c`, `check_let` and `check_for` of `sema_stmt.c`,
  `parse_group` of `pattern.c` and `sema_check_binary` of
  `sema_operator.c`. Then the parameter lists of
  `sema_check_field_inits`, `sema_check` and `sema_doc_warnings`.
- `docs/audit/back-end.md` under "The functions past a threshold":
  `driver_build_c_library` and `bundle` of `driver_library.c`,
  `driver_interface` of `driver.c`, `driver_load_libraries` of
  `driver_search.c` and `frames_kept`, now in `llvm_emit_facts.c`.
- `docs/audit/middle.md`: the four parameter lists of `lower_owning.c`.
- `docs/audit/library-file.md`: `read_signature` and `antl_read_ir` of
  `antl_read_ir.c`, `read_declaration` and `read_extern` of
  `antl_tree.c`, and the parameters of `antl_read`.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 29 lines before and after, ten of them tests. No new feature,
option or command. `emit_identity` and `link_identity_macos-arm64`, the
one target of the Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34d-build1.log`,
  `s34d-asan-build.log` and `s34d-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 347 s and `-I 2001,5153` 277 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34d-host1.log`, `s34d-host2.log`.
- ASan, 5151 tests, in four parts, `-I 1,1150`, `-I 1151,2250`,
  `-I 2251,3700` and `-I 3701,5151`, 246 s, 165 s, 175 s and 507 s. All
  1150, 1119, 1458 and 1459 pass: `s34d-asan1.log` to `s34d-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 243 s, 140 s
  and 360 s. All 1150, 1119 and 2909 pass: `s34d-ubsan1.log` to
  `s34d-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34d-docs1.log` and `s34d-docs2.log`.

A first run of the tests of the parser, `s34d-quick.log`, was started
without a time limit. The tool moved it to the background after two
minutes. It passed 399 of 399. Every later run was in the foreground.

## Not run on the target

Nothing. The move is in the parser, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
2e75369c Record the split of statement_level of step 34 in the summary
a07f6378 Split the for, switch, select and expression statements out of statement_level
1bf63543 Report the third session of step 34
$ git status --short
?? docs/reports/2026-10-19-solid-step34-statement-level.md
$ git rev-parse HEAD origin/main
2e75369c662feab01da58aeebf67c14903c3216d
2e75369c662feab01da58aeebf67c14903c3216d
```

The untracked file is this report, whose commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `primary` and `postfix_links` of `parser_expr.c`.
