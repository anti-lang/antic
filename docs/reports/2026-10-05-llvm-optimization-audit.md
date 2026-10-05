# Audit of the LLVM text for optimization facts

## What the session did

The session read the optimized IR of `tests/bench/map_work` and its C
twin. It measured candidate changes through `antic --opt <wrapper>`,
which edits the LLVM text before the pinned `opt` runs. It read
[Performance Tips for Frontend Authors](https://llvm.org/docs/Frontend/PerformanceTips.html)
against `src/antic/llvm_emit.c`. It wrote
`docs/work-order-llvm-optimization.md` from the results.

No file under `src/`, `tests/` or `tools/` changed. The four wrappers
and the C twin with a seed read at run time stand in
`build/bench-scratch/ablate/` for the step `ablate`. `opt_hash.py` and
`opt_noreturn.py` read the pinned `opt` from `REAL_OPT`. `opt-inline` and
`opt_m1.sh` name it by an absolute path of this Mac.

## Findings

`map_work` runs 1.46 to 1.50 times as long as its C twin on macos-arm64.
The failure path of `catch fatal` is the main cause. It calls
`Error.fatal` through the table and then branches back to the normal
path. `anti_rt_exit` and `anti_rt_out_of_memory` carry no `noreturn` in
the text. The emitter writes no `unreachable`.

LLVM therefore treats the failure block as part of every loop:

- The block blocks the inlining of `HashMap.get`, `HashMap.remove` and
  `SlotTable.slot_of` at the default threshold.
- Its calls force a reload of the map's fields and a new `mix(seed)` on
  every lookup.

Medians of 15 alternating runs, each against the baseline of its run:

| Change | To baseline |
|---|---:|
| `noreturn cold` and `unreachable` on the fatal path | 0.91 |
| inline threshold 2000 | 0.83 to 0.84 |
| inline threshold 2000 and `anti_rt_hash_seeded` in the text | 0.81 to 0.82 |
| `--no-hooks` or `--no-checks` | 1.00 |
| `"target-cpu"="apple-m1"` alone | 1.01 |
| C twin | 0.67 to 0.68 |

The C twin is a fair reference. Reading its seed at run time costs
1.8 ms. `HashMap.set` stays a call in every Anti build, and clang
inlines `set` and `resize` of the twin.

## Decisions

Eddie decided on 2026-10-05 that antic uses every optimization LLVM
offers. The faster generated program wins, under three conditions:

- Options within 2 percent count as equal, and the cheaper one in compile
  time and size wins.
- A fact LLVM cannot check goes in only where the language guarantees it,
  each with a program test at the edge of the guarantee.
- Profile-guided optimisation is opt-in. Shipping the runtime as bitcode
  waits for a separate yes.

He chose the result type `never` for a never-returning function and
`final fn fatal(self) -> never`. The work order holds the revised
decision 910, which its step `noreturn` writes into `docs/decisions.md`.

## Questions

- Shipping the runtime as bitcode with LTO. The step `runtime-lto`
  measures it, and the default changes only after Eddie's answer.

## State

```text
$ git log --oneline -1
2992919b Add ./c, which builds a Debug, Release or sanitizer tree
$ git status --short
?? docs/reports/2026-10-05-llvm-optimization-audit.md
?? docs/work-order-llvm-optimization.md
```
