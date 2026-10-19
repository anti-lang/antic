# Step 34, rules 18 and 19, third session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The first session,
`docs/reports/2026-10-19-solid-step34.md`, split `sema_stmt.c`, and the
second, `docs/reports/2026-10-19-solid-step34-llvm-emit.md`, split
`llvm_emit.c`. This session took the first item of the list the second
left, `sema.c`. The logs stand in `build/drive/logs/` under the prefix
`s34c-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 19, `sema.c` past 3000 lines (tool-pass, front-end) | `e5928ae0` | One file held the helpers of the checker, every pass that declares the items and the passes over the bodies, 3134 lines. |

The defect was confirmed by its count: 3134 lines at `884c4a96`, past
the 2948 of the audit. The passes that declare the items moved to
`sema_declare.c`, the split the front-end report names:

| File | Lines | Holds |
|---|---|---|
| `sema.c` | 1475 | The helpers, scopes and lookup, narrowing, the resolution of types and signatures, the standard items the checker names, the passes over the bodies and over what crosses to C, and `sema_check` |
| `sema_declare.c` | 1673 | The root, the imports, the items, the bases, the enums and variants, the constants of the bodies, the fields, the function types, the members and the signatures, with the checks of `provides`, `link` and a simd struct |

The move changes no line of code. A script moved each top-level item
whole and renamed the sixteen functions the two files now share to the
prefix `sema_`, thirteen passes of `sema_declare.c` and `sema_scope_put`,
`sema_function_type` and `sema_lang_error_or_null` of `sema.c`. It
realigned the continuation lines of a signature or a call whose
parenthesis moved. A comparison of every line of the old file with the
two new ones, the renames undone, finds only the header comments, the
includes and one call of `sema_check_link_names` rewrapped to 80
columns. `lang_error_or_null` stays in `sema.c` beside
`sema_error_class`, which reads the same constant `lang_module`. `cpu.h`
moved with `check_simd_struct`, its one user.

The test of the finding is the suite, as in the earlier sessions.
`source_tree` failed until `sema_declare.c` had its row in
`docs/notes/source-tree.md`. `header_sections` checks the new section
of `sema_checker.h`. The developers' guide and `docs/notes/` now name
the file of each moved function, and `guide_examples` passes.

`98ff4fe5` records the commit in the row of rule 19 of the summary.

## Findings left

Rule 18, the functions the reports judged clearer split:
`docs/audit/front-end.md` under "Functions past a threshold",
`docs/audit/back-end.md` under "The functions past a threshold",
`docs/audit/middle.md` for the four parameter lists of `lower_owning.c`,
and `docs/audit/library-file.md` for `read_signature`, `antl_read_ir`,
`read_declaration` and `read_extern`. The step takes one file per
session, so they wait.

## New defects and entries

No new defect. One `[provisional]` entry under "Files of the checker" in
`docs/decisions.md` records what `sema_declare.c` holds. The entry on
the checker counts twenty files, and it and the entry on M10 name
`sema_declare.c` as the file that checks the `provides` and `link`
lines.

`tests/known-failures.txt` holds 29 lines before and after, ten of them
tests. No new feature, option or command. `emit_identity` and every
`link_identity_<target>` pass unchanged.

## Gates

- The three builds report no warnings: `s34c-build2.log`,
  `s34c-asan-build.log` and `s34c-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 357 s and `-I 2001,5153` 288 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34c-host1.log`, `s34c-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`,
  248 s, 178 s and 552 s. All 1150, 1119 and 2909 pass:
  `s34c-asan1.log` to `s34c-asan3.log`. The third part stands above 9
  minutes, and the next session runs it in two.
- UBSan, the same parts, 243 s, 140 s and 369 s. All 1150, 1119 and
  2909 pass: `s34c-ubsan1.log` to `s34c-ubsan3.log`.
- The docs-style checker reports no error on each `.md` file the
  session touched: `s34c-docs1.log`, `s34c-docs2.log` and
  `s34c-docs3.log`. Its one warning stands on line 511 of
  `docs/notes/source-tree.md`, the line the second session reported at
  510, which the new row moved.

## Not run on the target

Nothing. The move is in the front end, which runs the same on every
host.

## State at the push

```text
$ git log --oneline -3
98ff4fe5 Record the split of sema.c of step 34 in the summary
e5928ae0 Split the declaration passes out of sema.c into sema_declare.c
884c4a96 Report the second session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
98ff4fe508ac45d3ce9407fe44c9da8da2bb17fe
98ff4fe508ac45d3ce9407fe44c9da8da2bb17fe
```

## Left for the next session

1. The functions of rule 18 that the reports judged clearer split, one
   file per session, from the list under "Findings left".
