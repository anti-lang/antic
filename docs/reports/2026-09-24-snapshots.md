# Snapshots

The step was "Snapshots" of `docs/anti-language-additions.md`. It had blocked
on the question of how a kept snapshot fits a kept function value of one C
function pointer. Eddie decided that `own` chooses the representation, and
then that an owned function value is two words. The step is built on that
decision, which `docs/decisions.md` holds under "Anonymous functions and
closures" and the additions document under "Snapshots" and "Calling
convention".

## Done

- `snapshot fn` copies each value it captures when it is made. It is
  read-only and holds numbers, `bool`, `char`, enums, structs of those and
  `str`. A write and every other type are refused.
- `own fn` is the form of two words that owns its snapshot. An `own` field
  of function type and a `keep own` parameter hold it. There the snapshot
  goes on the heap as one block that starts with its size, and the bytes of
  each `str` follow the record. Anywhere else it sits in the caller's frame.
- The teardown of a class frees the snapshot of each `own fn` field, the
  copy of `dup` copies it, and `=` into the field frees the old one. `=`
  never copies an existing owned value, and `dup(f)` does. A `keep own`
  parameter frees its snapshot at every exit unless it moved into an owner.
- A plain `keep` parameter, a plain field and a C function pointer refuse a
  snapshot and an `own fn`. Each refusal of a closure by reference names
  `snapshot fn`, and the one at a plain `keep` names `keep own`.
- The C header writes an `own fn` field as a struct of the code and the
  snapshot and declares `anti_rt_snapshot_free`. The library format is 56.
- `src/rt/snapshot.c` holds the runtime side and a count of the snapshots
  alive, which the tests read as their leak check.
- An `own fn` field has the type id none in its field record, so the
  default `serialize` and reflection pass over it.

Tests: `programs/snapshots.anti` in release and dev mode runs every rule
under the leak check: a kept snapshot freed with its owner, `dup` of the
object and of the value, replacement, a `keep own` parameter that moves and
one that does not, frames, borrowing, an inline class value and a nested
closure. `errors/snapshots.anti` holds the 18 refusals, the one at a plain
`keep` parameter included. `clib_handlers` calls through the header's struct
from C. `closures_modules` takes `keep own` and an `own` field through a
library file. `std/serialize_snapshot.anti` checks the field record.

## Failures and fixes

- A named function as the default of an `own fn` field declared before the
  function has no type when the fields are checked. The checker marks no
  conversion, and the first build copied a function address as a pair. The
  lowering of a default now pairs such a value with `none`. The same gap
  keeps a default like that out of the library file, before this change as
  well. `closures_modules` names the function in the literal, as it did
  already for `Button`.
- `anti fmt` moved the brace of `let f = snapshot fn(...) {` to its own
  line. `anonymous_fn` in `src/anti/fmt.c` now looks past the word.
- Adding a member to the runtime archive changes the linked bytes of
  `return42` on macos-arm64, although no snapshot code links into it. With
  the member left out the old digest came back. The pin was written from
  this Mac, as its message asks, and the emit manifest took the six
  `snapshots` listings.
- The first reflection record gave an `own fn` field the id of a function
  pointer of one word. A deserialized object then held code 0 there.

## Provisional entries added

Under "Anonymous functions and closures" in `docs/decisions.md`: the refusals
that name `snapshot fn`, a snapshot in the frame at a parameter that does
not keep and in a local, the types a snapshot holds, the offsets of its
`str` bytes, the moves of a `keep own` parameter, the free on `=` into an
`own fn` field, `dup` of a function value, `keep own` in lists and in the
library file, the count of the snapshots alive, the field record, and the
header declaration.

## Proof

The suites on `a8e2661`: the host 882 of 882, ASan 881 of 881, UBSan 881 of
881. The sanitizer builds leave out `no_paths` as before. Logs: `build/drive/logs/snap-final-host.log`,
`build/drive/logs/snap-final-asan.log` and
`build/drive/logs/snap-final-ubsan.log`.

After the push of the report:

```text
$ git log --oneline -3
fd80c8f Report snapshots and owned function values
a8e2661 Pass over an own fn field in reflection and serialize
2fc08ac Record snapshots and owned function values in the documents
$ git status --short
$ git rev-parse HEAD origin/main
fd80c8f1e974e26a38b8fb141818371d315dd0d0
fd80c8f1e974e26a38b8fb141818371d315dd0d0
```
