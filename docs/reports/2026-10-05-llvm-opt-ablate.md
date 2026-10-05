# Step ablate of the optimization facts

The step `ablate` of `docs/work-order-llvm-optimization.md`: the measurement of
"How a fact is measured" and the program of condition C1.

## What the step built

- `tests/bench/ablate/` holds the four wrappers `opt-inline`, `opt_hash.py`,
  `opt_m1.sh` and `opt_noreturn.py`. Each takes the pinned opt as `bin/opt` of
  the runtime directory in `ANTI_ABLATE_RUNTIME`, runs the wrappers that
  `ANTI_ABLATE_NEXT` lists before it, and passes the edited text on through
  standard input, so no file is left behind. The combinations of run B and run
  D, which the scratch wrappers made through `EXTRA_OPT` and `HASH=1`, are now
  two wrappers in a row, such as `opt-inline opt_hash.py`.
- `tests/bench/ablate/run.py` builds one program of `tests/bench` or of its own
  directory once per build, a build being wrappers and antic options. It checks
  that every build prints the line of the C twin. It runs the builds and the
  twins alternately, 15 times each. It prints the medians, the object and
  executable sizes and the median of three release compiles. `--table A` to
  `--table D` give the builds of the four runs of the work order.
- `tests/bench/map_work_rtseed.c`, built as `bench_map_work_rtseed`, is the twin
  of run C that reads its seed at run time.
- `tests/bench/ablate/mixed_work.anti` imports eleven modules of `src/std/anti/`
  and works in each. It uses lists, hash maps and sets, sorted maps and sets, a
  deque and a ring. It uses a priority queue over a grid, a pool, a tree, a bit
  set, JSON, TOML and a pattern. Its object is 791,320 B, 6.84 times the 115,624 B of
  `map_work`, where C1 asks for five.

`repo_layout` lists only the directories directly under `tests/`, so
`tests/bench/ablate` needed no entry.

## Tests

- `program_mixed_work` failed without its expected file, then passed with
  `615670597359`. The program has no twin, so `run.py` holds each build to the
  line of its baseline, and this test holds the baseline to the right line.
- `raw_output` refused the twin of run C at `tests/bench/ablate/`, because every
  C file of `tests/` includes `../binary_stdio.h`. The file moved beside
  `map_work.c`.
- The step changes neither antic nor the LLVM text, so `emit_identity`,
  `link_identity_<target>` and `llvm_verify` pass unchanged and nothing is
  re-pinned.

## Reproduction of run A to run D

`map_work`, 15 runs each, every build printing `150009000000`. Each cell is
this run against the work order, with the difference.

| Run | Build | Median | Work order | Difference |
|---|---|---:|---:|---:|
| A | baseline | 198.3 ms | 197.6 ms | +0.4 % |
| A | `--no-hooks` | 198.3 ms | 198.2 ms | +0.1 % |
| A | `--no-checks` | 197.5 ms | 198.1 ms | -0.3 % |
| A | `opt-inline` | 166.1 ms | 166.2 ms | -0.1 % |
| A | `opt-inline` and `--no-hooks` | 168.0 ms | 165.5 ms | +1.5 % |
| A | C twin | 136.6 ms | 135.1 ms | +1.1 % |
| B | baseline | 196.8 ms | 196.1 ms | +0.4 % |
| B | `opt_hash.py` | 196.8 ms | 196.2 ms | +0.3 % |
| B | `opt-inline` | 168.4 ms | 165.3 ms | +1.9 % |
| B | `opt-inline` and `opt_hash.py` | 161.2 ms | 160.4 ms | +0.5 % |
| B | C twin | 135.7 ms | 133.1 ms | +2.0 % |
| C | baseline | 192.0 ms | 193.9 ms | -1.0 % |
| C | `opt_m1.sh` | 193.1 ms | 196.4 ms | -1.7 % |
| C | `opt-inline` | 160.3 ms | 162.5 ms | -1.4 % |
| C | `opt-inline` and `opt_m1.sh` | 155.1 ms | 157.7 ms | -1.6 % |
| C | C twin | 128.1 ms | 130.3 ms | -1.7 % |
| C | twin with the seed read at run time | 128.8 ms | 132.1 ms | -2.5 % |
| D | baseline | 191.8 ms | 193.8 ms | -1.0 % |
| D | `opt_noreturn.py` | 175.6 ms | 176.3 ms | -0.4 % |
| D | `opt_noreturn.py` and `opt_hash.py` | 173.9 ms | 174.5 ms | -0.3 % |
| D | `opt-inline` | 161.2 ms | 161.7 ms | -0.3 % |
| D | `opt-inline` and `opt_hash.py` | 156.4 ms | 156.8 ms | -0.3 % |
| D | C twin | 128.7 ms | 129.2 ms | -0.4 % |

