# Undo of a failing deserialize

This session carries out Eddie's answers to the two questions of
`2026-09-23-entry-corrections.md`.

## What changed

- `delete(p)` and `destroy(p)` without an allocator mean the C library's `malloc` and `free`
  on every system, the same as the language's `alloc` and `free`. The decision and the
  object model no longer name `LibcAllocator.get()` for that case. The code did not change,
  since it already passed zero for the C library.
- The two entries of the last session lost their `[provisional]` tag: the allocator as the
  second argument with its import, and the zero allocator of the teardown.
- A text that fails undoes what `Object.deserialize` built. The reader keeps a list of every
  object whose `construct` ran and of the field that owns it. On a failure it clears those
  fields and runs each teardown newest first with `anti_rt_give_nothing`, which gives no
  memory back. It then gives every block back to `from`. Clearing the fields first keeps the
  teardown of an owner from reaching an object that has its own place on the list.
- `std_deserialize_undo` fails one text in the last member of the outer object and one
  inside an owned object. `Kept` shows no block left, and `Tally` shows every piece of
  memory and every resource a `construct` took given back.
- The link identity digest of `return42` was written again, since the runtime changed.

No `[provisional]` entry was added.

## Commits

```text
653ce85 Say that no allocator means the C library, and settle the two entries
4a6d01d Undo what a failing Object.deserialize built
```

The first host run after the change failed `link_identity_macos-arm64` alone, on the
digest, which was written again from the Mac.

## Results

- Host: 787 of 787 pass, none skipped.
- ASan: 786 of 786 pass. UBSan: 786 of 786 pass. Both leave out `no_paths`, as before.
- docs-style: 0 findings on every file touched.

Logs are in the session scratchpad: `full5.log`, `asan2.log` and `ubsan2.log`.
