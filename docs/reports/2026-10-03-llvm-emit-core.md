# Step emit-core of the LLVM back end

Step `emit-core` of `docs/work-order-llvm-back-end.md`: the skeleton of the
translation. Commits `b2cdd790` and `22232052`. The native back end is unchanged and
stays the default.

## What the step built

- `src/antic/llvm_emit.c` and `llvm_emit.h`. `llvm_emit_module` writes the source
  name, the data layout and the triple. It gives every IR temporary an alloca in the
  entry block and every IR block an LLVM block `b<index>`. It translates `copy`,
  `jump`, `branch` and `ret`. A branch into the failure arm of an assertion or a check
  carries the weights 1 and 2000. Signatures come from `abi_classify`, with `signext`
  or `zeroext` on `i8` and `i16` parameters. Definitions carry `nounwind`, the frame
  pointer, `uwtable(sync)` and the stack probe on Windows, and `"target-cpu"` and
  `"target-features"`. The module flags are `wchar_size`, `PIC Level` and `uwtable`,
  with `!llvm.ident`.
- Every other operation is refused. The message names it, the step that adds it and
  the function: ``the LLVM back end does not translate `add` before the step
  emit-arith, in main.f``. An operand that names a global or a function, and an
  aggregate parameter or result, wait for `emit-memory`.
- `--dump-llvm` and `--backend native|llvm` in `src/antic/main.c` and `driver.h`.
  `llvm_back_end` in `src/antic/driver.c` runs `layout_data`, `layout_resolve`, the
  optimizer on folded functions and `memcheck_function` as `select_module` does, then
  the translation. `--backend llvm` without `--dump-llvm` is refused until `emit-run`.
- `ANTIC_OPT` in `tools/pinned-compiler.cmake` names opt of the pinned release once.

## What it tested

- `unit_llvm_emit`: the head and flags, constants, temporaries and cold branches. It
  also covers the attributes of three kinds of target, the linkage of the four kinds
  of object, declarations and the refusals.
- `dump_llvm_control_flow_<target>` for macos-arm64 in release mode and for
  linux-x86_64 and windows-x86_64 in dev mode. The goldens were written by hand from
  the work order and checked with opt and llc first.
- `llvm_verify_<target>`, one test per target. Each translates every program of
  `tests/dump` and `tests/programs` in release and dev mode and runs
  `opt -passes=verify` on each accepted text. `control_flow` must be accepted.
  At `22232052` 14 texts per target verify (`control_flow`, `return42`,
  `return_pieces`, `deep_nesting`, `long_float`, `switch_fallthrough`, and `asserts`
  and `mutex_size` in one mode), 440 are refused, and `format` and `generics_syntax`
  fail in the front end too. The lists stand in
  `build/host/tests/llvm_verify/<target>/`.
- `llvm_backend_no_object` and `llvm_backend_name` test the option.
- Red first. Before the code the five new tests failed on the unknown option and on
  the stub (`build/drive/logs/llvm-emit-core-red.log`, `llvm-emit-core-red2.log`).
  Then a test was broken on purpose. With `trunc` replaced by `zext` to `i16`,
  `llvm_verify` failed with the verifier's message. The text stands in
  `build/drive/logs/llvm-emit-core-mutant.ll`. With `--backend llvm` ignored,
  `llvm_backend_no_object` failed (`llvm-emit-core-mut-run.log`).
- The single test of the first draft ran twelve antic calls per program and took over
  ten minutes under ASan. Its run was stopped and the test split per target.

## Provisional entries

Five, after the entries of the step `abi` in `docs/decisions.md`:

- `--dump-llvm` runs the LLVM back end whichever back end `--backend` names, and
  `--backend llvm` alone is refused until `emit-run`.
- A function keeps the symbol of the native back end without the `_` of Mach-O, and
  its linkage follows `emit.c`, with `weak_odr` copies in a COMDAT on COFF.
- Every declaration is `nounwind`.
- `noreturn cold`, `nonnull`, `dereferenceable`, `noalias` and a result's `signext`
  or `zeroext` are left out, since the IR records nothing to derive them from.
- No global data before `emit-memory`.

## Gates

The three builds had no warnings. `emit_identity` and `link_identity_macos-arm64`
passed unchanged. The docs-style checker gave 0 findings on `docs/decisions.md` and
this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1482 of 1482 | 135 s | `build/drive/logs/llvm-emit-core-host.log` |
| asan | 1481 of 1481 | 410 s | `build/drive/logs/llvm-emit-core-asan.log` |
| ubsan | 1481 of 1481 | 260 s | `build/drive/logs/llvm-emit-core-ubsan.log` |

After the push of the code commits:

```text
$ git log --oneline -3
22232052 Add --dump-llvm, --backend and the test llvm_verify
b2cdd790 Translate control flow into LLVM IR text
aeaff42f Report step abi of the LLVM back end
$ git status --short
$ git rev-parse HEAD origin/main
22232052402a7949a68186c762a75853c4f06a73
22232052402a7949a68186c762a75853c4f06a73
```
