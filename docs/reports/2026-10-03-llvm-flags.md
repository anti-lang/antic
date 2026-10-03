# Step flags of the LLVM back end

Step `flags` of `docs/work-order-llvm-back-end.md`. Commits `49aac449`, `4d868bb4` and
`294b4020`. The step is BLOCKED on one question, at the end. Under the LLVM back end, 18
tests that run a program fail, each because release mode inlines a function of the
program that the test expects as a frame or a symbol of its own. Nothing is pushed.

## What the step built

- `ANTIC_BACKEND`, a CMake option, `native` or `llvm`, names the back end of antic
  without `--backend`. A tree configured with `-DANTIC_BACKEND=llvm` runs the whole
  suite under the LLVM back end. The default stays `native`. The tests of native
  assembly name `--backend native`, and the tests that use `-S` without a runtime
  archive pass `--opt` and `--llc`.
- `-g` through `src/antic/llvm_debug.c`, lines only. COFF writes the compile unit and
  the subprograms in every build, so CodeView names every static function in the PDB.
- The build id digests the `.ll` text without the spans that `-g` added.
- COFF plugins and hosts of plugins: `dllimport` declarations, the `__imp_` entries
  in data with `anti_rt_imports`, and the `.def` names of a host from the IR. Both
  refusals of the step `emit-run` are gone.
- `StackTrace.capture`, `debug.backtrace` and a function that passes them a skip of
  1 or more keep their frame, `noinline` and without tail calls. Each counts its own
  frame in its skip. Inlined, `capture()` skipped the frame of its caller, and a tail
  call in `debug.backtrace` lost one more.
- An integer constant of a float type is the bits of the float. `std/sorted_map` did
  not verify before (`store double 0`).
- `--lib static`, `--lib shared`, `--closed`, `--no-runtime`, `--memory-checks` and
  `--linker platform` needed no change. Their tests pass in the LLVM tree.

## What it tested

- Unit tests in `tests/unit/test_llvm_emit.c`: `float_bits`, `debug_lines`,
  `debug_spans` on four targets, `coff_plugin`, `coff_names` and `frames_kept`. Red
  first, under `build/drive/logs/flags/`: `unit-red.log`, `b4.log`,
  `unit-frames-red.log`, `unit-frames-red2.log` and `unit-frames4.log`.
- `llvm_build_id_<target>`, host and windows-x86_64. `llvm_inline_lines` reads the
  line table with `llvm-objdump -d -l`, since the archive has no `llvm-dwarfdump`.
  Neither was red first: before this step `-g` wrote nothing under LLVM.
- `mixed_backends_chain_*` and `mixed_backends_generics_*`, in both directions.
- `llvm_coff_plugin_<target>`, which compares the exports of a COFF host and plugin
  with the native ones.
- 788 texts of `tests/programs` with `-g`, release and dev, for macos-arm64 and
  windows-x86_64, pass `opt -passes=verify` (`verify-g.log` is empty).

## The suite under the LLVM back end

`build/llvm`, configured with `-DANTIC_BACKEND=llvm`: 1560 of 1578 pass in 200 s
(`llvm-suite-final.log`). This covers macos-arm64 and, through the
`program_*_macos-x86_64` tests, macos-x86_64 under Rosetta. The 18 that fail:
`std_backtrace`, `anti_memory_checks`, `anti_build`, `anti_symbols`,
`pdb_names_windows-x86_64`, `pdb_names_windows-arm64`, `debug_info`, `memory_checks`,
`memory_checks_list`, `trace_stack`, `trace_stack_g`, `trace_forged`, `trace_copies`,
`trace_copies_g` and `trace_symbols_<target>` on four targets. In each, a small
function such as `inner` is inlined into `main` and deleted. The frame, the symbol,
the PDB record or the ASan frame names the caller, with the callee's line under
`-g`, as "Debug information" of the work order describes. The texts:
`build/drive/logs/flags/stack_trace-release.ll` and `backtrace-release-g.ll`.

## Provisional entries

Seven, after the entries of the step `emit-run` in `docs/decisions.md`:
`ANTIC_BACKEND`, COFF plugins and hosts, the COFF debug information and the locations
of line 0, the frames a trace counts, the bits of a float constant, `llvm-objdump -l`
in `llvm_inline_lines` and the form of `mixed_backends`.

## Gates

The three builds had no warnings. `link_identity_macos-arm64` is unchanged.
`emit_identity` keeps every digest and gains 6 lines, the native assembly of
`programs/llvm_inline_lines.anti`. The docs-style checker gave 0 findings on
`docs/decisions.md` and this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1578 of 1578 | 214 s | `build/drive/logs/flags/gate-host.log` |
| asan | 1577 of 1577 | 484 s | `gate-asan-1.log` (790), `gate-asan-2.log` (798) |
| ubsan | 1577 of 1577 | 284 s | `build/drive/logs/flags/gate-ubsan.log` |

The 11 `dev_object_*` fixtures ran in both ASan parts.

```text
$ git log --oneline -3
294b4020 Link dev objects of both back ends in mixed_backends
4d868bb4 Build -g, the build id, COFF plugins and stack traces under LLVM
49aac449 Select the back end of the whole suite with ANTIC_BACKEND
$ git status --short
$ git rev-parse HEAD origin/main
294b4020c9f3baef196c33d90bc0d57f95e739d9
5a0380a3b3cc7d5b79431ed38b829efb93ae222a
```

## Question

1. Release mode under LLVM inlines small functions, as "What to expect from the
   generated code" wants. The 18 tests above expect each function of their program
   as a frame or a symbol of its own, which the native back end gives. Should those
   tests take the LLVM result in release mode, with the frames of the functions that
   are not inlined and the callee's line under `-g`? Or should the LLVM back end
   keep these frames, and by which rule? Candidates: `noinline` on every function
   of a build with backtraces on, a `noinline` marker in Anti, or a test option
   that turns inlining off.
