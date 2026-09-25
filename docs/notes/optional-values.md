# Optional values

The choices of the passes for `?T` of any type. "Optional values" in
`docs/anti-language-additions.md` holds the rules, and the entries under
"Generics and collections" in `docs/decisions.md` hold the decisions.

## Types

- `TYPE_OPTIONAL` is the kind of a `?T` of a value. Its `element` is the T,
  and its two fields `value` and `has` are the struct C lays out, so the
  passes after the checker see a struct. `type_has_fields` holds for it.
- `types_with_none` gives the `?*T` of a `*T`, the `?fn(...)` of a function
  type and the `?T` of anything else, a type that may be `none` among them.
  `types_without_none` gives the T back.
- The parser reads `?` before any type as `TYPEX_OPTIONAL`, apart from `?*`,
  which is one token, and `?fn`. `??T` is two of them.

## Checker

- `sema_require` records `to_optional` on an expression of type T that
  stands where a `?T` is expected. The type of the expression stays T. A
  `?T` where a T is expected is the error of a value that may be `none`.
- A literal where a `?T` is expected is checked against T, which
  `value_expected` of `sema_expr.c` decides.
- Narrowing needs nothing new, since `type_is_nullable` holds for a `?T`. A
  narrowed name has type T. A comparison with `none` and a test read the whole
  variable, which `sema_whole_optional` puts back.
- `if let` on any type that may be `none` is the rewrite that `if let` on a
  match had, now `sema_if_let_none`.
- A copy of a generic transforms `to_optional` with the other types. Where a
  pointer takes the place of T, it is a `?*U`, and lowering writes nothing.

## Lowering

- The value of a `?T` lies at offset 0, so the address of a `?T` is the
  address of its value. A narrowed name reads its value there, and
  `lower_place` keys the storage of a name on its declared type.
- `lower_expr` and `lower_build_into` write the value and set the flag of an
  expression with `to_optional`. `lower_address` never does, since it gives
  the address of the value of the expression's own type.
- `lower_optional_flag` reads the flag and `lower_set_optional` writes it,
  into each `?T` nested at offset 0 when the value is written there.
- `let v = o else { }` and the guards of `catch` build `o` in a slot of its
  own, test the flag and copy the value into `v` on the path where it is
  there.
- A local, a field or an assigned place of a `?T` of a class value that needs
  the teardown tests the flag before the teardown. `lower_clear_tables`
  clears the flag of an out place of that type.
