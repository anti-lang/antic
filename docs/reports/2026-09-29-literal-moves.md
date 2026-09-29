# An owning local moves into a literal, Pool frees each element once

`Pool<T>` freed an owning element twice. `take_place` and `get_versioned`
built a tuple from a local of the element type, and both the local and the
tuple tore the element down.

## What the compiler did

It did not move the local. A local of a class value that owns something,
named in a tuple or struct literal, was refused as a copy with `` use `dup`
instead of `=` ``. A local of a type parameter was not refused, since the
checker checks a generic once with `T` open and `T` owns nothing there. The
copy for `Pool<Held>` then copied the bytes into the tuple, and the local
kept its teardown. `remove_all` of the old build crashed with `table not
set: the object is no Leaf`.

## What changed

- `b9ea9c5`. A local, an `own` parameter or a local of a type parameter
  named in a tuple, struct or class literal moves into it once the literal
  has read every part. A name after the move is refused with
  `` `k` was moved into `(Key, int)` ``, and `(k, k)` is refused too. A
  parameter that is not `own` is refused as before. Lowering clears the
  tables of the local after the last part is stored.
  `sema_literal_moves` and `sema_move_into_literal` of `sema_expr.c` hold
  the rule.
- `take_place` and `get_versioned` of `Pool<T>` and `take_place` of
  `Tree<T>` needed no change in their source. Under the rule each already
  gives the element one owner, the tuple.
- The other collections: `List`, `Deque`, `Ring`, `Grid`, the two maps and
  the base of `anti.collection` build the tuple from `items[at]` and then
  zero the slot, which moves the bytes out of the storage with no local.
  `SortedMap` and `SortedSet` build it from a call. None had the fault.
- Tests that failed first. `std/collection_pool.anti` takes an owning
  element through `remove`, `remove_all`, `remove_first`, `remove_one`,
  `get_versioned`, `set_if_version` with the current and a stale version,
  and `clear`, counting teardowns and live leaves. `std/collection_tree.anti`
  does the same for `remove_all`, `remove_first` and `clear`. Both crashed
  on the old compiler. `programs/literal_moves.anti` covers the literal
  forms in both modes and was refused by the old one.
  `errors/literal_moves.anti` holds the refusals.
- `errors/own_copy.anti` pinned a refusal of `Holder { item: a }`, which is
  now a move. It reads `h.item` instead, which stays refused.
- `docs/decisions.md` records the rule beside the entries on owning values.

## Gates

At `b9ea9c5` no build shows a warning. The host suite passes 1235 of 1235
(`build/scratch/suite2.log`). ASan passes 1234 of 1234
(`build/scratch/asan.log`) and UBSan 1234 of 1234
(`build/scratch/ubsan.log`), each without `no_paths`. The docs-style
checker reports nothing on `docs/decisions.md` and this report.

## Provisional decisions

- The literal of a variant case takes no local. It still refuses one that
  owns something as a copy.

## Questions

- A variant whose case holds an owning value is never torn down. Even
  `Slot.Full { k: key(4) }`, which takes a fresh value, leaves its leaf alive
  at the end of the block. `docs/decisions.md` lists structs and
  tuples as owning and names no variant. Should a variant with an owning
  case follow the rules of an owning struct, torn down by its tag? The move
  into a case literal would then follow.
- In a generic, `let a = b;` with `b` a local of a type parameter still
  copies the bytes, and the copy for an owning `T` then has two owners.
  `relet<Key>` with `let a = x; let b = a;` crashes on a double free. It is
  the same hole as the one this session closed for literals. Should
  `let` and `=` of such a local move it, as they move an `own` parameter?