Every median lies within 3 percent. Run A took three runs and run B two. The
first run A lay 4.9 to 7.8 percent below the work order, the second 2.1 to 5.4
percent above it, and the first run B 3.0 to 5.2 percent below. The C twin, which
no wrapper touches, moved by the same amount, and the column "To baseline"
agreed within 0.02 in every run. The load average of the Mac stood between 3
and 9 while VMware ran. Logs: `build/drive/logs/ablate-run-A.log`,
`ablate-run-A-2.log`, `ablate-run-A-3.log`, `ablate-run-B.log`,
`ablate-run-B-2.log`, `ablate-run-C.log` and `ablate-run-D.log`.

The sizes equal the work order: 115,624 B and 229,968 B for the baseline,
149,856 B and 262,416 B for `opt-inline`, and an object of 109,904 B for
`opt_noreturn.py`, which ends 21 fatal sites. The release compile took 288.6
to 295.1 ms for the baseline, 423.7 to 434.3 ms for `opt-inline` and 318.8 ms
for `opt_noreturn.py`.

## Measurement of tests/bench and mixed_work

The step changes no code that antic emits, so the table before the step is the
table after it. Baseline builds, 15 runs each:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 294.3 ms | 289.1 ms | 1.02 | 1,896 B | 38.1 ms |
| `objects` | 136.9 ms | 104.2 ms | 1.31 | 13,464 B | 48.4 ms |
| `builder` | 263.6 ms | 44.2 ms | 5.97 | 15,328 B | 63.1 ms |
| `simd_loop` | 120.9 ms | 117.1 ms | 1.03 | 2,016 B | 38.7 ms |
| `map_work` | 195.3 ms | 131.4 ms | 1.49 | 115,624 B | 304.9 ms |
| `mixed_work` | 206.1 ms | none | | 791,320 B | 2164.8 ms |

`mixed_work` under the wrappers of run D, with `opt_m1.sh`, `--no-hooks` and
`--no-checks`: `opt_noreturn.py` ran in 0.90 of the baseline with an object of
694,072 B, and `opt-inline` in 0.90 with an object of 1,250,088 B and a release
compile of 4479.6 ms against 2187.3 ms. Logs:
`build/drive/logs/ablate-bench-<program>.log` and
`build/drive/logs/ablate-mixed-D.log`.

## Provisional entry

Under "Repository layout" in `docs/decisions.md`, after the entry of
`tests/bench`: the measurement of `tests/bench/ablate/`, how a wrapper finds opt
and combines with another, the untimed first run of each executable, the twin of
run C and `program_mixed_work`.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`.
- Host suite: 1540 of 1540 passed, `build/drive/logs/ablate-host-suite.log`. The
  first run failed `raw_output`, fixed as above.
- ASan suite: 1539 of 1539 passed, `build/drive/logs/ablate-asan-suite.log`.
- UBSan suite: 1539 of 1539 passed, `build/drive/logs/ablate-ubsan-suite.log`.
- The docs-style checker reports nothing on `docs/decisions.md`, `CLAUDE.md` and
  this report.

State after the push of the two commits of the step, before this report:

```console
$ git log --oneline -3
a4a59f37 Add the measurement of tests/bench/ablate
a39773f5 Add tests/bench/ablate/mixed_work, the program of condition C1
21014149 Add the work order for optimization facts in the LLVM text
$ git status --short
$ git rev-parse HEAD origin/main
a4a59f3761a1332e30cb634700ee0dbb7d6d6229
a4a59f3761a1332e30cb634700ee0dbb7d6d6229
```
