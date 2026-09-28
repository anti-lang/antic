# Fresh values torn down once, collections hashed by their elements

The session fixed the four defects of
`docs/reports/2026-09-28-field-copies.md` and renamed the made-up module of
the order test, as Eddie asked.

## What changed

- `7432289`. Three defects of the value rules.
  - A call result or a literal passed to a parameter that takes a value and
    is not `own` was never torn down. `lower_drop_arguments` tears it down
    after the call, in `lower_call` and `lower_construct`.
  - A call result that a statement drops was never torn down.
    `drop_result` tears it down where the statement ends.
  - `if let` wrote its binding as a `let` without the checks of a `let`, and
    `lower_let_unwrap` never cleared a moved `own` parameter. Both tore an
    `own` parameter down twice. `if let` now binds as `let` does. An `own`
    parameter moves into the binding whenever its type has a teardown, owned
    memory among them.
  - `programs/temporaries.anti` counts every case in both modes, and
    `errors/if_let_moves.anti` holds the refusals. Both failed on the
    compiler before the change: 7 boxes, then 10, stayed alive, the
    bindings ended at 8 boxes and -1 plain, and no refusal was reported.
    `std/collection_dup.anti` uses the forms it avoided before.
- `300a147`. Every collection hashes its elements. Each collection module
  declares a free `operator fn hash` beside its `operator fn eq`, and the
  compiler writes the `hash` of a class copy as a call of it. A sequence
  hashes in order and a map or a set whatever the order of its entries.
  `Map` and `Set` now compare regardless of order. `std/collection_hash.anti`
  failed on every line first. `std/collection_fields.anti` compares the hash
  of two equal owners again, and `std/map.anti` and `std/set.anti` expect
  the new `==`.
- `b6ee96a`. The order test names its module `synchronized/list`.

## Gates

At `b6ee96a` no build shows a warning. The host suite passes 1217 of 1217
(`build/scratch/suite6.log`). ASan passes 1216 of 1216
(`build/scratch/ctest-asan2.log`) and UBSan 1216 of 1216
(`build/scratch/ctest-ubsan2.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- A parameter of a pointer type, `self` among them, and a call through a
  function value leave a fresh argument alone.
- An `own` parameter moves into a place whenever its type has a teardown.
- A collection module gives its class a hash with a free `operator fn hash`,
  which the compiler writes as the class's `hash`. The two helpers of
  `anti.collection` combine the element hashes with the tuple hash.

## Found, not fixed

- A fresh value whose field or function a statement reads is never torn
  down: `Box.make(1).n` leaves one box alive (`build/scratch/fld/r2.anti`).
  The provisional rule above leaves a receiver alone, since a function with
  `self` may give out a pointer into it.
- A statement that drops the result of a call with a handler,
  `made(2) catch { return 1; };` of a `may fail` function giving a class
  value, ends the program with a segmentation fault
  (`build/scratch/fld/r3.anti`). The compiler of `f18f069`, before this
  session, does the same, so the fault is older than these changes.
- `let b = a;` and `if let b = a` copy the bytes of an existing value whose
  class has a `destruct` and no `own` field, and both are torn down
  (`build/scratch/fld/f1.anti`). The refusal of a copy reads `own` fields
  alone.

## Questions

- Should a fresh value read by a field or a receiver live to the end of its
  statement and then be torn down?
- Should `=`, `let` and `if let` refuse to copy a value whose class has a
  `destruct`, as they refuse one with `own` fields?
