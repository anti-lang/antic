# Step 34, rules 18 and 19, eighth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The seventh
session, `docs/reports/2026-10-19-solid-step34-check-expr.md`, left the
next file of its list, `sema.c`, with `resolve_type_inner` of
`docs/audit/front-end.md` under "Functions past a threshold". The logs
stand in `build/drive/logs/` under the prefix `s34h-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `resolve_type_inner` (front-end) | `20359a0f` | The named and function type arms stood inline in its switch, 209 lines. |

The finding was confirmed at `1aec7a07`. `resolve_type_inner` stood at
209 lines, past the 206 of the report. The `TYPEX_NAMED` arm is now
`resolve_named`, whose form `module.Name` is `resolve_imported`, and
the `TYPEX_FN` arm is `resolve_fn_type`. No statement changed its
order. A line diff of the old and the new code shows the three headers,
the dispatch and three comments and two calls rewrapped to the wider
indent alone. `resolve_type_inner` stands at 75 lines.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds every type form and its refusals in `tests/matrix/`,
`tests/errors/` and `tests/programs/`.

`dded05a2` records the commit in the row of rule 18 of the summary. No
page of `docs/site/developers-guide/` names the function.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
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

- The three builds report no warnings: `s34h-build1.log`,
  `s34h-asan-build.log` and `s34h-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 362 s and `-I 2001,5153` 283 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34h-host1.log`, `s34h-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 246 s, 165 s, 178 s and 517 s. All 1150, 1119, 1458
  and 1459 pass: `s34h-asan1.log` to `s34h-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 245 s, 141 s
  and 368 s. All 1150, 1119 and 2909 pass: `s34h-ubsan1.log` to
  `s34h-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34h-docs1.log` and `s34h-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
dded05a2 Record the split of resolve_type_inner of step 34 in the summary
20359a0f Split the named and function type arms out of resolve_type_inner
1aec7a07 Report the seventh session of step 34
$ git status --short
?? docs/reports/2026-10-19-solid-step34-resolve-type.md
$ git rev-parse HEAD origin/main
dded05a29274aa7a81dffa975a68bb40ae866f15
dded05a29274aa7a81dffa975a68bb40ae866f15
```

The untracked file is this report, whose commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `sema_generic_call` of `sema_generic.c`.
