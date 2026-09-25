# Iteration and the language hooks

The choices of the passes for "Language hooks" and "Iteration" of
`docs/anti-language-additions.md`. The decisions stand under "Language hooks
and iteration" in `docs/decisions.md`.

## Checker

- `sema_operator_named` holds the nineteen names. `check_hook` of `sema.c`
  checks the signature of each hook, for a function of a class body and for a
  free `operator fn` of a module alike.
- `sema_hook` finds a hook on a type or on the type a `*T` points to. A
  member of the class comes first. A free function of the module counts when
  its first parameter is the type or a pointer to it.
- `sema_iterate` writes a `struct iteration` for `for` and for `to_slice`: the
  hidden local `<iterator>`, the expression that gives the iterator, and the
  calls `next()` and `value()` on the local. Both calls go through the method
  path, so an abstract `next` dispatches through the table.
- `e[i]` becomes the call `e.index(i)` in place. `e[i] = v` becomes the
  statement `e.set_index(i, v);`.
- `e[x, y]` reaches the checker as an index node whose index is the tuple
  `(x, y)`, marked `several`. It becomes `e.index(x, y)`, and `e[x, y] = v`
  becomes `e.set_index(x, y, v);`.
- `it.to_slice()` becomes `EXPR_COLLECT` when the type of `it` is an iterator
  and declares no `to_slice`.

## Lowering

- `lower_for_hooks` binds the local once, branches on `next` in the test and
  binds the value at the head of the body. `continue` goes to the test. A scope
  around the loop holds the teardown of the local, and a scope around each
  pass the teardown of the value.
- `lower_collect` walks the iterator the same way and writes each value into
  memory from `realloc`. The slice is a slot of the frame with the memory and
  the count.

## Walking a collection

The choices of the passes for "Walking a collection" of round five. The
decisions stand under "Generics and collections" in `docs/decisions.md`.

- The parser keeps the source of the walked expression in `over_text` of the
  `for`, which the refusal of a change to a copy and the trap name.
- `sema_iterate` moves a `value` call that gives a `lent` pointer into
  `place` and makes `current` a `*` of it, typed by hand, since the call is
  checked already. `for x in &e` binds `place`, and every other walk binds
  `current`.
- `watch_changes` finds the first plain field of type `anti.lang.Watch` of
  the iterator. It builds four checked trees on the hidden local. One compares
  the counts, and three give the pointer, the length and the line of the last
  change. The fields are marked `promoted`, so the visibility of the
  program does not apply to them.
- `mark_walked` gives the element variable `copy_of`, the source of the walked
  expression, or `lent_turn`. `walked_copy` of `check_assign` follows a place
  through fields and array elements to such a variable and stops at a pointer
  or a slice.
- `walk_check` of `lower_for_hooks` writes the comparison in the test before
  `next` and a failure block of the kind `IR_FAIL_CHECK`, which reads the
  place of the change and calls `anti_rt_walk_changed`.
- The copies of a generic clone `place` and the four trees with the rest of
  the iteration, and the tree of a library file carries them.
