# Review of chapters 1 to 11 of the developer's guide

Date: 2026-10-26, from `ba2e424e`. The step read the parts "How antic builds it" and "What it costs" of chapters 1 to 11 of `docs/site/developers-guide/` against the code, and every "Where it lives" line. It changed `.md` files under `docs/` alone, so no suite ran. The probes stand under `build/grev/`, and the logs under `build/drive/logs/grev-*`.

## Method

- Two scripts, `build/grev/wil.py` and `build/grev/wil2.py`, compared each file and each name of the 61 "Where it lives" lines with `src/`. Every file exists. Every name is defined in a file its line names, with two exceptions corrected below.
- Each claim of a build or cost part was read in the code. A probe built with the host antic of `ba2e424e` showed it, or an excerpt that `guide_examples` checks.
- `build/grev/page.sh` ran the example runner on one page after each change.

## Changes by chapter

Each line names the code that was read.

### Chapter 1

- `grow` in dev mode: `antic --dev grow.anti` alone fails to link, with `undefined symbol: _anti.io.println`. The page now names `io.o` and the status 134. Probe `build/grev/c1b`.
- The key of a library file: `<runtime>` stands for the directory of the package, `src/anti/build.c:336`.

### Chapter 2

- A NUL byte stands in a comment. `src/antic/lexer.c:854`, `:879`, `:910` and `:945` refuse it in a name and in a literal alone. Probe `build/grev/c2/nulc.anti`.
- The list of contextual words lacked eleven. `transient`, `operator`, `mutable`, `compatible`, `concurrent` and `synchronized` were missing, and so were `inject`, `guarded by`, `fatal`, `link framework` and `link linux`. `src/antic/parser_item.c:64`, `:79`, `:90`, `:168`, `:448`, `parser_decl.c:34`, `:587`, `:737` and `parser_stmt.c:329` read them.
- `--dump-opt` runs the whole-program pass and the optimizer without the step that drops checks, `dump_ir` of `src/antic/driver.c:662-680`. The drop stands in `back_end` alone, `:1102-1107`.
- Three checks of dev mode stand in the dev IR alone: `lower_load_table` of `src/antic/lower_expr.c:1258-1270`, `src/antic/lower_sync.c:204-256` and `src/antic/lower_worker.c:365-400`. The page said the two IRs after lowering are the same text.
- The archive holds the AddressSanitizer runtimes in `lib/<target>/` on macOS and windows-x86_64, and in `lib/<target>-glibc/`.
- A Windows program that hosts plugins refuses `--lto`, `src/antic/driver.c:1987-1991` and `src/antic/driver_link.c:530-534`.
- `alloc(T, n)` calls `anti_rt_alloc_aligned` always, `src/antic/lower_expr.c:2070-2095`, which calls `calloc` for zeroed elements, `src/rt/mem.c:56-79`.
- An `export fn` is `dso_local` in every build but the local export of musl, `src/antic/llvm_emit_call.c:789-840`. The linkage row names the COFF and plugin exceptions.
- Microsoft x64 passes an aggregate of 1, 2, 4 or 8 bytes by value, `src/antic/abi.c:273-286`.
- The sizes of `walk` measured again: 228,800, 261,256 and 262,440 bytes, unchanged. The page names `ba2e424e`.

### Chapter 3

- `f16` stays `i16` in the LLVM text and is cast to `half` where it converts, `src/antic/llvm_emit_function.c:489-560`.
- Three claims without evidence are gone. The first gave one instruction for an add at every width. The second gave the same code for a call bound to a local, and the third the same branches for `&&` and a nested `if`.
- The static atomic field gets the block `counter` and a dev excerpt of `atomicrmw` and `load atomic`. Probe `build/grev/c3/cnt.anti` showed one global for two equal literals.
- `==` and `<` on a `str` call `anti_rt_same_bytes` and `anti_rt_compare_bytes`, `src/rt/text.c:131-148`.
- A signed `/` and `%` carry a second dev check for the least value by -1, `src/antic/lower_check.c:155-178`. `<<%` got its row, `:179-193`.
- x86_64 divides with a branch that `llc` adds for a 32-bit `divl`. arm64 divides in eight instructions and `ret`. The mask of a shift folds into `shlx` and `lsl`. `*|` takes 13 instructions on x86_64 and 12 on arm64. `mul_high` is `mulx` at `v3`. Probe `build/grev/c3/ints.anti`.
- A flag costs one instruction where it is read, shown by the new block `flagcost`. Probe `flg.anti`.
- `u64` to float takes 5 instructions on x86_64, and float to `u64` 15. Probe `flt.anti`.
- The size of PCRE2 measured again, unchanged, at `ba2e424e`. Probes `build/grev/c3/re`.

### Chapter 4

- `text.equal` compares bytes through `memcmp` of the C library, `src/std/anti/text.anti:32-38`.

