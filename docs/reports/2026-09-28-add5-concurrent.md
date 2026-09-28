# The concurrent collections

The step built `ConcurrentMap<K, V>` and `SpscRing<T, N>` of "Thread-safe
collections" in round five, in `anti.collection.concurrent`. The logs of this
session stand under `build/drive/logs/` of the worktree `add5-concurrent`.

## What was built

- `src/std/anti/collection/concurrent.anti` holds both classes, each a
  `concurrent class`, and `Snapshot<T>`, the copies a walk of either goes over.
- `ConcurrentMap` splits its entries into 16 parts by the seeded hash of the
  key. Each part is a lock, a `HashMap` of the entries and a `HashMap` of their
  versions. An operation on one key locks the part of that key alone. It has
  the operations of `Map` less those by position, the versioned set, `==`,
  `to_text`, `serialize` and `dup`.
- `SpscRing` holds two atomic counters, one per side, and no lock. Each side
  keeps a plain copy of the other side's counter, which
  `unchecked(unguarded-field, "...")` covers with its reason. It has the
  operations of `Ring` and of every collection.
- Four tests, each in release and dev mode: `std_concurrent_map` and
  `std_spsc_ring` take every operation on one thread and count the blocks
  alive. `std_concurrent_map_threads` runs 16 chunks of a `parallel` on
  200000 keys of their own. It then counts on eight shared keys with `update`
  and with a loop of `set_if_version`, while a dispatched job walks the map.
  `std_spsc_ring_threads` passes two million values through a ring of 64 in
  each direction at full speed, then tuples through a ring of 7 and objects
  that own memory. Nothing is lost, reordered or torn.
- `tests/run_anti_check.cmake` counts 34 modules of the standard library.

A ring that published its counter before it wrote the slot put 12546 values
out of order in `std_spsc_ring_threads`. The test catches the fault it exists
for. The run was a check by hand and is not part of the suite.

## What failed and how it was fixed

- `copy` first moved a new object into `to` with `*to = moved(made)`. That
  assignment reads the table of the old value, which is zero memory there, and
  the program stopped (`build/drive/logs/lldb.log`). `copy` now writes the
  bytes of the object and then the fields that differ, as `Ring` does.
- `SpscRing.pop` returned `got.0` of a local tuple, which tore the element down
  twice. `take` now returns the tuple, as `take_place` of `Ring` does.
- `anti_check` expected 33 modules of the standard library
  (`build/drive/logs/anti_check.log`).
- antic fails to lower `remove` of a `Map` or `HashMap` whose values are `int`
  and whose key class has a `destruct`:
  `anti.collection.map.HashMap<hm.Id, int>.remove b2: copy i64 has an operand
  of type ptr`. `build/scratch/hm.anti` reproduces it with a class `Id` of one
  `int` field, `eq`, `hash` and an empty `destruct`. The versions of a part are
  a one-field struct `Stamp` instead, which lowers. The fix belongs to the
  compiler and lies outside this step.

## Gates

At `25d56e1`: the build has no warning of our own code. The host suite passes
1207 of 1207 (`build/drive/logs/ctest-host2.log`). ASan passes 1206 of 1206
(`build/drive/logs/ctest-asan2.log`) and UBSan passes 1206 of 1206
(`build/drive/logs/ctest-ubsan2.log`). The docs-style checker reports nothing
on both new `.md` files.

## Provisional decisions

`docs/decisions-concurrent.md` holds each with its reason:

- 16 parts, chosen by the top bits of the seeded hash.
- A part is a struct of the module, so `guarded by` does not reach its tables.
- `count()` and `is_empty()` are functions, not a field.
- A version is a stamp from the clock of the part, never repeated after a
  remove and an add. `set_if_version` fails with `pool.Changed`.
- The `_one` forms of the map lock every part in order.
- Walks, `to_text`, `serialize` and `==` work on copies.
- The map writes text as a `HashMap` does.
- The ring keeps a plain copy of each counter per side, under `unchecked`.
- The split of the ring's functions between producer and consumer.
- The ring has no versioned set.
- `parts` and `items` carry `unchecked`, since `copy` writes them.
- The versions are kept as `Stamp`, because of the compiler defect above.

## Questions

- Should `copy` count as a lifecycle function of the check `unguarded-field`,
  as `construct` does? It writes into an object no other thread can reach, and
  both classes carry an `unchecked` for that alone.
- The thread-safe collections give `count()` where the plain ones give `count`.
  The synchronized collections of `anti.collection.sync` face the same choice.
