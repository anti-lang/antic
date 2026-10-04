# Step switch of the LLVM back end

Step `switch` of `docs/work-order-llvm-back-end.md`, run a second time after Eddie's
decision on `coff.c`. LLVM is the only back end. The logs lie in
`build/drive/logs/switch/` and the VM logs in `build/drive/logs/switch/vm/`.

## What the step built

- `9d3d1fa8` removes `--backend`, `--dump-select`, `--dump-alloc` and the CMake option
  `ANTIC_BACKEND`, and deletes `select.c`, `expand.c`, `regalloc.c`, `x86_64.c`,
  `arm64.c`, `emit.c`, `debug.c` and `mach.c` with their headers and `target_desc.h`.
  It also deletes the seven unit tests of machine code, the machine helpers of
  `tests/unit/pipeline.c`, the 59 `.alloc`, `.sel` and `.s` goldens and
  `mixed_backends`. `coff.c`, `coff.h`, `test_coff.c`, the two `f16` functions of the
  runtime, `test_f16.c` and `test_float_read.c` stay. `--llvm-mc` stays accepted and
  runs nothing. `struct debug_spans` and `debug_spans_free` moved to `llvm_debug.h`.
- The same commit adds a `--dump-llvm` golden for each program and target that had a
  golden of machine code, 56 in all. The tests that read assembly read llc's. The
  level and `f16` tests read a dev build, since release mode drops every function of
  a source without `main`. `trap_table` reads the body of `main`, into which release
  mode inlines `run`. `llvm_coff_plugin_<target>` checks each import of the plugin
  against the host's `.def` file, and `tests/dump/unwind.anti` passes its arguments,
  so release mode keeps the frame of more than a page.
- `a9cebae7`: at x86-64-v1 and v2 the text calls `anti_rt_f16_to_f32` and
  `anti_rt_f32_to_f16`, as the entry on `f16` in `docs/decisions.md` says.
- `9051587c`, `1f4e0987`, `295ba381` and `6d9895e2` make the changes of
  "Documentation changes", write `docs/notes/llvm.md`, delete the six notes of the
  native back end and cut four others.
- `7398fada`: the first Windows run failed `emit_identity` on all 1188 digests. The
  standard output of antic ends lines with CRLF there, the variable of
  `execute_process` dropped the CR and `file(WRITE)` wrote it back. The digest now
  reads the variable. The text was the same.

## Re-pinned values

| Pin | Old, at `07de5942` | New, at `7398fada` |
|---|---|---|
| `link_identity_macos-arm64` | `581809c6bd79b03428628f4aa992bc45a4723598077628863ea349564a7df006` | `e8b0f1dad4580659fa95c3950a6d359eb9cd4bb93f73e485f62e521a632d8477` |
| `emit_identity`, the file | blob `e35708db`, SHA-256 `aed376e4…0d799e` | blob `48553857`, SHA-256 `01c254a3…805477` |
| `return42.macos-arm64` | `e7f4aa75…bde62`, the `.s` | `6a7eeae2…4bb539`, the `.ll` |
| `unwind_windows-x86_64` | blob `57a6fdd2` | blob `47034926` |
| `unwind_windows-arm64` | blob `2537da28` | blob `3dd59b91` |

`emit_identity` holds 1188 digests, old and new, which do not fit this page. Each
stands in full in the blob above, as `git show <blob>` prints it. Every digest
changed, since the old ones digest native assembly and the new ones the
`--dump-llvm` text, with the version read as `VERSION`.

The old x86_64 unwind data had `spin` with 17 codes: XMM6, six registers, an
allocation of 5104 bytes and RBP, then `main`. The new data has one function,
`anti.rt.main`, with `spin` inlined and 9 codes: XMM6, an allocation of 5056 bytes,
and RBX, RDI, RSI, R14 and R15. The old ARM64 data saved d8 and x19 to x23 under a
frame of 5008 bytes. The new data saves d8, x19, x20, x23 and x28 under the same size.

## Tests

| Host | Suite | Passed | Skipped | Failed | Log |
|---|---|---|---|---|---|
| Mac | host | 1539 of 1539 | 0 | 0 | `host-suite-final.log`, 216 s |
| Mac | ASan | 1538 of 1538 | 0 | 0 | `asan-suite-final.log`, 399 s |
| Mac | UBSan | 1538 of 1538 | 0 | 0 | `ubsan-suite-final.log`, 280 s |
| anti-linux | host | 1271 of 1273 | 2 | 0 | `vm/linux-host-suite.log` |
| anti-linux | ASan | 1270 of 1272 | 2 | 0 | `vm/linux-asan-suite.log` |
| anti-linux | UBSan | 1270 of 1272 | 2 | 0 | `vm/linux-ubsan-suite.log` |
| anti-windows | host | 1252 of 1261 | 9 | 0 | `vm/win-suite-{1,2,3}b.log` |

anti-linux ran the export of `6d9895e2`. `7398fada` changes `run_emit_identity.cmake`
alone, which was copied there, and `emit_identity` passed in all three trees
(`vm/linux-emit-identity.log`). anti-windows ran the export of `7398fada` in three
parts, `-I 1,400`, `401,800` and `801,1261`. `vm/win-results.txt` holds each result
once. Only raylib's own sources warned, 67 times per build. `emit_identity` takes 400 s
under ASan on the Mac and sets that suite's time. A grep of `src/`, `tests/`, `tools/`,
`docs/notes/` and `docs/site/` finds no deleted file, option or note.

## Corrections of the work order

- "Targets, CPU levels and object formats", the rows `HEXT` and `HTRUNC` and the
  question on `f16` had compiler-rt convert below `v3`, and the step remove the two
  functions. That contradicted the entry on `f16` in `docs/decisions.md`, the tests
  `f16_<target>_<level>` and the list "What stays and what goes". They now follow the
  entry.
- "Integration route" said that `--llvm-mc` goes in this step. It now says that it
  stays.

## Provisional entries added

None. "Questions an implementer asks" allows no change of `docs/decisions.md` beyond
the listed lines. The choices of the tests stand in `docs/notes/llvm.md`.

## Questions

- `docs/decisions.md` lines 197 and 198 still call a division by zero and a shift by
  the width undefined in release. The list did not name them.
- Line 298 of `docs/decisions.md` and lines 63 to 65 of `docs/tooling.md` still say
  that antic writes assembly. The list did not name them either.

## State before this report

```text
$ git log --oneline -3
7398fada Digest the LLVM text of emit_identity from the variable
6d9895e2 Name opt and llc in the README and the package list of the tooling
295ba381 Describe the LLVM back end in the state of CLAUDE.md
$ git status --short
$ git rev-parse HEAD origin/main
7398fada6d3383520cd0e8f3e82e6cef093d0121
7398fada6d3383520cd0e8f3e82e6cef093d0121
```

## The first attempt

The first run stopped before its first change. The work order deleted `coff.c` with
the native back end, while `--bundle-runtime` and the plugin host on Windows need it,
and the entry on `--bundle-runtime` in `docs/decisions.md` names it.

Eddie decided on 2026-10-04 for option 1. `src/antic/coff.c`, `coff.h` and `tests/unit/test_coff.c` stay. "What stays and what goes" of the work order now lists what goes and what stays, file by file, with the unit tests, the goldens and `--llvm-mc`. The entry stands in `docs/decisions.md` after the entry on frame records. The redo of this step follows the work order as it stands now.
