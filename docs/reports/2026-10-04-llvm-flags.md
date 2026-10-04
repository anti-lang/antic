# Step flags of the LLVM back end, completed

Step `flags` of `docs/work-order-llvm-back-end.md`, after Eddie's decision of
2026-10-04 on inlined frames. `docs/reports/2026-10-03-llvm-flags.md` reports the
first part: `ANTIC_BACKEND`, `-g` through `src/antic/llvm_debug.c`, the build id,
COFF plugins and the tests `llvm_build_id`, `mixed_backends` and
`llvm_inline_lines`. This part adds commits `349f21eb` and `7d113f0f`. The step is
done, and every commit of it is pushed.

## What it built

- llc runs in the directory of its output and writes the output by its file name.
  Its input and its own path are absolute. llc records the name of its output as
  the object name of the CodeView of a COFF object, and lld-link carries it into
  the PDB. With the new names of `pdb_names_<target>`, that test found the build
  directory in the release PDB under LLVM.
- The 18 tests that failed under `-DANTIC_BACKEND=llvm` take the LLVM result of
  release mode:
  - `std_backtrace` reads `tests/std/backtrace.llvm.expected`.
  - `trace_stack`, `trace_stack_g`, `trace_forged`, `trace_copies` and
    `trace_copies_g` read `NAME.llvm.pattern` of `tests/trace`.
  - `anti_build`, `anti_symbols`, `anti_memory_checks`, `memory_checks`,
    `memory_checks_list` and `trace_symbols_<target>` read `BACKEND`.
  - `pdb_names_<target>` expects `skipped` and `main`, the functions that keep a
    record.
- `trace_symbols_<target>` finds `inner` in a dev build under LLVM.
- `debug_info` builds in dev mode under both back ends, since its breakpoints need
  every function as a frame. It accepts the `.file` form of llc that writes the
  directory apart.
- Under LLVM `anti_build` checks the `-g` rule in the line table of the object,
  since that back end keeps no assembly.
- `anti_symbols` under LLVM checks one frame, `main` at line 11 of `inner`. The
  check that the map names `through<float, str>` runs under the native back end
  alone, because release mode under LLVM leaves no copy to name.

## What it tested

The 18 tests red under LLVM first: `build/drive/logs/flags2/fail18.log`, then
`fail18-2.log` with the PDB path. The affected 110 tests pass in both trees:
`fail18-3.log` and `host-subset.log`.

| Suite | Passed | Time | Log |
|---|---|---|---|
| llvm (`-DANTIC_BACKEND=llvm`) | 1578 of 1578 | 202 s | `build/drive/logs/flags2/llvm-suite.log` |
| host | 1578 of 1578 | 215 s | `build/drive/logs/flags2/gate-host.log` |
| asan | 1577 of 1577 | 517 s | `gate-asan-1.log` (790), `gate-asan-2.log` (798) |
| ubsan | 1577 of 1577 | 320 s | `build/drive/logs/flags2/gate-ubsan.log` |

The llvm suite covers macos-arm64 and, through 228 tests named `*_macos-x86_64`,
macos-x86_64 under Rosetta. The 11 `dev_object_*` fixtures ran in both ASan parts,
which hold 1577 distinct tests. The three builds had no warnings. `emit_identity`
and `link_identity_<target>` are unchanged by this part. The docs-style checker
gave 0 findings on `docs/decisions.md` and this report.

## Provisional entries

Two, after Eddie's entry on inlined frames in `docs/decisions.md`:

- llc runs in the directory of its output, for the object name of the CodeView.
- Until the step `switch`, a test of release frames takes the LLVM result from
  `ANTIC_BACKEND` through `NAME.llvm.expected`, `NAME.llvm.pattern` and `BACKEND`.
  `debug_info` builds in dev mode, and `anti_build` reads the line table of the
  object under LLVM.

The seven of the first part stand as that report lists them.

## State

```text
$ git log --oneline -3
7d113f0f Take the LLVM result of release mode in the tests of frames
349f21eb Run llc in the directory of its output
a09f9add Record the decision on inlined frames under the LLVM back end
$ git status --short
$ git rev-parse HEAD origin/main
7d113f0f3f2a1f82639e51689020b9c5e5d31800
7d113f0f3f2a1f82639e51689020b9c5e5d31800
```
