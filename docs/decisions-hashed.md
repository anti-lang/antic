# Decisions of the hashed maps and sets

The entries below belong under "Generics and collections" in
`docs/decisions.md`, which a later step folds them into. They cover
`anti.collection.map` and `anti.collection.set` of round five.

## Built

- "Maps" and "Sets" of round five are built. `anti.collection.map` holds
  `Map<K, V>` and `HashMap<K, V>`, `anti.collection.set` holds `Set<T>`,
  `HashSet<T>` and `BitSet`. Each has every operation of its table in
  `docs/anti-language-additions.md` and the parts of `Collection<T>`, with
  `capacity()`, `reserve(n)` and `shrink()`.
  `std_map`, `std_hash_map`, `std_set` and `std_hash_set` run them in release
  and in dev mode, with the order after removals and heavy churn under a leak
  check. The checks `map_changed` and `set_changed` stop a dev build whose walk
  sees the collection change.
- `serialize` of a `HashMap` writes its entries sorted by the bytes of the text
  `serialize` writes for each key, not by `lt`. The order is the same whatever
  order a walk takes, needs no test of whether a key has `lt`, and holds for
  every key type `serialize` can write. So `9`, `10` and `-3` stand as `-3`,
  `10`, `9`. `serialize` of a `HashSet` sorts its elements the same way.
  Eddie decided it, and `std_hash_map` and `std_hash_set` check it over
  numbers, `str`, structs with and without `lt` and every built-in key type.

## Provisional

- [provisional] `anti.collection.map` holds two public abstract bases.
  `EntryTable<E>` keeps the entries of `Map` and `Set` in one array in
  insertion order with a hash table of positions. `SlotTable<E>` keeps the
  entries of `HashMap` and `HashSet` in the slots of the table. Reason: a set
  follows its map in order and in constraints, so one table serves both, and
  the sets of another module reach the table through `protected` members.
- [provisional] A `Map` or a `Set` holds one block of room from its allocator:
  the entries, the hash of each and the buckets, two words per bucket. A
  removed entry keeps its place with a hash of 0 and its bucket is marked
  removed, so nothing moves. The room is rebuilt when it is full, in place when
  a quarter of it holds removed entries and twice as large otherwise, from 8
  entries. Buckets are a power of two, at least 16 and at least twice the
  entries. Reason: `reserve(n)` and `new(capacity)` allocate once, as the
  principles ask, and a removal keeps the insertion order.
- [provisional] A `HashMap` or a `HashSet` probes linearly and marks a removed
  slot, which the next growth clears. It grows before the live and the removed
  entries reach half the slots. It doubles when more than a quarter are live
  and keeps its size otherwise, from 16 slots. `capacity()` is half the slots.
  Reason: a removal then moves no entry, so a walk that removes finds every
  entry once. Churn at a steady size cleans the table without growing it.
- [provisional] A walk of a `HashMap` or a `HashSet` starts at a random slot,
  and a `for` also steps by a random odd stride, which visits every slot of a
  table of a power of two once. The number comes from the random source of the
  system, drawn once per table at its first walk, and each walk mixes in its own
  count. The functions of `Collection<T>` walk from a random slot in slot order.
  Reason: the specification asks for a new order on every walk and says that the
  hash seed, which `--anti.hash_seed` may fix, never decides the order.
- [provisional] `set(k, own v)` and `add(k, own v)` store a copy of `k`, which
  the caller keeps. `Set.add(own x)` and `HashSet.add(own x)` store `x`, and a
  refused `add` tears down what it was given. Reason: the table of maps passes
  the key without `own` and the value with it.
- [provisional] The keys of a map never change in place. `update_all` and the
  other functions of `Collection<T>` lend an entry, and when the change leaves a
  key that is not `==` to a copy taken before, the map puts the copy back. A set
  lends a copy of its element and drops the change, and `BitSet` lends a copy of
  the number. Reason: a changed key would stand in the wrong bucket, and the
  iterator rule already says that the keys of a map never change in place.
