# The rules of the messages and the options of antic

The step built R136 to R154 and R190 to R197 of "Rules of 2026-10-13" in
`docs/decisions.md` in one session, on 2026-10-14. No rule of the step
waits for Eddie.

## The rules

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R136 | Change | `matrix_ownership_dup_of_a_uint_refused`, `_of_an_i64_`, `_of_a_u8_`, `_of_an_f64_`, `matrix_values_negate_uint_refused`, 23 other refusals of the matrix, 8 of `tests/errors`, `unit_sema`, `guide_examples` | `88c3585b`, `c80239a7` |
| R137 | Change | `error_enum_value_called`, `matrix_classes_constant_of_a_class_called_refused` | `1d2b2931`, `7c4fed86` |
| R138 | Change | `matrix_ownership_destroy_of_a_value_refused` | `1d2b2931` |
| R139 | Change | `error_modules_member_private_function`, `_from_a_class_below`, `error_modules_member_private_static_function`, `error_operator_access_modules`, `antl_scale`, `antl_generic`, `unit_modules` | `717ee466`, `92bd2142` |
| R140 | Change | `error_pattern_exponential_assertions`, the two programs `matrix_values_pattern_repeat_before_a_boundary_in_a_repeat` and `matrix_values_pattern_repeat_before_a_lookahead_in_a_repeat` | `4d6ceec7`, `98d66c73` |
| R141 | Change | `antic_usage_lists_options` | `334ca13e`, `3a548697` |
| R142 | Keep | `antic_help_among_arguments` | before |
| R143 | Keep | `antic_option_lib_without_a_value` and the other 21 `_without_a_value` cases, `error_modules_library_kind` | before |
| R144 | Keep | `antic_option_checks_last_decides` | before |
| R145 | Change | `antic_option_asserts_text_dropped` | `aefba39b` |
| R146 | Keep | `error_lang_error_origin` | before |
| R147 | Change | `antic_message_module_path_under_the_current_directory`, `_under_a_root_named_twice`, `_under_an_absolute_root` | `1cdd7d4a` |
| R148 | Keep | `antic_message_library_of_the_module_compiled` | before |
| R149 | Change | `antic_message_output_program_no_directory`, `antic_message_output_library_no_directory` | `1cdd7d4a`, `399cc926` |
| R150 | Change | `antic_message_library_static_with_a_soname` | `57d64cae` |
| R151 | Change | `anti_option_soname_macos`, `unit_link`, the `clib_*` tests of a shared library | `b0d9f0f1` |
| R152 | Change | `antic_option_windows_library_reproducible`, `unit_link` | `759e2bd9` |
| R153 | Change, held already | `dump_ast_test_blocks` | `ed529d7b` |
| R154 | Keep | `unit_link` | before |
| R190 to R197 | Keep | the tests the record step named, 27 of them expected failures | before |

A Change test failed before its code changed, in `msg-r136-red.log`,
`msg-r139-red.log`, `msg-r139-red2.log`, `msg-r137-red.log`,
`msg-r140-red.log`, `msg-r141-red.log`, `msg-r150-red.log`,
`msg-r145-red.log`, `msg-r147-red.log` (R147 and R149),
`msg-r151-red.log` and `msg-r152-red.log` of `build/drive/logs/`.

## What changed

- R136. The parser records which of `i64`, `u8`, `f64` and `uint` a file
  writes, and `sema_tn` names the type so.
- R137 and R138. A message each, in `sema_call.c` and `sema_expr.c`.
- R139. The library file carries the names of the private functions of
  a class, and its format is version 84.
- R140. An assertion right after the inner repeat narrows the bytes that
  may follow it, in `follow_within` of `src/antic/pattern.c`.
- R141. Seventeen lines of the usage.
- R145. A literal of a dropped failure block is no entry of the dev
  object, in `optimize_drop_failures` and `ir_reach`.
- R147. `modpath.c` compares the resolved directory of the source with
  the resolved root.
- R149. `driver_write_file` names the directory that is missing.
- R150. `library_options_usable` refuses the option.
- R151 and R152. One flag each in `linker.c`.

`docs/decisions.md` holds the readings of R136 to R140, R145, R147 and
R149 to R152 under "What the rules run added", each `[provisional]`.

## Findings

The step closed no finding and opened none. `tests/known-failures.txt`
holds 374 tests, as before.

## `emit_identity` and `link_identity`

Both passed unchanged in the three suites. R151 and R152 change the link
of a library alone, and R145 a dev build without assertions.

## Where the task and the code differed

- R153. antic printed the functions of both blocks under `--tests
  --dump-ast` at the start of the step. The test run reported none. The
  step wrote the test and changed no code.
- R136 holds per file. A file that writes `i64` and `int` names both
  `i64`, since one node of the type stands for both names.
- R149. The first form checked the directory before the source and broke
  ten refusals of `tests/errors`, which pass `-o` into a directory that
  is not there. The message now names the file antic creates,
  `none/main.ll`, and its directory, after the source.
- R137 took the one test of `` `%s` is not a function ``, which a
  constant of a class called as a function still reaches. A new
  refusal of the matrix holds it.
- R151 makes a C program that links a library of macOS name its
  directory as an rpath, which the clib runner now does.
- R190 to R197. The record step named the tests. 27 of them are
  expected failures of open findings, and their expected text is the
  wording the rules keep.

## Counts and checks

- Tests: 4910 before and 4924 after on the host. The sanitizer trees
  hold 4922.
- Host suite: every test passed, 374 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 380 s
  and `-I 2001,4924` in 137 s, in `msg-host-suite-a.log` and
  `msg-host-suite-b.log`.
- ASan: `-I 1,1150`, `-I 1151,2250` and `-I 2251,4922` in 275 s, 381 s
  and 349 s, all passed, in `msg-asan-suite-a.log` to
  `msg-asan-suite-c.log`. The parts count 1150, 1112 and 2691 with the
  setup tests of fixtures.
- UBSan: the same parts in 256 s, 360 s and 271 s, all passed, in
  `msg-ubsan-suite-a.log` to `msg-ubsan-suite-c.log`.
- The three trees built with zero warnings, in
  `msg-host-build-final.log`, `msg-asan-build.log` and
  `msg-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every
  `.md` file the session touched.

## Not done

- Neither VM ran the suite. `/Brepro` and the install name ran on the
  Mac alone, through the cross links of the tests.

## Proof

`git log --oneline -3` after the push of the step:

```text
3367d719 Record the step of the messages and the options of antic
7c4fed86 Hold the refusal of a constant of a class called as a function, after R137
399cc926 Name a missing directory where antic creates a file, after the source, by R149
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `3367d719055137b2954d678bce6e6d67cc139257` twice.
