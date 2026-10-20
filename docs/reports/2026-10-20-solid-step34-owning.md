# Step 34, rules 18 and 19, twentieth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The nineteenth
session, `docs/reports/2026-10-20-solid-step34-frames-kept.md`, left the
four parameter lists of `lower_owning.c` first on its list. This session
took them. The logs stand in `build/drive/logs/` under the prefix
`s34t-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, the parameters of `work_on`, `each_element`, `each_array_element` and `each_case` of `lower_owning.c` (middle) | `617d6dd0` | The kind of work, the allocator and `made_only` travelled as three parameters through three levels unchanged, for seven or eight parameters each. |

The code at `6bf6ae14` confirmed the finding: `each_element` took 8
parameters and the other three 7. `struct owned_work` now holds the
kind of work, the allocator and `made_only`, and `plain_work` makes the
work of a copy, a clear and an init. The four take 5, 6, 5 and 5
parameters. The places `at` and `into` stay parameters, since each
level computes them anew. The order of the calls that emit code is the
same, so the IR is the same.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts
parameters. The 705 tests of variants, ownership, leaks, copies, slices,
`emit_identity` and `link_identity` passed first, `s34t-quick.log`.

`6dbee179` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session, from `docs/audit/library-file.md`: `read_signature` and
`antl_read_ir` of `antl_read_ir.c`, `read_declaration` and
`read_extern` of `antl_tree.c`, and the parameters of `antl_read`.

## New defects and entries

No new defect and no `[provisional]` entry. `tests/known-failures.txt`
holds 33 lines before and after. No new feature, option or command.
`emit_identity` and `link_identity_macos-arm64`, the one target of the
Mac, pass unchanged.

## Gates

- The three builds report no warnings: `s34t-build2.log`,
  `s34t-asan-build.log` and `s34t-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 349 s and `-I 2001,5194` 299 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34t-host1.log`, `s34t-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  504 s: `s34t-asan1.log` to `s34t-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 386 s: `s34t-ubsan1.log` to
  `s34t-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34t-docs1.log` and `s34t-docs2.log`.

## Not run on the target

The change moves no code of a platform. Lowering writes the same IR for
every target, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
6dbee179 Record the parameters of lower_owning.c of step 34 in the summary
617d6dd0 Pass the work of lower_owning.c as one struct
6bf6ae14 Report the nineteenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
6dbee1799565f371378d1e27864ee44f99f2c5ce
6dbee1799565f371378d1e27864ee44f99f2c5ce
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The functions of `docs/audit/library-file.md` under "Findings left",
   one file per session, starting with `antl_read_ir.c`.
