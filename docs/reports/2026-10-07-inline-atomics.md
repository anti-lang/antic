# Inline atomics

Item 4 of the session that closes the move to LLVM, and item 17 of the first sessions.
`bfcd51b2` writes every atomic operation as an LLVM atomic instruction.

## The form

An atomic operation stays a call of `anti_rt_atomic_*` in the IR, so the IR, the
optimizer of antic and the library file are unchanged. `atomic_call` of
`src/antic/llvm_emit.c` writes the call as the instruction of its width:

| Operation | LLVM |
|---|---|
| `load` | `load atomic iN, ptr p seq_cst, align N/8` |
| `store` | `store atomic iN v, ptr p seq_cst, align N/8` |
| `swap`, `add`, `sub`, `and`, `or` | `atomicrmw xchg`, `add`, `sub`, `and`, `or` with `seq_cst` |
| `compare_swap` | `cmpxchg ptr p, iN e, iN d seq_cst seq_cst`, the flag as a byte |

Every body of `src/rt/atomic.c` is sequentially consistent, a compare and swap on both
sides, so each instruction keeps the ordering the runtime gave it. A result is the old
value sign-extended to 64 bits, as the runtime returns it, and the narrowing of lowering
follows unchanged. A pointer field takes the bits through `inttoptr`. A width that is no
constant of 1, 2, 4 or 8 bytes would stay a call, and no lowering writes one.

The level decides the instructions through the features of the function, as it decided
those of the runtime of the level. armv8.0 names no outline atomics, so llc writes the
load-store exclusive loop of the runtime there, and `casal`, `ldaddal` and `swpal` from
`armv8.2`. x86_64 takes `lock xadd`, `lock cmpxchg` and `xchg`. The functions of the
runtime stay for C callers and for the runtime itself.

## Tests

Each failed before the change, and each passes after it:

- `llvm_atomics` reads the text of all six targets: one instruction of each kind and
  width, the sign extension, the `inttoptr`, and no call of `anti_rt_atomic_*`.
- `atomics_<target>_<level>`, nine cases through `run_cpu_level.cmake`: linux-arm64 at
  `armv8.0`, `armv8.2` and `armv8.5`, windows-arm64 at `armv8.2`, macos-arm64 at
  `armv8.5`, linux-x86_64 at `v1` and `v3`, and macos-x86_64 and windows-x86_64 at `v3`.
  Each finds the instructions of its level and no call of `anti_rt_atomic_*` or of the
  `__aarch64_*` helpers.
- `programs/atomic_widths.anti` runs every operation on a field of `i8`, `i16`, `i32`,
  `int`, `u8`, `bool` and a pointer, with values below zero and wrapping at each width.
  Its output equals that of the build with the runtime calls, and it runs on every
  target the hosts run.

The tests of the orderings are the concurrency programs that existed:
`programs/concurrent`, `lock_free_queue`, `channels`, `channel_workers` and the `sync_*`
programs, and `std/spsc_ring`, `shared` and the `concurrent_map_*` tests. They passed on the Mac, under ASan and UBSan, on
anti-linux and on anti-windows. In the manifest of `emit_identity` 378 lines changed, one
program for one target each, those with an atomic field or a `static atomic` counter,
and the six of `atomic_widths` joined it.

## Documents

`docs/decisions.md` holds the entry of the form, and the entries on `volatile` and on
the instructions of the ARM64 levels say that the atomics are instructions now. The
DESIGN comment of `EXPR_ATOMIC` in `src/antic/lower_expr.c` names `atomic_call`.

## Gates

At `fce81e21`, which holds item 4 and the fixes of item 3 after `4f72f64b`: host 1659
of 1659 in 277 s, ASan 1658 of 1658 in 465 s, UBSan 1658 of 1658 in 317 s, no warning.
The first run of each failed `raw_output` on `run_runtime_hosts.cmake` of item 3, which
read the errors of opt into a variable. `fce81e21` writes them to a file, and
`raw_output` and `runtime_hosts` then passed in all three trees. anti-linux passed 1366
of 1366 in 205 s with the build, and anti-windows 1345 of 1354 with its nine usual skips
in 994 s with the build, `-j4`. The builds print no warning of our code, and those of the
VMs print the warnings of raylib's own headers alone.
