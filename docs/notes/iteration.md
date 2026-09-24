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
