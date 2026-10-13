# The record step of the rules of 2026-10-13

The step wrote the rules of 2026-10-13 into the documents and found the
test of every Keep rule. It built no rule in code, and no file under `src/`
changed.

## What the step did

- `docs/decisions.md` has the section "Rules of 2026-10-13". It holds R001
  to R197 in the order of the summary of the test run. Each is a Keep, a
  Change or a case that waits for Eddie. The four answers to the review
  follow as Q1 to Q4. Commit `8df5147f`.
- The ten cases that wait stand under "Open" with their ids: R009, R017,
  R037, R051, R054, R066, R074, R083, R100 and R134. Each states what antic
  does today and the question.
- Eight older entries name the rule that replaces them. Three are the
  entries on `alloc(T, n)`, on variadic arguments and on the lanes of a
  simd struct, for R050, R128 and R062. Three are those on a pointer into
  the fields, on the chain of path dependencies and on an array literal,
  for R094, R165 and R079. Decision 19 names R001 and R002, and decision
  26 names R126.
- `docs/audit/tests-findings.md` holds the four answers after the sentence
  on two grades, and T043 is `severe`. Commit `774e7b4a`.
- Eleven Keep rules had no test, or a test of one part. Eight matrix files
  and three CMake tests hold them now, 21 tests in all. Commits `e3953709`
  and `ca9c1c72`.
- `CLAUDE.md` holds the new counts and the suite times of the step.

## The Keep rules and their tests

A name `matrix_<name>` without `_refused` stands for the three tests
`matrix_<name>_dev`, `matrix_<name>_release` and
`matrix_<name>_memory_checks`. A test marked new came with commit
`e3953709`, or with `ca9c1c72` for `plugin_module_not_imported`,
`antic_help_among_arguments` and `antic_option_checks_last_decides`. Every
other test stood before the step, so its rule has no commit.

