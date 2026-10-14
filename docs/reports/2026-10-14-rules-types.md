# The rules of types and of generics

The step built R048 to R069 of "Rules of 2026-10-13" in `docs/decisions.md`
in one session. R051, R054 and R066 wait for Eddie, and antic stays as it
is there.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R048 | Change | six refusals `matrix_types_bitfield_*_past_its_width` and `..._below_its_range`, `matrix_types_bitfield_constants_at_the_ends`, `check_bitfield_write`, `_write_signed`, `_compound`, `_literal`, `_class` | `18f90d6b` |
| R049 | Change | `matrix_types_bitfield_wraps_and_saturates_at_its_width` | `18f90d6b` |
| R050 | Change | `matrix_types_alloc_of_a_type_aligned_above_16` | `2a7b72b1` |
| R052 | Keep | `matrix_types_enum_without_values` | `70c58e6a` |
| R053 | Keep | the lines `one number twice` of `matrix_types_enums_and_variants` | before |
| R055 | Change | `matrix_types_as_from_a_float_to_an_enum_refused`, `..._float_literal_...`, `matrix_types_as_from_an_enum_to_another_enum_refused` | `b24dd083` |
| R056 | Keep | `matrix_types_variant_case_with_empty_braces` | `70c58e6a` |
| R057 | Keep | `matrix_types_switch_on_a_tag_with_a_number_refused` | before |
| R058 | Change | `matrix_types_as_with_a_question_mark_before_its_type_refused`, `..._before_a_number_type_refused`, `anti_fmt` over `fmt/loose` | `b24dd083` |
| R059 | Change | `matrix_types_as_to_a_tuple`, `matrix_types_as_to_a_channel`, `matrix_types_as_to_the_type_of_the_value`, `matrix_types_as_to_the_type_of_an_owning_value_refused` | `b24dd083` |
| R060 | Keep, new messages | `matrix_types_packed_enum_refused`, `_packed_simd_struct_`, `_packed_synchronized_class_`, `_union_with_type_parameters_`, `_alias_with_type_parameters_` | `d8b4ebea` |
| R061 | Change | `matrix_types_union_field_default_refused` | `d8b4ebea` |
| R062 | Change | `matrix_types_simd_struct_with_char_lanes_refused`, one lane in `matrix_types_nested_and_simd` | `53a0cd56` |
| R063 | Keep, one change | `matrix_types_lent_before_a_result_that_is_an_int_refused`, `matrix_types_lent_before_the_int_of_an_operator_refused`, `matrix_types_lent_as_the_name_of_a_result_type` | `0b151607` |
| R064 | Keep | `dump_types_lent_optional` | before |
| R065 | Change | `matrix_types_generic_call_with_an_int_for_a_slice_refused`, `_with_a_tuple_of_three_`, `_with_a_value_for_a_pointer_`, `matrix_types_generic_function_of_a_class_with_another_shape_refused` | `52c1a18c` |
| R067 | Change | `matrix_types_generic_operator_with_a_parameter_no_operand_gives_refused`, `..._its_result_alone_names_refused`, `matrix_types_generic_operator_whose_operands_name_its_parameters` | `2a765a8d` |
| R068 | Change | `error_copies_deep_front_end` | `3450750c` |
| R069 | Keep | `matrix_types_generic_receiver_with_two_paths_to_its_interface_refused`, a known failure of T113 | before |

- Each new test of a Change failed before the code changed. The logs
  `build/drive/logs/rt-red-48.log`, `rt-red-49.log` and `rt-red-55.log`
  hold the runs of R048, R049, R055, R058 and R059. The runs of the other
  rules went to the output of the session and left no log.
- Tests that held an old behaviour changed with their rule: the part
  `values_past_the_width` of `matrix/types_bitfields_and_unions`, the
  refusals `types_as_to_a_tuple` and `types_as_to_a_channel`, two
  conversions in `types_enums_and_variants` and `values_as_other_kinds`,
  thirteen `.err` files of R060 to R063, R065 and R067, a case of
  `unit_parser`, and
  `dump_types_char_lanes`, which is gone.

## Findings

No rule of the step closed a finding, and no finding is new. The list
holds 376 tests, as before.

## `emit_identity` and `link_identity`

- R048 changed the digests of `class_bitfields`, `eq_default` and
  `out_place_forms` in `emit_identity`, the dump of `programs/bitfields`
  and the IR of `unit_lower`. Each stores a value that is no constant into
  a bitfield, which is checked now.
- R050 changed `link_identity_macos-arm64`: the runtime gained
  `anti_rt_alloc_aligned`.

## Where the task and the code differed

- R050 on Windows. `free` of the Universal C Runtime releases the memory
  of `malloc` alone, so the two Windows targets keep `malloc` and stand
  at 16. The three tests are disabled on a Windows host. `alloc T { }` of
  such a type stays `malloc` everywhere.
- R062 says integer and float lanes alone. A `bool` lane stays: the mask
  of a comparison is a simd struct of `bool` under "Simd structs".
- R058. `?*` is one token, and `p as ?*T` is the conversion to a nullable
  pointer, which `src/std/` writes 113 times. The rule is built for a `?`
  apart from `as`.
- R059 says everywhere, and R058 refuses `o as ?int`. An alias of `?int`
  is the spelling the identity takes.
- R048 checks a field of a literal as well, and neither `deserialize` nor
  `reflect.set`.
- R063. The function of the old test is no operator, so its message is
  the one on `operator fn value`.
- R067 leaves a constant parameter unchecked.
- R068 puts every refusal of a copy under `--front-end`.
- `docs/decisions.md` holds these readings under "What the rules run
  added", each `[provisional]`.

## Questions

- R050: how does Windows get memory above 16 that the program releases:
  a `free` that knows the type, a refusal for the target, or nothing?
  Does `alloc T { }` of such a type follow the rule?
- R062: does a `bool` lane stay?
- R058: does `p as ?*T` stay the conversion to a nullable pointer?

## Counts and checks

- Tests: 4774 before and 4825 after, by `ctest --test-dir build/host -N`.
- Host suite: every test passed, 376 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 374 s
  and `-I 2001,4825` in 239 s, in `rt-host-1.log` and `rt-host-2.log`.
- ASan: 4823 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4823`, in
  266 s, 365 s and 339 s, all passed, in `rt-asan-1.log` to
  `rt-asan-3.log`.
- UBSan: the same three parts in 242 s, 346 s and 257 s, all passed, in
  `rt-ubsan-1.log` to `rt-ubsan-3.log`.
- The three trees built with zero warnings, in `rt-build-18.log`,
  `rt-asan-build.log` and `rt-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new
  tests. The cross build of the runtime compiled `anti_rt_malloc_aligned`
  of `src/rt/platform_windows.c`, and no test ran it.

## Proof

`git log --oneline -3` before the commit of this report:

```text
70c58e6a Hold an enum without values and a case with empty braces, by R052 and R056
3450750c Compile the copies of generics under --front-end, by R068
2a765a8d Refuse an operator whose type parameter no operand names, by R067
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds `git log --oneline -3`, `git status --short`
and `git rev-parse HEAD origin/main` after the push, since the hash of
this commit is not known inside it.
