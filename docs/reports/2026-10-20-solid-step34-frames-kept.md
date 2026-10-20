# Step 34, rules 18 and 19, nineteenth session

Step 34 of "Fix steps" in `docs/audit/summary.md` takes rules 18 and 19
of `docs/c-guidelines.md` last, one file per session. The eighteenth
session, `docs/reports/2026-10-20-solid-step34-driver-search.md`, left
`frames_kept` of `llvm_emit_facts.c` first on its list. This session
took it. The logs stand in `build/drive/logs/` under the prefix `s34s-`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 18, `llvm_emit_frames_kept` of `llvm_emit_facts.c` (back-end) | `45ce6b89` | The fixed point held the walk over the instructions of one function, at a nest of 5. |

The audit named `frames_kept` of `llvm_emit.c`. The split of rule 19
moved it to `llvm_emit_facts.c` as `llvm_emit_frames_kept`, and its code
at `e58d8bc9` confirmed the finding. The walk is now
`calls_frame_keeper`, at a nest of 3, and the loop of the fixed point
reaches 3. The rule of the `DESIGN` comment stays as it was.

The test of the finding is the suite, as in the earlier sessions. Rule
18 says the threshold is a reason to look, so no test counts lines. The
72 tests of traces, backtraces, `emit_identity` and `link_identity`
passed first, `s34s-quick.log`.

`a7030f49` records the commit in the row of rule 18 of the summary.

## Findings left

Rule 18, the other functions the reports judged clearer split, one file
per session:

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

- The three builds report no warnings: `s34s-build.log`,
  `s34s-asan-build.log` and `s34s-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 352 s and `-I 2001,5194` 280 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `s34s-host1.log`, `s34s-host2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250`, `-I 2251,3700` and
  `-I 3701,5192`. All 1150, 1119, 1457 and 1497 pass, the last part in
  494 s: `s34s-asan1.log` to `s34s-asan4.log`.
- UBSan, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All 1150, 1119
  and 2950 pass, the last part in 371 s: `s34s-ubsan1.log` to
  `s34s-ubsan3.log`.
- The docs-style checker reports nothing on `docs/audit/summary.md` and
  on this report: `s34s-docs1.log` and `s34s-docs2.log`.

## Not run on the target

The split moves no code of a platform. The kept frames are computed the
same for every target, and the Mac ran it.

## State at the push

```text
$ git log --oneline -3
a7030f49 Record the split of llvm_emit_facts.c of step 34 in the summary
45ce6b89 Split the test of one function out of llvm_emit_frames_kept
e58d8bc9 Report the eighteenth session of step 34
$ git status --short
$ git rev-parse HEAD origin/main
a7030f499a7ca5b2d166d5fe80841f2765c5ba09
a7030f499a7ca5b2d166d5fe80841f2765c5ba09
```

This report is written after that push, and its commit follows the
two.

## Left for the next session

1. The functions under "Findings left", one file per session, starting
   with the four parameter lists of `lower_owning.c`.
