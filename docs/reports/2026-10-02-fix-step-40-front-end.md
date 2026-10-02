# Fix step 40, the minor findings of the antic front end

Step 40 of "Fix steps" in `docs/audit/summary.md`, the session for the antic
front end: the rows of the minor table whose places lie in the lexer, the
parser, the checker, the types, the library reader, the AST dump and the
diagnostics. The detail is in `docs/audit/front-end.md` and
`docs/audit/tool-pass.md`. Logs are in `build/drive/logs/s40-*`.

Status: green and pushed.

## Findings

| Rule or question | Result |
|---|---|
| Q3, shared helpers, `sema_stmt.c:27` | Fixed by step 14: `ptrset.c` holds the three tables and `sema_new_node` stands in `sema.c`. No change here. |
| Q4, header sections, `sema_checker.h:376` | Fixed by step 14, which added the test `header_sections`. |
| Q4, the 29 wrappers, `antl.c:3631` | Fixed by step 2: `antl_io.c` holds the primitives. |
| 1, format attribute, `diagnostic.h:60` | Fixed in `78add679`. `diagnostic.h` and `antl_io.h` use `ATTRIBUTE_PRINTF`, and `add` of `diagnostic.c` carries it. |
| 3, tree reader conversions, `antl_tree.c:247` | Fixed by step 2. |
| 5, sums of `arena.c` | Fixed by step 25. |
| 6, `names[]` of `ast_dump.c:443` | Fixed in `cc02e10d`. Each table is indexed by its enumerators, a static assertion holds its length, and `name_in` gives `?` for an index it lacks. |
| 9, `diagnostic.c:31` and the `snprintf` sites | Fixed in `db054102`, `78add679`, `1b05f98b` and `7529fa33`. Every message goes through `text_vformat`, which ends a cut text in `...`. `vformat_to` and `sema_format_to` were copies of it and are gone. `shared_operator` gave up on a type name past 255 bytes, so the default `==` ran: `programs/eq_shared_long_name.anti` printed `long 0` before (`s40-red1.log`). `test_diagnostic_cut` failed before (`s40-red2.log`). |
| 11, node builders of the checker | Fixed in `21270a2d`. `antl_io.h` was fixed by step 2. |
| 14, the lexer's diagnostics without a cap | Fixed in `1b05f98b`. The lexer stops after 100 errors. `error_cap` of the unit tests failed before (`s40-red3.log`). |
| 14, `m.<digits>`, `sema_pattern.c:681` | Fixed in `30136c85`. The lexer refuses a number past u64 in a source, and a library file can hold any name. `arith_decimal` reads it with a check. Its unit test did not link before (`s40-build8.log`). |
| 14, global alignment, `CONST_NULL`, `present` byte | Fixed by steps 2 and 3. |
| 16, `sema_call.c:3392` | Fixed in `ec4bd9fd`. `errors/worker_params.anti` printed 18446744073709551615 before (`s40-w.log`, `s40-red7.log`). Counts print with `%zu`. |
| 20, `sema_shared_name` | Fixed by step 14. It is exported again, since `shared_operator` of `sema_call.c` now calls it. |
| 23, `sema_tn`, `sema.c:85` | Fixed in `3d66c3f8`. `errors/literal_type_name.anti` named `A` for `E` before. |
| 24, casts that remove `const` | Fixed in `46cf326b` for both writes through a const field of `sema_safety.c`, the type walk of `antl_tree.c`, `builds` and four more type fields of the tree, and three results of the checker. Left: eight casts, see below. |
| 25, names without the module's prefix | Fixed in `37dea845`. |
| 26, repeated blocks and dead code | Fixed in `e572114c`, `b0f45b48` and `0f46bc9b`. `antl_float_bits` went in step 2, and the out-of-memory block in step 25. |
| 27, comments above the wrong item | Fixed in `db054102` and `cc02e10d`, with eight more of the same kind in `sema_call.c`, `sema_expr.c` and `antl.c`. |
| Warnings, `(void)negative` | Fixed in `e572114c`. The parameter is gone. |
| 18 and 19 | Left for step 41, which takes the splits. |

Left under rule 24. `io_csym` of `antl_tree.c` keeps its cast: the walk adds
the record of a symbol to a table of `void *`, which reading fills, so one
path serves reading and writing. Seven casts in `sema_call.c`,
`sema_class.c`, `sema_copies.c`, `sema_export.c` and `sema_generic.c`
give a mutable type, item or value to a function that makes a type from it.
Each comes from a field the tree or the types hold as const, among them
`ifaces`, `owner` and the value of a default parameter. `types_pointer` stores its element in the type it makes. Making those
lists mutable runs through the type pool and the library reader, which is a
change of its own.

Not a defect: `sema_const.c:191` formats a float with at most 17 digits
into 40 bytes, so the result is never cut.

## Provisional entries added

- The lexer records at most 100 errors, `LEX_ERRORS_MAX`.
- The exported functions of the lexer, the parser, the module paths and the
  types take the prefix of their file.

## Gates

- Build: zero warnings on host, asan and ubsan (`s40-final-build.log`,
  `s40-asan-build.log`, `s40-ubsan-build.log`).
- Host: 1381 of 1381 pass (`s40-final-host.log`).
- ASan: 1380 of 1380 pass (`s40-asan.log`).
- UBSan: 1380 of 1380 pass (`s40-ubsan.log`).
- Docs style: every `.md` file touched reports nothing.

## State

Taken after the push of the code and before the commit of this report.

```console
$ git log --oneline -3
21270a2d Say who owns the nodes the checker builds
0f46bc9b Build a struct literal of primary through two helpers
b0f45b48 Read the prefix and the content of both string literals in one place
$ git status --short
$ git rev-parse HEAD origin/main
21270a2d9246b5611d59ac443ffc81f2857db361
21270a2d9246b5611d59ac443ffc81f2857db361
```
