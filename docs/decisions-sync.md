# Decisions of the synchronized collections

The decisions of the step that built `SyncList<T>`, `SyncMap<K, V>`,
`SyncSet<T>` and `SyncPool<T>` of "Thread-safe collections" in
`docs/anti-language-additions.md`, in `anti.collection.synchronized`. A later
step folds them into "Generics and collections" in `docs/decisions.md`.

## Built

- The four classes are built in `src/std/anti/collection/synchronized.anti`,
  each a `synchronized class` that holds the plain collection: a `List`, a
  `Map`, a `Set` or a `Pool`. Every public function runs under the hidden lock
  of its object, so `read`, `modify` and the functions given to the find,
  update and remove forms run inside it. The documentation of each class says
  that a slow function holds up the other threads.
- None has an operation by position or `lend_slice`. `SyncList` has no
  `l[i]`, `get(i)`, `insert(i, x)`, `remove_at(i)`, `read(i, f)` or
  `modify(i, f)`.
- Each has a versioned set, `set_if_version`, which fails with
  `anti.collection.pool.Changed`.
- The tests `std_sync_list`, `std_sync_map`, `std_sync_set` and
  `std_sync_pool` take every operation on one thread under a count of the live
  blocks and of the allocations. The four `_threads` tests run the chunks of a
  `parallel` against one collection while a dispatched job walks it. They count
  with a loop of a versioned read and `set_if_version`, check that a versioned
  set fails after a worker changed the element, and use a `dup` copy from
  another thread. All eight run in release and dev mode.
- `std_sync_pool` takes `get_versioned`, `remove_all`, `remove_first`,
  `remove_one` and `clear` through a `SyncPool` of an owning element, under the
  count of live blocks and allocations.
- Not built: an `==` and a hash that compare the elements alone. The reason
  stands under "Found" below.

## Provisional

- [provisional] `count()` and `is_empty()` are functions, as on
  `ConcurrentMap`. Reason: a program reaches no field of a synchronized class.
- [provisional] `SyncList` keeps `push`, `pop`, `first` and `last`, which act
  at an end and never go stale. It keeps `reserve`, `shrink`, `reverse`,
  `sort_with` and `filter`, and the module holds `sort`, `sort_by` and `map` as
  `anti.collection.list` does. `SyncList<T>.from_list(own items, from)` makes a
  list of a plain one, which `map` needs. Reason: the specification removes the
  operations by position alone. `map` has type parameters of its own, and
  outside the class it cannot write the fields of the list it returns.
- [provisional] Every element of a `SyncList` carries a stamp from a clock of
  the list. `push`, `set_if_version` and the change of each update form give
  the next stamp. `find_first_versioned(test)` and `find_one_versioned(test)`
  give a copy with its version, and `set_if_version(own x, version)` replaces
  the element that holds `version`. It fails with `found` -1 when no element
  holds it. Reason: the list has no key or handle, and a stamp that no other
  element ever holds addresses one element until it changes or leaves.
- [provisional] `filter` keeps the version of each copy, and `from_list` and
  `map` number the elements from 1. `sort_with` and `reverse` keep the
  versions, since they change no element.
- [provisional] `SyncMap` keeps its versions as `ConcurrentMap` does: a stamp
  from a clock of the map in a second `Map` of the keys. `get_versioned(k)`
  and `set_if_version(k, own v, version)` follow it. Reason: one rule for both
  maps. The key of a changed entry is copied before the change, since `Map`
  puts a changed key back after it.
- [provisional] `SyncSet` has `version_of(x) -> ?int` and
  `set_if_version(own x, version)`, which replaces the element equal to `x`
  with `x`. The element moves to the end of the order and takes a new stamp.
  Reason: a set has no `get`, and its element is its own key, so the read side
  gives the version alone.
- [provisional] `union`, `intersect`, `minus` and `is_subset` of `SyncSet`
  take a plain `Set`, and `to_set()` gives one. Two synchronized sets combine as
  `a.union(b.to_set())`. The elements of a set they make start at version 0,
  which no stamp is. Reason: a synchronized value cannot be passed by value, a
  function of the class that locked the other set would hold two locks, and the
  parser takes `union` as the name of a function of a class alone.
- [provisional] `SyncPool` keeps the versions of `Pool`. A walk takes copies
  through `find_first` and counts no change, where `for` over a `Pool` counts
  one. Reason: the walk of a synchronized collection gives copies, so it
  changes nothing.
- [provisional] `for x in c`, `keys()` and `values()` give the `Snapshot<T>`
  of `anti.collection.concurrent`: copies taken under the lock. Reason: a walk
  that lent an element would reach it without the lock, as for
  `ConcurrentMap`.
- [provisional] `copy` of each class takes the lock and writes the bytes of an
  empty collection of the same allocator into the new object. It then copies
  each collection of the object into it. Reason: `dup` hands `copy` memory that
  nothing has written, and the bytes of the object hold its hidden lock as this
  thread holds it. The empty object gives the table and a free lock.
- `push` and `set_if_version` of `SyncList` move `x` into the pair they
  store. An `own` parameter moves into a literal since `b9ea9c5`. The
  provisional copy of the first run is gone.

## Found

- `==` of a class value hands the `operator fn eq` of its module a byte copy of
  each operand, the hidden lock included. A lock that another thread holds at
  that moment is copied as held, and the comparison waits on the copy forever.
  An `operator fn eq` over pointers is refused by `==` and by the default `==`
  of any class with such a field, and the table entry `equals` of a collection
  is the default. The fix lies in `src/antic/`: `==` of a synchronized class
  passes the address of each operand.
- A public function of a synchronized class that returns a plain value where
  its result is a `?T` fails the verification of the IR, `copy i64 has an
  operand of type ptr`. The module returns such values through plain
  functions of the module, `copied`, `taken` and `versioned`.
- The default `==` and hash of each class take every field, the lock aside,
  as "Hashing and order" gives them. They compare the versions, the clock and
  the allocator beside the elements, so two `SyncList` values that hold `[1]`
  are unequal once one of them took a `push` and a `pop` before it. The
  default also reads the fields without the hidden lock, while another thread
  may change them.
- The first run found that `take_place` and `get_versioned` of `Pool` freed an
  owning element twice. `b9ea9c5` on main fixed it in the compiler, and
  `SyncPool` needed no change.
- A call of `SyncList<T>.new` in a function of the class reports that nothing
  in the arguments gives `T`. The module calls `blank_list` and the other
  `blank_` functions instead.
