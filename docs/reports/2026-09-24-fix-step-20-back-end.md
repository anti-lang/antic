# Fix step 20, the minor findings of the antic back end

Commits `f7a3de0` to `155eed5` on `fix/mback`. Logs are under
`build/drive/logs/`, named `mback-*`. The decisions are in
`docs/decisions-mback.md`.

## Findings

The rows of the minor table whose places lie in the back end, named by
rule and place as the summary gives them.

- Rule 1, `text.h:17`. Fixed for the back end. `attributes.h` holds
  `ATTRIBUTE_PRINTF`, which `text.h`, `ir.h`, `select.c`, `layout.c` and
  `ir_verify.c` use. `diagnostic.h`, `sema.c` and `antl.c` belong to the
  front end.
- Rule 3, `x86_64.c:367`. Fixed. `arith_signed`, `arith_shift_right` and
  `arith_to_f32` replace the casts in both back ends, `select.c`,
  `optimize.c`, `arith.c`, `ir_print.c`, `coff.c` and `layout.c`, where a
  negative value was also shifted right.
- Rule 3, `lower.c:1710`. Fixed. `array_agg` builds the element before
  the length symbol.
- Rule 5, `ir.c:19`. Fixed. Every product that reaches an allocation goes
  through `ir_alloc`, `ir_resize` or `ir_product`. `whole.c:901` counts
  the function lists in memory, not item 10 of a library's descriptor.
  `emit.c:277` no longer wraps.
- Rule 6, `regalloc.c:852`. Fixed. The saved registers have room for the
  whole register set, and `select_emit` refuses a fifth operand.
  `whole.c` allocates its fields, and the linker's argv grows and checks
  its strings. The verifier refuses a variadic aggregate.
- Rule 9, `select.c:231`. Fixed. `ir_vformat` marks a cut message, the
  COFF join checks `snprintf`, and the narrowing check of `lower.c`
  builds its text in a `struct text`. The two places in `header.c` became
  texts in step 18.
- Rule 11, `lower.c:336`. Fixed. The comments name the owner.
- Rule 12, `lower.c:1852`. Fixed, `select_module` included, and
  `struct_global` no longer frees the name it hands out.
- Rule 14, `optimize.c:747`. Fixed. `deep_chain` of `test_optimize.c`
  crashed with 200000 blocks before the worklist (`mback-unit-old.log`).
  `emit.c:196` refuses two overlapping addresses, which
  `data_relocation_overlap` checks.
- Rule 16, `regalloc.c:469`. Fixed. Positions and slots are `int64_t`,
  the counters and table indices of `lower.c` `size_t`, `ir.c:348` and
  `mach.c` check their 32-bit indices, and the COFF join refuses 4 GiB.
- Rule 18 and rule 19. Left. Step 21 does the splits.
- Rule 20, `select.c:76`. Fixed, `ir_print.c` and `ir.c` included.
  `select_fail` is gone.
- Rule 24, `lower.c:7126`. Fixed, but for `lower.c:1441`, which writes a
  `const` type into `const_value.type`. Its type is in `sema.h`, outside
  the step.
- Rule 25, `optimize.h`. Left. The summary gives rule 25 to the session
  that crosses the boundary, and the renames reach `driver.c`.
- Rule 26, `ir.c:21`. Fixed for the back end. The five `allocate` and the
  hand-grown arrays are gone, and the 50 exits share one function.
- Rule 26, `lower.c:113`. Fixed. The dead `failed` and `diags`, the
  declarations, `<stdarg.h>`, the save of `l->b` and the `memset` are
  gone, and `ir_function_free` resets the capacities.
- Rule 26, `lower.c:498`. Fixed. In `lower.c`: the signatures, the three
  array aggregates, the six global searches, the thunks, the packing and
  `HANDLE_FATAL`. Elsewhere: the three blocks of `emit.c`, the guard of
  `header.c`, the class frees, `InjectSlot` and the local `codeview`.
- Rule 26, `x86_64.c:364`. Fixed in part. `mach.c` holds `widened`,
  `memory_at`, `cond`, `block` and `append`, and `match_copy` and the
  hand-built call are gone. `emit_memcopy`, `jump`, `resolve_slot`,
  `copy_memory`, `call_c`, `is_float_register` and `copy_argument`
  differ in their opcodes or their registers. Sharing them needs a hook
  per target in `target_desc`, which is more than a minor fix.
- Rule 27, `lower.c:1674`. Fixed, with every place the finding lists, and
  `arm64.c` `conditional`.

## Gates

- Host: 837 of 837 passed (`mback-ctest-host.log`).
- ASan: 836 of 836 passed (`mback-ctest-asan.log`).
- UBSan: 836 of 836 passed (`mback-ctest-ubsan.log`).
- Build: no compiler warnings. The linker's note on `libLTO.dylib` was
  there before.
- Docs style: nothing on the changed files.

## Note

While profiling the slow first version of `deep_chain`, a `pkill` of
`antic_unit_tests` ended every process of that name on the machine. It
may have ended a unit test run of another lane, which then needs
repeating if it failed at that time.
