# Step 34, rules 18 and 19, twenty-first session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The twentieth
session, `docs/reports/2026-10-20-solid-step34-owning.md`, left
`antl_read_ir.c` first on its list. This session took it. The logs
stand in `build/drive/logs/` under the prefix `s34u-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `read_signature` and `antl_read_ir` of `antl_read_ir.c` (library-file) | `1fa16bc1` | One body read the head of a signature, merged a second declaration, made a new function and read or compared its parameters, and another read the globals, placed their addresses twice and read the functions. |

The code at `c70d7152` confirmed the finding: `read_signature` stood at
176 lines and `antl_read_ir` at 143. `struct signature` now carries the
head of a signature record, and `read_signature_head`, `find_function`,
`merge_declaration`, `new_function`, `read_params` and `same_signature`
take its parts. `read_global`, `read_relocs`, `place_addresses` and
`read_functions` take those of `antl_read_ir`, and one pass places the
addresses of a global or of a function. Every read keeps its order, so
a library file reads as before and every message stays.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
786 tests of modules, the unit tests, generics, plugins, C libraries,
`emit_identity` and `link_identity` passed first, `s34u-quick.log`.

`c04fc497` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session, from `docs/audit/library-file.md`: `read_declaration` and
`read_extern` of `antl_tree.c`, and the parameters of `antl_read` of
`antl.c`.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34u-build1.log`,
  `s34u-asan-build.log` and `s34u-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 346 s and `-I 2001,5194` 288 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34u-host1.log`, `s34u-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  497 s: `s34u-asan1.log` to `s34u-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 367 s: `s34u-ubsan1.log` to
  `s34u-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34u-docs1.log` and `s34u-docs2.log`.

## Not run on the target

The change moves no code of a platform. The reader of a library file is
the same on every host, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
c04fc497 Record the split of antl_read_ir.c of step 34 in the summary
1fa16bc1 Split read_signature and antl_read_ir of antl_read_ir.c
c70d7152 Report the twentieth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
c04fc497201969bd4ca6a5850f96745eb03e8097
c04fc497201969bd4ca6a5850f96745eb03e8097
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. `read_declaration` and `read_extern` of `antl_tree.c`.
2. The 9 parameters of `antl_read` of `antl.c`, in a later session.
