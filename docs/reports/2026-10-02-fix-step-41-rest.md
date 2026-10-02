# Fix step 41, the rest of the splits

The rest of fix step 41 of `docs/audit/summary.md`: every file of `src/`
past 3000 lines, and every function of the thresholds list where a report
judged that a split reads better. Behaviour is unchanged. Logs are under
`build/drive/logs/`, named `s41r-*`.

## Files past 3000 lines

- Rule 19, `src/antic/sema_expr.c`, 3693 lines, and `src/antic/sema_call.c`,
  3422. Fixed. They are now 2505 and 2669 lines, and no file of `src/`
  passes 3000. Three new files take the parts the structure and front-end
  reports name:

  | File | Lines | Part |
  |---|---|---|
  | `sema_operator.c` | 843 | binary operators, `in`, `??`, the operator table, the `operator fn` of a type |
  | `sema_simd.c` | 544 | simd structs: lane operators, `as` with `fixed_layout`, the built-ins |
  | `sema_thread.c` | 600 | atomics, the Mutex, channels, `sync`, workers |

  The pattern literal and `Regex.compile` moved into `sema_pattern.c`.
  `build/s41r/run_moves.py` moved the top-level chunks and exported the
  static functions a move made cross-file under `sema_`, and
  `build/s41r/verify.py` compared all 674 functions with the base, token
  by token with the new names mapped back, and found no difference
  (`s41r-verify-sema.log`). The notes that named a moved function by its
  file name the new one.

## Functions past a threshold

Rule 18. Fixed, each the split its report judged:

- Front end. `check_call` along its callee forms, with the two copies of
  `T.f(args)` and `m.T.f(args)` made one, `method_call` and
  `sema_check_field` (front-end, both audits). `sema_eval_const` with one
  function per long kind (front-end, first audit). `check_stmt` with the
  cases that carried its nest of 6 and 7 in functions (tool pass, first
  audit). `antl_read_types` with one reader per kind (tool pass).
- Lowering. `lower_stmt_kind`, `lower_for`, `lower_function_body` and
  `lower_call` (back-end).
- Back end. `class_view`, `header_write`, `link_shared_command`,
  `write_provides`, `split_slots`, `remove_unused_functions`,
  `check_singletons`, `write_slots`, `reach_calls` and
  `rematerialise_constants` (back-end and tool pass, first audit).
- Runtime. `unit_line` and `anti_rt_read_float` (rt, first audit). The
  return42 executable links to other bytes for macos-arm64, so
  `tests/link-identity/return42.macos-arm64.sha256` holds the digest this
  Mac wrote, as after earlier runtime changes.
- `anti`. `bind_read_clang`, `read_preprocessed`, `build_target`,
  `build_project`, `syms_resolve`, `emit_token` and `resolve_from_index`
  (anti-tool, both audits).

Left, with the reason:

- `whole.c` into its analysis and its writers (back-end). It has 2366
  lines, and this step takes the files past 3000 alone.
- The parameter lists the reports judged clearer as a struct: `antl_read`,
  `lower_module`, `sema_doc_warnings`, `check_run`, `doc_run`,
  `test_run`, `fn_signature`, `cache_key`, `read_level` and `put_level`.
  Each is a change of an interface and no split.
- The functions judged to follow the shape of the problem, or judged from
  their counts alone, stay as the reports left them.
- Already gone at the base: `sema_check`, `main` of `anti`,
  `resolve_frame`, `copy_field`, `reach_program` and the stub loop of
  `anti_rt_plugin_load`.

## Gates

- Zero warnings in the host, ASan and UBSan builds (`s41r-build7.log`,
  `s41r-asan-build.log`, `s41r-ubsan-build.log`).
- Host: 1459 of 1459 passed, `s41r-ctest7.log`.
- ASan: 1458 of 1458 passed, `s41r-ctest-asan.log`.
- UBSan: 1458 of 1458 passed, `s41r-ctest-ubsan.log`.
- Docs style: nothing on every touched `.md` file, `s41r-docs1.log` and
  `s41r-docs2.log`.

## Decisions

One `[provisional]` entry under "Files of the checker" in
`docs/decisions.md`. It names the three new files of the checker with the
part each holds, and the parts `sema_pattern.c`, `sema_expr.c` and
`sema_call.c` keep.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
25c67580 Split the long functions of anti the reports judged (rule 18)
177c24e3 Split unit_line and anti_rt_read_float (rule 18)
fb3414cd Split the long functions of the back end along their parts (rule 18)
$ git status --short
$ git rev-parse HEAD origin/main
25c67580d07489e2629e302df8fcd4f51fe9b07b
25c67580d07489e2629e302df8fcd4f51fe9b07b
```
