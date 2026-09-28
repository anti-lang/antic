# Copies reached through fields, dup of every collection

The session made the copies of generics that a field of a class reaches. It
gave every collection a `dup` of its own at Eddie's request and named the
module of the synchronized collections. It settles blockers 1 and 2 of the
report of the unmerged branch `add5/sync`.

## What changed

- `5901d18`. The checker built the default `==` and hash of each class after
  the pass of the copies, so `equals` of a class with `items: List<Item>`
  called `anti.collection.list.eq`, a generic with no copy, and the link
  failed. `sema.c` now builds the defaults first, and `sema_compile_copies`
  walks the calls they hold. It does so for each class of the module and for
  each class copy it makes, whose defaults it builds as it makes them. The
  owner's `serialize` then wrote each collection field as its bytes: the room
  pointer, the seed and the place of the last change. The runtime now writes a
  class value, in place or behind an `own` pointer, with the `serialize` of its
  table, as it already wrote an element. The digest of `return42` for
  macos-arm64 moved with the runtime. `std_collection_fields` and its dev twin
  check a plain class, a generic class and a struct with `List`, `Map` and
  `Pool` fields under a leak check.
- `2516583`. `SortedMap`, `SortedSet`, `Pool`, `Tree`, `PriorityQueue` and
  `Copies<T>` replace `copy`. The B-tree copies node by node with its links,
  the queue its array with the spare slot. `Slots` copies every block and the
  words of each slot, so the handles of the original find the same elements in
  the copy. `std_collection_dup` and its dev twin dup an owner with a field of
  each of the 17 collection types, change both sides apart and tear both down.
  Every element block and every block of room reads 0 at the end. Both tests
  failed first: the link error of `eq`, then a double free in the pool of the
  queue.
- `33e0118`. `anti.collection.synchronized` in the specification, the overview
  and the decisions.

## Gates

The code stands at `2516583`, and `33e0118` changes docs alone. No build shows a
warning. The host suite passes 1211 of 1211 (`build/scratch/suite2.log`). ASan passes 1210 of 1210
(`build/scratch/ctest-asan.log`) and UBSan 1210 of 1210
(`build/scratch/ctest-ubsan.log`), each without `no_paths`. The
docs-style checker reports nothing on each changed `.md` file.

## Provisional decisions

- A class value field and an owned object are written by the `serialize` of
  their table.
- `dup` of a `Pool` or a `Tree` keeps the index, the generation and the version
  of every slot and the free list. `dup` of a B-tree keeps its node indices and
  takes room for the nodes in use alone.

## Defects found, not fixed

Each has a repro under `build/scratch/fld/`. The tests avoid these forms, so
their leak counts measure the change alone.

- A temporary class value passed to a plain parameter is never torn down.
  `take(Item.of(1))` leaks one block, for a plain function, a generic one and a
  function of a class (`d1.anti` to `d4.anti`). `Grid.new(2, 2, Item.of(0))`
  leaks its fill this way.
- A `?T` result of a call that the statement drops is never torn down:
  `p.remove(h);` leaks the element (`c1.anti`).
- `fn number(own x: ?Item)` that binds `x` with `if let` or with `let ... else`
  frees the value twice (`v1.anti`). A plain parameter does not.
- The collections have `==` over their elements and keep the default class
  hash, which hashes the room pointer. Two equal lists hash apart, against "Two
  values that are equal by `eq` have the same `hash`". The tests hash each
  owner and compare it with itself alone.
- `Snapshot<T>` of `anti.collection.concurrent` owns its copies and keeps the
  copy of `Object`. It is a walk, not a collection, and was left alone.

## Questions

- Should a collection hash its elements, as its `==` compares them? The
  question went out in this session and has no answer yet.
- `tests/run_std_collection_modules.cmake` builds a synthetic tree with a
  `sync/list` module to test the order. It passes, and renaming it to
  `synchronized/list` is a change of a test only. Should it follow the name?
