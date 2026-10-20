# Step 34, rules 18 and 19, twenty-third session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The twenty-second
session, `docs/reports/2026-10-20-solid-step34-tree.md`, left the
parameters of `antl_read` of `antl.c` first on its list. This session
took them. The logs stand in `build/drive/logs/` under the prefix
`s34w-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, the 9 parameters of `antl_read` of `antl.c` (library-file) | `6ad48f73` | The libraries with their count, the types, the memory pool and the program that every read of one compilation shares were five separate parameters beside the bytes and the message. |

The code at `ae1c1561` confirmed the finding: `antl_read` took 9
parameters. `struct antl_context` of `antl.h` now holds the libraries
with their count, the types, the memory pool and the program, and `antl_read`
takes the bytes with their size, the context and the message with its
size, 5 parameters. The driver fills the context once and sets the count
before each read. The unit tests read a file against no library through
`compilation_read` of `tests/unit/pipeline.c`, and the two reads against
libraries fill a context of their own. Every read keeps its order, so a
library file reads as before and every message stays.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts parameters.
The 173 unit, module, `emit_identity` and `link_identity` tests passed
first, `s34w-quick.log`.

`1631f27a` records the commit in the row of rule 18 of the summary. With
it, every function of `docs/audit/library-file.md` that the report
judged clearer split is fixed.

## Findings left

Rule 18, the other functions the reports of tool-pass, front-end,
middle, back-end and rt judged clearer split, one file per session.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34w-build1.log`,
  `s34w-asan-build.log` and `s34w-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 357 s and `-I 2001,5194` 282 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34w-host1.log`, `s34w-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  498 s: `s34w-asan1.log` to `s34w-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 374 s: `s34w-ubsan1.log` to
  `s34w-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34w-docs1.log` and `s34w-docs2.log`.

## Not run on the target

The change moves no code of a platform. The reader of a library file is
the same on every host, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
1631f27a Record the parameters of antl_read of step 34 in the summary
6ad48f73 Pass what antl_read reads against as one struct
ae1c1561 Report the twenty-second session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
1631f27a55a5543276a06c1585e353b8fe180dcd
1631f27a55a5543276a06c1585e353b8fe180dcd
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The other functions the reports judged clearer split, one file per
   session, from the reports of tool-pass, front-end, middle, back-end
   and rt.
