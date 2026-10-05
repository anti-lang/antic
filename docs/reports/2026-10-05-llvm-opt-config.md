# Step config of the optimization facts

The step `config` of `docs/work-order-llvm-optimization.md`, choices D3, D4
and D8 under condition C1.

## What the step built

- Release mode runs `opt -passes=default<O3> -inline-threshold=225`, where it
  ran `default<O2>`. `llvm_opt_options` of `src/antic/llvm_run.c` holds the two
  options, under a DESIGN comment with the measurement.
- `llvm_tune_cpu` of `src/antic/llvm_target.c` gives `apple-m1` for
  macos-arm64 and nothing for the other five targets. The text writes
  `"tune-cpu"="apple-m1"` after `"target-features"` on macos-arm64 alone. Its
  DESIGN comment holds the measurement.
- Six wrappers in `tests/bench/ablate/`: `opt-inline-225`, `opt-inline-500`
  and `opt-inline-1000` beside the 2000 of `opt-inline`, `opt_tune_m1.sh`,
  `opt_o3.sh` and `opt_o2_loops.sh`.
- `--lto` and llc are unchanged, as the third provisional entry below says.

## Tests

The unit tests failed to compile before the code existed,
`build/drive/logs/config-red.log`. `tune_cpu` of `unit_llvm_emit` checks that
the text of each target writes the attribute that `llvm_tune_cpu` names, and
none where it names none. `unit_llvm_target` checks that the five targets of
D4 write none, and that `llvm_opt_options` holds a pipeline and a threshold.
No test pins a measured value.

`opt -verify-each` accepted `default<O2>`, `default<O3>` and
`default<O2>,function(loop-mssa(licm,simple-loop-unswitch),irce)` on the text
of every program of `tests/bench` and on `mixed_work`,
`build/drive/logs/config-verify-each.log`. The final options on the final text
passed as well, `build/drive/logs/config-verify-each-final.log`. licm needs
MemorySSA, so the third pipeline runs it under `loop-mssa`.

## Measurement

`tests/bench/ablate/run.py`, 15 runs each, on macos-arm64. Each row is the
median against the baseline in the first row. Every build printed the line of
its C twin, and `mixed_work` its baseline.

Run 1, each option against `default<O2>`, `build/drive/logs/config-bench-<program>.md`:

| Build | `scalar_loop` | `objects` | `builder` | `simd_loop` | `map_work` | `mixed_work` |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 299.9 ms | 141.1 ms | 264.9 ms | 130.3 ms | 182.5 ms | 193.3 ms |
| `opt-inline-225` | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.00 |
| `opt-inline-500` | 1.00 | 1.00 | 0.97 | 1.00 | 0.86 | 0.93 |
| `opt-inline-1000` | 1.00 | 1.00 | 0.94 | 1.00 | 0.87 | 0.92 |
| `opt-inline`, 2000 | 1.00 | 1.01 | 0.95 | 1.00 | 0.85 | 0.91 |
| `opt_tune_m1.sh` | 0.98 | 1.00 | 1.00 | 0.97 | 0.97 | 0.98 |
| `opt_o3.sh` | 1.00 | 1.01 | 0.94 | 1.00 | 0.82 | 0.89 |
| `opt_o2_loops.sh` | 1.00 | 1.01 | 1.00 | 1.00 | 0.96 | 0.99 |

| Build | `map_work` object, compile | `mixed_work` object, compile |
|---|---|---|
| baseline | 108,592 B, 311.5 ms | 668,352 B, 2021.9 ms |
| `opt-inline-225` | 108,592 B, 315.2 ms | 668,392 B, 2020.5 ms |
| `opt-inline-500` | 119,512 B, 363.0 ms | 751,288 B, 2732.6 ms |
| `opt-inline-1000` | 122,272 B, 390.9 ms | 791,144 B, 3146.4 ms |
| `opt-inline`, 2000 | 133,040 B, 435.7 ms | 933,088 B, 3940.8 ms |
| `opt_tune_m1.sh` | 108,568 B, 315.0 ms | 667,960 B, 2029.3 ms |
| `opt_o3.sh` | 111,192 B, 331.1 ms | 713,528 B, 2532.4 ms |
| `opt_o2_loops.sh` | 108,528 B, 316.0 ms | 668,160 B, 2036.9 ms |

D8: `default<O3>` is the fastest pipeline, beyond 2 percent on `builder`,
`map_work` and `mixed_work`. The loop passes after `default<O2>` lose to it.

Run 2, the thresholds and the tuning under `default<O3>`, against
`default<O2>`, `build/drive/logs/config-bench2-<program>.md`:

| Build | `scalar_loop` | `objects` | `builder` | `simd_loop` | `map_work` | `mixed_work` |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 300.0 ms | 141.1 ms | 265.2 ms | 130.3 ms | 182.7 ms | 193.9 ms |
| `opt_o3.sh`, 250 | 1.00 | 1.00 | 0.94 | 1.00 | 0.81 | 0.89 |
| and 225 | 1.00 | 1.00 | 0.94 | 1.00 | 0.82 | 0.90 |
| and 500 | 1.00 | 1.00 | 0.94 | 1.00 | 0.82 | 0.92 |
| and 1000 | 1.00 | 1.01 | 0.94 | 1.00 | 0.83 | 0.91 |
| and 2000 | 1.00 | 1.00 | 0.94 | 1.00 | 0.83 | 0.90 |
| and `opt_tune_m1.sh` | 0.99 | 1.00 | 0.94 | 0.97 | 0.82 | 0.90 |

