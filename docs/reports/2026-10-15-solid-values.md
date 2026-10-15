# Solid: the findings of the area values

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_values_`, in one session on 2026-10-15. Every
one is fixed, and `tests/known-failures.txt` holds no line of a test of
the area. None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-values/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T008 | severe | `df4a817a` | The fold converted an integer to an `f64` and then to an `f32`, which rounds twice. |
| T011 | severe | `cc487e28` | A conversion of an enum read the sign of the enum type, which has none, instead of its base. |
| T009 | major | `f7232d1a` | The range check compared with the least value minus one, which an `f64` rounds back to the least value. |
| T013 | major | `0af13485` | `[v; n]` checked its value against the element type of its context and kept the value's own type. |
| T016 | major | `5acb4f9c` | The check of a constant index refused one below zero and let one past the end through. |
| T018 | major | `eaf056e8` | The fold of a field took a constant of a class for a function or a static. |
| T019 | major | `cbe179c3` | The defaults of fields are checked before the members of a class have symbols, so `Limits.MAX` took the error type silently. |
| T111 | major | `67f75fb8` | Two function types compared their lists of parameters by address, and no two types share one. |
| T010 | minor | `f49df38e` | The fold took every conversion to a type that is no number and no `char` for a pointer conversion. |
| T014 | minor | `38d63000` | The fold took every name that is no constant for a variable. |
| T015 | minor | `ef75b31c` | `is` on a value with an error resolved `Shape.Circle` as a type of a module `Shape`. |
| T109 | minor | `5c8be3cd` | The default hash checked a call of a part's hook whose declaration had its refusal already. |
| T100 | minor | `94fcb596` | The recovery of the parser took the `}` of a brace the refused statement had opened as the end of the block. |

- T100 has the row of a test of the area flow. Six of its tests are
  tests of this area, so the step fixed it, and its other eleven tests
  pass with them. Three refusals of the matrix held the second message
  the defect gave: `flow_switch_arm_not_statement`,
  `flow_switch_two_else` and `threads_select_arm_is_a_name`. They now
  hold the first alone.
- T016 closes its three tests of the area types as well. The fold of an
  index into a constant array gave its own message, which no program
  reaches any more. `values_const_index_past_end.err` now names the
  length of the array, as every other refusal of an index does.
- The test of T018 declared its constant without `pub`. T075 made such a
  constant private to its class since, so the test now marks it `pub`.
- `emit_identity` and every `link_identity_<target>` stayed as they
  were.

## Findings left open

None of the area. The step recorded no new finding.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- `n as E` of an enum is no constant expression, and its refusal names a
  conversion to an enum.
- Decision 24 holds a constant index to a length that is a number. An
  array whose length comes from `size_of` takes the bounds check of dev
  mode past its end.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 228 lines, 208 of them tests, before the
step. It holds 181 lines, 161 of them tests, after it.

- Host, `ctest -j14` in two parts: 2000 and 2976 tests pass, two
  `mimalloc_environment` tests skip, and the 161 listed tests fail as
  expected. Logs `h1.log` and `h2.log`.
- ASan: 1150, 1112 and 2724 pass, `a1.log` to `a3.log`.
- UBSan: 1150, 1112 and 2724 pass, `u1.log` to `u3.log`.
- The build of all three trees has no warning, `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- No change touched a platform layer, a link, a sysroot or a package,
  and no finding named a host, so no VM ran.
- The suites ran at `53ae89a4`. This report changes `docs/` alone.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
53ae89a4 Record the commits of the findings the step values fixed
94fcb596 Skip the braces of a refused statement when the parser recovers
5c8be3cd Give the default hash over a refused hook no second message
$ git status --short
$ git rev-parse HEAD origin/main
53ae89a4cfef1704be57127db037c373847185b2
53ae89a4cfef1704be57127db037c373847185b2
```
