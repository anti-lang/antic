# Step 34, rules 18 and 19, sixth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The fifth session,
`docs/reports/2026-10-19-solid-step34-primary.md`, left the next file of
its list, `parser_item.c`, with `item_level` and `class_item` of
`docs/audit/front-end.md` under "Functions past a threshold". The logs
stand in `build/drive/logs/` under the prefix `s34f-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `item_level` (front-end) | `1152d6df` | The struct, union and enum items stood inline in its switch, 317 lines. |
| Rule 18, `class_item` (front-end) | `1152d6df` | The field loop of a class body repeated half of what a struct field reads. |

Both were confirmed at `ccd2d3d2`. `item_level` stood at 317 lines,
past the 298 of the report. The field loop of `class_item` had moved
into `class_body` by `c7669087` since the audit, and stood there at 139
lines with every mark of a field inline. The struct and union case is
now `struct_item` and the enum case `enum_item`, as `class_item` and
`variant_item` already were. The marks before the name of a class field
are `field_marks`. The name, the type, the guard and clauses and the
width are `field_declaration`, which a struct field and a class field
share. No statement changed its order. `item_level` stands at 220
lines, the marks of an item header and the dispatch, and `class_body`
at 79. `parser_item.c` stands at 684 lines.

The sentence of `docs/site/developers-guide/08-structs-enums.md` that
names the functions reading `packed` and `align` names `struct_item` as
well.

The test of the findings is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds every item form and its refusals in `tests/matrix/`,
`tests/errors/`, `tests/dump/` and `unit_parser`.

`dcdfc3e8` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `check_expr_inner` of `sema_expr.c`, `resolve_type_inner` of
  `sema.c`, `sema_generic_call` of `sema_generic.c`, `xe` and `xs` of
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

- The three builds report no warnings: `s34f-build1.log`,
  `s34f-asan-build.log` and `s34f-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 355 s and `-I 2001,5153` 283 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34f-host1.log`, `s34f-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 245 s, 164 s, 174 s and 504 s. All 1150, 1119, 1458
  and 1459 pass: `s34f-asan1.log` to `s34f-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 242 s, 146 s
  and 365 s. All 1150, 1119 and 2909 pass: `s34f-ubsan1.log`,
  `s34f-ubsan2.log` and `s34f-ubsan3b.log`. The first run of the third
  part, `s34f-ubsan3.log`, stopped `matrix_values_aggregates_dev` at
  the 60 s of the matrix runner under the full load. Alone it passed in
  25 s, `s34f-ubsan-retry.log`. The step 33 report on the tests asks
  Eddie about this limit.
- The docs-style checker reports nothing on the guide page, on
  `docs/audit/summary.md` and on this report: `s34f-docs1.log` to
  `s34f-docs3.log`.

## Not run on the target

Nothing. The move is in the parser, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
dcdfc3e8 Record the split of parser_item.c of step 34 in the summary
1152d6df Split the struct and enum items and the class field out of parser_item.c
ccd2d3d2 Report the fifth session of step 34
$ git status --short
?? docs/reports/2026-10-19-solid-step34-item-level.md
$ git rev-parse HEAD origin/main
dcdfc3e8a756d1249731ce3ab3902f6b175531b9
dcdfc3e8a756d1249731ce3ab3902f6b175531b9
```

The untracked file is this report, whose commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `check_expr_inner` of `sema_expr.c`.
