# Step emit-run of the LLVM back end

Step `emit-run` of `docs/work-order-llvm-back-end.md`: antic runs opt and llc itself.
Commit `1aca3e86`. The native back end is unchanged and stays the default.

## What the step built

- `src/antic/llvm_run.c` runs `opt -passes='default<O2>'` and `llc -O2 -filetype=obj`
  in release mode, and `llc -O1` on the text alone in dev mode. `-S` gives
  `-filetype=asm`. The relocation model comes from `llvm_relocation_model`. The
  process runner is `process_run`, the one of llvm-mc and the links.
- `driver_compile_llvm` in `src/antic/driver.c` writes `<output>.ll`, finds opt and
  llc through `archive_tool`, or takes `--opt` and `--llc`, and deletes `<output>.ll`
  and `<output>.bc` unless `--keep-llvm` is given. A failed tool prints its own
  message, then `antic: opt failed` or `antic: llc failed`.
- `llvm_back_end` hands the text to `driver_run`. It writes the notice after the build
  id through `llvm_emit_licenses`, fills the export list and gives status 3 for a dev
  object of a module without `main`, as `back_end` does. The copy of the package
  header of a static library is compiled from `llvm_emit_package`.
- A program for a COFF target that loads libraries is refused until the step `flags`.

## What it tested

- `run_llvm_program.cmake` now builds with `antic --backend llvm` and no step by hand,
  and checks that `<output>.ll` and `<output>.bc` are gone. All 60
  `llvm_program_*` tests pass that way, in release and dev mode, on macos-arm64 and
  macos-x86_64. `run_clib.cmake` with `BACKEND=llvm` passes `--backend llvm` to
  antic, and the 19 `clib_llvm_*` tests pass.
- `llvm_run`: `--keep-llvm` keeps the text and the bitcode in release mode, a dev build
  writes no bitcode, and `-S` writes the assembly of llc in both modes.
  `llvm_opt_failed` and `llvm_llc_failed` check the two messages after the tool's own,
  with `--opt` naming llc and `--llc` naming llvm-mc. opt takes the options of llc
  and succeeds, so it cannot stand in for a failing llc. `llvm_coff_host` checks the
  refusal.
- Each new test failed first: `build/drive/logs/emit-run/red.log` and
  `red-full.log` show the unknown options and the old refusal, and `coff-red2.log`
  shows exit 0 without the refusal.
- A sweep by hand of the 195 programs of `tests/programs` outside `llvm_*` on
  macos-arm64 (`build/drive/logs/emit-run/sweep-release.txt`): 193 pass in release
  mode. `args` and `nullable` need their environment, as in the step `emit-memory`.
  In dev mode, with dev objects of the standard library built through the LLVM back
  end, every program that fails its `.expected` file gives the output and the status
  of the native dev build with the same objects. `nested_try` differs in the build id
  and the addresses of its trace alone (`sweep-devstd.txt`). No text of a failing
  program had to be kept.

## Provisional entries

Two, after the entries of the step `emit-wide` in `docs/decisions.md`:

- antic runs opt and llc from this step on, and the routes by hand end. The
  intermediate files take the name of the object without its suffix, and antic
  deletes them as soon as llc ends unless `--keep-llvm` is given. The package header
  goes through the same run.
- A program for a COFF target that loads libraries is refused until the step `flags`,
  since `emit_names` lists the names its link exports from native machine code.

## Gates

The three builds had no warnings. `emit_identity` and `link_identity_macos-arm64`
passed unchanged. The docs-style checker gave 0 findings on `docs/decisions.md` and
this report.

| Suite | Passed | Time | Log |
|---|---|---|---|
| host | 1566 of 1566 | 134 s | `build/drive/logs/llvm-emit-run-host.log` |
| asan | 1565 of 1565 | 545 s | `llvm-emit-run-asan-1.log`, `llvm-emit-run-asan-2.log` |
| ubsan | 1565 of 1565 | 282 s | `build/drive/logs/llvm-emit-run-ubsan.log` |

ASan ran in two parts, 780 and 796 tests. The 11 `dev_object_*` fixtures ran in both,
so 1565 distinct tests passed.

After the push of the code commit:

```text
$ git log --oneline -3
1aca3e86 Run opt and llc from antic for the LLVM back end
3d4e4670 Report step emit-wide of the LLVM back end
2b2b80b6 Run the wide programs under the LLVM back end and verify every text
$ git status --short
$ git rev-parse HEAD origin/main
1aca3e866fd8796156c0f5230ed22d769afa963c
1aca3e866fd8796156c0f5230ed22d769afa963c
```