| Build | `map_work` object, compile | `mixed_work` object, compile |
|---|---|---|
| `opt_o3.sh`, 250 | 111,192 B, 328.2 ms | 713,528 B, 2546.0 ms |
| and 225 | 110,264 B, 325.6 ms | 695,312 B, 2387.9 ms |
| and 500 | 119,488 B, 366.8 ms | 762,016 B, 2850.2 ms |
| and 1000 | 122,512 B, 395.7 ms | 793,808 B, 3170.7 ms |
| and 2000 | 132,584 B, 435.6 ms | 933,600 B, 3920.1 ms |
| and `opt_tune_m1.sh` | 111,168 B, 331.3 ms | 713,224 B, 2530.9 ms |

D3: the four thresholds lie within 2 percent of each other on every program.
The widest gap is 1.9 percent, 500 against 225 on `mixed_work`. Under C1, 225
wins with the shortest release compile and the smallest object.

D4: `apple-m1` runs `simd_loop` 3.2 percent faster than `generic` and
`scalar_loop` 1.5 percent faster, and no object grows. It wins on macos-arm64.

Before and after, the three choices together against the old default,
`build/drive/logs/config-before-after-<program>.md`:

| Build | `scalar_loop` | `objects` | `builder` | `simd_loop` | `map_work` | `mixed_work` |
|---|---:|---:|---:|---:|---:|---:|
| before | 297.3 ms | 141.2 ms | 264.9 ms | 130.1 ms | 182.6 ms | 192.8 ms |
| after | 0.99 | 1.00 | 0.94 | 0.97 | 0.82 | 0.90 |

| Build | `map_work` object, compile | `mixed_work` object, compile |
|---|---|---|
| before | 108,592 B, 305.4 ms | 668,352 B, 2031.8 ms |
| after | 110,240 B, 328.9 ms | 695,344 B, 2403.5 ms |

The built antic gives the objects of the after row byte for byte and runs
`map_work` in 149.8 ms and `mixed_work` in 173.6 ms,
`build/drive/logs/config-after-<program>.md`. Against the C twin, `map_work`
went from 1.32 to 1.09. A compile through the wrappers includes the start of
the shell, a few milliseconds on the small programs.

## Pins

The text of macos-arm64 changed, so the step re-pinned:

- the 21 goldens `tests/dump/*.macos-arm64.ll`, one line each
- 221 lines of `tests/emit-identity/programs.sha256`, all of macos-arm64. The
  file's SHA-256 went from
  `5006244e63cdf2dd9b3f68f7b5e558bdeb6aaa290bb975b319b46e0b6e672d55` to
  `aa254eaca12d58dd5b603f9223fc7ff8447a1ddd87bca0e2930c1ab3c0e7f7b0`
- `link_identity_macos-arm64`, from
  `7b77362af481521442c40a0a54c87b3173995d232e3034fd7dd917f67378d8ec` to
  `fcdfa5bc95da399375940074c9bc8e3f3da28f3520ca16b416f8168b55e54c88`

## Decisions

The entry of the step stands under "Compiler behaviour" in
`docs/decisions.md`, and the entry on the integration route now points to it.
The provisional entries it added:

- The thresholds were measured under the winning pipeline, `default<O3>`, not
  under `default<O2>`.
- The five targets other than macos-arm64 write no `"tune-cpu"`.
- `--lto` keeps `lto-pre-link<O2>` or `thinlto-pre-link<O2>` and the LTO of
  lld at `-O2`, and llc stays at `-O2`. Neither was measured at `O3`.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/config-{host,asan,ubsan}-build.log`.
- Host suite: 1619 of 1619, 232 s, `build/drive/logs/config-host-suite.log`.
- ASan suite: 1618 of 1618 in two parts, `-I 1,810` with 810 tests in 195 s
  and `-I 811,1618` with 825 tests in 351 s. The parts share 17 tests of the
  fixtures, and their union is the 1618 of `ctest --preset asan -N`,
  `build/drive/logs/config-asan-suite-{1,2}.log`.
- UBSan suite: 1618 of 1618, 314 s, `build/drive/logs/config-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` of
  all six targets passed in the host suite.
- The docs-style checker reports nothing on `docs/decisions.md`,
  `docs/notes/llvm.md` and this report.

State after the push of the step, before this report:

```console
$ git log --oneline -3
ced9153a Run default<O3> at an inline threshold of 225 and tune for apple-m1
0280b03e Add the wrappers that measure the build configuration
e4fabba5 Report step runtime-lto of the optimization facts
$ git status --short
$ git rev-parse HEAD origin/main
ced9153a1871834e961ab44ffd8bbc3f4dce4c01
ced9153a1871834e961ab44ffd8bbc3f4dce4c01
```
