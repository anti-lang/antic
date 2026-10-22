# Step probe-fix: the six findings of the step probe

The step fixed the six findings that the step probe recorded, severe first.
Each fix is one commit, and `797f4be6` writes the commits into the rows of
`docs/audit/tests-findings.md`. The logs are under `build/drive/logs/`,
named `pf-*`.

## Findings closed

- T183, severe, `a72b4aca`. The struct of a case of a variant had no link to
  its variant, so the substitution of a copy of a generic function left the
  binding of `Ok o` with the case of the open `Result<T, E>`. Each case
  struct now names its variant in `case_of`, and `subst_case` of
  `src/antic/sema_generic.c` takes the case of the variant's copy.
- T187, severe, `249fc070`. The type `?*Node` owns nothing by itself, so the
  check of an existing owning value never saw the `own` of the field.
  `sema_refuse_own_field_copy` of `src/antic/sema_value.c` refuses an `own`
  field read into an `own` field by `=` and by a class literal. A field given
  its own pointer, `b.one = b.one`, keeps it.
- T185, major, `6d5e4ec3`. The declaration of a class compared its functions
  with each other and never with its fields. A function named as a field
  now declares the name twice, and a call reaches the function, so the
  mistake gives one message.
- T186, major, `2ff84343`. The parser read `catch` after the value of a
  `let`, an assignment and a call statement alone. A `catch` after the `)`
  of a call now belongs to the call wherever it stands.
- T188, major, `6db3870d`. The check of a replacement took a protected or a
  private function of the base as a table entry. A name that no public
  function of the chain above declares now fills no entry. `destruct` of
  `anti.lang.Error` keeps the entry of `Object`, and a static function of
  the base keeps the message that names the `self` it lacks.
- T184, minor, `14f9af45`. The message of two fields of one name named the
  kind as `union` or `struct` alone, and now names a class `class`.

None was left.

## Found and fixed on the way

- A `catch` on a call that cannot fail and gives a `?*T` guards the
  pointer, and only a `let` takes such a handler over. Anywhere else antic
  stopped with a segmentation fault, in an assignment before this step as
  well. The commit of T186 refuses it with `this call cannot fail, and a
  catch that guards the pointer it gives stands in a let`, and
  `matrix_errors_catch_guarding_a_pointer_as_an_argument_refused` holds it.
- `tests/errors/pointer_leak.anti` named its getters after the fields they
  read, which the fix of T185 refuses. `8757628a` renames the fields, and
  the test keeps its messages.

No new finding went into the table.

## New tests

`ownership_own_pointer_field_copied_into_a_class_literal`,
`classes_concrete_over_a_private_function` and
`errors_catch_guarding_a_pointer_as_an_argument` of `tests/matrix/`, three
refusals.

## Counts

`tests/known-failures.txt` held 142 lines and 123 entries before, and holds
133 lines and 114 entries after. The Mac expects 121 tests to fail before
and 112 after.

## `[provisional]` entries

None. The refusal of a guarding `catch` outside a `let` follows the entry
on both forms of a failing call in `docs/decisions.md`.

## Emit identity

`0fcb621c` writes the digests of `programs/generic_variants.anti` on the six
targets again. Its copies of generic functions bind cases of generic
variants. Before the fix of T183 they took the size of the open case, 192
bytes for a case of 3 and 8 for a case of 16. No other program moved, and
`link_identity_macos-arm64` passes unchanged.

## Gates

- No warning of our code: `pf-host-build.log`, `pf-asan-build.log` and
  `pf-ubsan-build.log`.
- Host, 6362 tests, `-I 1,2100`, `2101,4200` and `4201,6362`: 2100, 2127
  and 2168 of 2169 passed in 288 s, 174 s and 281 s, `pf-host-1.log` to
  `-3.log`. The one failure was `emit_identity`, whose digests `0fcb621c`
  writes again. It passes after that commit, `pf-ei2.log`. The 112 expected failures fail
  as listed, and the two `mimalloc_environment` tests skip.
- ASan, 6360 tests, `-I 1,1150`, `1151,2250`, `2251,3500`, `3501,4500`,
  `4501,5400` and `5401,6360`: 1151, 1109, 1277, 1012, 907 and 960
  passed in 216 s, 194 s, 177 s, 137 s, 460 s and 345 s, `pf-asan-1.log`
  to `-6.log`.
- UBSan, 6360 tests, the same parts and the same counts, in 244 s, 177 s,
  144 s, 114 s, 175 s and 295 s, `pf-ubsan-1.log` to `-6.log`.
- The docs-style checker reports nothing on the `.md` files of the step.

## Not run on the target

Nothing. Every change is in the front end of antic, which the suite of the
Mac runs for all six targets.

## State at the push

The state after the push of `797f4be6`, before the commit of this report:

```text
$ git log --oneline -3
797f4be6 Record the commits of T183 to T188
0fcb621c Write the digests of generic_variants again after the fix of T183
14f9af45 Name a class as a class in the message of two fields of one name
$ git status --short
?? docs/reports/2026-10-22-solid-probe-fix.md
$ git rev-parse HEAD origin/main
797f4be690545327d947cbb1861c23fa860b65fe
797f4be690545327d947cbb1861c23fa860b65fe
```
