# Own and lent parameters

The choices of the passes for "Ownership at a call" and "Lending". The two
sections of `docs/anti-language-additions.md` hold the rules, and the entries
under "Generics and collections" in `docs/decisions.md` hold the decisions.

## Types

- `lent *T` is a `TYPE_POINTER` with `lent` set, interned beside `*T` and
  `?*T`. `types_lent` and `types_unlent` give one form from the other, and
  `types_without_none` keeps the mark. The type printer writes `lent *T`.
- A function type names the lent form among its parameters, so `fn(lent *T)`
  and `fn(*T)` are two types and nothing converts between them.
- `lent []T` is a `TYPE_SLICE` with `lent` set, and `type_is_lent` holds for
  both kinds. Every rule of the checker keys on `type_is_lent`, so a lent
  slice meets the refusals of a lent pointer. Slicing a slice gives its own
  type, so a part of a lent slice is lent.

## Parser

- `lent` is a contextual word before the name of a parameter of a named or
  an anonymous function. It stands as well before the type of a parameter
  of a function type. Followed by `,` or `)` in a type list it is a type
  name.

## Checker

- `sema_lent_form` gives a parameter marked `lent` the lent form, and
  refuses the mark on a type that is no pointer, with `own` and in an
  `extern fn`.
- `sema_require` refuses a lent pointer where a plain pointer is expected and
  compares the two forms as `*T` everywhere else. `c->lent_use` names what the
  refusal is: a store, a `return` or an argument. The object of a call of a
  class is never required, so it takes a lent pointer.
- `as` of a lent pointer gives the lent form, `dup` the plain one, and
  `delete` and `destroy` refuse it. `unify` takes the plain form, so a type
  argument is never lent.
- `note_move` reads the `own` list of the function. The error a handler binds
  moves as before. Any other local or parameter moves through
  `sema_move_local`, which refuses the four moves the text order cannot
  follow and records `moved`, `moved_to` and `moved_by` on the symbol. A name
  of a moved symbol is refused where it is resolved.
- `moves_own_param` makes `=` and `let` from an `own` parameter a move when
  its value owns memory or its type holds a type parameter. A copy of a
  generic reads those marks from its generic and is never checked again.
- `sema_declare` records the loops around every symbol, `sema_capture` marks
  a captured one, and a name in a `defer` marks every local, not the error of
  a handler alone.

## Lowering

- `lower_move_argument` copies a moved value that needs a teardown into an
  entry slot, clears the local's tables and passes the slot. A value without
  a teardown passes as it is.
- `lower_clear_moved` clears the tables of an `own` parameter after `=` or
  `let` copied it.
- `lower_push_own_action` registers the teardown of an `own` parameter at
  the start of its function. `destroy_local` tears a moved symbol and an
  `own` parameter down only when the table is not zero, the path an
  assignment takes over an unfilled place.

## Library file

- The byte after the element of a pointer type holds `?` in bit 0 and `lent`
  in bit 1. The tree of a generic carries `lent` on a parameter and a type
  expression, and the moves, the loops, the captures and `own` of a symbol.
  The format is version 64. The byte after the element of a slice type is 1
  for `lent []T`, from version 67.
