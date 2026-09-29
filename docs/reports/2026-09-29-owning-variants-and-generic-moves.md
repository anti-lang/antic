# Owning variants, and the move of a local of a type parameter

The session built Eddie's two answers to the questions of
`docs/reports/2026-09-29-literal-moves.md`.

## What changed

- `09a7a79`. A variant with an owning case is owning. `each_case` of
  `lower.c` reads the tag once and tears down, copies or clears the fields
  of the case it names. The variant follows the value rules of an owning
  struct. `=` refuses it and `dup` copies it. It moves on return and into
  an `own` parameter. A field, an array element, a `?T` and a collection
  element tear it down with their owner. A local named in a case literal
  moves into it, and the provisional refusal of the last session is gone.
  The header marks an owning variant as it marks an owning struct.
- An arm of `switch` or `if let` that binds an owning case copied the
  bytes of the case alone. With the variant torn down too, that gave what
  the case owns two owners. The arm now copies what the fields own, as
  `dup` does, and tears the copy down on every exit. A scope of its own
  around the arm holds that teardown.
- `8cc372d`. In generic code a local of a type parameter, or of a value
  that holds one in place, moves on `let` and `=` for every `T`. Naming it
  afterwards is refused, and `dup` copies. `moves_own_param` of
  `sema_stmt.c` holds the rule beside the move of an `own` parameter.
- The copy a `for` walk gives keeps the rule of the loop, which a DESIGN
  comment in `sema_stmt.c` settles. `let kept = x;` in a generic walk is
  therefore refused. `generic_copies.anti`, `modules/generics` and
  `dump/constraints.anti` now write `dup(x)`, and `constraints.types`
  shows the new node.
- `find_one` and `remove_one` of the concurrent map moved a local that
  their closure captured. The closure now writes through a pointer, so the
  local is not captured and moves out. Before this, an owning key or value
  there was copied and had two owners.
- Tests that failed first. `programs/variant_owning.anti` takes a variant
  through a `let`, a literal, a return, an `own` parameter, a struct and a
  class field, an array, a `?T`, `dup` and `if let`, in both modes.
  `std/collection_variants.anti` holds a `List` of them through `pop`,
  `remove_all`, a copy and `get`. Both leaked every key on the old
  compiler. `programs/generic_moves.anti` runs `let a = x; let b = a;`,
  a return, `=`, a tuple local and `dup` with an owning `T` and with
  `int`, and it aborted on the old compiler. `errors/variant_owning.anti`
  and `errors/generic_moves.anti` hold the refusals.

## Gates

At `8cc372d` no build shows a warning. The host suite passes 1244 of 1244
(`build/scratch/suite-g.log`), and 1240 of 1240 at `09a7a79`
(`build/scratch/suite-v.log`). At `8cc372d` ASan passes 1243 of 1243
(`build/scratch/asan2.log`) and UBSan 1243 of 1243
(`build/scratch/ubsan2.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- An arm that binds an owning case gets a copy of what it owns, which the
  arm tears down.
- A local of `(T, int)`, `?T` or `[4]T` moves as a local of `T` does. A
  pointer or a slice to one does not.
- A name after a move into a case literal reads
  `` `v` was moved into `Slot.Full` ``. This line replaces the removed
  entry, which held the message of the literal moves.

## Questions

- A parameter of a type parameter that is not `own`, `fn f<T>(p: T)`,
  keeps its rule, and `let a = p;` still copies the bytes. In the copy for
  an owning `T`, the caller and `a` then both tear the value down, and
  `borrowed<Key>(k)` with `let a = p;` crashes. Should
  `let` and `=` of such a parameter be refused with the message of a
  parameter that belongs to the caller, as for an owning class value?
- The specifications still say that a struct or a tuple is owning, in
  "Ownership and copies" of `docs/anti-object-model.md` and in
  `docs/anti-language-additions.md`. Should they name the variant too?
