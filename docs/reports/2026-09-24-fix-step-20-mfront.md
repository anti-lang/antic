# Fix step 20, the minor findings of the antic front end

Branch `fix/mfront`, commits `43bb75e` to the report commit. Logs are
under `build/drive/logs/`, named `mfront-*`. The places are the lexer,
the parser, the checker, the types, the library reader, the layout pass
and the driver of `src/antic/`. The decisions are in
`docs/decisions-mfront.md`.

## Findings

Rule numbers follow the minor table of `docs/audit/summary.md`.

- Rule 3. Fixed. `signed_bits` of `sema.c` and `signed_value` of
  `layout.c` compute a negative constant from its complement, the signed
  right shift uses unsigned operations, `append_byte` of `lexer.c` no
  longer converts to `char`, and the two `map_global` calls of
  `antl.c` are bound to locals.
- Rule 5. Fixed. `types_alloc_array` checks the product before the pool
  allocates, and `types.c`, `sema.c` and `antl.c` use it. Every growth by
  `realloc` in `lexer.c`, `parser.c` and `sema.c` checks its product.
- Rule 6. Fixed for `parser.c:189`: `marker_length` reads the token
  length. The `names[]` tables of `ast_dump.c` are left, outside the
  fence.
- Rule 9. Fixed in `sema.c`, `antl.c`, `layout.c`, `types.c` and
  `lexer.c:1545`. `diagnostic.c:28` is left, outside the fence.
- Rule 10. Fixed. `types_member_symbol` returns a `struct name`, the
  reader refuses a C string with a NUL inside, and `parser.c`, `sema.c`
  and `lexer.c` keep the lengths they had.
- Rule 11. Fixed. `lex`, `types_member_symbol`, `keep_name` and
  `antl_read` say who frees what, and what `antl_read` leaves behind.
- Rule 14. Fixed. The failure kind of a block is checked, and parameter
  names ask 4 bytes each of the count bound. `damaged_files` feeds both
  refusals, and each failed before its check (`mfront-unit.log`).
- Rule 16. Fixed. `A.construct` with a parameter of an unknown type no
  longer reports 18446744073709551615 arguments;
  `errors/construct_unknown.anti` failed before the fix. The case index
  and the atomic argument count are `size_t`, and the writer's counts go
  through `put_count`, which refuses more than 32 bits.
- Rule 21. Fixed. `sema.h` and `driver.h` include `<stddef.h>`.
- Rule 23. Fixed. The cache of `token_kind_name` carries its reason.
- Rule 24. Fixed. The casts before `tn` and `is_error`, and the cast of
  the checker, are gone. `starts_member`, the parameter arrays of
  `types.h` and `bit_unit` have the `const` they need.
- Rule 26, front end. Fixed. `same_name_text`, the repeated tests of
  `types.c` and `antl.c`, the six lookups of a standard struct
  (`std_item`), the shared half of `check_parallel` and
  `check_dispatch`, the three declarations of a caught name, and the dead
  code of `sema.c:3212` to `4484` with its memsets.
- Rule 1. Left. The finding asks one macro in one header per side for
  all ten places, among them `text.h`, `diagnostic.h`, `select.c` and
  `ir_verify.c`, which lie outside the fence. `format_to` of `sema.c`
  carries the attribute as `error_at` beside it does.
- Rule 25. Left. The renames reach callers across the back end, and the
  summary gives rule 25 its own session.
- Rule 26, shared code. Left. The out-of-memory block, `allocate` of
  `layout.c:13`, the UTF-8 code of `lexer.c:310` and `write_file` of
  `driver.c` belong to the session for code both sides write.
- Rules 18 and 19. Left for fix step 21.
- Rule 15. No new work beyond the two refusals above. The deep-nesting
  and malformed-library tests came with steps 1, 4 and 5.
- The six defects no rule names, at `sema.c:8637`, `8466`, `1764` and
  `11115`, `parser.c:552` and `lexer.c:963`. Left, since they are not
  rows of the minor table.

## Gates

- Host: 838 of 838 passed (`mfront-test4.log`).
- ASan: 837 of 837 passed (`mfront-asan-suite2.log`). The first run lost
  `unit` to a SIGTERM with no output after 45 seconds
  (`mfront-asan-suite.log`). It passed alone and in the full rerun, and
  nothing in the code sends that signal.
- UBSan: 837 of 837 passed (`mfront-ubsan-suite.log`).
- Build: no compiler warnings. The linker's note on a missing
  `libLTO.dylib` was there before this step.
- Docs style: nothing on every changed file (`mfront-docs.log`).
