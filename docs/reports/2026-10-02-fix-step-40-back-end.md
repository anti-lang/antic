# Fix step 40, the minor findings of the antic back end

Step 40 of "Fix steps" in `docs/audit/summary.md`, the session for the antic
back end: the rows of the minor table whose places lie in the IR, lowering,
the optimizer, the allocator, the two targets, the emitter, COFF, the header,
debug information, the driver and the linker. The detail is in
`docs/audit/back-end.md`, `docs/audit/tool-pass.md` and
`docs/audit/structure.md`. Logs are in `build/drive/logs/s40b-*`.

Status: green and pushed.

## Findings

| Rule or question | Result |
|---|---|
| Q2, placement, `mach.c:9` | Fixed in `0a262269`, `3d29ada8` and `cc7dbecd`. `target_desc.h` holds the target description, so `mach.c`, `regalloc.c` and `emit.c` no longer include `select.h`. The runtime names moved from `target.h` into `rt_abi.h`, and lowering includes `target.h` nowhere. Bitfield expansion moved from `layout.c` into `expand.c`. The reader refuses overlapping addresses, which the emitter checked: `damaged_relocations` failed before (`s40b-red-relocs.log`). `link_relative` is `path_relative` of `platform.c`. `target_host` moved into `platform.c` with step 26. Two bullets are not defects, see below. |
| Q2, `notice.h:6` | Fixed by step 27. |
| Q3, copy found by `<`, `whole.c:281` | Fixed in `f49282a6`: `ir_is_copy_name` is the one rule. |
| Q3, NUL rule of a source, `driver.c:114` | Fixed in `3943d07e`. The driver reads a source as bytes, and the lexer decides. `error_source_nul` failed before (`s40b-red-nul.log`). `files_read` of `src/anti/` is left to the `anti` session. |
| IR free of sizes, `lower.c:1534` | Fixed in `b64223cf` for the pattern slot and the plugin holder, through `ir_const_int`. Left: `layout_data`, see below. |
| 3, `x86_64.c:688` | Fixed in `7f017478`. `arith_divide` gives the IEEE 754 quotient, and its unit test did not compile before (`s40b-red-divide.log`). |
| 5, `ir.c:644` | Fixed in `b64223cf`. The relocation list doubles, with checked sizes. |
| 6, `regalloc.c:664` | Fixed in `422a5555`. The ARM64 offsets were fixed by step 22. |
| 9, `lower.c:450` | Fixed in `b64223cf`, `ddc299c0` and `e8b73311`. `ir_array_of` names every array aggregate. |
| 14, `optimize.c:1361` | Fixed in `93ce7de7` for COFF section numbers 0xFF00 to 0xFFFD. Two unit tests failed before (`s40b-red-coff.log`). `mark_function` became a work list with step 21. |
| 16, `ir.c:398` | Fixed in `8422bcb7` through `ir_index`. |
| 18 and 19 | Left for step 41, which takes the splits. |
| 20 and 25, `memcheck.h:43` | Fixed in `261ff475`. |
| 24, `memcheck.h:55` | Fixed in `46a522f2`. Left: one cast, see below. |
| 26, out of memory, `lower.c:1542` | Fixed by step 25. |
| 26, repeated blocks, `optimize.c:94` and `arm64.c:1779` | Fixed in `a09ca5e1`, `ee314c78`, `ddc299c0` and `44c19c0e`. The selector holds what both targets shared: bits, memory, address, float register, jump, memcopy and the setup and result of a call. |
| 26, tool pass, `lower_eq.c:375`, `lower.c:2687`, `header.c:619` | The place rule fixed in `ee314c78`. The rest was fixed by steps 17, 18 and 23. |
| 26, dead code, `lower.c:3188` | Fixed in `0356cefa` and `217ab190`. |
| 27, `lower_lowerer.h:4` | Fixed in `2d5636c0` and `217ab190`. Six pinned headers were rewritten for the vtable comment. |
| Warnings, `arm64.c:406` | Fixed in `0356cefa`. 25 sites remained. Each one in a callback whose table fixes the signature now names that table, and the `diags` of `lower_module` is gone. |
| None, CodeView name, `debug.c:440` | Fixed in `f3a1413d`. `debug_names` failed before (`s40b-red-debug.log`). |

## Not defects and left

- `select_module` lays out, folds, expands and adds the memory checks in place
  before selection. Each of those needs the layouts of the target, and
  selection then reads the same layouts, as the DESIGN at `select.c` says. The
  memory checks are split on purpose. The declaration needs the module and the
  link mode of the driver, and the instrumentation needs the sizes of the
  target.
- The archive reader of `coff.c` reads COFF import libraries and the symbol
  tables of their COFF members with the primitives of the joiner. A file of its
  own would export those internals.
- `copy_memory`, `call_c` and `copy_argument` keep one copy per target. Each
  writes instructions of its target. The shared steps around them moved into
  `select.c`.
- `layout_data` writes the bytes, size and alignment of a valued global into
  the IR. Keeping them out needs a store of laid-out data beside the module
  through the emitter, which is a design change. antic lays out once per run.
- `v->type = (struct type *)t` of `lower_place.c` stays.
  `const_value.type` is mutable because `struct symbolic` and the substitution
  of copies hand out mutable types. The front-end session left the same chain
  as a change of its own (`s40b-build17.log`).

## Provisional entries added

- The exported functions of the optimizer, the targets, the linker and the
  layouts take the prefix of their file.
- The lexer alone holds the rule on the bytes of a source.

## Gates

- Build: zero warnings on host, asan and ubsan (`s40b-final-build.log`,
  `s40b-asan-build.log`, `s40b-ubsan-build.log`).
- Host: 1381 of 1381 pass (`s40b-final-host.log`).
- ASan: 1380 of 1380 pass (`s40b-asan.log`).
- UBSan: 1380 of 1380 pass (`s40b-ubsan.log`).
- Docs style: every `.md` file touched reports nothing.

## State

Taken after the push of the code and before the commit of this report.

```console
$ git log --oneline -3
ba1a03e6 Record that the lexer holds the rule on the bytes of a source
3943d07e Leave the bytes of a source to the lexer
f49282a6 Ask one function whether a name is that of a generic copy
$ git status --short
$ git rev-parse HEAD origin/main
ba1a03e669aeb328590809d3c8d516702c0fe982
ba1a03e669aeb328590809d3c8d516702c0fe982
```
