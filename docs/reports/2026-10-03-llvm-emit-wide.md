# Step emit-wide of the LLVM back end

Step `emit-wide` of `docs/work-order-llvm-back-end.md`: overflow, flags, saturation,
`MULH` and the simd operations. Commits `83af0287` and `2b2b80b6`. The native back
end is unchanged and stays the default.

## What the step built

- `src/antic/llvm_emit.c` translates `addov`, `subov` and `mulov` through
  `llvm.s*.with.overflow`, and `branchov` branches on the `i1` of the operation right
  before it. `smulh` and `umulh` multiply in twice the width, `i128` at 64 bits.
- The four saturating sums and differences are `llvm.{s,u}{add,sub}.sat`. A saturating
  product multiplies in twice the width, clamps with `smax` and `smin` or `umin` and
  truncates.
- `mulfl` takes its flags from `smul` and `umul.with.overflow`. The shifts with flags
  mask the count to the width, and `negfl` overflows at the least value. The four
  `i1` of the last flag operation are kept as values or constants for the reads.
- `vunary` and `vshuffle` work on `<N x T>`. A fold of float lanes halves the vector
  with `shufflevector`, see the first provisional entry.
- The refusals of operations of later steps are gone, so the translator takes every
  operation of the IR. `llvm_verify` drops its list of accepted programs and fails on
  any program antic does not translate.

## What it tested

- `unit_llvm_emit`: one test per row group, red first
  (`build/drive/logs/llvm-emit-wide-red.log`, 26 failed checks), then green
  (`llvm-emit-wide-green.log`). The halving fold was red first as well
  (`llvm-emit-wide-fold-red.log`).
- `llvm_verify_<target>`: 457 texts verify per target on all six, none refused
  (`llvm-emit-wide-verify2.log`). The first run refused `trace_handlers` in dev mode,
  a comparison stored as `i64`. Its text is
  `build/drive/logs/llvm-emit-wide-trace_handlers-dev.ll`.
- `llvm_program_<name>_<target>_<mode>`: `flags`, `saturating`, `mul_high`,
  `carry_chain`, `wrapping` and `simd` in release and dev mode on macos-arm64 and
  macos-x86_64, 24 tests, each with the `.expected` file of its native test. `simd`
  first failed with `halving 1` against `halving 2` in all four
  (`llvm-emit-wide-programs1.log`).
- `llvm_flags`: `tests/programs/llvm_flags.anti` checks the result and the four flags
  of 364 cases in release mode, the test of the same name of the work order. The
  table comes from a model of two's complement arithmetic. The native back end meets
  all 364 (`llvm-emit-wide-flags-native-run.log`), and the program runs under the LLVM
  back end alone, as every `llvm_*` program does.

## Provisional entries

Three, after the entries of the step `emit-memory` in `docs/decisions.md`:

- A fold of float lanes halves the vector, the order the entry on `v.sum()` fixes.
  The work order's `llvm.vector.reduce.fadd`, `fmin` and `fmax` add in another order
  and treat NaN and the two zeros another way. Integer folds keep their intrinsics.
- A shift with flags masks its count to the width. A left shift overflows when the
  arithmetic shift back differs, and a right shift never overflows, as in `expand.c`.
- A comparison widens its `i1` to the type of the instruction, for the `ne` of type
  `i64` that the trampolines of `whole_tables.c` write.

## Questions

1. The row of `vreduce` in "Instruction mapping" contradicts the entry on `v.sum()` in
   `docs/decisions.md`. The step follows the decision. Should the row be rewritten in
   the step `switch`?
2. `whole_tables.c:360` writes `ne` with the type `i64`, and the IR verifier reports it
   under `--dump-opt --dev`. Should the whole-program pass write `i8` there?

## Gates

The three builds had no warnings. `link_identity_macos-arm64` passed unchanged.
`emit_identity` keeps every digest it had and gains 6 lines, the native assembly of
`llvm_flags` on six targets. The docs-style checker gave 0 findings on
`docs/decisions.md` and this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1563 of 1563 | 200 s | `build/drive/logs/llvm-emit-wide-host.log` |
| asan | 1562 of 1562 | 497 s | `llvm-emit-wide-asan-1.log`, `llvm-emit-wide-asan-2.log` |
| ubsan | 1562 of 1562 | 401 s | `build/drive/logs/llvm-emit-wide-ubsan.log` |

ASan ran in two parts, 781 and 792 tests. The 11 `dev_object_*` fixtures ran in both,
so 1562 distinct tests passed.

After the push of the code commits:

```text
$ git log --oneline -3
2b2b80b6 Run the wide programs under the LLVM back end and verify every text
83af0287 Translate the wide operations in the LLVM back end
abb5aa5a Report step emit-memory of the LLVM back end
$ git status --short
$ git rev-parse HEAD origin/main
2b2b80b61dc960283dc58b9512c1ccd8f8f3d141
2b2b80b61dc960283dc58b9512c1ccd8f8f3d141
```
