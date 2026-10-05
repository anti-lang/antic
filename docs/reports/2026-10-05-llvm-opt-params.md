# Step params of the optimization facts

The step `params` of `docs/work-order-llvm-optimization.md`: "Parameters and
results", under condition C2.

## What the step built

- `struct ir_param` carries `nonnull`, `deref_size` and `own`, and
  `struct ir_function` carries `result_ext` and `allocates`, in
  `src/antic/ir.h`.
- Lowering fills them in `lower_add_param` and `lower_add_params` of
  `src/antic/lower.c`. A `*T` is nonnull and dereferenceable for
  `size_of T`, a `?*T` is neither, an `own` pointer is `own`, and a result
  of 8 or 16 bits extends by its signedness. `lower_mark_allocates` marks
  `anti_rt_copy_buffer`, `anti_rt_dup` and `anti_rt_mem_alloc`. The thunk
  of an interface and the `.reach` function keep `nonnull` alone on their
  first parameter.
- The LLVM text writes `noundef` on each scalar parameter and result,
  `nonnull dereferenceable(N)` on a `*T`, `noalias` on an `own` pointer and
  on the result of a function that allocates, and `signext` or `zeroext` on
  a result. A coerced aggregate, a vector and a parameter in memory take
  none, so no `noundef` stands on words that may hold padding. A
  declaration of C writes a result extension on System V x86_64 and Apple
  arm64 alone.
- `ir_print` shows ` nonnull`, ` deref(...)`, ` own`, the result extension
  and ` allocates`. `ir_verify` refuses each fact where it does not belong.
- The library file carries the facts. A function holds them in bit 64 of
  its flags and a byte of the result extension. A parameter holds a byte of
  facts and the symbolic size. The format is version 79.

The work order needed no correction.

## Tests

Each failed before its code existed. The unit tests did not compile, and
the golden and the round trip of `unit_modules` differed.

- `param_facts` in `unit_ir`: the printed form and six refusals of the
  verifier.
- `fills_param_facts` in `unit_lower`: `*T`, `?*T`, `own`, `i8`, `bool`,
  `u16`, `self`, the thunk of an interface and `allocates`.
- `param_attributes` in `unit_llvm_emit`: every attribute, no `noundef` on
  a padded struct in words, and the result extension of C on all six
  conventions.
- `unit_modules`: the bytes of `scale_antl` and refusals of a result
  extension on `i64`, of bit 128 of the flags, of a fact of a pointer on
  `i64` and of a size without `nonnull`.
- `dump_llvm_params_macos-arm64`, golden
  `tests/dump/params.dev.macos-arm64.ll`.
- `program_param_facts`, release mode, C2: a `*Point` at the last element
  of a block of four, read on one call in a hundred. An `own` node stored
  in a holder and written through both in one call. `dup` written beside
  its original. The ends of the ranges of `i8`, `u8`, `i16`, `u16` and
  `bool` through recursive calls. It pins behaviour, so it passes in both
  modes before and after the change.

## Re-pinned values

| Pin | Old | New |
|---|---|---|
| SHA-256 of `emit-identity/programs.sha256` | `3b978a20…d9941c5b`, 1218 lines | `ff264d72…a0eb0d3b2`, 1224 lines |
| `link-identity/return42.macos-arm64.sha256` | `e4d77bea…3e3049d` | `7b77362a…7378d8ec` |

Every line of the manifest changed, since every program has a scalar
parameter or result. Six lines are new for `param_facts`. 74 `.ll`, `.ir`
and `.opt` goldens of `tests/dump` and `tests/opt`, the hex listings
`scale.antl.hex` and `generics/pick.antl.hex` and the line of
`ir_tuple_out` changed. With the new attributes removed, each golden equals
its old text. The `.ir` and `.opt` goldens of `devirt` add one line,
`type anti.lang.FieldDescriptor`, which the size of a parameter names.

## Measurement

`tests/bench/ablate/run.py <program> opt_no_params.sh`, 15 runs each. The
wrapper removes the new attributes, so it builds the program as it was
before the step. Its objects of `map_work` and `mixed_work`, 108,640 B and
667,872 B, equal those of the step `tables`. Logs:
`build/drive/logs/params-bench-<program>.log`.

Before, the build through `opt_no_params.sh`:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 299.9 ms | 294.6 ms | 1.02 | 1,896 B | 42.8 ms |
| `objects` | 142.6 ms | 107.1 ms | 1.33 | 13,448 B | 54.6 ms |
| `builder` | 264.9 ms | 45.9 ms | 5.78 | 15,328 B | 69.8 ms |
| `simd_loop` | 127.2 ms | 123.0 ms | 1.03 | 2,016 B | 44.4 ms |
| `map_work` | 181.5 ms | 137.1 ms | 1.32 | 108,640 B | 330.9 ms |
| `mixed_work` | 193.4 ms | none | | 667,872 B | 2142.6 ms |

After, the baseline of the same runs:

| Program | Anti | To before | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 299.6 ms | 1.00 | 1.02 | 1,896 B | 41.7 ms |
| `objects` | 141.6 ms | 0.99 | 1.32 | 13,448 B | 50.5 ms |
| `builder` | 263.4 ms | 0.99 | 5.74 | 15,328 B | 65.5 ms |
| `simd_loop` | 127.3 ms | 1.00 | 1.03 | 2,016 B | 41.4 ms |
| `map_work` | 181.1 ms | 1.00 | 1.32 | 108,624 B | 300.8 ms |
| `mixed_work` | 193.4 ms | 1.00 | | 667,712 B | 2008.3 ms |

Every build printed the line of its C twin, or of its baseline for
`mixed_work`. All medians lie within 2 percent. The objects of `map_work`
and `mixed_work` shrink by 16 B and 160 B. The compile times of the wrapper
include its `sed`.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the entries of the
step `tables`:

- Why a `*T` is dereferenceable, the two `*T` that point at no T, and the
  first parameter of a thunk.
- The result extension of a declaration of C on four conventions.
- `own` marks a pointer parameter alone.
- The size of a parameter declares the aggregate it names in the module.

## Questions

- A `*T` narrowed from the `.ptr` of an empty slice or from `alloc(T, 0)`
  points at no T, and `dereferenceable(N)` is then wrong. Should such a
  `*T` be refused, or the fact narrowed?
- Before this step, a definition already wrote `signext` or `zeroext` on an
  `i8` or `i16` parameter on every target. A C caller on linux-arm64 or
  Windows does not extend it, so an `export fn` with such a parameter reads
  undefined upper bits there. The step left it as it stood.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/params-{host,asan,ubsan}-build.log`.
- Host suite: 1559 of 1559, 212 s, `build/drive/logs/params-host-suite.log`.
- ASan suite: 1558 of 1558, 414 s, `build/drive/logs/params-asan-suite.log`.
- UBSan suite: 1558 of 1558, 290 s,
  `build/drive/logs/params-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>`
  for all six targets passed in the host suite.
- The docs-style checker reports nothing on `docs/decisions.md`,
  `CLAUDE.md` and this report.

State after the push of the step, before this report:

```console
$ git log --oneline -3
e0bc105b Add the wrapper that removes the facts of parameters and results
024f6e61 Add the facts of parameters and results
b82cb061 Report step tables of the optimization facts
$ git status --short
$ git rev-parse HEAD origin/main
e0bc105b821bb29c7df4a73d8a770678e60d0a9e
e0bc105b821bb29c7df4a73d8a770678e60d0a9e
```
