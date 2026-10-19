# Step 34, rules 18 and 19, tenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The ninth session,
`docs/reports/2026-10-19-solid-step34-generic-call.md`, left the next
file of the list of `docs/audit/front-end.md` under "Functions past a
threshold": `xe` and `xs` of `sema_copies.c`. This session took it. The
logs stand in `build/drive/logs/` under the prefix `s34j-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `xe` and `xs` of `sema_copies.c` (front-end) | `4bf82295` | `xe` wrote the marks and the second reading of an open node four times and held the call and the `f"..."` text, and `xs` held the assignment through `set_index` and the loop over a type parameter. |

The finding was confirmed by its count: `xe` stood at 243 lines and `xs`
at 185 at `904b82b0`, against 229 and 185 at the audit. The report names
three recheck blocks as one helper. A fourth, on unary `-` and `~`, came
since. `reread` now marks the operands and reads the node again for all
four. The call and the text moved to `xcall` and `xformat`, and the
`STMT_ASSIGN` and `STMT_FOR` cases the report names to `xassign` and
`xfor`. `redirect_iter` holds the eight redirects of the hooks of a walk,
and `xarms` the arms that `switch` and `select` wrote out twice. `xe`
stands at 184 lines and `xs` at 97.

In the binary and the call arm the marks of the operands now follow the
clearing of the default `==` and hash. The clearing writes other fields
of the node, so the order makes no difference. No other statement
changed its order.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds the copies, their second readings and their refusals in
`tests/matrix/`, `tests/errors/` and `tests/programs/`. The 380 tests
whose names hold `generic`, `infer`, `copies` or `collection` passed
first, `s34j-quick.log`.

`9a703745` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `patch_call` of `sema_pattern.c`, `check_let` and `check_for` of
  `sema_stmt.c`, `parse_group` of `pattern.c` and `sema_check_binary` of
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

- The three builds report no warnings: `s34j-build1.log`,
  `s34j-asan-build.log` and `s34j-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 348 s and `-I 2001,5153` 280 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34j-host1.log`, `s34j-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 250 s, 163 s, 175 s and 519 s. All 1150, 1119, 1458
  and 1459 pass: `s34j-asan1.log` to `s34j-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 252 s, 144 s
  and 402 s. All 1150, 1119 and 2909 pass: `s34j-ubsan1.log`,
  `s34j-ubsan2.log` and `s34j-ubsan3b.log`. The first run of the third
  part, `s34j-ubsan3.log`, lost `matrix_values_bindings_and_calls_dev`
  to the 60 s of the matrix runner. antic took 60.17 s there, against
  50 to 58 s in the runs of the earlier sessions of this step. Run
  alone it passed in 19 s, `s34j-ubsan-rerun.log`, and in 31 s in the
  next run of the part. `docs/notes/hosts-and-harness.md` holds the trap,
  and `docs/reports/2026-10-12-tests-types.md` met it first.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34j-docs1.log` and `s34j-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
9a703745 Record the split of xe and xs of step 34 in the summary
4bf82295 Split the call, assignment and loop out of xe and xs of sema_copies.c
904b82b0 Report the ninth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
9a703745da867db057c22d8a69c450f93cbc03d2
9a703745da867db057c22d8a69c450f93cbc03d2
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `patch_call` of `sema_pattern.c`.
