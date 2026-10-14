# The rules of classes

The step built R101 to R120 of "Rules of 2026-10-13" in `docs/decisions.md`
in one session, on 2026-10-14. R100 waits for Eddie, and the step did not
touch it.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R101 | Keep | `matrix_classes_visibility`, `other_object` | before |
| R102 | Keep | `matrix_classes_construct_without_pub_in_its_module` | `d3b6f2f5` |
| R103 | Keep | `matrix_classes_equality` | before |
| R104 | Keep | `matrix_classes_equals_an_object_of_a_class_below` | `d3b6f2f5` |
| R105 | Change | `matrix_classes_equals_with_a_module_operator_that_gives_an_int_refused`, `matrix_classes_equals_with_a_module_operator_over_values_with_a_mutex_refused`, `matrix_classes_operator_eq_of_a_body_gives_an_int_refused`, `error_sync_pairs` | `03ea2f13`, `b9c8595d` |
| R106 | Keep | `matrix_classes_equals_passes_over_an_operator_whose_hook_the_argument_lacks`, `..._whose_interface_the_argument_lacks` | before |
| R107 | Keep | `matrix_classes_equal_over_a_concurrent_part_with_its_own_operator` | before |
| R108 | Keep | `matrix_values_equal_of_a_struct_whose_part_has_a_mixed_operator_refused` | before |
| R109 | Change | `matrix_classes_singleton_construct_with_parameters_refused` | `b5d1f2c9` |
| R110 | Change | `matrix_classes_singleton_copied_by_dup_refused`, `matrix_classes_singleton_copied_by_a_dereference_refused`, `error_modules_singleton_copied_by_dup` | `b5d1f2c9` |
| R111 | Change | `matrix_classes_singleton_inherited_by_a_class_refused`, `matrix_classes_singleton_inherited_by_a_singleton_refused`, `error_modules_singleton_as_a_base` | `b5d1f2c9` |
| R112 | Change | `matrix_classes_singleton_declares_get_refused` | `b5d1f2c9` |
| R113 | Keep | `matrix_classes_inline_field_of_a_class_with_an_inject_field_left_out_refused` | before |
| R114 | Change | `matrix_classes_construct_fails_in_each_kind_of_class`, `construct_fails_modules_release`, `construct_fails_modules_dev` | `fc97cb97` |
| R115 | Change | `matrix_classes_destroyed_hook_without_destruct`, `guide_examples` | `8ba099df`, `3bfbd85b` |
| R116 | Keep | `matrix_classes_bodies_qualified_by_two_bases_refused` | before |
| R117 | Change | `matrix_classes_concrete_function_with_self_for_one_without_refused`, `matrix_classes_concrete_function_without_self_and_one_parameter_less_refused` | `d25083ea` |
| R118 | Keep | `antic_message_provider_names_two_functions`, `singleton class console` | before |
| R119 | Keep | `antic_message_provider_names_two_functions` | before |
| R120 | Keep | `matrix_classes_changed_hook_of_a_transient_field_trace_writes` | before |

- Each test of a Change failed before the code changed. The logs are
  `cls-sing-red.log`, `cls-sing-mod-red.log`, `cls-r105-red.log` and
  `cls-r114-red.log` of `build/drive/logs/`. R115 printed `gone 0` in each
  place and R117 the old messages, both in the session.
- `d747e516` wrote four new tests in the form of `anti fmt`.

## What changed

- R105. `operator fn eq` that gives another type than `bool` is refused at
  its name. One that takes by value a class with a Mutex in its chain is
  refused at the parameter. Two programs of the matrix became refusals.
- R109 to R112. `struct type` carries `is_singleton`, and the library file
  writes it as flag 64 of a struct. Its format is version 83, and
  `scale.antl.hex`, `pick.antl.hex` and `test_modules.c` changed with it.
  `sema_singleton_type` reads the mark, so a singleton of another module
  now refuses a literal, `alloc`, `delete` and a copy there as well.
- R114. `calls_base_construct` of `lower_function.c` tested the owner of the
  called `construct`, which a function read from a library file lacks. A
  base of another module then missed its `destruct` when the class below
  failed. It tests `self` now.
- R115. `needs_teardown` counts a level that replaces `destroyed(self)`. The
  example of chapter 15 of the guide counts `copied` as live, since its
  value in a block now runs the hook.
- R117. An abstract function without `self` is refused at its name. A
  `concrete fn` that fills it gives no second message.

## Findings

No rule of the step closed or opened a finding. `tests/known-failures.txt`
holds 379 tests, as before.

## `emit_identity` and `link_identity`

Both pass, and no expected file of either changed.

## Where the task and the code differed

- R105 names a class with a Mutex. The checker counts the hidden lock of a
  synchronized class as one. `errors/sync_pairs` therefore gets the refusal
  where its operator is declared, beside the two at the use.
- R114 is a Change that R025 had built, apart from a base of another module.
  `construct_fails_modules` names the failing class through a direct import,
  since T030 refuses `alloc parts.Failing(2)`.
- R115 gives the class a teardown, so it follows the value rules of a class
  that owns something: `let b = a;` of an existing value is refused.
- R117 leaves two tests without their old subject. They keep their names
  and expect the new refusal.
- `docs/decisions.md` holds the readings under "What the rules run added",
  each `[provisional]`.

## Counts and checks

- Tests: 4862 before and 4878 after on the host, by
  `ctest --test-dir build/host -N`. The sanitizer trees hold 4876.
- Host suite: every test passed, 379 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 389 s and
  `-I 2001,4878` in 246 s, in `cls-host-suite-a.log` and
  `cls-host-suite-b.log`.
- ASan: 4876 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4876`, in
  285 s, 381 s and 338 s, all passed, in `cls-asan-suite-a.log` to
  `cls-asan-suite-c.log`.
- UBSan: the same three parts in 255 s, 367 s and 271 s, all passed, in
  `cls-ubsan-suite-a.log` to `cls-ubsan-suite-c.log`.
- The three trees built with zero warnings, in `cls-host-build-final.log`,
  `cls-asan-build.log` and `cls-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new tests.

## Proof

`git log --oneline -3` after the push of the step:

```text
36c1c2bf Record the step of the rules of classes
d747e516 Write the tests of the rules of classes in the canonical form
b9c8595d Expect the refusal of an operator over a synchronized class value, by R105
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `36c1c2bffb3620a94c7c405656172fefb506a21f` twice.
