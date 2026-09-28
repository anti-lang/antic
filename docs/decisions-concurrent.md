# Decisions of the concurrent collections

The decisions of the step that built `ConcurrentMap<K, V>` and `SpscRing<T, N>`
of "Thread-safe collections" in `docs/anti-language-additions.md`, in
`anti.collection.concurrent`. A later step folds them into "Generics and
collections" in `docs/decisions.md`.

- `ConcurrentMap<K, V>` and `SpscRing<T, N>` are built in
  `src/std/anti/collection/concurrent.anti`, both as a `concurrent class`.
  `ConcurrentMap` has the operations of `Map` less those by position, with
  `reserve`, `shrink` and `capacity`. It adds the versioned set, `==`,
  `to_text`, `serialize` and `dup`. `SpscRing` has those of `Ring` and of
  every collection, `==`, `to_text`, `serialize` and `dup`. The tests `std_concurrent_map`,
  `std_concurrent_map_threads`, `std_spsc_ring` and `std_spsc_ring_threads`
  run in both modes. `deserialize` of either is not built, as it is not for
  any collection.
- [provisional] A `ConcurrentMap` holds 16 parts, a fixed number. The part of a
  key is the top four bits of its hash mixed with the seed of
  `lang.hash_seed()`. Reason: the table of a part chooses its slot from the low
  bits of the same mixed hash. The top bits then leave the keys of one part
  spread over its slots. Sixteen locks cost a lock word and two empty tables
  each.
- [provisional] A part is a struct of the module that holds a `Mutex`, a
  `HashMap<K, V>` of the entries and a `HashMap` of their versions. Every
  function of the map reaches a part inside `sync part.lock`. The fields of the
  map itself are fixed or atomic, so the checker proves them. Reason: a library
  file carries no type nested in a generic class yet, and `guarded by` marks a
  field of a concurrent class or of a type nested in one alone. The checker
  therefore does not see that the lock guards the tables.
- [provisional] `count()` and `is_empty()` are functions on both classes, and
  `count` is an atomic field of neither that a program reads. Reason: another
  thread changes the count at any moment, and a plain field would be a read
  without the lock or the atomic load. `Shared<T>` already gives its count as
  `count()`.
- [provisional] The version of an entry is a stamp from a clock of its part,
  which counts every change of the part. `set`, `add`, `set_if_version`,
  `update`, `modify` and the change of each `update_*` give the entry the next
  stamp. `get`, `read` and the `find_*` forms give none. An entry removed and
  added again takes a stamp it never had. `get_versioned(k)` gives `?(V, int)`
  and `set_if_version(k, own v, version) may fail` fails with
  `anti.collection.pool.Changed`, whose `found` is -1 for a key that is gone.
  Reason: a key carries no generation as a handle of a `Pool` does, so a count
  from 0 would match again after a remove and an add, and the write would go
  through although the entry changed.
- [provisional] The `_all` and `_first` forms of `ConcurrentMap` lock one part at
  a time, and `_first` acts on an entry that the test accepts, since a map has
  no order. The `_one` forms lock every part, in the order of the parts, while
  they count the matches and act. Reason: `_one` must find one match in the
  whole map, which another thread could change between two parts. One order of
  the locks keeps two threads that take them all from a cycle.
- [provisional] `for (k, v) in m`, `keys()` and `values()` of a `ConcurrentMap`
  and `for x in r` of an `SpscRing` walk a `Snapshot<T>` of
  `anti.collection.concurrent`: copies of the elements taken part by part,
  from the allocator of the collection. Its
  `value` gives a copy, so `for x in &c` lends nothing. `to_text`,
  `serialize` and `==` of a map work on copies as well, and `==` holds one
  lock at a time. Reason: a walk that lent an element would hold a pointer past
  the lock of its part, and a walk that held every lock would stop every other
  thread for the whole loop.
- [provisional] `to_text` and `serialize` of a `ConcurrentMap` write what a
  `HashMap` of the same entries writes: no order in the text, and the order of
  the text of each key in `serialize`. Reason: the map has no order, and a
  `HashMap` already holds that rule.
- [provisional] `SpscRing` holds two atomic counters: `given`, which the producer
  alone writes, and `taken`, which the consumer alone writes. The element at
  position `p` stands in slot `p % N`. Each side keeps a plain copy of the
  other side's counter and reads the counter again only when the copy says the
  ring is full or empty. The two copies carry
  `unchecked(unguarded-field, "...")` with the thread that owns each. Reason:
  the specification marks the ring `unchecked` because its correctness rests on
  atomics in a pattern the checker cannot follow. The slots stand in room behind
  a pointer, which the check of fields does not reach, so the copies are the
  fields the clause covers. They also keep each side from reading the other's
  counter on every call.
- [provisional] The producer of an `SpscRing` calls `push` and `is_full`, and the
  consumer every other function but `count` and `is_empty`, which either side
  may call. The `find`, `update` and `remove` forms, `clear`, `for`,
  `to_text`, `serialize`, `==` and `dup` reach the elements the consumer holds
  at the call. A removal moves the elements that stay towards the newest and
  moves `taken` past the room they leave. Reason: the producer writes no slot
  below `given`, so the consumer may change and move its own slots without a
  lock.
- [provisional] `SpscRing` has no versioned set. Reason: it has no `set`, and an
  element is addressed by criteria alone. A position would go stale as the
  consumer takes elements.
- [provisional] The field that holds the room of each class, `parts` and
  `items`, carries `unchecked(unguarded-field)`, since `copy` writes it into the
  new object. Reason: `dup` hands `copy` memory that no other thread can reach,
  as in `construct`, but the checker exempts `construct` and `destruct` alone.
  `copy` writes the bytes of the object first and then the fields that differ,
  as `Ring` does. An assignment of a whole class value through the pointer `to`
  reads the table of the zero memory there and stops the program.
- [provisional] The versions of a part are kept as a one-field struct `Stamp`,
  not as `int`. Reason: antic fails to lower `remove` of a `Map` or `HashMap`
  whose values are `int` and whose key class has a `destruct`, with
  `copy i64 has an operand of type ptr`.
  `docs/reports/2026-09-28-add5-concurrent.md` records the case.
