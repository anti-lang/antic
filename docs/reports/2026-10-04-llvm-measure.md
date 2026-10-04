# Step measure of the LLVM back end

## What the step built

`tests/bench` holds five programs, each with a C twin:

- `scalar_loop`, a scalar loop over a slice of a million integers, 1000 passes.
- `objects`, 4096 objects of two classes with two calls through the table each, and
  two direct calls of a counter, 20000 passes.
- `builder`, a `text.Builder` filled with 50000 entries of a word, a number and a
  comma, 200 times.
- `simd_loop`, a dot product of two buffers of 4096 `f32` in vectors of four lanes,
  200000 times.
- `map_work`, a `HashMap<int, int>` with 100000 keys, 500000 lookups and 50000
  removals, 20 rounds.

Each Anti program prints one line and its twin prints the same line. A twin follows
the algorithm of the library code it stands for: the growth of the builder's buffer,
the digits of `append_int` and the table of `SlotTable<E>`.

`tests/run_bench.cmake` compiles every program three times in release mode and three
times in dev mode, runs the release executable and its twin five times each, and
prints the medians. The target `bench` of `tests/CMakeLists.txt` builds the twins
with the pinned clang at `-O2` and runs the script. It stands outside the suite and
outside the default build. A dev compile links against dev objects of `anti.lang`,
`anti.mem`, `anti.text`, `anti.collection` and `anti.collection.map`, built once
before the timing.

The old back end ran from an export of `38e9abd1`, the last commit of the step `vm`,
with `git archive` into a directory from `mktemp -d` in `/tmp`. Its antic was given
`--backend native` and ran the same script. Both directories, on the Mac and on
anti-linux, are removed. No worktree, branch or checkout touched the repository.

## The numbers

Times in milliseconds, wall clock, process start included. A ratio divides the
Anti run by the C run of the same table. Release and dev are compile times.

macos-arm64, the development Mac, 14 cores:

| Program | C | LLVM | Native | LLVM / C | Native / C | Release LLVM | Release native | Dev LLVM | Dev native |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| scalar_loop | 289.9 | 295.2 | 538.3 | 1.02 | 1.86 | 38.3 | 22.5 | 33.4 | 24.6 |
| objects | 104.6 | 137.8 | 199.3 | 1.32 | 1.91 | 48.3 | 25.9 | 41.9 | 28.3 |
| builder | 45.7 | 264.6 | 569.6 | 5.78 | 12.29 | 63.3 | 32.5 | 33.9 | 25.7 |
| simd_loop | 123.7 | 127.8 | 973.2 | 1.03 | 7.84 | 38.4 | 21.8 | 35.4 | 24.4 |
| map_work | 135.3 | 196.9 | 262.7 | 1.46 | 1.94 | 305.1 | 120.7 | 192.3 | 107.6 |

linux-arm64 on anti-linux, 6 cores:

| Program | C | LLVM | Native | LLVM / C | Native / C | Release LLVM | Release native | Dev LLVM | Dev native |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| scalar_loop | 297.6 | 307.3 | 537.8 | 1.03 | 1.80 | 27.0 | 13.9 | 28.7 | 17.7 |
| objects | 137.7 | 108.1 | 198.7 | 0.79 | 1.43 | 39.3 | 20.3 | 41.2 | 23.7 |
| builder | 45.0 | 267.7 | 530.6 | 5.95 | 12.46 | 58.5 | 28.9 | 29.3 | 22.7 |
| simd_loop | 126.4 | 126.3 | 963.8 | 1.00 | 7.62 | 27.4 | 17.5 | 30.6 | 19.3 |
| map_work | 138.3 | 236.2 | 306.4 | 1.71 | 2.23 | 326.4 | 119.6 | 201.8 | 104.1 |

The C column is that of the LLVM run. The native run measured its own C times, at
most 0.7 ms away on the Mac and 2.5 ms on anti-linux, and its ratios use them. The
tables as the script printed them are `build/drive/logs/measure-mac-llvm.md`,
`build/drive/logs/measure-mac-native.md`, `build/drive/logs/measure-bench-linux-llvm.log`
and `build/drive/logs/measure-bench-linux-native.log`.

The Linux LLVM run measured an export of the first version of `710240b2`, before an
amend added only `docs/decisions.md` and the work order to it.

## Tests

No test was added to the suite. `raw_output` required `../binary_stdio.h` in every C
file under `tests/`, so the twins include it. `repo_layout` lists `tests/bench`, and
`fmt_canonical` reads the five programs in the form `anti fmt` writes.

- host: 1539 of 1539 pass, 129 s, `build/drive/logs/measure-suite-host.log`.
- asan: 1538 of 1538 pass, 397 s, `build/drive/logs/measure-suite-asan.log`.
- ubsan: 1538 of 1538 pass, 278 s, `build/drive/logs/measure-suite-ubsan.log`.

All three builds had no warnings. `emit_identity` and `link_identity_<target>`
pass unchanged, since nothing under `src/` changed.

## Decisions

- [provisional] under "Repository layout" in `docs/decisions.md`. The benchmarks stand
  outside the suite and the default build, and the script prints medians and judges
  nothing. A dev compile time covers the module and the link against prebuilt dev
  objects of the standard library. `tests/bench` joins the list of `repo_layout`.

## Corrections of the work order

- The step named linux-x86_64 as the second machine. `docs/vm-setup.md` has no such
  machine, and anti-linux runs linux-arm64. The sentence now names linux-arm64 of
  anti-linux.

## Questions

- Two sections of the work order, "What to expect from the generated code" and
  "Compile time", say more. In them the step also measures every program of
  `tests/programs` under both back ends.
  The step text and this session's prompt name the five programs alone, and this step
  measured those five.
- `tests/bench` is a new directory under `tests/`. The work order names it, which this
  step read as Eddie's decision to add it to the list of `repo_layout`.

## State before this report

```text
$ git log --oneline -3
8f57369d Include binary_stdio.h in the C twins of tests/bench
710240b2 Add the benchmarks of tests/bench and the target bench
b0c507e0 Report step switch of the LLVM back end
$ git status --short
$ git rev-parse HEAD origin/main
8f57369d1489fd8824665323c129beef56f21e80
8f57369d1489fd8824665323c129beef56f21e80
```
