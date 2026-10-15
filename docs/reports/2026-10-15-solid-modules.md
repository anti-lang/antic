# Solid: the findings of the area modules

The step fixed the open findings of `docs/audit/tests-findings.md` whose
tests start with `matrix_modules_`, in one session on 2026-10-15. All
four are fixed, and `tests/known-failures.txt` holds no line of a test of
the area. None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-modules/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T090 | major | `71f0f5bc` | `count` is `pub count` of `anti.collection.Collection<T>`, and nothing refused a write of a public field from another module. |
| T091 | major | `d305361e` | The parser read neither `maybe.Maybe<int>.Some { }` nor the case after `is Maybe<int>`, and the checker dropped the type arguments of a case of another module. |
| T087 | minor | `d925d552` | The recovery after `pub` stopped at the `import` the item had failed at, and an `implements` of a refused type was checked again as `<error>`. |
| T110 | minor | `b3977d26` | The second half of the message spelled `struct` for every kind of item. |

- T090: the checker refuses `=`, every compound assignment and `&` of
  `count` outside the module of the collection. A class of the program
  that inherits a collection still writes its own `count`. The new
  tests `matrix/modules_count_of_a_list_address_taken` and
  `matrix/modules_count_written_by_an_inheriting_class` hold both. The
  table of chapter 16 of the guide says who writes the field.
- T091: `maybe.Maybe<int>.Nothing` takes its arguments under the name
  of a module as well. `is` with type arguments requires the copy they
  name to be the type of the value, and the new test
  `matrix/modules_is_names_a_case_with_other_type_arguments` holds the
  message of the other case. The listed tests `generic_variant_modules_*`
  and `matrix_modules_is_names_a_case_with_its_type_arguments_*` pass.
- T087: the third case of the row, an `extern fn` in a class body,
  already gave one message when the step began.
  `error_modules_internal_interface` and `error_modules_private_interface`
  share the cause of the interface and pass.
- `emit_identity` and `link_identity` are unchanged.

## Findings left open

None of the area.

## New findings

None.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- The module of a collection is the module of any class from the type
  of the receiver up to `Collection<T>`, and every other module is
  refused `=`, a compound assignment and `&` of `count`.
- `v is Maybe<int>.Some` names a case with the type arguments written
  out, and they must name the type of `v`. A case under the name of a
  module takes its arguments in a literal and without one.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 94 lines, 74 of them tests, before the
step. It holds 75 lines, 55 of them tests, after it. 47 of them are
expected to fail on the Mac, and the other 8 name another host.

- The build of all three trees has no warning, `b7.log`, `ba.log` and
  `bu.log`.
- Host, through `ctest --preset host -j14` in two parts, 4970 tests:
  1999 of 2000 passed in `h1.log`. The one failure, `fmt_canonical`,
  refused the comment of the step's new test as `anti fmt` fills it,
  fixed in `dccb2f0d` and passed with that test in `fmt3.log`. The
  second part passed 2991, `h2.log`, which repeats 21 tests that are
  fixtures of its tests. Two `mimalloc_environment` tests skip.
- ASan, 4968 tests: 1150, 1112 and 2737 pass, two skip in the second
  part, `a1.log` to `a3.log`.
- UBSan, 4968 tests: 1150, 1112 and 2737 pass, two skip in the second
  part, `u1.log` to `u3.log`.
- No step changed a platform layer, a runtime path, a link, a sysroot or
  a package, so no VM ran.
- The docs-style checker reports nothing on every `.md` file the step
  touched.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
dccb2f0d Format the comment of the new test of is with other type arguments
402afc29 Record the commits of the findings the step modules fixed
b3977d26 Name the union in the refusal of its aligned bitfield
$ git status --short
$ git rev-parse HEAD origin/main
dccb2f0d95eeae108f2ce3f504fb7a54d9aa59a0
dccb2f0d95eeae108f2ce3f504fb7a54d9aa59a0
```
