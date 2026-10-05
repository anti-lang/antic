# Step effects of the optimization facts

The step `effects` of `docs/work-order-llvm-optimization.md`: "Memory effects of
runtime functions".

## What the step built

- A column of memory effects in `RT_FUNCTIONS` of `src/antic/rt_abi.h`, written
  for every row from its C body in `src/rt/`. A row names one of six sets,
  `RT_ANY`, `RT_PURE`, `RT_READS`, `RT_WRITES`, `RT_ALLOCATES` and
  `RT_REALLOCATES`. Each set is a class of `enum ir_effects` and the guarantees
  `IR_WILLRETURN`, `IR_NOSYNC` and `IR_NOFREE`.
- `effects` and `guarantees` in `struct ir_function`. `lower_rt_declare` sets
  them from the row, `ir_print` shows them after the result, and the library
  file carries one byte of them after the flags of every function. Two library
  files that declare one C function leave it the weaker reading. The format is
  version 77.
- The attributes on the declarations of `llvm_emit.c`, through `declaration`
  and through `runtime_function` for the two f16 conversions.
- `anti.lang.seeded` is `(hash ^ with.hash()).hash()` in
  `src/std/anti/lang.anti`, and `anti_rt_hash_seeded` is gone from `src/rt/`.

The rows with a class other than any:

| Row | Class | Guarantees |
|---|---|---|
| `anti_rt_f16_to_f32`, `anti_rt_f32_to_f16` | `memory(none)` | all three |
| `anti_rt_same_bytes`, `anti_rt_compare_bytes`, `anti_rt_hash_bytes` | `memory(argmem: read)` | all three |
| `anti_rt_snapshot_text` | writes and allocates | all three |
| `anti_rt_copy_buffer` | writes and allocates | `willreturn nofree` |
| `anti_rt_grow` | writes and allocates | none |

The other 61 rows are any. They are the atomics, the locks, the channels, the
hooks and the threads, and the functions that read through a pointer they
loaded. The snapshot functions count in a global, and the six functions that
never return write a report.

## Correction of the work order

The table of "Memory effects of runtime functions" spelled the third class
`memory(argmem: readwrite, inaccessiblemem: readwrite)`. A function of that
class calls malloc or realloc, which set errno when they fail, and the pinned
clang declares both with `errnomem: write`. A class without it says less than
the body does, which C2 refuses. The class and the table now carry
`errnomem: write`, and the work order says why.

The work order's examples `anti_rt_builder_append` and `anti_rt_hash_seeded` are
no rows of `RT_FUNCTIONS`. Both are an `extern fn` of the standard library, and
the step leaves those without a class.

## Tests

Each new test failed first, except the two program tests, which pin behaviour
that holds before and after.

- `runtime_effects` in `unit_lower`: a table written from the C bodies, checked
  against every row. It did not compile before the column existed.
- `dump_llvm_effects_linux-x86_64`, golden `effects.dev.linux-x86_64.ll`, at
  `--cpu v2` so the f16 conversions are calls. It failed on the seven missing
  attribute lists.
- `keeps_effects` in `unit_modules`: the fact through a library file, and the
  weaker reading after a second file declares the function with an `extern fn`.
  `damaged_files` reads a valid byte of effects and refuses one past the
  guarantees.
- `program_effects_edges`, release mode, C2: each classified function is called
  again after a write to the memory it reads. A `str` that shares a buffer
  compares and hashes before and after a byte changes, a snapshot copies the text
  before the buffer changes, `dup` copies an owned buffer before and after a
  write, and `to_slice` grows its buffer.
- `program_hash_seeded`: six values of `lang.seeded`, computed in Python from
  `mix(hash ^ mix(with))`.

## Re-pinned values

| Pin | Old | New |
|---|---|---|
| `link-identity/return42.macos-arm64.sha256` | `d287ba79…b67b6029` | `e4d77bea…39e3049d` |
| SHA-256 of `emit-identity/programs.sha256` | `44218a41…3285696f`, 1194 lines | `918fa48c…e209d044`, 1206 lines |

444 lines of the manifest changed, and the 12 of `effects_edges` and
`hash_seeded` are new. `return42` changes with `hash.o` of the runtime. The
listings `scale.antl.hex` and `generics/pick.antl.hex` follow the format.

## Measurement

`tests/bench/ablate/run.py`, 15 runs each. The baseline executables were built
before the change under `build/effects-before/` and timed after it through
`--twin`, linked from `build/effects-twins/`.

Before, `build/drive/logs/effects-before-<program>.log`:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 296.6 ms | 292.4 ms | 1.01 | 1,896 B | 51.5 ms |
| `objects` | 141.6 ms | 106.1 ms | 1.33 | 13,448 B | 50.9 ms |
| `builder` | 263.8 ms | 45.3 ms | 5.82 | 15,328 B | 67.2 ms |
| `simd_loop` | 126.8 ms | 123.0 ms | 1.03 | 2,016 B | 41.4 ms |
| `map_work` | 181.2 ms | 136.0 ms | 1.33 | 109,248 B | 305.6 ms |
| `mixed_work` | 192.0 ms | none | | 669,416 B | 1979.8 ms |

After, `build/drive/logs/effects-after-<program>.log`:

| Program | Anti | Baseline executable | To baseline | C twin | Object | Release compile |
|---|---:|---:|---:|---:|---:|---:|
| `scalar_loop` | 298.2 ms | 298.7 ms | 1.00 | 293.6 ms | 1,896 B | 42.4 ms |
| `objects` | 145.8 ms | 144.5 ms | 1.01 | 107.2 ms | 13,448 B | 53.6 ms |
| `builder` | 269.2 ms | 267.7 ms | 1.01 | 45.1 ms | 15,328 B | 71.8 ms |
| `simd_loop` | 130.6 ms | 129.7 ms | 1.01 | 126.6 ms | 2,016 B | 45.1 ms |
| `map_work` | 180.8 ms | 181.8 ms | 0.99 | 136.9 ms | 108,720 B | 308.9 ms |
| `mixed_work` | 191.8 ms | 192.5 ms | 1.00 | none | 668,496 B | 2015.7 ms |

Every program prints its old line: the script checks each build and the baseline
executable against the line of the C twin, or of the baseline for `mixed_work`.
All medians lie within 2 percent of the baseline. The objects of `map_work` and
`mixed_work` shrank by 528 B and 920 B. The release text of `map_work` holds no
call of `anti_rt_hash_seeded`, `build/drive/logs/effects-map_work.ll`.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the entries of the step
`noreturn`:

- The class of a function that allocates writes errno as well.
- The eight rows of a class other than any and the reasons of the rest.
- Two library files that declare one C function leave it the weaker reading.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/effects-{host,asan,ubsan}-build.log`.
- Host suite: 1550 of 1550, `build/drive/logs/effects-host-suite.log`.
- ASan suite: 1549 of 1549, `build/drive/logs/effects-asan-suite.log`.
- UBSan suite: 1549 of 1549, `build/drive/logs/effects-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite.
- The docs-style checker reports no error on every `.md` file the step touched.
  It warns on line 3 of the work order, as it did before the step.

State after the push of the step, before this report:

```console
$ git log --oneline -3
884da5b0 Add the memory effects of the runtime functions
2f32e84b Compute anti.lang.seeded in Anti
9e93734e Report step noreturn of the optimization facts
$ git status --short
$ git rev-parse HEAD origin/main
884da5b01411437abcb40036d3948c5ff2337358
884da5b01411437abcb40036d3948c5ff2337358
```
