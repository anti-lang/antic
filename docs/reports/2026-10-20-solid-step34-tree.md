# Step 34, rules 18 and 19, twenty-second session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The twenty-first
session, `docs/reports/2026-10-20-solid-step34-read-ir.md`, left
`antl_tree.c` first on its list. This session took it. The logs stand
in `build/drive/logs/` under the prefix `s34v-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `read_declaration` and `read_extern` of `antl_tree.c` (library-file) | `aa1af212` | One body joined a declaration to its symbol and read both the function half and the type half, and another read the function record of a symbol and searched for the library's own symbol. |

The code at `d8db64b2` confirmed the finding: `read_declaration` stood
at 120 lines and `read_extern` at 110. `declared_symbol`,
`read_fn_declaration` and `read_type_declaration` now take the parts of
the first, which keeps 38 lines. `read_extern_fn` and `library_symbol`
take those of the second, which keeps 61. Every read keeps its order, so
a library file reads as before and every message stays.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
780 tests of modules, the unit tests, generics, plugins, C libraries,
`emit_identity` and `link_identity` passed first, `s34v-quick.log`.

`634a551c` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the parameters of `antl_read` of `antl.c`, from
`docs/audit/library-file.md`, and the other functions the reports
judged clearer split, one file per session.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34v-build1.log`,
  `s34v-asan-build.log` and `s34v-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 345 s and `-I 2001,5194` 276 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34v-host1.log`, `s34v-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  500 s: `s34v-asan1.log` to `s34v-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 362 s: `s34v-ubsan1.log` to
  `s34v-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34v-docs1.log` and `s34v-docs2.log`.

## Not run on the target

The change moves no code of a platform. The reader of a library file is
the same on every host, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
634a551c Record the split of antl_tree.c of step 34 in the summary
aa1af212 Split read_declaration and read_extern of antl_tree.c
d8db64b2 Report the twenty-first session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
634a551ca6df2d39310554b869b668afc46094cc
634a551ca6df2d39310554b869b668afc46094cc
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The 9 parameters of `antl_read` of `antl.c`.
2. The other functions the reports judged clearer split, one file per
   session.
