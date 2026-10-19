# Step 34, rules 18 and 19, ninth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The eighth session,
`docs/reports/2026-10-19-solid-step34-resolve-type.md`, left the next
function of the list of `docs/audit/front-end.md` under "Functions past a
threshold", `sema_generic_call` of `sema_generic.c`. This session took
it. The logs stand in `build/drive/logs/` under the prefix `s34i-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `sema_generic_call` (front-end) | `ac55d2b6` | It laid out the map, took the arguments of the class, inferred from the arguments of the call and made the copy of the class in one body of 143 lines. |

The finding was confirmed by its count: `sema_generic_call` stood at 143
lines at `1b92fdc3`, against 151 at the audit. The report names three
parts: the map builder, the inference loop and the tail that repeated
`finish_copy`. The tail is shared since `5d9b4457`, the fix of rule 26,
through `refuse_unbound`, `meets_all` and `record_copy`. The other
parts are now `open_map`, `class_args`, `infer` and `complete_class`.
The DESIGN comment on inference stands above `infer`, and the comment on
the class of the call above `class_args`. No statement changed its
order. `sema_generic_call` stands at 71 lines and keeps its six
parameters, the threshold being more than six.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
suite holds the inference, the copies of a class and their refusals in
`tests/matrix/`, `tests/errors/` and `tests/programs/`. The 380 tests
whose names hold `generic`, `infer`, `copies` or `collection` passed
first, `s34i-quick.log`.

`2069c578` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold": `xe`
  and `xs` of `sema_copies.c`, `patch_call` of `sema_pattern.c`,
  `check_let` and `check_for` of `sema_stmt.c`, `parse_group` of
  `pattern.c` and `sema_check_binary` of `sema_operator.c`. Then the
  parameter lists of `sema_check_field_inits`, `sema_check` and
  `sema_doc_warnings`.
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

- The three builds report no warnings: `s34i-build1.log`,
  `s34i-asan-build.log` and `s34i-ubsan-build.log`.
- Host, 5153 tests, `-I 1,2000` 348 s and `-I 2001,5153` 288 s. All
  2000 and 3179 pass, a part counting the fixtures it pulls in. The 8
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34i-host1.log`, `s34i-host2.log`.
- ASan, 5151 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5151`, 248 s, 165 s, 176 s and 500 s. All 1150, 1119, 1458
  and 1459 pass: `s34i-asan1.log` to `s34i-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5151`, 243 s, 140 s
  and 366 s. All 1150, 1119 and 2909 pass: `s34i-ubsan1.log` to
  `s34i-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34i-docs1.log` and `s34i-docs2.log`.

## Not run on the target

Nothing. The move is in the checker, which runs the same on every host.

## State at the push

```text
$ git log --oneline -3
2069c578 Record the split of sema_generic_call of step 34 in the summary
ac55d2b6 Split the map builder and the inference loop out of sema_generic_call
1b92fdc3 Report the eighth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
2069c578169db5f39dcc7ad61febb0bda817a896
2069c578169db5f39dcc7ad61febb0bda817a896
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `xe` and `xs` of `sema_copies.c`.
