# The parts a loop owns

Eddie answered the question of `docs/reports/2026-09-25-lent-answers.md`: the
loop owns every part of an iterator's value that it receives by value. It is
built, recorded in `docs/decisions.md` and
`docs/anti-language-additions.md`, and tested under a leak check.

## What was done

- The loop tears down each part of the value it receives by value at the
  end of its turn. It does so on every exit of the body: its end, `break`,
  `continue`, `return` and `fail`. The lent parts are borrowed and never
  torn down.
- A value with lent parts now lives whole in a symbol of no scope for the
  turn. This holds in the form `for x in e` and under a pattern, and
  `for x in &e` binds the value as its variable. The exit action of that
  symbol tears a tuple down part by part and passes over the pointers. The
  copy of `for x in e` and the names of a pattern read the value, so what the
  loop received is torn down once.
- Before this commit no part was torn down. The test counted 3 leaked leaves
  after one walk of three turns, and 22 after all of them.

Commit: `4f13925`.

## Tests

`programs/walk_owned_parts.anti` runs in release mode, in dev mode and under
Rosetta. Its iterator builds a fresh `Key` per turn, which owns a `Leaf`, and
gives `(Key, lent *int)`. The leak check is the count of leaves alive, which
must be 0 after each walk:

- `for (k, v) in &m` to the end, with `break`, with `continue`, with `return`
  from a function and with `fail` out of a `may fail` function;
- `for e in &m` to the end;
- `for (k, v) in m` with `break`, and `for e in m` to the end and with
  `return`;
- an iterator that gives a whole `Key` by value, left with `break`.

## What failed and how it was fixed

- A tuple local had no teardown at all, since the teardown of a local covers
  class values, arrays and `?T`. `destroy_local` now tears a tuple down part
  by part. Only the value a loop holds reaches it, so a `let` of a tuple is
  unchanged.
- The copy of `for e in m` first read garbage, since the value it reads had
  no slot of its own without a pattern. The slots of a `for` now go to the
  held value whenever there is one.
- The assembly of `lent_tuples` changed with the new binding, and its six
  entries of the manifest were written again beside those of the new program.

## Proof

- Host suite: 1106 tests before `4f13925`, which failed `emit_identity`
  alone for the manifest. After the manifest was written, the rerun passed.
- At `4f13925`: host 1106 of 1106, ASan 1105 of 1105 and UBSan 1105 of
  1105, the host suite without `no_paths`.

## Provisional decisions

In `docs/decisions.md`: the value held whole for the turn, its part-by-part
teardown and `to_slice`, which hands every part to the slice it returns.

## Questions for Eddie

- A `let` of a tuple that holds a value that owns memory is never torn
  down. A scratch program with `let t = (key(1), 2);` leaves one leaf alive
  after it. A destructuring `let (a, b)` tears its names down. The loop now tears
  its value down part by part. Should every tuple local be torn down so?
