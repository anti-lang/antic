# Step noreturn of the optimization facts

The step `noreturn` of `docs/work-order-llvm-optimization.md`: "Facts that end
a path" and the choices D1 and D2.

## What the step built

- The result `never`. `-> never` is a contextual word on a function, an
  `extern fn` and a function type. The checker refuses a reachable end, a
  `return`, `may fail` and any value of the type, and a call of such a function
  ends its path for the missing-return rule, for narrowing and for the sets of
  `construct`. Lowering sets `never_returns` from the type. The flag rides on
  `TYPE_FN`, so the library file carries it in bit 7 of a function type, and
  the checked trees of generics carry it as well. The format is version 76.
- `never_returns` in `struct ir_function`, the column `NEVER` or `RETURNS` of
  `RT_FUNCTIONS` and the terminator `IR_UNREACHABLE` after `IR_RET`. Lowering
  ends the block after every call of such a function. `ir_verify.c` refuses an
  instruction after `IR_UNREACHABLE` and a block that goes on after a direct
  call of a function that never returns.
- `noreturn cold` on the declaration and the definition of such a function,
  and `unreachable` in the LLVM text.
- `IR_FAIL_GUARD`. The `none` arm of `p catch fatal` and `p catch e { }`, and of
  the same guards of a `?T`, gets the weights of a cold block. No switch drops
  it. The DESIGN comment above `enum ir_fail` says so.
- `pub final fn fatal(self) -> never` in `src/std/anti/lang.anti`. `catch
  fatal` calls `anti.lang.Error.fatal` directly and ends with
  `IR_UNREACHABLE`, and `lower_fatal_signature` is gone.
- `extern fn anti_rt_exit(status: c_int) -> never` in `lang.anti` and
  `io.anti`. `anti_rt_exit` and the five other functions of the column are
  `_Noreturn` in `src/rt/`.
- The revised entry of decision 910 in `docs/decisions.md`, an entry for the
  built step, the result in both specifications and the overview.

## Corrections of the work order

- "Facts that end a path" named four functions for the column. Reading every
  row against `src/rt/` found two more that end the program on every path,
  `anti_rt_table_unset` and `anti_rt_walk_changed`. The sentence lists six now.
- An assertion and a dev-mode check called their runtime function and jumped
  to the rest. That jump is a block that goes on after a call that never
  returns, which the verifier the work order asks for refuses. The arms now end
  with the call. The DESIGN comments above `assert_branch` and
  `lower_check_branch` said "falls through to the rest", and the work order did
  not name them. Both comments and the work order say so now.

## Tests

Each failed first: the checker refused `never` as an unknown type, and the unit
tests did not compile without the new fields.

- `error_never_result`: a reachable end, a `return`, `may fail` and a variable
  of type `never`.
- `dump_llvm_fatal_guard_macos-arm64`, golden `fatal_guard.dev.macos-arm64.ll`:
  `unreachable` after the direct call of `Error.fatal`, `!prof` on the guard,
  and `noreturn cold` on `anti_rt_exit`, `Error.fatal`, `anti_rt_check_failed`
  and the definition of `stop`.
- `never_returns` in `unit_ir`: a block that goes on after the call, and an
  instruction after `unreachable`.
- `runtime_functions` in `unit_lower` reads the prototype of every row in the
  headers of `src/rt/` and refuses a column that disagrees with `_Noreturn`.
- `trap_fatal_none`, release mode: `catch fatal` on `none` runs the function
  of `error.on_fatal`, prints `error 1: the pointer is none` and exits with 1.
  `run_trap.cmake` takes `STATUS` for a program that exits without aborting.
- `program_never_paths`, release mode: a function with a result that ends with
  a call of a `never` function, and narrowing after one, which exits with 3.

`unit_modules` keeps refusing the two damaged forms with new values: `never`
with `may fail` in place of the unused bit 7, and the block kind past
`IR_FAIL_GUARD`. `unit_optimize` lost the `sdiv` after a check of a known zero
divisor, which the check now ends.

## Re-pinned values

Twenty-nine goldens of `tests/dump` and `tests/modules` differ only in `->
never` on the extern line of a runtime function, `unreachable` in place of the
jump after its call and `noreturn cold` on its declaration. The listings
`scale.antl.hex` and `generics/pick.antl.hex` follow the format.

