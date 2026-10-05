# Step measure of the optimization facts

The step `measure` of `docs/work-order-llvm-optimization.md`. It records the
table of `tests/bench` and of `mixed_work` on macos-arm64 and on linux-arm64
of anti-linux, as the step `measure` of `docs/work-order-llvm-back-end.md`
recorded it, and compares each program with
`docs/reports/2026-10-04-llvm-measure.md`. Numbers only, no pass or fail.

## Method

Both machines measured `0e483a44`, the commit before this report. Nothing
under `src/` or `tests/` changed.

- The five programs: the target `bench` of `build/host`, which runs
  `tests/run_bench.cmake`. Three release and three dev compiles of each
  program, five runs of the release executable and of its C twin, medians.
- `mixed_work`: `tests/bench/ablate/run.py mixed_work`, the baseline alone,
  15 runs and three release compiles, medians. It has no C twin. Its dev
  compile came from a scratch script, `build/bench-scratch/measure2/dev_mixed.py`,
  which does what `run_bench.cmake` does for the five. It builds dev objects of
  the 19 modules `mixed_work` reaches once, then times three dev compiles of
  the program linked against them. Every build printed `615670597359`.

On anti-linux the tree came from `git archive HEAD` into a directory from
`mktemp -d` in `/tmp`, configured with the four directories of
`docs/vm-setup.md` and removed afterwards. No worktree, branch or checkout
touched the repository. The Linux build printed 67 warnings, all from the
headers of raylib under `~/.local/share/anti-vm/raylib/`, which build with
raylib's own flags. The Mac build printed none.

`--backend native` no longer exists, so the tables have no native column.

## The numbers

Times in milliseconds, wall clock, process start included. Release and dev
are compile times. "To 10-04" divides the Anti run by the LLVM run of the
2026-10-04 report on the same machine.

macos-arm64, the development Mac, 14 cores, `build/drive/logs/measure2-mac-bench.md`:

| Program | C | Anti | Anti / C | Release | Dev | To 10-04 |
|---|---:|---:|---:|---:|---:|---:|
| scalar_loop | 293.4 | 294.3 | 1.00 | 42.0 | 35.1 | 1.00 |
| objects | 108.2 | 143.5 | 1.33 | 50.0 | 44.0 | 1.04 |
| builder | 46.0 | 247.7 | 5.38 | 75.3 | 35.3 | 0.94 |
| simd_loop | 124.3 | 125.7 | 1.01 | 40.6 | 37.5 | 0.98 |
| map_work | 136.5 | 148.0 | 1.08 | 316.9 | 180.4 | 0.75 |
| mixed_work | | 171.6 | | 2359.3 | 1748.3 | |

linux-arm64 on anti-linux, 6 cores, `build/drive/logs/measure2-linux-bench.md`:

| Program | C | Anti | Anti / C | Release | Dev | To 10-04 |
|---|---:|---:|---:|---:|---:|---:|
| scalar_loop | 299.7 | 309.5 | 1.03 | 24.4 | 24.5 | 1.01 |
| objects | 141.8 | 108.2 | 0.76 | 37.2 | 35.9 | 1.00 |
| builder | 43.4 | 249.0 | 5.73 | 60.7 | 23.7 | 0.93 |
| simd_loop | 127.5 | 127.2 | 1.00 | 24.5 | 27.8 | 1.01 |
| map_work | 139.7 | 182.4 | 1.31 | 335.1 | 201.1 | 0.77 |
| mixed_work | | 202.5 | | 2694.2 | 1967.5 | |

The `mixed_work` rows come from `build/drive/logs/measure2-mac-mixed.md`,
`build/drive/logs/measure2-mac-mixed-dev.log` and
`build/drive/logs/measure2-linux-mixed.log`. Its object is 695,344 B on the
Mac and 902,872 B on anti-linux, its executable 1,056,336 B and 1,369,624 B.

## Against 2026-10-04

The same columns of `docs/reports/2026-10-04-llvm-measure.md`, then and now:

| Program | Host | Anti / C | Release | Dev |
|---|---|---|---|---|
| scalar_loop | Mac | 1.02, 1.00 | 38.3, 42.0 | 33.4, 35.1 |
| objects | Mac | 1.32, 1.33 | 48.3, 50.0 | 41.9, 44.0 |
| builder | Mac | 5.78, 5.38 | 63.3, 75.3 | 33.9, 35.3 |
| simd_loop | Mac | 1.03, 1.01 | 38.4, 40.6 | 35.4, 37.5 |
| map_work | Mac | 1.46, 1.08 | 305.1, 316.9 | 192.3, 180.4 |
| scalar_loop | anti-linux | 1.03, 1.03 | 27.0, 24.4 | 28.7, 24.5 |
| objects | anti-linux | 0.79, 0.76 | 39.3, 37.2 | 41.2, 35.9 |
| builder | anti-linux | 5.95, 5.73 | 58.5, 60.7 | 29.3, 23.7 |
| simd_loop | anti-linux | 1.00, 1.00 | 27.4, 24.5 | 30.6, 27.8 |
| map_work | anti-linux | 1.71, 1.31 | 326.4, 335.1 | 201.8, 201.1 |

The 2026-10-04 report holds no row of `mixed_work`, which the step `ablate`
added later. The step `config` measured its baseline after its choices at
173.6 ms on the Mac with the same script.

## Tests and gates

No test was added or changed, and no `[provisional]` entry was added.

The suites ran on the Mac at `0e483a44` with this report staged.

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/measure2-{mac,asan,ubsan}-build.log`.
- Host suite: 1619 of 1619, 225 s, `build/drive/logs/measure2-host-suite.log`.
- ASan suite: 1618 of 1618 in two parts, `-I 1,810` with 810 tests in 191 s
  and `-I 811,1618` with 825 tests in 371 s. The parts share the tests of the
  fixtures, and their union holds the 1618 of `ctest --preset asan -N`,
  `build/drive/logs/measure2-asan-suite-{1,2}.log`.
- UBSan suite: 1618 of 1618, 331 s, `build/drive/logs/measure2-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` of
  all six targets passed in the host suite, unchanged, since the LLVM text
  did not change.
- The docs-style checker reports nothing on this report.

## State before this report

Before the commit of this report, `build/drive/logs/measure2-state-before.txt`:

```console
$ git log --oneline -3
0e483a44 Report step pgo of the optimization facts as blocked
b3bb4712 Report step config of the optimization facts
ced9153a Run default<O3> at an inline threshold of 225 and tune for apple-m1
$ git status --short
A  docs/reports/2026-10-05-llvm-opt-measure.md
$ git rev-parse HEAD origin/main
0e483a44bc584c2ab4a62cebc4610f9e64b7a07f
0e483a44bc584c2ab4a62cebc4610f9e64b7a07f
```
