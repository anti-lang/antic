# Items 10 to 13 of the first sessions

Items 10 to 13 of "First sessions" in `CLAUDE.md` are done, in order. The development Mac
passes 439 of 439 tests, and the ASan and UBSan builds pass 438 each, without `no_paths`.
At `03d1064` the Linux VM passed 385 of 385. The Windows VM passed 367 and skipped
`sysroot_digest`, which a Windows host always skips. `emit_identity` passed on both.

## What was built

1. Item 10, `179f485`. `delete` deletes the object behind an `own` pointer, so its chain
   runs and it destroys what it owns in turn. A pointer to an interface is deleted from
   the start of its object. A class value held inline, as a plain or a `use` field, runs
   its chain in place, and `dup` copies it with its own `copy` entry. A local whose inline
   field needs a teardown is torn down at the end of its block. The sentence of
   "Destruction" now reads "destroys every owned object and frees every owned buffer".
   `lifetime.expected` expects the `destruct` of the owned `Node`. The new program
   `delete_owned` covers a chain of three, a pointer to an interface, inline and `use`
   fields, `dup` and a local.
2. `c8a66e1`. The reader of a library file makes its aggregates and symbolic values in an
   order that keeps both orders of the file. See the defects below.
3. Item 11, `b5219b5`. The module that declares a struct writes a descriptor for every
   struct it declares. Every other module refers to it by symbol. A unit test checks
   both sides. `shared_class` now links a struct of the library that a class of each
   module holds, in release and dev mode. Release mode drops the descriptors that nothing
   reaches. The IR listings and the unit tests that print a module with a struct show the
   new data. The literals after it are numbered higher.
4. Item 12, `03d1064`. The entry on the `str` that `Object.deserialize` reads says that
   its bytes come from libc until `anti.mem` exists, and that the form with an `Allocator`
   replaces it. The tag stays. The comment in `rt/registry.c` says the same.
5. Item 13. Both VMs ran the suite at `03d1064`, with the counts above.

## Provisional decisions for review

New in `docs/decisions.md`:

- A class value held inline, as a plain field or a `use` field, is an owned object.
  `delete` and `destroy` run its chain in place, and `dup` copies it with its own `copy`
  entry. A local whose inline field needs a teardown is torn down at the end of its
  block, although the rule for locals names only `destruct` and `own` fields.
- `delete` frees the buffer of an `own` slice of class values without destroying its
  elements, since `alloc(T, n)` gives them no table.
- The old entry on struct descriptors is split. The part that item 11 decides has no
  tag. The rest keeps it: a descriptor of a struct holds the name, the size and the field
  list, and a union and a `Job` have none.

The entry on what `delete` destroys records item 10 and has no tag.

## Defects found and fixed

- `dup` copied a class value held inline byte for byte, so the copy and the original
  shared every object it owned. `delete_owned` showed the copy's leaf as the original's.
- A library read and written again gave other bytes when its lowering had made a
  symbolic value before an array. A class descriptor does that with its size before its
  field list. The reader made every aggregate first. No test had put a class through a
  round trip, and `round_trip_class` now does. Item 11 gave every struct a descriptor,
  which made the old test `round_trip` fail the same way.

## Not done

- With `--no-reflect` a descriptor has no field list, so `delete` and `dup` find no
  `own` field. `delete` neither runs the `destruct` of an owned object nor frees it, and
  `dup` shares it. A program built both ways shows it. See the question below.
- `=` between two values of a class with `own` fields is accepted, which the
  specification refuses. Both values are then destroyed at the end of their block, and
  the owned object is freed twice.
- The walk of `delete` and `dup` does not enter an inline struct or an inline array. A
  class value inside either is neither destroyed nor copied.
- Carried from items 7 to 9: the field record has no visibility, `type_of(T)` does not
  exist, `equals` and `hash` pass over `str`, slices and inline values, and `serialize`
  writes a NaN or an infinity as text that is not JSON.

## Questions

- `--no-reflect` drops the field list, and `delete` and `dup` read it to find what an
  object owns. Should `--no-reflect` keep the records of the `own` fields and the inline
  class values? The other way is a teardown and a copy that the compiler writes per
  class, which no flag removes.
