# The result of the thunk of a callback of C

Eddie answered the two questions of
`docs/reports/2026-11-07-release-link-c-context.md` with "1a, 2. no". The
session recorded both answers and built the second. The first waits for a
release of `anti-lang/llvm-tools`, which another session makes.

## What changed

- `69dffca8` records the answers as "Answers of 2026-11-07, fourth set" in
  `docs/decisions.md`, with the line in the entry of the feature freeze.
  The row of T211 in `docs/audit/tests-findings.md` names route (a).
- `ce199260` drops the select of the canonical NaN on the float result of
  the thunk `callback.N`.

Q1 builds nothing, and `symbols_moved_frame` stays on the list under T211.

## Q2: the result of the thunk

- `store_params` takes the address of the thunk, and `llvm_emit_nan_module`
  gave every function whose address is taken `nan_reached`, the select on
  its return. `lower_c_callback` now marks the thunk `result_of_c`, and its
  address no longer counts. Every other function keeps the select, and the
  thunk keeps it on each float argument.
- `dump_llvm_c_callback_macos-arm64` shows the result without the select.
  It failed first, at line 83, on the select the thunk still wrote.
- `program_abi_nan` gains three lines. C passes `nan_produce` with a context
  of its own to `anti_nan_result`, and it returns each of the three NaNs.
  The program hands the result to C, writes it into a union and stores it
  through a pointer of C. Each gives the canonical NaN in all four runs,
  release, dev, `--memory-checks` and `v1` under Rosetta, as do the
  earlier lines.

## The measurement

C calls the exported function `drive`, which calls a callback of C with a
context of C 200 million times. Each argument is the result of the call
before. The median of 11 interleaved runs per call:

| Build | arm64 | x86_64 at `v1` |
|---|---:|---:|
| with the select on the result | 4.63 ns | 9.20 ns |
| without it | 3.33 ns | 5.57 ns |

Another session built LLVM on the Mac meanwhile. `pgrep` found no `ninja`
before and after these runs. A first set, which may have run beside it,
gave 4.20 and 3.04 ns, and 8.58 and 5.27 ns. The arm64 loop lost the
`fcsel` after its `blr`. Before the thunk the figures were 2.15 and 2.39 ns.

## Suites

The host suite ran its 6953 tests in three parts with `-j14` before
`ce199260`: 399 s, 277 s and 327 s. Every test passed, with the two skips
of `mimalloc_environment`. The LLVM build may have run beside it, so the
times are not comparable with earlier ones. Only `docs/notes/math.md`
changed after that run.

Each sanitizer suite ran its 6951 tests at `ce199260` in six parts,
`ctest -I 1,1150` to `-I 6001,6951`: `asan` 251 s, 191 s, 258 s, 194 s,
451 s and 456 s, `ubsan` 259 s, 189 s, 258 s, 208 s, 469 s and 481 s.
Every test passed, with the same two skips. `pgrep` every 20 seconds found
no `ninja` during them. The VMs stayed untouched.

## Provisional lines

None new. The provisional entry on the exported functions that wrap a
callback of C keeps its tag. It names those functions and the call from
Anti. Its part on the result left it, and stands as the line after it.

## Questions for Eddie

1. Q2 calls the result of the thunk a value that C produced. The session
   kept form B where Anti hands that value back to C. An exported function
   that returns it gives the canonical NaN, as every exported function
   does, and so does an argument of C. Should such a float reach C again
   with its bits as C made them?