| Rule | Tests |
|---|---|
| R001 | `matrix_flow_break`, `matrix_flow_continue` |
| R004 | `matrix_flow_defer_closure_two_exits` |
| R006 | `matrix_errors_values_before_the_defers`, `matrix_flow_yield_in_loop_in_handler` |
| R007 | `matrix_flow_defer_that_never_returns_before_a_plain_return`, `matrix_flow_defer_that_never_returns_before_a_return_of_a_value` |
| R008 | `matrix_flow_for_range_no_binding` |
| R011 | `matrix_types_generic_walk_in_place_over_a_copying_value_refused` |
| R013 | `matrix_errors_fail`, `matrix_flow_fail_in_worker_dispatched_refused`, `matrix_flow_try_in_worker_parallel_refused` |
| R018 | `matrix_errors_never_fails_with_a_fail_in_a_closure_refused` |
| R019 | `matrix_errors_origins` |
| R021 | `matrix_errors_fatal_in_a_worker`, `matrix_errors_fatal_in_a_worker_runs_no_defer` (new) |
| R022 | `matrix_ownership_lent_values`, `matrix_errors_yield_of_a_lent_pointer_refused`, `matrix_ownership_lent_pointer_in_a_yield_refused` |
| R023 | `matrix_errors_yield_kinds` |
| R024 | `matrix_errors_handler_exits` |
| R028 | `matrix_values_const_inf_as_int_refused`, `matrix_values_const_nan_as_int_refused` |
| R035 | `matrix_types_struct_of_a_simd_struct_compared_refused` |
| R036 | `matrix_values_other_operators` |
| R038 | `matrix_ownership_lent_values`, `matrix_ownership_lent_pointer_in_an_array_repeat` (new), `matrix_ownership_lent_pointer_in_a_field_refused`, `matrix_ownership_lent_pointer_in_a_tuple_refused`, `matrix_values_lent_in_slice_literal_refused` |
| R040 | `matrix_values_variant_case_as_the_default_of_a_parameter_refused` (new) |
| R041 | `matrix_values_flags_into_names_whose_address_is_taken` |
| R042 | `matrix_values_c_wchar_from_an_integer` |
| R044 | `matrix_types_struct_whose_length_adds_to_its_own_size_refused` |
| R052 | `matrix_types_enums_and_variants` |
| R053 | `matrix_types_enums_and_variants`, `matrix_types_switch_on_one_number_twice_refused` |
| R056 | `matrix_types_enums_and_variants` |
| R057 | `matrix_types_switch_on_a_tag_with_a_number_refused`, `matrix_types_enums_and_variants` |
| R060 | `matrix_types_packed_enum_refused`, `matrix_types_packed_simd_struct_refused`, `matrix_types_packed_synchronized_class_refused`, `matrix_types_union_with_type_parameters_refused`, `matrix_types_alias_with_type_parameters_refused` |
| R063 | `error_lent_result_no_pointer`, `dump_types_lent_optional`, `matrix_types_lent_before_a_result_that_is_an_int_refused` (new) |
| R064 | `dump_types_lent_optional` |
| R069 | `matrix_types_generic_receiver_with_two_paths_to_its_interface_refused` |
| R070 | `matrix_ownership_dup_of_a_str_refused`, `matrix_ownership_dup_of_a_slice_refused`, `matrix_ownership_dup_of_a_plain_array_refused`, `matrix_ownership_dup_of_a_union_refused`, `matrix_ownership_dup_of_an_enum_refused`, `matrix_ownership_dup_of_a_channel_refused`, `matrix_ownership_dup_of_an_int_refused`, `matrix_ownership_dup_kinds`, `matrix_ownership_dup_in_a_generic_of_what_assignment_copies` (new) |
| R071 | `matrix_ownership_dup_of_a_value_that_may_be_none_refused`, `matrix_ownership_dup_kinds` |
| R072 | `matrix_classes_root`, `matrix_classes_copies` |
| R073 | `matrix_ownership_own_fields_and_parameters` |
| R076 | `matrix_ownership_values_in_a_dispatch`, `matrix_ownership_values_in_a_channel`, `matrix_threads_channel_of_pointers_refused`, `matrix_threads_worker_argument_is_a_class_with_a_pointer_refused` |
| R077 | `matrix_ownership_delete_in_a_dispatch` |
| R078 | `matrix_ownership_lent_values` |
| R080 | `matrix_ownership_values_in_an_argument` |
| R082 | `error_moved_into_no_name` |
| R085 | `error_function_not_kept`, `error_function_not_concurrent` |
| R087 | `matrix_threads_parallel_split` |
| R089 | `matrix_threads_atomic_add_past_the_highest_value` (new) |
| R091 | `matrix_threads_atomic_kinds` |
| R095 | `matrix_threads_channel_kinds`, `matrix_threads_channel_of_a_mutex` (new) |
| R096 | `matrix_threads_jobs` |
| R098 | `matrix_threads_channel_kinds` |
| R099 | `matrix_values_struct_with_a_mutex_compared_and_hashed` |
| R101 | `matrix_classes_visibility` |
| R102 | `matrix_classes_objects` |
| R103 | `matrix_classes_equality` |
| R104 | `matrix_classes_equality` |
| R106 | `matrix_classes_equals_passes_over_an_operator_whose_hook_the_argument_lacks`, `matrix_classes_equals_passes_over_an_operator_whose_interface_the_argument_lacks` |
| R107 | `matrix_classes_equal_over_a_concurrent_part_with_its_own_operator` |
| R108 | `matrix_values_equal_of_a_struct_whose_part_has_a_mixed_operator_refused` |
| R113 | `matrix_classes_injection`, `matrix_classes_inline_field_of_a_class_with_an_inject_field_left_out_refused` (new) |
| R116 | `matrix_classes_bodies_qualified_by_two_bases_refused` |
| R118 | `antic_message_provider_names_two_functions` |
| R119 | `antic_message_provider_names_two_functions` |
| R120 | `matrix_classes_changed_hook_of_a_transient_field_trace_writes` |
| R121 | `matrix_modules_import_plain_and_direct_refused`, `matrix_modules_qualified_names` |
| R122 | `matrix_modules_file_clauses` |
| R123 | `matrix_modules_module_by_its_whole_path_refused`, `matrix_modules_std_aliases` |
| R124 | `error_modules_module_of_a_dependency` |
| R133 | `plugin_module_not_imported` (new) |
| R135 | `antic_message_plugin_name_with_both_quotes` |
| R142 | `antic_help_among_arguments` (new) |
| R143 | `antic_option_lib_without_a_value`, `error_modules_library_kind` |
| R144 | `antic_option_checks_last_decides` (new) |
| R146 | `error_lang_error_origin` |
| R148 | `antic_message_library_of_the_module_compiled` |
| R154 | `unit_link` |
| R161 | `anti_option_check_root_library`, `anti_option_check_root_source` |
| R162 | `anti_build_message_repository_refuses`, `anti_message_sdk_export_no_tar` |
| R169 | `anti_build_message_memory_checks_lines_that_are_no_frames` |
| R173 | `anti_bind_api_types`, `anti_bind_api_callbacks` |
| R175 | `anti_bind_forms` |
| R176 | `anti_bind_forms`, `anti_bind_functions` |
| R177 | `anti_bind_api_parts` |
| R181 | `fmt_canonical` |
| R185 | `anti_symbols_message_check_build_id_with_another_letter` |
| R186 | `anti_option_symbols_target` |
| R190 | `matrix_flow_return_value_in_defer_refused` of T001, `matrix_flow_yield_in_defer_refused` of T002 |
| R191 | `matrix_values_index_past_end_refused` of T016, `check_slice_range` of T017 |
| R192 | `matrix_errors_handler_reaches_its_end_in_a_let_refused` of T044, `matrix_errors_try_on_an_optional_call_refused` of T045, `matrix_errors_try_on_an_optional_name_refused` of T046, `matrix_threads_join_twice_refused` of T053 |
| R193 | `matrix_values_as_enum_in_constant_refused` of T010, `matrix_values_const_fn_refused` of T014, `matrix_types_enum_value_from_size_of_refused` of T021, `matrix_types_is_case_on_a_number_refused` of T026, `matrix_types_enum_unknown_value_refused` of T027, `matrix_ownership_none_or_of_none_at_module_level_refused` of T028 |
| R194 | `matrix_ownership_yield_copies_a_class_with_an_own_field_refused` of T038 |
| R195 | `matrix_threads_atomic_field_of_f64_refused` of T059, `matrix_threads_atomic_add_on_a_bool_refused` of T060, `matrix_classes_mutable_field_of_a_plain_class_refused` of T065, `matrix_classes_equal_pointers_of_unrelated_classes_refused` of T066, `matrix_classes_equals_replaced_without_hash_refused` of T067, `matrix_classes_static_field_through_self_refused` of T077, `matrix_classes_literal_of_a_type_parameter_refused` of T080 |
| R196 | `matrix_threads_channel_of_a_type_parameter_that_is_a_pointer_refused` of T058, `matrix_threads_pointer_into_fields_in_a_tuple_refused` of T062 |
| R197 | `error_modules_member_protected_static_function` of T086, `error_modules_import_cycle` of T088, `matrix_modules_extern_function_takes_a_str_refused` of T089, `matrix_modules_count_of_a_list_assigned_refused` of T090, `error_modules_type_nested_in_a_generic_class` of T095 |

