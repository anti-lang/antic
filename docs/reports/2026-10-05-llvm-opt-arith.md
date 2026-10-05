# Step arith of the optimization facts

The step `arith` of `docs/work-order-llvm-optimization.md`: "Arithmetic,
addresses and ranges", under condition C2 and choice D5.

## What the step built

- The IR of `src/antic/ir.h` holds the facts. The field of an `add`, `sub` or
  `mul` holds `IR_NSW` and `IR_NUW`, and that of an overflow operation
  `IR_NSW`. The field of a `ptradd` holds `IR_INBOUNDS`, a `load` holds its
  range in its operands b and c, `struct ir_function` carries the range of its
  result, and the new operations `lifestart` and `lifeend` name a slot.
- Lowering marks a checked operation `nsw`, and dropping the checks clears it.
  The step of a range loop is `nsw` on a signed range, `nuw` as well from a
  constant low bound of 0 or more, and `nuw` alone on an unsigned range. The
  address of a field is `inbounds` unless the object comes through a view by
  `as`, which `sema_value_view` and `sema_place_view` follow and the symbol mark
  `holds_view` keeps for a local. A load of a `bool`, an enum or a tag takes the
  range of its values, and an Anti function that returns one carries it. A `let`
  starts the lifetime of its slot, and every exit of its block ends it, apart
  from an exit of the function.
- The LLVM text writes `nuw nsw`, `getelementptr inbounds`, `!range` with one
  node per range, `range()` on a result and on a direct call, and the lifetime
  intrinsics on the alloca of the slot. The value of a checked operation is the
  plain operation with `nsw` beside `llvm.s*.with.overflow`. The DESIGN comment
  above `ptradd` in `src/antic/llvm_emit.c` holds the exception for fields.
- `ir_print`, `ir_verify`, the optimizer and the library file follow the facts.
  The format is version 80.
- `tests/bench/ablate/opt_no_arith.sh` removes the facts for the measurement.

Two defects came up on the way, each with a commit and a test of its own. A
`return` that wraps its value into a `?T` beside a `defer` failed the verifier
at HEAD, `programs/return_wrap_defer.anti`. A step of half the range of a
signed type, `by -128` on an `i8`, carried a wrong `nsw`, which an automated
review of the step found. `programs/by_unsigned.anti` held such a step.

The work order wrote `nsw` for every range step. C2 rules it out on an unsigned
range, whose counter may pass the top of the signed type. The sentence now
names the signed range, and a provisional entry records it.

## Tests

Each failed before its code existed.

- `arith_facts` in `unit_ir`: the printed form and seven refusals.
- `fills_arith_facts` in `unit_lower`: every fact, no `inbounds` through a
  view, no `nuw` from a negative bound and no `nsw` on a half step.
- `arith_text` in `unit_llvm_emit`: every attribute, metadata and intrinsic.
- `dump_llvm_arith_macos-arm64`, golden `tests/dump/arith.dev.macos-arm64.ll`.
- `program_arith_facts` in release mode and `program_arith_facts_checks` with
  `--checks`, C2: a view by `as` past the end of an array beside the field of
  the real object, range steps at both ends of `i8` and `int`, across the sign
  of `u8` and by half a type, an enum zeroed by `alloc` with no case 0, the full
  `u8` enum, locals in loop turns and blocks, and a struct returned from its
  block. It pins behaviour, so the text without the facts gives its output too.
- `llvm_program_llvm_wrap_<target>_release` and `_dev` with `--no-checks`:
  `x + 1 > x` is false at the top of `int`, which a stray `nsw` would fold.

## Re-pinned values

| Pin | Old | New |
|---|---|---|
| SHA-256 of `emit-identity/programs.sha256` | `ff264d72…a0eb0d3b2`, 1224 lines | `684ae58a…63c4d9537d`, 1242 lines |
| `link-identity/return42.macos-arm64.sha256` | `7b77362a…7378d8ec` | unchanged |

