# Step 34, rules 18 and 19, fifteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The fourteenth
session, `docs/reports/2026-10-20-solid-step34-check-binary.md`, left the
parameter lists that `docs/audit/front-end.md` names under "Functions
past a threshold": `sema_check_field_inits`, `sema_check` and
`sema_doc_warnings`. This session took them. The logs stand in
`build/drive/logs/` under the prefix `s34o-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, the 8 parameters of `sema_check_field_inits` of `sema_call.c` (front-end) | `65e69d7d` | Every caller passed the field inits and their count of the literal `e` beside `e` itself. |
| Rule 18, the 9 parameters of `sema_check` and the 7 of `sema_doc_warnings` (front-end) | `0d9d6647` | Both took the inputs of one compilation as loose parameters. |

Each finding was confirmed by its signature at `1d14dbe8`: 8, 9 and 7
parameters, as the report counts. `sema_check_field_inits` now reads the
inits from the struct or slice literal it checks and takes 6.
`struct sema_inputs` of `sema.h` holds the module path, the package, the
libraries, `types`, `arena` and `diags`. `sema_check` takes
the module, the inputs and `program`, and `sema_doc_warnings` the
module, the inputs and `undocumented`. The four callers of `driver.c`
and the unit tests fill the struct. No check moved and no message
changed.

The test of the findings is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts
parameters. The 1147 tests of the units, the literals, the variants, the
doc warnings, `anti check`, the headers, `emit_identity` and
`link_identity` passed first, `s34o-quick.log`.

`3363aa81` records both commits in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

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
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34o-build3.log`,
  `s34o-asan-build.log` and `s34o-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 308 s and `-I 2001,5194` 308 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34o-host1.log`, `s34o-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  503 s: `s34o-asan1.log` to `s34o-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 382 s: `s34o-ubsan1.log` to
  `s34o-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34o-docs1.log` and `s34o-docs2.log`.

## Not run on the target

Nothing. The change is in the checker and its callers, which run the
same on every host.

## State at the push

```text
$ git log --oneline -3
3363aa81 Record the parameter lists of step 34 in the summary
0d9d6647 Pass the inputs of one compilation to the checker as one struct
65e69d7d Read the fields of a literal in sema_check_field_inits from the literal
$ git status --short
$ git rev-parse HEAD origin/main
3363aa81ddc56b8926516fa734c723ccd9354cae
3363aa81ddc56b8926516fa734c723ccd9354cae
```

This report is written after that push, and its commit follows the
three.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `driver_build_c_library` and `bundle` of `driver_library.c`.
