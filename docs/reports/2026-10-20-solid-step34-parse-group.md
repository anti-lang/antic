# Step 34, rules 18 and 19, thirteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The twelfth
session, `docs/reports/2026-10-20-solid-step34-check-let.md`, left the
next file of the list of `docs/audit/front-end.md` under "Functions past
a threshold": `parse_group` of `pattern.c`. This session took it. The
logs stand in `build/drive/logs/` under the prefix `s34m-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `parse_group` of `pattern.c` (front-end) | `fb6443a1` | `parse_group` held the `(*VERB)` block and the tail of recursions and flags beside the dispatch on the byte after `(?`. |

The finding was confirmed by its count: `parse_group` stood at 130 lines
at `ade13dec`, against 129 at the audit. The split is the one the report
names. `verb_group` reads a group that starts `(*`, a named group such as
`(*atomic:` or a verb, and `flag_group` reads a recursion or a call, which
the reader does not follow, or flags. `parse_group` stands at 93 lines.
No pattern reads differently, and the flags are saved and restored around
the same body as before.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
82 tests whose names hold `regex`, `pattern`, `exponential` or `match`,
with `emit_identity` and `link_identity`, passed first, `s34m-quick.log`.

`3909b796` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

- `docs/audit/front-end.md` under "Functions past a threshold":
  `sema_check_binary` of `sema_operator.c`. Then the parameter lists of
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

- The three builds report no warnings: `s34m-build1.log`,
  `s34m-asan-build.log` and `s34m-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 347 s and `-I 2001,5194` 311 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34m-host1.log`, `s34m-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  503 s: `s34m-asan1.log` to `s34m-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass: `s34m-ubsan1.log` to `s34m-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34m-docs1.log` and `s34m-docs2.log`.

## Not run on the target

Nothing. The move is in the reader of patterns, which runs the same on
every host.

## State at the push

```text
$ git log --oneline -3
3909b796 Record the split of parse_group of step 34 in the summary
fb6443a1 Split the verb block and the flag tail out of parse_group
ade13dec Report the twelfth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
3909b796f9d047136c3256afc00185f1b4e3e985
3909b796f9d047136c3256afc00185f1b4e3e985
```

This report is written after that push, and its commit follows the two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with `sema_check_binary` of `sema_operator.c`.
