# Step emit-memory of the LLVM back end

Step `emit-memory` of `docs/work-order-llvm-back-end.md`: memory, calls, globals,
sections and constructors. Commits `eec7e49b`, `49259a0b`, `f6b556b9` and `138fe644`.
The native back end is unchanged and stays the default.

## What the step built

- `src/antic/llvm_emit.c` translates `slot`, `load`, `store`, `ptradd`, `memcopy`,
  `addr`, `bitload` and `bitstore`. A slot is an alloca of its bytes in the entry
  block. A bitfield reads and writes its unit as `expand.c` does.
- A global that holds addresses is a packed struct of byte runs and `ptr` members. A
  global of another object is an `external global` declaration, written once per
  symbol. The datum of a copy of a generic is `weak_odr` in an object of one module.
- `llvm.global_ctors` names `patterns.start` of each module and, through the new
  option `constructor`, `anti_rt_init` of a shared library. `llvm_emit_package` and
  `llvm_emit_licenses` write the two sections of `emit.c`. The step `emit-run` calls
  them, since no build writes an object before it.
- `call` takes the signature `abi_classify` gives: coerced words through a slot of
  their size, `byval`, a pointer to a copy, a vector, `sret`, variadic C calls,
  indirect calls and calls through a table.
- `vsplat`, `vbinary` without division, `vselect`, `vreduce`, `addfl`, `subfl` and
  `flag` come from the step `emit-wide`, since `program_abi_simd`, `clib_simd` and
  `clib_flags` reach them.
- `abi.c` had a defect of the step `abi`. After nine float arguments it made an
  aggregate of integers `byval`, which clang passes in a register. `49259a0b` fixes
  it, with a unit test that failed first.

## What it tested

- `unit_llvm_emit` and `unit_abi`. Each group failed before its code:
  `build/drive/logs/llvm-emit-memory-red.log`, `-calls-red.log`, `-vec-red.log`,
  `-wide-red.log` and `-abi-red.log`.
- `llvm_program_abi_<case>_<target>_release`: `structs`, `simd`, `wchar` and
  `raymath` on macos-arm64 and macos-x86_64, against the C objects of their native
  tests. The native `program_abi_*` tests run in release mode alone. Dev mode needs
  `addov` and `mulov` of `emit-wide`.
- `clib_llvm_<case>`: all 19 cases of `clib_*` on this host, by the route of
  `run_clib.cmake` with `BACKEND=llvm`.
- `llvm_verify_<target>`: 295 texts verify per target and 163 are refused, all for
  `emit-wide`. Six more programs must be accepted.
- A mutant turned `add` into `sub`. `clib_llvm_static` and
  `llvm_program_abi_structs_macos-arm64_release` failed and `clib_static` passed
  (`build/drive/logs/llvm-emit-memory-mutant.log`).
- A sweep by hand ran 179 of the 196 programs of `tests/programs` in release mode on
  macos-arm64 with the expected output (`llvm-emit-memory-sweep1.log`). Of the other
  17, seven are refused for `emit-wide`. `args` and `nullable` pass with their
  environment. Eight need PCRE2, which the link by hand does not take.
- The text that showed the `abi.c` defect is
  `build/drive/logs/llvm-emit-memory-abi_structs-x86_64-byval-rgb.ll`.

## Provisional entries

Eleven, after the entries of the step `emit-arith` in `docs/decisions.md`:

- Natural alignment for loads and stores, and the alignment its offset allows for the
  unit of a bitfield.
- An extern global declared with the size and alignment of its IR global.
- `weak_odr` for the datum of a copy of a generic in an object of one module.
- A COFF plugin refused until the step `flags`.
- `@anti.package` kept by `@llvm.used`, and the two section writers called from
  `emit-run`.
- The function type of a call, the variadic part outside it.
- A call result converted to the type of the instruction.
- One declaration per symbol for extern globals.
- The rows of `emit-wide` this step translates, and the alignment 1 of a mask.
- `xor` of the two overflows of a sum with a carry in.
- The route by hand for libraries for C and the programs of `tests/abi`.

## Questions

1. The IR marks no access as unaligned, so a field of a `packed struct` is loaded with
   its natural alignment. LLVM may treat such an access at a misaligned address as
   undefined. Should the translation take alignment 1 for every access, or should a
   later step mark unaligned accesses?
2. The work order ors the overflows of a sum with a carry in. This step uses `xor`,
   which gives the flag of `adc`. Is that the intended flag?

## Gates

The three builds had no warnings. `emit_identity` and `link_identity_macos-arm64`
passed unchanged. The docs-style checker gave 0 findings on `docs/decisions.md` and
this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1537 of 1537 | 206 s | `build/drive/logs/llvm-emit-memory-host.log` |
| asan | 1536 of 1536 | 488 s | `llvm-emit-memory-asan-1.log`, `llvm-emit-memory-asan-2.log` |
| ubsan | 1536 of 1536 | 279 s | `build/drive/logs/llvm-emit-memory-ubsan.log` |

ASan ran in two parts, 768 and 779 tests. The 11 `dev_object_*` fixtures ran in
both, so 1536 tests passed once each.

After the push of the code commits:

```text
$ git log --oneline -3
138fe644 Run tests/abi and the clib cases under the LLVM back end
f6b556b9 Translate calls, and the simd and flag rows the C libraries need
49259a0b Check only the needed register classes in the System V classification
$ git status --short
$ git rev-parse HEAD origin/main
138fe644dffa5f3fc1c93d5a889fb53373782810
138fe644dffa5f3fc1c93d5a889fb53373782810
```
