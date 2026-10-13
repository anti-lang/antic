# The rules of values and of constants, R026 to R047

The first session of the step built eleven of the thirteen Change rules of
"Values and constants" under "Rules of 2026-10-13" in `docs/decisions.md`.
R043 is built in half, and R045 is not built. The eight Keep rules have
their tests. R037 waits for Eddie, and nothing of it changed.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`. A Keep rule has no commit.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R026 | Change | `matrix_values_constant_as_past_u8_refused`, `matrix_values_constant_as_below_zero_to_u32_refused`, `matrix_values_constant_as_past_i8_from_u8_refused`, `matrix_values_constant_as_below_i16_refused`, `matrix_values_constant_as_at_the_ends`, `matrix_values_conversion_kinds` | `ce2a0d82` |
| R027 | Change | `matrix_values_constant_surrogate_as_char_refused`, `matrix_values_constant_past_the_last_char_refused`, `matrix_values_constant_as_char_at_the_ends` | `ce2a0d82` |
| R028 | Keep | `matrix_values_const_inf_as_int_refused`, `matrix_values_const_nan_as_int_refused` | |
| R029 | Change | `matrix_values_constant_sum_past_its_type_refused`, `matrix_values_constant_difference_below_zero_of_u8_refused`, `matrix_values_constant_product_past_i8_refused`, `matrix_values_constant_arithmetic_at_the_ends`, `matrix_values_arith_kinds`, `unit_sema` | `8c7e20c0` |
| R030 | Change | `matrix_values_index_below_zero_refused`, `matrix_values_index_of_a_constant_below_zero_refused`, `matrix_types_array_field_index_below_zero_refused`, `matrix_types_array_of_size_of_length_index_below_zero_refused` | `867596bc` |
| R031 | Change | `matrix_values_constant_minus_of_the_least_refused`, `matrix_values_constant_least_remainder_by_minus_one_refused`, `check_overflow_divide_least`, `check_overflow_remainder_least_i8`, `check_overflow_divide_least_compound`, `check_overflow_negate_least`, `check_overflow_negate_least_i8` | `8c7e20c0`, `4eb8c88a` |
| R032 | Change | `matrix_values_constant_wrapping_shift_by_negative_refused`, `matrix_values_constant_shift_left_by_negative_refused`, `check_shift_wrap_negative`, `check_shift_wrap_negative_compound` | `8c7e20c0`, `4eb8c88a` |
| R033 | Change | `matrix_values_const_len_of_str`, `matrix_values_const_channel_refused` | `922627a5` |
| R034 | Change, a slice alone | `matrix_values_operator_eq_on_a_slice_refused`, `matrix_values_operator_eq_on_a_pointer_to_a_slice_refused`, `matrix_values_equal_on_a_union_with_an_operator` | `dfdebe8d` |
| R035 | Keep | `matrix_types_struct_of_a_simd_struct_compared_refused` | |
| R036 | Keep | `matrix_values_other_operators` | |
| R037 | Eddie | untouched | |
| R038 | Keep | `matrix_ownership_lent_values`, `matrix_ownership_lent_pointer_in_an_array_repeat`, and the three refusals of the record | |
| R039 | Change | `matrix_values_assign_concurrent_class_refused`, `matrix_values_let_copies_a_concurrent_class_refused`, `matrix_values_assign_through_a_pointer_copies_a_concurrent_class_refused`, `matrix_values_assign_a_struct_that_holds_a_concurrent_class_refused`, `matrix_values_concurrent_class_takes_a_fresh_value` | `b107678c` |
| R040 | Keep | `matrix_values_variant_case_as_the_default_of_a_parameter_refused` | |
| R041 | Keep | `matrix_values_flags_into_names_whose_address_is_taken` | |
| R042 | Keep | `matrix_values_c_wchar_from_an_integer` | |
| R043 | Change, the field alone | `matrix_classes_field_length_from_size_of` | `4b040731` |
| R044 | Keep | `matrix_types_struct_whose_length_adds_to_its_own_size_refused` | |
| R045 | Change, not built | `matrix_classes_default_repeats_over_a_constant_parameter_refused` holds the old refusal | |
| R046 | Change | `matrix_types_length_with_a_size_negated_twice_refused`, `dump_types_symbolic_operators` | `371cf899` |
| R047 | Change | `matrix_threads_const_of_a_struct_with_a_mutex_refused`, `matrix_threads_const_of_an_array_of_mutexes_refused`, `matrix_threads_mutex_new_at_module_level_refused` | `12292e11` |

## Findings

- T056 is closed by R047. antic crashed on a read of a constant of a
  struct with a Mutex, and such a constant is refused now. Its three lines
  left `tests/known-failures.txt`.
- T125 is new. The default of a field that names a constant parameter of
  its class, `pub n: int = N + 1`, is 41 in `Row<3>` and in `Row<5>`. A
  copy takes the defaults of the generic as they stand.
  `matrix_classes_default_names_a_constant_parameter` shows it.
- The list holds 376 tests, as before: three left and three came.
- T016 stays open. R030 refuses the index below zero alone, with the
  message the tests of T016 expect for an index past the end.

## `emit_identity` and `link_identity`

- R031 and R032 changed 168 digests in 28 programs. The text of a check
  takes a number among the constants of its module, also in a build that
  drops the check. With those numbers taken out, the LLVM text of each of
  the 28 is the same as before, compared for linux-x86_64.
- R029 and R032 changed the source of `programs/wrapping` and
  `programs/compound_wrapping`, which held a constant and a shift the
  rules refuse. Their digests are among the 168.
- `link_identity_macos-arm64` passes with its expected file unchanged.

## Where the task and the code differed

- R034 names a union, "since `==` on those is refused". That holds for a
  union without the operator alone. `==` on a union that declares
  `operator fn eq` calls it, and the default `==` of a struct that holds
  the union compares through it. An entry under "Files of lowering" and
  `program_eq_hash_agree` hold that. The operator on a union stays, and
  the one on a slice is refused.
- R029 holds for a `const`, a length, a default and an enum value, signed
  or unsigned. In an expression that runs, a signed operation on constant
  operands is refused and an unsigned one wraps, as "Dev-mode checks" in
  `docs/anti-language-additions.md` says. `unit_sema` held
  `let x: u8 = 250 + 10;` as taken, and still does.
- R032 names `>>%`, which is no operator. `<<` and `>>` refused a
  constant count below zero already.
- R039 names `=`. An assignment and a `let` are refused. An argument, a
  `return` and a part of a literal still copy a concurrent class that
  holds no Mutex.
- R043 says that a class field refuses a length from `size_of`. antic
  takes it, and the default alone is refused.
- A value computed from `size_of` keeps what the back end folds, in all
  of R026 to R032, since the checker knows no layout.
- `docs/decisions.md` holds each of these readings as a `[provisional]`
  entry under "What the rules run added".

## Questions

- R034: should `operator fn eq` on a union be refused, which also takes
  `==` from every struct that holds such a union?
- R029: should `let x: u8 = 250 + 10;` in a function be refused as well?
- R039: should an argument, a `return` and a part of a literal refuse an
  existing concurrent class, as they refuse a value with a Mutex?
- R031: should unary `-` on a simd struct and `%` on its lanes be checked?

## Counts and checks

- Tests: 4708 before and 4758 after, by `ctest --preset host -N`.
- Host suite: every test passed, 376 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 378 s
  and `-I 2001,4758` in 233 s, in `build/drive/logs/rv-host-1.log` and
  `rv-host-2.log`.
- ASan: 4756 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4756`, in
  266 s, 368 s and 322 s. Every part passed, in `rv-asan-1.log` to
  `rv-asan-3.log`.
