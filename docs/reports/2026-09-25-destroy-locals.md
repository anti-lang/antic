# Destroy of a local

Eddie answered the question of `docs/reports/2026-09-25-owning-values.md`:
`destroy(&c)` of a local counts as moving `c` out. It is built and recorded in
`docs/decisions.md` and `docs/anti-object-model.md`.

## What was done

- `destroy(&c)` of a local or an `own` parameter moves it through the checks
  of a move. The block skips its teardown, and naming it again is refused
  with `` `k` was moved into `destroy` ``. A `destroy` in one branch is a
  move in one branch: lowering clears the tables of the local, as after every
  move, so the teardown at the end of the block passes over it.
- `destroy` of a part of a local is refused: a field or an array element that
  the local reaches without a pointer. The message says to destroy the whole
  value or to give the part a class with its own teardown.
- `destroy(p)` through a pointer, such as the memory of `alloc`, is
  unchanged.
- Before this commit the block tore a destroyed local down again, and the
  `destruct` of its class ran twice.

Commit: `fea14a7`.

## Tests

- `programs/destroy_locals.anti`, in release and dev mode and under Rosetta,
  counts the leaves alive and the teardowns of the class that owns them. A
  class local and an owning struct local are each destroyed early. Each is
  destroyed in a branch that runs and kept by a branch that does not. An owning struct
  in the memory of `alloc` is destroyed through its pointer.
- `errors/destroy_locals.anti` refuses a name after `destroy`, for a class
  and for a struct in a branch. It refuses `destroy` of a field of a struct,
  of a field of a class and of an element of an array.

## What failed and how it was fixed

- The first form of the test counted the teardowns of the leaf, which a
  second teardown does not reach, since the first clears the `own` field. It
  passed before the change. It now counts the teardowns of the class with its
  own `destruct`, and failed first as it should.

## Proof

- At `fea14a7`: host 1114 of 1114, ASan 1113 of 1113 and UBSan 1113 of 1113,
  the host suite without `no_paths`.

## Provisional decisions

In `docs/decisions.md`: the message of a name after `destroy`, the checks of a
move it takes, what a part is, and `destroy(&c, from)` as a move.

## Questions for Eddie

None.
