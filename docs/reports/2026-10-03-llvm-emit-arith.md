# Step emit-arith of the LLVM back end

Step `emit-arith` of `docs/work-order-llvm-back-end.md`: the scalar operations and
the defined results of release mode. Commits `6bb64ee6` and `e99fca00`. The native
back end is unchanged and stays the default.

## What the step built

- `src/antic/llvm_emit.c` translates `add sub mul`, `sdiv udiv srem urem` with the
  guards, `and or xor`, `shl ashr lshr` with the count masked to the width,
  `fadd fsub fmul fdiv` without fast-math flags, `neg not fneg`, the 16 compares with
  `zext i1 to i8`, `trunc sext zext`, `sitofp uitofp`, `fpext fptrunc`,
  `llvm.fptosi.sat` and `llvm.fptoui.sat` declared once each, and `hext` and
  `htrunc` through `half`.
- A signed division divides by 1 where the divisor is 0 or minus one. The emitted
  form of the work order divides by minus one, which LLVM leaves undefined for the
  least value. The results are the ones the section defines.
- Every program holds `anti_rt_slots` and `anti_rt_injectable`, and the runtime calls
  `anti.rt.main`, so no program links without them. The text now writes each global
  without an address as a packed struct of its bytes, and the entry as an alias of
  `main`. A global that holds an address is refused until `emit-memory`.
- `tests/run_llvm_program.cmake` runs the route by hand: `--dump-llvm`, opt at
  `default<O2>` and llc at `-O2` in release mode, llc at `-O1` in dev mode, then
  ld64.lld of the runtime archive. It links an empty `anti_licenses`, since the
  notice carries a build id that digests the runtime library and stays out of the
  dump. `ANTIC_LLC` joins `ANTIC_OPT` in `tools/pinned-compiler.cmake`.

## What it tested

- `unit_llvm_emit`: the form of each operation, each guard, the intrinsic
  declarations and a constant divisor. It also covers the entry on Mach-O and COFF,
  the globals and the refusal of a global with an address. Red first:
  `build/drive/logs/llvm-emit-arith-red.log`.
- The scalar programs of `tests/programs` are the ones whose text needs no operation
  of a later step. The sweep at the first build found five
  (`build/drive/logs/llvm-emit-arith-sweep2.log`): `return42`, `return_pieces`,
  `deep_nesting`, `long_float` and `asserts`, which antic's optimizer folds to
  constants. The other 379 program and mode pairs were refused, 371 for `emit-memory`
  and 8 for `emit-wide`.
- So `tests/programs/scalar_ops.anti` reaches every scalar operation on values the
  optimizer cannot fold. It runs under both back ends and gives 0 under each.
- `tests/programs/llvm_division.anti` is the test `llvm_division`. The native glob
  leaves out `llvm_*`, since `program_llvm_division` failed under the native back end
  on both Mac targets in the first host run.
- `llvm_program_<name>_<target>_<mode>`: 26 tests over macos-arm64 and macos-x86_64.
  All seven programs run in release mode and the four that fold run in dev mode.
  `scalar_ops` and `llvm_division` take `--no-checks` in dev mode, since the checks
  call the runtime. `asserts` runs in release alone. Each gives the output of its
  `.expected` file, which the native back end meets.
- Mutants: dividing by the raw divisor failed `llvm_division` on all three runs, with
  a floating-point exception on x86_64. `fcmp one` for `fne` failed `scalar_ops`.
  Logs: `build/drive/logs/llvm-emit-arith-mut-*.log`, `llvm-emit-arith-mut2.log`.
- `llvm_verify` now must accept the four folding programs on every target, and
  verifies `scalar_ops` in release mode on all six.

## Provisional entries

Three, after the entries of the step `emit-core` in `docs/decisions.md`:

- Globals without an address and the entry alias from this step, with the linkage
  of functions. A global with an address waits for `emit-memory`.
- The signed division divides by 1 for a divisor of 0 or minus one.
- The tests build by hand until `emit-run`, with an empty `anti_licenses`,
  `--no-checks` in dev mode where checks call the runtime, and `llvm_*` programs
  under the LLVM back end alone.

## Gates

The three builds had no warnings. `link_identity_macos-arm64` passed unchanged.
`emit_identity` keeps every digest it had and gains 12 lines, the native assembly of
the two new programs on six targets. The docs-style checker gave 0 findings on
`docs/decisions.md` and this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1510 of 1510 | 125 s | `build/drive/logs/llvm-emit-arith-host3.log` |
| asan | 1509 of 1509 | 504 s | `llvm-emit-arith-asan-1b.log`, `llvm-emit-arith-asan-2.log` |
| ubsan | 1509 of 1509 | 274 s | `build/drive/logs/llvm-emit-arith-ubsan.log` |

ASan ran in two parts, 755 and 765 tests. The 11 `dev_object_*` fixtures ran in
both parts, so 1509 tests passed once each.

The first ASan part failed `llvm_program_long_float_macos-arm64_release` once. All
tests of a target shared one licence object, which parallel tests wrote at the same
time. Each test now writes its own, and the host suite ran again after the fix.

The first host run was started without the timeout parameter. The tool moved it to
the background after 120 seconds, against the instructions. It finished before
any other command ran, and it found the three failures fixed above:
`fmt_canonical`, `raw_output` and `emit_identity`.

After the push of the code commits:

```text
$ git log --oneline -3
e99fca00 Run the scalar programs and llvm_division under the LLVM back end
6bb64ee6 Translate the scalar operations into LLVM IR text
301efcad Report step emit-core of the LLVM back end
$ git status --short
$ git rev-parse HEAD origin/main
e99fca00543d60e14f2cc4e19fe1dc809984fd60
e99fca00543d60e14f2cc4e19fe1dc809984fd60
```
