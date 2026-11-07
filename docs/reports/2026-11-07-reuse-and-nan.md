# Object reuse and one NaN on every target

Eddie answered the three questions of
`docs/reports/2026-11-06-math-answers.md`. The session recorded the
answers, built object reuse by Q2 and one NaN on every target by Q1, form
B. Q3 keeps the code as it is.

## What changed

- `35ef6061` records the answers as "Answers of 2026-11-06, fourth set" in
  `docs/decisions.md`, with the line in the entry of the feature freeze.
  The specification and the overview name form B. Q3 had no provisional
  entry, and the answer stands as decided.
- `e0bf685e` builds the reuse. `src/anti/cache.c` holds the keys for
  `anti build` and `anti test`. A build of a program keys it by its source,
  the options, the digests of the library files it reaches, the keys of
  its objects, the runtime and the link, and links nothing when the key
  holds. A dev run of `anti test` in a project links the object of
  `anti build` for every module it does not test, since an import has no
  `tests` block to keep. Its own library file, object and runner of a
  module under test stand in `obj/` of the build of its mode, under keys
  with the mark `tests`. The 20 tests `anti_reuse_<command>_<mode>_<case>`
  show that a second run writes no file and that a change of a source, an
  import, an option or the compiler version writes again what it reaches.
- `011089c5` builds one NaN. `src/antic/llvm_emit_nan.c` writes
  `fcmp uno` and a `select` of the canonical NaN at every place of form B:
  memory a view or C reaches, a float argument of C, the result of a
  function C calls, simd lanes and `as f16`. A slot that no view reaches
  needs none. A union, two types that overlap at one place and an
  escaping address make a slot one a view reaches, which closes the gap of
  the prototype. `copysign` of the runtime gives a NaN the canonical sign.
  A NaN that folding gives is canonical in the text and in constant bytes.

## Tests of one NaN

Seven programs `matrix/values_nan_*` hold a union, a pointer view with its
bytes, the hash, `copysign`, `f16`, simd lanes and `reflect.set`. Each runs
eight contexts in dev, release and `--memory-checks`, and again at `v1`
under Rosetta. `program_abi_nan` holds the boundary to C: an argument, a
variadic argument, a struct by value, an exported result and a callback.
All 32 tests failed before the change on arm64 and on x86_64. The inputs
are 0/0, its negation and the negative NaN `0xffd00000`, so each target
has a NaN of its own sign or payload.

The pinned outputs that moved: the LLVM text of `tests/dump/` and of
`emit_identity`, the unwind data of both Windows targets, the digest of
`return42` on macos-arm64, the unit test of the emitter and nine excerpts
of the developer's guide. `programs/half` now prints `7e00` for the NaN
`0x7fd00000` that came as bits.

## Reuse measured

`anti test` in the copy `build/pythagoras-tree`, median of 11 runs, each a
first run after `rm -rf build` and a second run:

| Run | First before | Second before | First after | Second after |
|---|---:|---:|---:|---:|
| dev | 0.286 s | 0.258 s | 0.341 s | 0.051 s |
| `--release` | 0.373 s | 0.354 s | 0.392 s | 0.047 s |

The first dev run is slower by the `-g` of the objects of `anti build`,
which the run now shares. In a scratch copy of the tree, `anti build
--release` took 2.44 s, 0.08 s for a second build, and 2.47 s after a change
of `tree.geometry`. A dev build took 0.72 s, 0.10 s and 0.30 s.

## Cost of one NaN

`tests/bench/ablate/opt_nan_off.py` takes the selects out of the text. Each
figure is the built form over the same program without it, the median of
21 runs. x86_64 ran at `v1` under Rosetta.

| Program | arm64 | x86_64 | Prototype arm64 | Prototype x86_64 |
|---|---:|---:|---:|---:|
| `float_work` | 1.01 | 1.00 | 1.01 | 1.03 |
| `pythagoras_geometry` | 1.10 | 1.22 | 1.10 | 1.22 |
| `pythagoras_frames` | 1.04 | none | 1.04 | none |

`std_graphics_allocations_memory_checks` passes, so a frame still
allocates nothing.

## Suites

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and `-I 4301,7000`
with `-j14`. Before `e0bf685e` it took 387 s, 277 s and 316 s, and before
`011089c5` 391 s, 285 s and 333 s. Each run passed all 6938 tests, with the
two skips of `mimalloc_environment`. Each sanitizer suite ran its 6936
tests in six parts at `011089c5`, `ctest -I 1,1150` to `-I 6001,7000`:
`asan` 252 s, 189 s, 259 s, 196 s, 428 s and 442 s, `ubsan` 256 s, 191 s,
261 s, 209 s, 472 s and 454 s. Every test passed, with the same two skips.
The VMs stayed untouched, so no suite ran on Linux or Windows.

## Provisional lines

- The test run caches its files in `obj/` as `<module>.tests.antl`,
  `<module>.tests.o` and `<module>.runner`. It copies the library file
  into the emptied `lib/` of the run.
- A run outside a project or with `-I` caches nothing.
- The key of a program names the runtime by the digest of `libanti_rt.a`
  and the native libraries by the compiler version. A library for C links
  on every build.
- Five rules of form B: the slots no view reaches, the three floats that
  take no select and the functions C calls. The calls of `anti_rt_` and
  through an address, and `as f16` of a NaN that came as bits, follow.

## Questions for Eddie

1. A release build after a change of one module takes 2.47 s, the time of
   a first build. Finer reuse would keep the bitcode of each unchanged
   module and run the LTO link alone, which needs a restructured release
   pipeline. Two whole-program calls make up most of the time, the program
   and the debug link of the symbols archive. Should a step measure a
   release that keeps bitcode per module, or one that writes the program
   and its symbols in one call?
2. The key of a program does not hold the digests of the native libraries,
   such as `libraylib.a`. A tree whose pin of raylib changes keeps the
   version of antic and would reuse a program linked against the old
   library. Should the key name every native library the link takes?
3. A call through an address that is no table call takes the canonical NaN
   for its float arguments. The address may be a function of C. A
   closure and a function value of Anti pay for it as well. Should a call
   through a function value that holds a context, which C never makes,
   keep its floats as they are?