Notes on single rows:

- R013, R060, R063, R082, R087 and R133 are a Keep with a change of one
  part. The tests hold the part that stays. The step that builds the change
  rewrites the expected text.
- R063: the parser refuses `-> lent int` with `` expected `{` `` today, and
  the new test holds that text until R063 moves the refusal to the checker.
- R069 and R071: the refusals expect one message. Each is an expected
  failure for a second message, T113 and T035. The first message is the
  rule.
- R087: `matrix_threads_parallel_split` holds a count below zero as well,
  which R087 changes.
- R133: `plugin_module_not_imported` runs on macOS and Linux. On Windows a
  plugin links against the import library of its host, so the same lack
  stops the link of the library. No Windows host ran it.
- R181: `fmt_canonical` holds that `anti fmt` leaves the sources of `tests/`
  as they are, and `tests/matrix/values_kinds_in_literals.anti` writes
  `[2] fn(int) -> int`.
- R190 to R197: each test is an expected failure of its finding. It holds
  the wording, and antic gives the message once the finding is fixed.

## Findings closed

None. The step built no rule.

## Where the task and the code differed

- R032 names a wrapping right shift. Anti has `<<%` and no such operator,
  so the rule stands for `<<`, `>>` and `<<%`, and its entry says so.
- The summary says a test holds every case unless its line says otherwise.
  Eleven Keep rules had none, or one for a part. R040, R089, R133, R142
  and R144 had none. R021 lacked the `defer`, R038 the array repeat and
  R063 the form `-> lent int`. R070 lacked five kinds in a generic, R095
  the Mutex and R113 the refusal. Only the line of R144 said so.
