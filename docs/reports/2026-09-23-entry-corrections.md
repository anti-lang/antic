# Entry corrections

This is the step that `2026-09-23-deserialize-destroy.md` blocked on. Eddie's decision
settles it: memory goes back to the allocator it came from, and whoever chose the allocator
says which one.

## What changed

- `delete(p, from)` and `destroy(p, from)` are built. The teardown in the `destruct` entry
  takes the object and a `*anti.mem.Allocator`. Owned objects go through
  `anti_rt_delete_from`, owned buffers through `anti_rt_give`, and the elements of an `own`
  slice through `anti_rt_destroy_elements` with the allocator. Zero is the C library, which
  `delete(p)` and `destroy(p)` pass. `dup` and `delete` of a channel refuse a second
  argument, and a module that passes one imports `anti.mem`.
- A deserialised object's `destruct` runs. `std_deserialize_alloc` calls `delete(t, &kept)`,
  `destroy(lent, &lender)` through the sub-object of an interface and `destroy(a, &arena)`
  before `free_all`. `Scratch` takes memory in its `construct` and keeps it in a `transient`
  field, which the reader leaves alone, and `Holder` holds a resource. `Tally` shows both
  given back. `std_destroy_from` covers every kind of owned field outside `deserialize`.
- The allocator form of `f"..."` is gone. `f"..."(x)` is a call of a `str` and is refused
  with `` cannot call `str` ``. `std_builder_take_in` replaces `std_format_alloc`, and
  `errors/format_call` replaces `errors/format_alloc`.
- The `[provisional]` tag is gone from every entry the step listed.
- The emit identity manifest, the link identity digest of `return42` and the four `devirt`
  listings were written again, since the teardown takes a second parameter.

Two `[provisional]` entries were added under "Object model" in `docs/decisions.md`. One
holds the allocator as the second argument and its import. The other holds the zero
allocator of the teardown, and the C header keeping one parameter.

## Commits

```text
a66cced Remove the provisional tag from the anti.mem entries
457b6af Drop the allocator form of f"..."
cb244c4 Give owned memory back to the allocator it came from
```

Each code commit ran the full suite on the host, and the tree of the last one ran both
sanitizer suites. The first run of the host suite failed seven tests: the identity digests,
the devirt listings and `fmt_canonical` on the rewritten test. The digests and listings
were written again from the Mac, and `anti fmt` fixed the test. The first split of the
commits left the overview's context block in the wrong commit, which `overview_examples`
found, and it was moved.

## Results

- Host: 786 of 786 pass, none skipped.
- ASan: 785 of 785 pass. UBSan: 785 of 785 pass. Both leave out `no_paths`, as before.
- docs-style: 0 findings on every file touched, `tests/CMakeLists.txt` aside, whose `#`
  comments the checker reads as headings and where only list lines changed.

Logs are in the session scratchpad: `full3.log`, `asan.log`, `ubsan.log`, `fullA.log` and
`fullB.log`.

## Questions for Eddie

1. The decision says `destroy(p)` and `delete(p)` without an allocator mean
   `LibcAllocator.get()`. They give the memory to the C library's `free`, which is the same
   on macOS and Linux. On Windows `LibcAllocator.free` is `_aligned_free`, which memory of
   `malloc` does not take, and `alloc` takes from `malloc`. The provisional entry records
   the C library. Should `alloc` take its memory from `LibcAllocator` instead, so the two
   are one allocator everywhere?
2. A text that fails after the reader prepared an object gives every block back without a
   `destruct`. Memory or a resource that a `construct` took then leaks. The test of the
   failing text uses `Branch`, a class without either. Should a failing `deserialize` run
   the destructs of what it prepared first?