| Pin | Old | New |
|---|---|---|
| `link-identity/return42.macos-arm64.sha256` | `e8b0f1da…632d8477` | `d287ba79…b67b6029` |
| SHA-256 of `emit-identity/programs.sha256` | `01c254a3…d1805477`, 1188 lines | `44218a41…3285696f`, 1194 lines |

654 lines of the manifest changed, and the six of `never_paths` are new.

## Measurement

`tests/bench/ablate/run.py`, 15 runs each. The executables of the baseline were
built before the change and kept under `build/noreturn-before/`. After the
change each program ran alternately with that executable through `--twin`.

Before, `build/drive/logs/noreturn-before-<program>.log`:

| Program | Anti | C twin | Anti / C | Object | Release compile |
|---|---:|---:|---:|---:|---:|
| `scalar_loop` | 297.1 ms | 292.2 ms | 1.02 | 1,896 B | 49.4 ms |
| `objects` | 140.3 ms | 106.0 ms | 1.32 | 13,464 B | 51.3 ms |
| `builder` | 264.8 ms | 47.1 ms | 5.62 | 15,328 B | 67.3 ms |
| `simd_loop` | 128.7 ms | 124.7 ms | 1.03 | 2,016 B | 41.9 ms |
| `map_work` | 198.0 ms | 135.9 ms | 1.46 | 115,624 B | 318.0 ms |
| `mixed_work` | 209.7 ms | none | | 791,320 B | 2216.4 ms |

After, `build/drive/logs/noreturn-after-<program>.log`, with the baseline
executable timed in the same run:

| Program | Anti | Baseline executable | To baseline | C twin | Object | Release compile |
|---|---:|---:|---:|---:|---:|---:|
| `scalar_loop` | 309.6 ms | 309.1 ms | 1.00 | 305.0 ms | 1,896 B | 43.5 ms |
| `objects` | 148.2 ms | 148.5 ms | 1.00 | 109.1 ms | 13,448 B | 55.1 ms |
| `builder` | 271.5 ms | 270.8 ms | 1.00 | 47.6 ms | 15,328 B | 72.4 ms |
| `simd_loop` | 129.5 ms | 129.7 ms | 1.00 | 125.5 ms | 2,016 B | 42.1 ms |
| `map_work` | 182.1 ms | 198.6 ms | 0.92 | 137.4 ms | 109,248 B | 306.2 ms |
| `mixed_work` | 192.5 ms | 210.8 ms | 0.91 | none | 669,416 B | 2005.9 ms |

`map_work` ran in 182.1 / 198.6 = 0.917 of its baseline, and in 182.8 / 199.8 =
0.915 in a run before the documents changed
(`build/drive/logs/noreturn-after-map_work-1.log`). Both are within the 0.92 of
the work order. After `default<O2>` its `main` holds 35 loads and 6 stores,
against 43 and 6 for `opt_noreturn.py`.

## Provisional entries

Under "Compiler behaviour" in `docs/decisions.md`, after the revised entry:

- `never` is a contextual word after `->`. `fn(A) -> never` converts to no
  other function type, a `never` function cannot fail, an anonymous function
  takes `never` from its target, and the header writes `_Noreturn`.
- The column marks six runtime functions, `_Noreturn` in `src/rt/`.
- The arm of a none guard is cold for both error forms and for `?T`, and the
  `else` of `let` is not.
- A C function that two library files declare never returns only where both
  say so.

## Gates

- Build: zero warnings in `host`, `asan` and `ubsan`.
- Host suite: 1545 of 1545, `build/drive/logs/noreturn-host-suite.log`.
- ASan suite: 1544 of 1544, `build/drive/logs/noreturn-asan-suite.log`.
- UBSan suite: 1544 of 1544, `build/drive/logs/noreturn-ubsan-suite.log`.
- `llvm_verify_<target>` passed for all six targets in the host suite.
- The docs-style checker reports no error on every `.md` file the step
  touched. It warns on line 3 of the work order, as it did before the step.

State after the push of the step, before this report:

```console
$ git log --oneline -3
7169c5ce Add the result never and the facts that end a path
457f50ae Report step ablate of the optimization facts
a4a59f37 Add the measurement of tests/bench/ablate
$ git status --short
$ git rev-parse HEAD origin/main
7169c5ce9acbdf94594033e0a1de4d2f9de29ac5
7169c5ce9acbdf94594033e0a1de4d2f9de29ac5
```