- R053 says that R010 refuses two arms that name two values of one number.
  antic refuses them today: `matrix_types_switch_on_one_number_twice_refused`
  holds it. R010 adds the arms written as `size_of(i64)` and `8`.
- R040 describes the message. Its text today is `a variant is not a
  constant expression`, and the new test holds it.
- T056 expects the value of a constant that holds a Mutex. R047 refuses
  such a constant, so the step that builds R047 changes that test.
- `CLAUDE.md` asks that a decision that changes the language goes into the
  specifications and the overview in the commit of its entry. The task has
  each document change with the step that builds its rule. This step
  changed `docs/decisions.md` alone, so the overview still states what
  antic does.
- `CLAUDE.md` asks for a report under two pages. The table of 88 rows is
  longer, since the task asks for every Keep rule with its test.

## Questions

- R062 takes lanes of integer and float types alone and names `char` as
  refused. The entry under "Simd structs" lists `bool` as a lane as well.
  Does R062 refuse a `bool` lane?

## Counts and checks

- Tests: 4650 before and 4671 after, by `ctest --preset host -N`.
- Host suite: 4671 of 4671 passed, 381 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 384 s,
  `-I 2001,4669` in 228 s and `-I 4670,4671`. The logs are
  `build/drive/logs/rec-host-1.log` to `rec-host-3.log`.
- ASan: 4669 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4669`, in
  301 s, 388 s and 318 s. Every part passed, in `rec-asan-1.log` to
  `rec-asan-3.log`.
- UBSan: the same three parts in 245 s, 349 s and 236 s, all passed, in
  `rec-ubsan-1.log` to `rec-ubsan-3.log`.
- A part counts the tests that set up its fixtures as well, so the parts
  of a sanitizer suite add up to 4700 runs of 4669 tests.
- The three trees built with zero warnings, in `rec-host-build.log`,
  `rec-asan-build.log` and `rec-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- The docs-style checker reports `0 error(s), 0 warning(s)` on
  `docs/decisions.md`, `docs/audit/tests-findings.md`, `CLAUDE.md` and this
  report.

## Not done

- Neither VM ran the suite.
- `docs/notes/test-matrix.md` names none of the eight new matrix files.
  They hold rules and no cell of the matrix.
- The search for the test of a Keep rule read the header comment of each
  candidate and, for the doubtful ones, its body. It ran no test with the
  rule broken.

## Proof

`git log --oneline -3` before the commit of this report and of `CLAUDE.md`:

```text
ca9c1c72 Hold R133, R142 and R144 of the rules of 2026-10-13
e3953709 Hold eight Keep rules of 2026-10-13 that no test held
774e7b4a Write the answers to the review into the findings and grade T043 severe
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds `git log --oneline -3`, `git status --short` and
`git rev-parse HEAD origin/main` after the push, since the hash of this
commit is not known inside it.