- [provisional] `update(k, change)`, `read(k, f)` and `modify(k, f)` each lend
  the value in place as a `lent *V` and give `false` when the map lacks `k`.
  Reason: the table gives `update` a `bool` and no result to the other two, and
  `lent` has no form that only reads.
- [provisional] The iterator of a map gives `(K, lent *V)`, a copy of the key
  and the value in place. `keys()` gives copies of the keys and `values()` lends
  each value, so `for v in &m.values()` changes them. The iterator of a set
  gives a copy of each element. Reason: "Walking a collection" gives a map
  `(K, lent *V)`, and an element of a set is its key.
- [provisional] `to_text` of a map writes `{"Ann": 41}`, each key and value as
  an element of a collection. `serialize` writes a JSON object when the key is
  `str` and an array of `[key, value]` pairs otherwise. `Map` and `Set` write in
  their order, and `HashMap` and `HashSet` in the order of the entry above.
  `to_text` of both writes in the order of a walk. Reason: the principles give
  the forms, and the specification sorts a `HashMap` for saved files, which
  holds for a `HashSet` as well.
- [provisional] Two keys of a `HashMap` may write the same text, as two keys
  that differ in a pointer alone do. The bytes of the text of the value then
  decide between them. Two entries that write the same key and value write
  the same bytes in either order. Reason: the order of the keys alone leaves
  such entries in slot order, so the file would change from run to run.
- [provisional] The map reads the type of a key from the record of the type
  argument, `anti_rt_type_arg` at depth 3, and keeps the type ids it needs as
  constants of its own. Reason: importing `anti.reflect` for `TypeId` links its
  descriptors into every program with a map. It needs the id of `str` alone,
  which decides between a JSON object and an array of pairs.
- [provisional] `==` of `Map` and of `Set` compares the entries in order, as
  `collection.equal` does. Reason: the principles say element by element.
- [provisional] `==` of `HashMap` and of `HashSet` holds when the counts agree
  and the other holds every entry of the one, in any order. Each is an
  `operator fn eq` of the module beside the one of `Map` or `Set`, under the
  shared names `eq:HashMap` and `eq:HashSet`. Reason: the hashed collections
  have no order to compare in, and the shared names give one module a hook per
  class.
- [provisional] `union` of `Set` gives the elements of this set in its order
  and then those of the other set it lacks, in the other's order. `union` of
  `HashSet` and of `BitSet` gives the elements of both. Each makes the new set
  from the allocator of this set. Reason: a `Set` keeps insertion order, and
  the order of `a.union(b)` is the order of adding `a` and then `b`.
- [provisional] `intersect`, `minus` and `is_subset` take the other set by
  value, `a.intersect(b)`, and make the new set from the allocator of `a` in the
  order of `a`. Reason: the table writes `intersect(o)`, and `==` takes its
  operands the same way.
- [provisional] `dup` of a map or a set copies each element with its own copy
  into new room of the same allocator, and the copy of a `HashMap` or a
  `HashSet` draws its own random number. Reason: a collection is copied by
  `dup`.
- [provisional] `BitSet` holds the numbers from 0 up. `add` of a number below 0
  gives `false` and `contains` gives `false` for one. `new(capacity)` and
  `reserve(n)` make room for the numbers below the count they name, and
  `shrink()` gives back the words above the largest number. A place of
  `Collection<int>` is the number itself. Reason: the specification calls it a
  set of small numbers, one bit per value, and its room follows the largest
  value, not the count.
- [provisional] No map or set has `deserialize` yet. Reason: `Collection<T>`
  has none, and this step names `serialize` alone.

## Resolved on main

- `==` of a generic class from a library file reaches a program since the
  library format carries the `operator` mark, version 71.
- One module gives `==` to `Map` and `HashMap`, and to `Set` and `HashSet`,
  through the shared names of `operator fn`.
- `union` names a member after `fn` and after a dot, so `s.union(o)` parses.
- `serialize` of a `HashMap` with a key of a struct, a class or a variant that
  has `operator fn lt` wrote in slot order, since a generic cannot ask whether
  its argument has `lt`. Eddie's decision to sort by the text of each key, the
  entry under "Built", takes the question away.
