# The third set of answers of 2026-11-06

Eddie answered questions 1 and 2 of `docs/reports/2026-11-06-math.md` and
question 2 of `docs/reports/2026-11-06-graphics-answers.md`. The session
recorded the answers, built Q1 and Q3 and wrote the design of Q2, which
waits for Eddie's choice.

## What changed

- `56485654` records the answers as "Answers of 2026-11-06, third set" in
  `docs/decisions.md`. The rule of one NaN enters the specification and the
  overview as not built. The entry of the feature freeze names what the
  answers let in.
- `1ef327bc` builds Q1. `PI`, `TAU`, `E`, `DEG2RAD` and `RAD2DEG` stay
  `f32`, and `PI_F64`, `TAU_F64`, `E_F64`, `DEG2RAD_F64` and `RAD2DEG_F64`
  hold the same values with the digits of an `f64`. `std_math` prints the
  five and shows that `PI_F64` is no widened `PI`.
- `ec164036` builds Q3. `anti test --release` of a project runs the
  release build of `anti build` for the host and reads
  `build/<target>/release/lib/`, which checks every module with
  `--warnings-as-errors`. A dev run reads the dev build as before.
  `anti_test_project_all_release` finds the release files and no dev
  directory, and `all_dev` the reverse.
- `6f2ea51a` writes the design of Q2 as "One NaN on every target" in
  `docs/notes/math.md`, with the probe and the wrappers of the measurement
  in `tests/bench/ablate/`. Nothing of it is built.

## NaN findings

`tests/bench/ablate/nan_bits.anti` ran in release mode on macos-arm64 and
on macos-x86_64 at `v1` under Rosetta.

- The default NaN of an invalid operation, `0/0`, `inf - inf` or
  `sqrt(-1)`, is `7fc00000` on arm64 and `ffc00000` on x86_64. Every
  domain error of `anti.math` gives the same, since musl makes it with an
  operation of the processor.
- The sign survives a conversion to `f64` and to `f16`, and negation,
  `abs` and `copysign` show it. `copysign(1, 0/0)` is `1` on arm64 and
  `-1` on x86_64.
- A NaN operand keeps its own bits on both. With two NaN operands the one
  that wins depends on the order of the instructions, which LLVM may
  commute.
- A program sees the bits through a union or a pointer view, bytes in
  memory, `hash`, the sign operand of `copysign`, C and `f16`. `==`,
  `serialize`, `f"..."` and `text.Builder` do not see them.

The three options, each the median of 21 runs over the baseline, in release
mode on an Apple M4 Pro. x86_64 ran at `v1` under Rosetta, and
`pythagoras_frames` has no `v1` column since `anti.raylib` exists at `v3`
alone. A selects the canonical NaN after every operation that can make
one, B where a program can see the bits, and C sets DN on ARM64 and takes A
on x86_64.

| Program | A arm64 | A x86_64 | B arm64 | B x86_64 | C arm64 |
|---|---:|---:|---:|---:|---:|
| `float_work` | 1.30 | 1.19 | 1.01 | 1.03 | 1.00 |
| `pythagoras_geometry` | 2.21 | 3.48 | 1.10 | 1.22 | 1.00 |
| `pythagoras_frames` | 1.12 | none | 1.04 | none | 1.00 |

C leaves two gaps. LLVM drops an operation it proves to be the identity,
so a NaN written as bits keeps its payload on ARM64 and not on x86_64. A
thread that C starts has no DN.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and
`-I 4301,6900` with `-j14`. Before `1ef327bc` it took 385 s, 271 s and
292 s. Before `ec164036` it took 380 s, 287 s and 275 s. Before `6f2ea51a`
it took 353 s, 273 s and 281 s. Each run passed all 6886 tests, with the
two skips of `mimalloc_environment`.

Each sanitizer suite ran its 6884 tests in the six parts of `asan` at
`6f2ea51a`: `asan` 250 s, 188 s, 273 s, 193 s, 433 s and 447 s, `ubsan`
255 s, 193 s, 258 s, 199 s, 453 s and 467 s. Every test passed, with the
two skips of `mimalloc_environment`. The VMs are down, so no suite ran on
Linux or Windows.

## Questions for Eddie

1. Which form of one NaN should a step build, A, B or C of "One NaN on
   every target" in `docs/notes/math.md`? The session recommends B. It
   gives the one NaN at every place a program can see it, at a tenth of
   the cost of A. A test then holds the list of those places.
2. Still open from `docs/reports/2026-11-06-graphics-answers.md`: should
   a dev run of `anti test` in a project take the objects of `anti build`?
   It compiles those of the imported modules again on every run.
3. Still open from `docs/reports/2026-11-06-math.md`: the exact functions
   of `anti.math` are one instruction in a release build. In dev mode and
   under `--lto none` each stays a call of a function of the runtime. Is
   that enough, or should antic write the intrinsic itself?
