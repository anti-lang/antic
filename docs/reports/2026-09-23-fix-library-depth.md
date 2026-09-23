# Fix step 5: the depth and the verification of the library reader

Step 5 of "Fix steps" in `docs/audit/summary.md`. The changes are in `src/antic/antl.c`, `src/antic/driver.c` and `src/antic/ir_verify.c`. Each finding got its test first, and each test was watched failing before its fix.

## Findings

| ID | Result |
|---|---|
| S13 | Fixed. One limit of 256 levels covers `map_agg`, `map_sym` and the struct nesting of the type table. An on-demand chain deeper than the limit is refused. Every aggregate and symbolic value gets a height, so a chain whose entries point back is refused too. `check_nesting` computes the height of each struct, tuple and variant of the type table with a memo, and it refuses a cycle or a deeper nesting before `types_find_cycle` runs. |
| M7 | Fixed. `compile_library_file` runs `ir_verify` after `load_libraries` and stops on a failure. |
| M26 | Fixed. An out-of-range function or global fails with a message. Every result, operand and call argument is checked before `check_inst` reads it. A parameter's temporary is checked, and the target of `jump`, `branch` and `branchov` must be a block. |
| M31, library reader | The chain tests below cover the deeply nested and oversized inputs of the reader. |

## Tests

- `deep_tables` in `tests/unit/test_modules.c` writes `scale.antl` with a chain of symbolic values, of IR structs and of structs in the type table, each forward and back. A chain of 16 reads. A chain of 300000 is refused. Before the fix all six chains of 300000 were accepted and the run took 538 seconds, `build/drive/logs/s5-unit3.log`. After it the run takes 3 seconds, `build/drive/logs/s5-unit4.log`.
- `out_of_range` in `tests/unit/test_ir.c` changes one index at a time in a verified module and expects each message. It failed at `build/drive/logs/s5-unit1.log`.
- `dev_library_verify` changes the `ret` of `scale.antl` to `i32`, which the reader accepts, and expects `antic --dev` to refuse it with the verifier's message. `tests/modules/poke_byte.c` writes the copy, since a CMake script cannot write a NUL byte. Before the fix antic wrote assembly that llvm-mc refused, `build/drive/logs/s5-m7-1.log`.

## Gates

- Host: 794 of 794, `build/drive/logs/s5-host-ctest.log`.
- ASan: 793 of 793, `build/drive/logs/s5-asan-ctest.log`. UBSan: 793 of 793, `build/drive/logs/s5-ubsan-ctest.log`. Both run without `no_paths`.
- No compiler warning. Each link prints the linker note `ignoring -lto_library`, because the pinned clang ships no `libLTO.dylib`. Step 4's logs show the same note.
- The docs-style checker reports nothing on the files of this step. It reads `tests/CMakeLists.txt` as Markdown and reports 206 findings on the comments already there. The new test adds none.

## Provisional decisions

- The tables of a library file nest at most 256 levels deep. See `docs/decisions-library.md`.

## Observations

- Before the fix, reading the six chains of 300000 entries took 538 seconds. Where the time went was not measured. A wide table that nests little is not bounded by the limit and may be as slow. No finding names this.
- A type that an earlier library holds counts as height 1 in `check_nesting`, since that library's reader bounded it. A struct that nests across k libraries can reach 256 levels in each of them.
