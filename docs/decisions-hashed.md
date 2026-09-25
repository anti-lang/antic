# Decisions of the hashed maps and sets

The entries below belong under "Generics and collections" in
`docs/decisions.md`, which a later step folds them into. They cover
`anti.collection.map` and `anti.collection.set` of round five.

## Built

- "Maps" and "Sets" of round five are built apart from the four points under
  "Blocked" below. `anti.collection.map` holds `Map<K, V>` and `HashMap<K, V>`,
  `anti.collection.set` holds `Set<T>`, `HashSet<T>` and `BitSet`. Each has
  every operation of its table in `docs/anti-language-additions.md` and the
  parts of `Collection<T>`, with `capacity()`, `reserve(n)` and `shrink()`.
  `std_map`, `std_hash_map`, `std_set` and `std_hash_set` run them in release
  and in dev mode, with the order after removals and heavy churn under a leak
  check. The checks `map_changed` and `set_changed` stop a dev build whose walk
  sees the collection change.

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
  their order. `HashMap` and `HashSet` sort when the key or the element is a
  number, a `bool`, a `char` or a `str`, and write in slot order otherwise.
  `to_text` of both writes in the order of a walk. Reason: the principles give
  the forms, and the specification sorts a `HashMap` for saved files, which
  holds for a `HashSet` as well.
- [provisional] The map reads the type of a key from the record of the type
  argument, `anti_rt_type_arg` at depth 3, and keeps the type ids it needs as
  constants of its own. Reason: importing `anti.reflect` for `TypeId` links its
  descriptors into every program with a map. `std_hash_map` sorts keys of every
  kind the constants name.
- [provisional] `==` of `Map` and of `Set` compares the entries in order, as
  `collection.equal` does. Reason: the principles say element by element.
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

## Blocked

Each needs a change outside `src/std/anti/collection/`, or a decision.

- `==` of a map and a set does not reach a program. A generic free
  `operator fn eq` of a library file is not found as the hook of its class, so
  `a == b` over `Map<str, int>` gives `` `==` is not defined ``. The same
  function in the module of the program works, and a free hook of a class that
  is not generic works from a library file. The fix lies in `src/antic/`,
  around `operator_symbol` and `check_operator` of `sema_expr.c`.
- A module gives `==` to one generic class only. A free `operator fn eq` is
  one name in its module, so `anti.collection.map` cannot give it to both `Map`
  and `HashMap`, nor `anti.collection.set` to both `Set` and `HashSet`. A hook
  in a class body cannot ask for `V: eq`. Which form should give the second
  class its hook?
- `union` is a keyword, so `s.union(o)` does not parse, for `Set`, `HashSet`
  and `BitSet`. Should the parser take a keyword as a member name after `.` and
  `fn`, or does the function take another name?
- `serialize` of a `HashMap` sorts a key of a struct, a class or a variant that
  has `operator fn lt` in slot order, since a generic without the constraint
  `lt` cannot ask whether its argument has it. The keys of the built-in types
  sort. This needs a compile-time test of a hook in `src/antic/`, or a
  decision on another form.