### Chapter 5

- The Windows symbol is `_A`, the length and the text of each segment, `_` and the name, `target_mangle` of `src/antic/target.c:106-135`.
- A large result goes into a slot of the caller and a `memcopy` copies it into the variable, shown by `--dump-ir` of the block `conv`.
- A debugger shows an inlined frame as `[inlined]`. The claim on stack traces is gone.
- A call into the runtime is inlined by an LTO link alone. The claim of full LTO alone is gone.
- The snapshot of the example is 39 bytes, not 38: 32 for the record, 6 for `report` and 1 for its NUL.
- `the ptr of each text is a C string, as for every str` lost its last clause, since a slice ends with no NUL.
- The release module of `fnval` unrolls the loop over `ops` and calls no function value. The page said both modes call through the pointer.

### Chapter 6

- An error makes one atomic load per hook call, and one load of the count of loaded libraries when it is made and when it is deleted. `src/rt/hooks.c:72-91`, `src/rt/loaded.c:48-51` and `:94-99`, and `src/rt/object.c:1170`.

### Chapter 7

- `if let` copies the whole `?T` into the new name, and `let ... else` copies the `?T` and then its value. The block `unwrap` shows it. `src/antic/sema_pattern.c:878-886` writes the form, and probe `build/grev/c4/iflet.anti` dumped both.

### Chapter 8

- `v as Plain` and back copy the bytes, one `memcopy` of the IR, shown by `--dump-ir` of `lanes`.

### Chapter 9

- The atomic loads of an object as in chapter 6, the heap load only for an object on the heap.
- Devirtualisation runs in release mode alone, `src/antic/whole.c:919-925`.
- A function record is 48 bytes, not 40, `src/antic/rt_abi.h:367-373`. The list holds the 16 records of the root first: `Circle.functions` of `mirror` is 864 bytes, 18 records.
- `reflect.call` compares signatures by `anti_rt_same_bytes`, not `strcmp`, `src/rt/call.c:35-44`.

### Chapter 10

- An operator on a simd struct argument is the vector operation of the simd struct, `src/antic/lower_simd.c:14-16`.

### Chapter 11

- The atomic loads of four rows as in chapter 6.
- The leak report of `--memory-checks` is that of macOS and Linux, `memcheck_leaks` of `src/antic/memcheck.c`.

### Chapter 21

The cost table repeated the atomic loads, the 40 bytes per function and the copy of `if let`. Its rows now follow the corrected sections.

## Claims checked and kept

Among them: the dev and release sizes of `walk` and its sections, the start of a program in `src/rt/start.c:117-137`, the 22 and 9 copies of `copies`, `shelf.antl` of 2871 bytes, the struct descriptor of 136 bytes, the 19 entries of a table, `ANTI_HOOK_ON_HEAP` of `src/rt/object.h:257`, the arm64 code of `length`, `twice`, `render`, `issue`, `put` and `tight_total`, the tag width of `src/antic/sema_declare.c:411`, and the snapshot runtime of `src/rt/snapshot.c`.

## Examples added

The blocks `counter` and `flagcost` of chapter 3 and `unwrap` of chapter 7, with three excerpts. The guide now holds 174 blocks and 284 excerpts.

## Older documents against the code

- `docs/anti-syntax-overview.md:785` says that `as` between a simd struct and the plain struct of its bytes is free. The IR copies the bytes. The step left the overview as it is, since the overview summarises the specifications.
- The page of chapter 2 said the dev and release IR after lowering are the same text. The code writes three checks in dev mode alone.

## Findings

No defect was recorded. One point for Eddie: the load of a packed field carries the alignment of its type. `Tight` reads its `u64` at offset 6 with `load i64 ... align 8`, which LLVM may read as a promise. No program on the six targets showed a wrong result, and no test shows a failure.

## Questions answered from the code

- Does `antic --dev` link the standard library? No. It links the objects on its command line.
- Does `--dump-opt` drop the checks of a release build? No.
- Does a dev object keep a function that nothing calls? Yes: `ir_reach` with `all` makes every function of the module a root, `src/antic/reach.c:156-190`.
- Does a stack slot carry poisoned bytes under `--memory-checks`? No, `src/antic/memcheck.c:83-87`.

## Gates

- `guide_examples` and `overview_examples` passed, 2 of 2, in `build/drive/logs/grev-examples-final.log`. The guide ran 174 blocks, 125 programs, 598 refusals and 284 excerpts. The overview ran 63 blocks, 7 programs and 16 refusals, with 9 skipped as not built. `build/drive/logs/grev-examples-counts.log` holds both counts.
- The docs-style checker reports nothing on the twelve pages, in `build/drive/logs/grev-style-all.log`, and on this report.
- `git grep` finds no TODO, FIXME or placeholder text in the twelve pages.
- No suite ran, since the step changed `.md` files under `docs/` alone.