- UBSan: the same three parts in 246 s, 347 s and 240 s, all passed, in
  `rv-ubsan-1.log` to `rv-ubsan-3.log`.
- The three trees built with zero warnings, in `rv-host-build-final.log`,
  `rv-asan-build.log` and `rv-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.
- The host suite ran whole after R026, after R029 to R032 and after R039.
  The other commits ran the tests of their rule and the guards.

## Not done

- Neither VM ran the suite, so no Linux or Windows host compared its LLVM
  text with the new manifest.
- No test holds R030 on an array reached through a pointer.
- No test holds R039 on a tuple or a variant with a concurrent class.

## Proof

`git log --oneline -3` before the commit of this report:

```text
4b040731 Hold a size_of length in a class field, and record finding T125
12292e11 Refuse a constant that holds a Mutex, by R047
371cf899 Print the parentheses a unary minus needs, by R046
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds `git log --oneline -3`, `git status --short`
and `git rev-parse HEAD origin/main` after the push, since the hash of
this commit is not known inside it.

## Left for the next session

- R043, the default: `pub cells: [size_of(P)]int = [3; size_of(P)]` in a
  class. The checker refuses the length in the default, and a constant
  has no form for a repeated array whose length the target decides.
- R045: `pub cells: [N]int = [0; N]` in `class Row<N: int>`. A first
  version skipped the constant in the generic. The copy then lowered the
  default of the generic and crashed, so it was taken out.
- Both need one thing: a constant that holds the element once, for an
  array type with a symbolic length. A copy of a generic expands it when
  its length is a number, lowering writes a loop for a length from
  `size_of`, and the library file carries it, so a class of another
  module keeps its default. `fill` of `src/antic/sema_generic.c`,
  `const_repeat` of `sema_const.c`, `lower_store_constant` and the
  reader and the writer of the library file are the places. T125 stands
  beside it and is the fix run's.
