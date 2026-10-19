# Step 34, rules 18 and 19, fifth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The fourth
session, `docs/reports/2026-10-19-solid-step34-statement-level.md`, left
the functions of rule 18 that the reports judged clearer split. This
session took the next file of its list, `parser_expr.c`, with `primary`
and `postfix_links` of `docs/audit/front-end.md` under "Functions past a
threshold". The logs stand in `build/drive/logs/` under the prefix
`s34e-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `primary` (front-end) | `29cde2ba` | Its cases held the whole of the forms a name opens, the group and the tuple, and the literals in brackets, 368 lines and a nest of 5. |
| Rule 18, `postfix_links` (front-end) | `29cde2ba` | Its `[` and `.` branches stood inline in the loop, 105 lines and a nest of 6. |

Both were confirmed by their counts at `f2c4f862`: `primary` stood at
368 lines, past the 356 of the report, and `postfix_links` at 105. The
`TOKEN_IDENT` case the report names is now `name_or_literal`, and the
`(` and `[` cases are `group_or_tuple` and `bracket_literal`. `primary`
keeps the test that a condition takes no `[]T { }`, so that form still
gives "expected an expression". It stands at 203 lines, a dispatch over
the tokens whose short cases stay in place. The `[` and `.` branches of
`postfix_links` are `index_link` and `field_link`. The loop keeps the
call branch and tests the link for NULL once. A script moved each case
whole. Each function declares its locals at its head, and no statement
changed. `parser_expr.c` stands at 1108 lines.

The test of the findings is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds every form of the moved cases, their refusals included, in
`tests/matrix/`, `tests/errors/`, `tests/dump/` and `unit_parser`.

`a112ab1b` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `item_level` and `class_item` of `parser_item.c`, `check_expr_inner`
  of `sema_expr.c`, `resolve_type_inner` of `sema.c`,
  `sema_generic_call` of `sema_generic.c`, `xe` and `xs` of
  `sema_copies.c`, `patch_call` of `sema_pattern.c`, `check_let` and
  `check_for` of `sema_stmt.c`, `parse_group` of `pattern.c` and
  `sema_check_binary` of `sema_operator.c`. Then the parameter lists of
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

- The three builds report no warnings: `s34e-build1.log`,
  `s34e-asan-build.log` and `s34e-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 165 s and `-I 2001,5153` 249 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34e-host1.log`, `s34e-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 357 s, 285 s, 139 s and 508 s. All 1150, 1119, 1458
  and 1459 pass: `s34e-asan1.log` to `s34e-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 245 s, 176 s
  and 354 s. All 1150, 1119 and 2909 pass: `s34e-ubsan1.log` to
  `s34e-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34e-docs1.log` and `s34e-docs2.log`.

## Not run on the target

Nothing. The move is in the parser, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
a112ab1b Record the split of primary of step 34 in the summary
29cde2ba Split the name, group and bracket forms out of primary and postfix_links
f2c4f862 Report the fourth session of step 34
$ git status --short
?? docs/reports/2026-10-19-solid-step34-primary.md
$ git rev-parse HEAD origin/main
a112ab1b2698b807d3e9818569e06727c84afd93
a112ab1b2698b807d3e9818569e06727c84afd93
```

The untracked file is this report, whose commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `item_level` and `class_item` of `parser_item.c`.
