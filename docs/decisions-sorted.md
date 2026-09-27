# Decisions of the sorted collections

The decisions of `SortedMap<K, V>` and `SortedSet<T>` of round five, for a
later step to fold into "Generics and collections" of `docs/decisions.md`. Both
are built in `anti.collection.sorted` with every operation of "Maps" and "Sets".
`docs/reports/2026-09-25-add5-sorted.md` and
`docs/reports/2026-09-27-add5-sorted.md` report the step.

- `SortedMap<K: Ordered, V>` and `SortedSet<T: Ordered>` are B-trees over one
  abstract base, `BTree<T>`. It inherits `collection.Collection<T>` and fills
  the five functions that reach an element by its place. The map is a
  `BTree<(K, V)>`. `tests/std/sorted_map.anti` and `tests/std/sorted_set.anti`
  run every operation in both modes. `tests/std/sorted_order.anti` checks the
  shape of the tree after thousands of insertions and removals.
- [provisional] A node holds up to eleven elements and twelve children, and
  every node but the root at least five. Reason: "Maps" asks for a node that
  holds many keys in one block. Eleven is the order of the B-tree of the
  standard library of Rust, whose search walks a node in a line.
- [provisional] Every node sits in one block from the allocator of the
  collection, the elements of all nodes first and then their links, and a node
  is an index into the block. `new(capacity)` and `reserve(n)` allocate once,
  the block doubles as it grows, and `shrink()` moves the nodes down and gives
  the rest back. A removed node joins a list of free nodes. Reason:
  "Principles" gives `new(capacity)`, `reserve(n)` and `shrink()` to the maps
  and the sets, and a block of indices grows in one allocation.
- [provisional] The place of an element is its node times eleven plus its
  slot, and the base walks places in key order. Reason: the functions of
  `Collection<T>` reach an element by an `int` place.
- The map adds `range(lo, hi)`, `floor(k)`, `ceiling(k)`, `first()`, `last()`,
  `pop_first()` and `pop_last()`, and the set adds `range`, `floor`,
  `ceiling`, `first` and `last`, as "Maps" and "Sets" list them. The set has
  `union(o)`, `intersect(o)`, `minus(o)` and `is_subset(o)`, which walk both
  sets once in order.
- [provisional] `range(lo, hi)` with `lo` not below `hi` walks nothing.
  Reason: the range stops before `hi`, so it holds no key.
- [provisional] `floor`, `ceiling`, `first`, `last`, `pop_first` and
  `pop_last` of the map give `?(K, V)`, and those of the set give `?T`.
  Reason: "Principles" gives a copy as a `?T` where nothing may be there, and
  the entry of a map is its key and its value.
- [provisional] The walk of a map, `iter` and `range`, is `SortedEntries<K, V>`,
  whose `value` gives `(K, lent *V)`: a copy of the key and the value in place.
  `for (k, v) in &m` changes the values, and `for (k, v) in m` takes a copy.
  `keys()` gives copies of the keys, and `values()` lends each value to
  `for v in &m.values()`. Reason: "Walking a collection" gives a map
  `(K, lent *V)`, and `SortedMap` by name.
- [provisional] The set walks copies alone, and `for x in &s` over a set is
  refused. Reason: the elements of a set are its keys, and "Walking a
  collection" says keys never change in place.
- [provisional] A function that `update_all`, `update_first`, `update_one`,
  `read` or `modify` lends an element may change it only in ways that keep its
  place. After each lend the collection compares the element with its
  neighbours, and a change that moves it out of the order ends the program
  with a fatal error. Reason: the base lends the whole element, the key of a
  map entry included, and a tree out of order finds nothing reliably. The test
  `std_sorted_trap` holds the trap.
- [provisional] The map keeps a copy of the key a call passes, with `dup`, and
  a lookup gives a copy of the value alone. The teardown and the copy of a
  whole entry are the base's, whose `destroy` and `dup` take the tuple part by
  part.
- [provisional] `union(o)`, `intersect(o)` and `minus(o)` make the new set from
  the allocator of this set, and an element both sets hold is copied from this
  one. Reason: "Principles" says everything a collection
  allocates comes from its allocator.
- [provisional] `serialize` writes a map with `str` keys as a JSON object,
  `{"Ann":41}`, and any other map as an array of pairs. It reads the type id in
  the record of the key type and compares it with `anti.reflect.TypeId.Str`, so
  `anti.collection.sorted` imports `anti.reflect`. Reason: "Principles" names
  both forms, and a generic body cannot name the type of `K` otherwise.
- The map and the set have `==` through a generic `operator fn eq` of the
  module that calls `collection.equal`, as "Generics and collections" gives
  every collection. The map's is `eq<K: Ordered, V: eq>` and compares the
  entries in key order, so a map whose values have no `eq` has no `==`, and the
  refusal names `V`. The set's is `eq<T: Ordered>`. `tests/errors/sorted.anti`
  holds the refusals.
- Not built: `deserialize` of a map, which no collection has yet.
