# The rules of values and of constants, the second session

The first session left R043 in half and R045 open, in
`docs/reports/2026-10-14-rules-values.md`. This session built both, so
the step holds every rule of R026 to R047 but R037, which waits for
Eddie. The table of the first report stands for the other rules.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`, and `<name>_modules` for
`_release` and `_dev`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R043 | Change | `matrix_classes_default_of_a_length_from_size_of`, `default_lengths_modules`, `matrix_classes_field_length_from_size_of`, `matrix_classes_default_indexes_a_repeated_array_of_a_size_of_length_refused` | `6ebb793f` |
| R045 | Change | `matrix_classes_default_repeats_over_a_constant_parameter`, `generic_defaults_modules`, `matrix_classes_default_names_a_constant_parameter` | `6ebb793f` |
| R030 | Change, built before | `matrix_values_index_below_zero_through_a_pointer_refused` | `14806fb6` |
| R039 | Change, built before | `matrix_values_assign_a_tuple_that_holds_a_concurrent_class_refused`, `matrix_values_assign_a_variant_that_holds_a_concurrent_class_refused` | `14806fb6` |

- `matrix/classes_default_repeats_over_a_constant_parameter` held the old
  refusal as an `.err` file. It is a program now, in the commit of R045.
- Each of the ten new tests of R043 and R045 failed before the change of
  the code, in `build/drive/logs/rv2-red.log`, and passes after it, in
  `rv2-green.log`.
- The three tests of `14806fb6` close the first two lines of "Not done" of
  the first report. antic held the rule in each place, so no code changed.

## How it is built

- `[v; n]` whose `n` is computed from `size_of` or from a constant
  parameter is the constant `CONST_REPEAT`, the one value `v`. The type of
  the field gives the count.
- `const_repeat` of `src/antic/sema_const.c` makes it where a symbolic
  length may stand. `declare_fields` of `sema.c` checks and computes the
  default of a field there.
- `store_repeat` of `lower_place.c` writes it in a loop, and
  `lower_store_constant` writes a constant that holds one part by part.
- `subst_value` of `sema_generic.c` puts the arguments of a copy into the
  constant of a default that names a constant parameter. That field of
  the copy drops the expression of the generic.
- The library file carries the one value. Its format is version 82, and
  the two hex listings and `unit_modules` pin the new byte.

## Findings

- T125 is closed by R045. `N + 1` is 4 in `Row<3>` and 6 in `Row<5>`, and
  its three lines left `tests/known-failures.txt`.
- T126 is new. `pub cell: Cell<T> = Cell<T> { }` in `class Outer<T>` ends
  antic with `opt failed` or `llc failed`. The tree before this session
  does the same, with a constant parameter as well. A copy keeps the
  expression of such a default with the type of the generic.
  `matrix_classes_default_is_a_copy_of_the_parameter` shows it.
- The list holds 376 tests, as before: three left and three came.

## `emit_identity` and `link_identity`

Both pass with their expected files unchanged. A default that names no
constant parameter lowers from its expression as before.

## Where the task and the code differed

- R043 says "as in a field of a struct". A field of a struct has no
  default, so that holds for the length alone, which the first session
  built.
- A `const` of a block in a generic function, `const C: [N]int = [0; N];`,
  stays refused with the message that names `size_of`. R045 names the
  default of a field.
- A default is computed once, in the generic. R029 is not checked again
  in a copy, so `pub n: i8 = N + 100` is taken and `Row<100>` holds what
  the back end folds.
- `docs/decisions.md` holds these readings and the form of the constant
  as four `[provisional]` entries under "What the rules run added".

## Questions

- T126: should a copy take every default as a constant with its
  arguments in place? `subst_value` would then report a changed type as
  well. That changes how the defaults of every generic class lower, so
  it is left to the fix run.
- Should `[0; N]` be a constant in a `const` of a generic function too?
- The three questions of the first report on R029, R034 and R039 stand.

## Counts and checks

- Tests: 4758 before and 4774 after, by `ctest --preset host -N`.
- Host suite: every test passed, 376 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 377 s
  and `-I 2001,4774` in 234 s, in `build/drive/logs/rv2-host-1.log` and
  `rv2-host-2.log`.
- ASan: 4772 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4772`, in
  260 s, 371 s and 324 s. Every part passed, in `rv2-asan-1.log` to
  `rv2-asan-3.log`.
- UBSan: the same three parts in 245 s, 347 s and 247 s, all passed, in
  `rv2-ubsan-1.log` to `rv2-ubsan-3.log`.
- The three trees built with zero warnings, in `rv2-build-2.log` and
  `rv2-build-7.log`, `rv2-asan-build.log` and `rv2-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new
  tests.
- No test holds a default over a symbolic length in a class that C
  builds through `anti_<Class>_init`.

## Proof

`git log --oneline -3` before the commit of this report:

```text
14806fb6 Hold R030 through a pointer and R039 in a tuple and a variant
6ebb793f Take a field default over a symbolic length, by R043 and R045
d6cd828b Report the first session of the rules of values and of constants
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds `git log --oneline -3`, `git status --short`
and `git rev-parse HEAD origin/main` after the push, since the hash of
this commit is not known inside it.
