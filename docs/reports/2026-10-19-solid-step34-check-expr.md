# Step 34, rules 18 and 19, seventh session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The sixth session,
`docs/reports/2026-10-19-solid-step34-item-level.md`, left the next file
of its list, `sema_expr.c`, with `check_expr_inner` of
`docs/audit/front-end.md` under "Functions past a threshold". The logs
stand in `build/drive/logs/` under the prefix `s34g-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `check_expr_inner` (front-end) | `601baefa` | The name, struct literal and object arms stood inline in its switch, 721 lines. |

The finding was confirmed at `053eb947`. `check_expr_inner` stood at
721 lines, past the 641 of the report. The `EXPR_NAME` arm is now
`check_name`, the `EXPR_STRUCT_LIT` arm `check_struct_lit` and the
`EXPR_OBJECT` arm `check_object`, with the DESIGN comment of `dup`,
`delete` and `destroy` above it. The `EXPR_INDEX` arm, 47 lines, is
`check_index` as well, which brings the dispatch to the size the report
expected. No statement changed its order. `check_expr_inner` stands at
321 lines.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds every expression form and its refusals in `tests/matrix/`,
`tests/errors/`, `tests/dump/` and `tests/programs/`.

`3771507a` records the commit in the row of rule 18 of the summary. No
page of `docs/site/developers-guide/` names the function.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
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

- The three builds report no warnings: `s34g-build1.log`,
  `s34g-asan-build.log` and `s34g-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 356 s and `-I 2001,5153` 280 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34g-host1.log`, `s34g-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 244 s, 164 s, 176 s and 509 s. All 1150, 1119, 1458
  and 1459 pass: `s34g-asan1.log` to `s34g-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 246 s, 140 s
  and 369 s. All 1150, 1119 and 2909 pass: `s34g-ubsan1.log` to
  `s34g-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34g-docs1.log` and `s34g-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
3771507a Record the split of check_expr_inner of step 34 in the summary
601baefa Split the name, index, literal and object arms out of check_expr_inner
053eb947 Report the sixth session of step 34
$ git status --short
?? docs/reports/2026-10-19-solid-step34-check-expr.md
$ git rev-parse HEAD origin/main
3771507a6ee8606f45f4fdf63a9acc8d0720539e
3771507a6ee8606f45f4fdf63a9acc8d0720539e
```

The untracked file is this report, whose commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `resolve_type_inner` of `sema.c`.
