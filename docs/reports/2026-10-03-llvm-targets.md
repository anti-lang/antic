# Step targets of the LLVM back end

Step `targets` of `docs/work-order-llvm-back-end.md`: the section "Targets, CPU levels
and object formats" and the data layout part of "Types and layout". Commit `6c7b8e8b`.

## What the step built

- `src/antic/llvm_target.c` and `llvm_target.h`. `llvm_triple` appends the triple of
  `target.c`, with `11.0` from `MACOS_MIN_MAJOR` and `MACOS_MIN_MINOR` on macOS, the
  version `emit.c` writes in `.build_version`. `llvm_data_layout` gives one string per
  target. `llvm_relocation_model` gives `pic` on Linux and macOS and `static` on
  Windows. `llvm_target_cpu` and `llvm_target_features` give the two attributes of a
  level. `llvm_clang_arch` gives the `-march=` value clang takes for the feature
  string.
- Every data layout and every feature string is copied from the output of the pinned
  clang 23.1.1 build 5 of `build/deps/clang`. The runs stand in
  `build/drive/logs/llvm-targets-dl.txt`, `llvm-targets-feat.txt`,
  `llvm-targets-feat2.txt` and `llvm-targets-feat3.txt`. llc accepted a module with the
  strings of three targets with no output on stderr
  (`build/drive/logs/llvm-targets-llc/`).
- `tools/cpu-levels` gains the `"target-cpu"` and `"target-features"` columns, and
  `antic --print-cpu-levels` prints them.
- `antic --print-llvm-targets` prints the triple, the relocation model and the data
  layout of each target, and the `-march=` value and the two attributes of each level.

## What it tested

- `cpu_levels_pin` compares the two new columns. It failed before the change to
  `print_cpu_levels` (`build/drive/logs/llvm-targets-red.log`).
- `llvm_datalayout_pin`, new, runs the pinned clang once per target and per level of
  its architecture, 18 runs, and compares the data layout, `"target-cpu"` and
  `"target-features"`. It failed before the option existed, and failed again with
  `-Fn32` dropped from macos-arm64 and with `+sb` dropped from `armv8.5`
  (`build/drive/logs/llvm-targets-mut.log`, `llvm-targets-mut3.log`).
- `unit_llvm_target`, new, checks the six triples, the six relocation models, the
  `"target-cpu"` column and that each feature of `cpu.c` (`+avx`, `+f16c`, `+sse4.2`,
  `+popcnt`, `+lse`, `+fullfp16`, `+dotprod`) is in the string of exactly the levels
  that have it.

## Provisional entries

Two, at the end of "CPU levels" in `docs/decisions.md`:

- The feature strings are the full lists clang writes, not the short strings of the
  table in the work order. clang runs with `-march=armv8.2-a+fp16+dotprod` and
  `armv8.5-a+fp16+dotprod` for the two upper ARM64 levels, and with `-mcpu=generic
  -mno-fmv` on ARM64. Without `-mcpu` clang picks `apple-m1` on macOS. With
  `-mno-fmv` the three ARM64 triples get one string, so there is one row per level.
- The two columns of `tools/cpu-levels`, the option `--print-llvm-targets` and the
  test `llvm_datalayout_pin`.

## Questions

The table under "Targets, CPU levels and object formats" in the work order gives `""`
for the x86_64 features and short ARM64 lists. The pinned clang writes neither. The
step took clang's strings, as the step prompt asked, and left the table of the work
order as written. Should the table be updated to the strings of `llvm_target.c`?

## Gates

The build has no warnings. `emit_identity` and `link_identity_macos-arm64` passed
unchanged. The docs-style checker gave 0 findings on `docs/decisions.md`,
`docs/notes/targets.md` and this report.

| Suite | Passed | Log |
|---|---|---|
| host | 1469 of 1469 | `build/drive/logs/llvm-targets-host.log` |
| asan | 1468 of 1468 | `build/drive/logs/llvm-targets-asan.log` |
| ubsan | 1468 of 1468 | `build/drive/logs/llvm-targets-ubsan.log` |

After the push of the code commit:

```text
$ git log --oneline -3
6c7b8e8b Add the targets of the LLVM back end
28582a92 Report step pins of the LLVM back end
67717b79 Check and ship opt and llc beside the other LLVM tools
$ git status --short
$ git rev-parse HEAD origin/main
6c7b8e8b6eb11ce3ea09942f95646b47d2bb7ac4
6c7b8e8b6eb11ce3ea09942f95646b47d2bb7ac4
```