The three commits pinned `51b7eadb…8df3f45` and `65c6574e…aff83a13b` on the
way. 51 goldens of `tests/dump` and `tests/opt`, `tests/modules/main.ir`, the hex listings `scale.antl.hex` and
`generics/pick.antl.hex`, the IR texts of `unit_lower`, `unit_optimize`,
`unit_whole` and `unit_modules` and the opcode of `run_antl_verify.cmake`
changed. `build/arith-scratch/strip_check.py` removes the facts from old and
new goldens. What then differs is the `add nsw` value of a checked operation in
four dev goldens and an empty line before the new intrinsic declarations.

## Measurement

`tests/bench/ablate/run.py <program> opt_no_arith.sh`, 15 runs each, logs
`build/drive/logs/arith-bench-<program>.log`. The wrapper gives the objects of
the step `params` for `map_work` and `mixed_work`, 108,624 B and 667,712 B.

Before, the build through `opt_no_arith.sh`:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 297.5 ms | 292.9 ms | 1.02 | 1,896 B | 44.1 ms |
| `objects` | 145.0 ms | 105.6 ms | 1.37 | 13,448 B | 55.5 ms |
| `builder` | 266.4 ms | 45.6 ms | 5.84 | 15,328 B | 72.6 ms |
| `simd_loop` | 129.0 ms | 125.2 ms | 1.03 | 2,016 B | 44.6 ms |
| `map_work` | 181.3 ms | 137.3 ms | 1.32 | 108,624 B | 364.4 ms |
| `mixed_work` | 192.5 ms | none | | 667,712 B | 2211.2 ms |

After, the baseline of the same runs:

| Program | Anti | To before | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 297.5 ms | 1.00 | 1.02 | 1,896 B | 40.8 ms |
| `objects` | 141.7 ms | 0.98 | 1.34 | 13,448 B | 51.8 ms |
| `builder` | 262.7 ms | 0.99 | 5.76 | 15,328 B | 67.3 ms |
| `simd_loop` | 129.1 ms | 1.00 | 1.03 | 2,016 B | 40.9 ms |
| `map_work` | 181.8 ms | 1.00 | 1.32 | 108,608 B | 310.2 ms |
| `mixed_work` | 191.3 ms | 0.99 | | 668,960 B | 1996.9 ms |

Every build printed the line of its C twin, or of its baseline for
`mixed_work`. All medians lie within 2 percent. The object of `mixed_work`
grows by 1,248 B. The compile times of the wrapper include its `sed`. The runs
predate the half-step fix, which changes no text of these programs.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after those of `params`:

- The step of an unsigned range carries `nuw` alone, and a half step no `nsw`.
- The range of an enum holds 0, and a function of C carries no `range()`.
- A field reached through a view by `as` is no `inbounds` address.
- A `let` alone starts and ends a lifetime, and an exit of the function ends
  none.

## Questions

- A `*T` that a view by `as` made and that a call or a field passes on loses
  the view, so a field through it is `inbounds`, as it is `dereferenceable`
  since the step `params`. Should the view be part of the type?
- A view of bytes as a `bool` or an enum, and an enum field that C writes, may
  hold a value outside the range that `!range` states. The emitter already read
  a `bool` as its low bit. Should such views be refused, or the ranges narrowed?

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`,
  `build/drive/logs/arith-{host,asan,ubsan}-build.log`.
- Host suite: 1569 of 1569, 216 s, `build/drive/logs/arith-host-suite.log`.
- ASan suite: 1568 of 1568, 419 s, `build/drive/logs/arith-asan-suite.log`.
- UBSan suite: 1568 of 1568, 291 s, `build/drive/logs/arith-ubsan-suite.log`.
- `emit_identity`, `link_identity_macos-arm64` and `llvm_verify_<target>` for
  all six targets passed in the host suite.
- The docs-style checker reports nothing on `docs/decisions.md`, `CLAUDE.md`
  and this report. It warns on line 3 of the work order, which this step did
  not touch.

State after the push of the step, before this report:

```console
$ git log --oneline -3
2bcc49e3 Leave nsw off a step of half the range of a signed type
9c55d3c1 Add the wrapper that removes the facts of arithmetic and ranges
68e38837 Add the facts of arithmetic, addresses and ranges
$ git status --short
$ git rev-parse HEAD origin/main
2bcc49e36f8a41c588f611461f1b6b6f79cc217a
2bcc49e36f8a41c588f611461f1b6b6f79cc217a
```
