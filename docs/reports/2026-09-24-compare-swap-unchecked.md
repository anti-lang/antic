# compare_swap on an unchecked field

Eddie decided that `compare_swap` works on a plain field of one machine word
that `unchecked` marks. The entry under "Warnings" in `docs/decisions.md` now
says so, without the tag, and the syntax overview lists it as built. The logs
are under `build/drive/logs/`, named `cas-*`.

## What was built

- `unchecked_swap` in `src/antic/sema_call.c` takes `compare_swap` on a plain
  field of an integer, a `bool` or a pointer that `unchecked(unguarded-field)`
  marks in a concurrent class, after its type or in the class header. It
  builds the node of an atomic field, so lowering calls
  `anti_rt_atomic_compare_swap` as for an atomic field, and it counts as a
  write, so the clause is used.
- Any other plain field is refused with `` `compare_swap` takes an atomic
  field, and `n` is a plain one: mark it `atomic`, or
  `unchecked(unguarded-field, "reason")` in a concurrent class ``. A marked
  field of another type is refused with the types it takes.
- The parser reads `unchecked` after the type of a struct's field. The node of
  a lock-free queue swaps its `next`, and a struct field refused the clause.

## Tests

- `programs/lock_free_queue.anti` is the spec's `Queue`, with `head`, `tail`
  and each node's `next` swapped by `compare_swap`. Eight chunks of a
  `parallel` push 8000 values and pop as many, so the values cross threads. It
  runs in release, dev and on x86_64 through Rosetta.
- `errors/compare_swap.anti` holds the refusals on a guarded field, a fixed
  field, a field of a plain struct and an `f64`. A class whose header clause
  lets `compare_swap` through compiles in it.
- `emit_identity` carries the six listings of the new program, written again
  on the Mac. The other manifest lines did not change.

## What failed and how it was fixed

- `fmt_canonical` refused the two new sources until `anti fmt` wrote them.
- `emit_identity` failed on the new program until the manifest held it.

## Provisional entries added

One, under "Warnings" in `docs/decisions.md`. The clause counts after the
type or in the header of a concurrent class or of a type nested in one. The
call counts as a write, and a struct field takes the clause. A field whose
class declares its own `compare_swap` keeps that call.

## Gates

- Build: zero warnings from the compilers in `host`, `asan` and `ubsan`.
- host: 902 of 902 passed, `build/drive/logs/cas-ctest-host.log`.
- ASan: 901 of 901 passed, `build/drive/logs/cas-ctest-asan.log`.
- UBSan: 901 of 901 passed, `build/drive/logs/cas-ctest-ubsan.log`.
- The docs-style checker reports nothing on every file this step touched.
