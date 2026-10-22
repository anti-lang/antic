# Step fuzz-campaign: two hours of the generator

The step ran the generator of `tools/fuzz/` at commit `cf4fc31a` for 116
minutes of tool calls, each under ten minutes, with `-DJOBS=14`. Each
seed builds its program in dev mode, in release mode and with
`--memory-checks`, runs the three and compares them. The logs are
`build/drive/logs/fc-run-*.log`. The step changed no code.

## Seeds run

| Seeds | Size | Count | Log | Time |
|---|---|---|---|---|
| 10001 to 10100 | 6 | 100 | `fc-run-10001.log` | 73 s |
| 10101 to 10800 | 6 | 700 | `fc-run-10101.log` | 501 s |
| 10801 to 11520 | 6 | 720 | `fc-run-10801.log` | 513 s |
| 11521 to 12240 | 6 | 720 | `fc-run-11521.log` | 515 s |
| 12241 to 12960 | 6 | 720 | `fc-run-12241.log` | 517 s |
| 12961 to 13680 | 6 | 720 | `fc-run-12961.log` | 531 s |
| 13681 to 14400 | 6 | 720 | `fc-run-13681.log` | 519 s |
| 14401 to 15120 | 6 | 720 | `fc-run-14401.log` | 519 s |
| 15121 to 15840 | 6 | 720 | `fc-run-15121.log` | 504 s |
| 20001 to 20300 | 16 | 300 | `fc-run-20001-s16.log` | 269 s |
| 20301 to 20880 | 16 | 580 | `fc-run-20301-s16.log` | 493 s |
| 20881 to 21460 | 16 | 580 | `fc-run-20881-s16.log` | 496 s |
| 21461 to 22040 | 16 | 580 | `fc-run-21461-s16.log` | 504 s |
| 22041 to 22620 | 16 | 580 | `fc-run-22041-s16.log` | 496 s |
| 22621 to 23200 | 16 | 580 | `fc-run-22621-s16.log` | 495 s |
| 30001 to 30150 | 40 | 150 | `fc-run-30001-s40.log` | 37 s |

5840 seeds at the default size of 6 functions and 3200 at size 16 make
9040 valid programs.

## Finds, causes and rate

- The 9040 valid programs gave no find. No cause, so the rate is 0
  causes per thousand programs.
- The 150 seeds at size 40 were all refused at `fn f16`. `f16` is a
  keyword of `src/antic/lexer.c`, so the refusal is correct and no
  defect of antic. The generator names its functions `f0` to `fN` and
  holds 16 at most. `docs/notes/fuzz.md` now states the limit. Their
  finds are kept in `build/fuzz/campaign/finds-size40/`. Per the task
  the step fixed nothing, the generator included.

The step therefore added no row to `docs/audit/tests-findings.md`, no
program to `tests/matrix/` and no line to `tests/known-failures.txt`.
The generator still steers around the open findings T180, T181 and
T182, so the campaign could not show them again.

## Findings

Closed none, left none, recorded none. `tests/known-failures.txt` held
142 lines before and after, 123 entries, of which the Mac expects 121 to
fail.

## `[provisional]` entries

None.

## Gates

- No warning of our code: `fc-gate-host-build.log`,
  `fc-gate-asan-build.log` and `fc-gate-ubsan-build.log`.
- Host, 5800 tests, `-I 1,2000`, `2001,3900` and `3901,5800`: 2000, 1927
  and 1906 passed in 374 s, 173 s and 282 s, `fc-gate-host-1.log` to
  `-3.log`. The later parts run the fixtures of the first again. The 121
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip.
- ASan, 5798 tests, `-I 1,1150`, `1151,2250`, `2251,4000`, `4001,4900`
  and `4901,5798`: 1151, 1106, 1777, 906 and 898 passed in 242 s, 187 s,
  205 s, 446 s and 361 s, `fc-gate-asan-1.log` to `-5.log`.
- UBSan, 5798 tests, the same parts: 1151, 1106, 1777, 906 and 898
  passed in 243 s, 166 s, 163 s, 185 s and 304 s, `fc-gate-ubsan-1.log`
  to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on `docs/notes/fuzz.md` and this
  report.

## Not run on the target

Nothing. The step changed no code.

## State at the push

```text
$ git log --oneline -3
6d5cf33d Record the size limit of the generator and its first campaign
cf4fc31a Report the step fuzz
8a5a2595 Hold the fuzzer to the rules of runners and scripts
$ git status --short
?? docs/reports/2026-10-22-solid-fuzz-campaign.md
$ git rev-parse HEAD origin/main
6d5cf33df15762ba60f4e9537a65cacfcfcb7598
6d5cf33df15762ba60f4e9537a65cacfcfcb7598
```

The one untracked file is this report, whose commit follows the push.
