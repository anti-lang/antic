# Two object locks and the equality of thread-safe classes

The session built Eddie's decision on the synchronized lane. The lane blocked
on `==` and the hash of a synchronized class, which read its fields without
its lock.

## What changed

- `sync a, b { }` takes the hidden locks of two synchronized objects. The
  runtime takes the lower address first, and one lock when both are the same
  object, in `anti_rt_object_lock_pair` of `src/rt/lock.c`. A dev build records
  one order per pair, so the check of lock orders accepts both ways round. A
  Mutex on either side is refused. The library format carries the second
  operand, version 73.
- The default `==` of a synchronized class runs under `sync a, b` and its
  default hash under `sync a`, also around an `operator fn hash` of its module.
- A concurrent class has no default `==` or hash. `==`, `x.hash()` and the
  constraint `hash` refuse one that declares neither, as does the default
  `==` of a holder in place. Its table entries compare identity and hash the
  address.
- An operator whose parameter is a pointer takes the address of either
  operand. An operand that holds a Mutex is refused at a by-value parameter,
  which copied the lock.
- `ConcurrentMap` compares and hashes its entries alone with every part
  locked, the parts of both maps for `==` in the order of their addresses.
- `equals` of a class whose module gives it `operator fn eq` calls that
  operator. `equals` through `*Object` compared the count of changes and the
  room of a collection before, and said not equal where `==` said equal.

## Tests

- `programs/sync_pairs.anti` compares `a == b` and `b == a` and holds
  `sync a, b` and `sync b, a` on two threads, and runs `sync a, a`. Its reader
  counted 149278 torn reads with the compiler before the change and none
  after it.
- `programs/sync_operators.anti`, `std/concurrent_map_equal.anti` and
  `std/collection_equals.anti` run in both modes. `traps/lock_order.anti` takes
  a pair both ways and expects no report. A runtime that locked in the given
  order failed it. `errors/sync_pairs.anti` holds the refusals.

## Gates

At `ebcd551` no build shows a warning. The host suite passes 1260 of 1260
(`build/scratch/pair/full3.log`, then the tests the manifest touched). ASan
passes 1259 of 1259 (`build/scratch/pair/asan-test.log`) and UBSan 1259 of
1259 (`build/scratch/pair/ubsan-test.log`), each without `no_paths`. The
docs-style checker reports nothing on each changed `.md` file.

## For the synchronized lane

`add5/sync` can now give `SyncList`, `SyncMap`, `SyncSet` and `SyncPool` an
`operator fn eq` and `operator fn hash` over pointers,
`operator fn eq<T: eq>(a: *SyncList<T>, b: *SyncList<T>)`, which holds
`sync a, b` and compares the elements alone. `programs/sync_operators.anti`
shows the form. The branch needs a rebase onto format 73.

## Provisional decisions

Each stands in `docs/decisions.md` under "Concurrent classes":

- A Mutex has no pair form of `sync`.
- The order of a pair is the order of the addresses, lower first.
- The table entries of a concurrent class without its own compare identity.
- An operator over pointers takes the address of both operands.
- The hash of a synchronized class runs its module's operator under the lock.
- `ConcurrentMap` has the public `equal_entries` and `hash_entries`.
- `equals` of a class calls the `operator fn eq` of its module.

## Questions

- `ConcurrentMap` needs `equal_entries` and `hash_entries` in its public
  interface, since a free operator reaches no private function. Should an
  operator of a module reach the private functions of the classes of that
  module instead?
