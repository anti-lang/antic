# The parts every collection shares

`anti.collection` holds `Iterable<T>`, `Iterator<T>` and the parts every
collection of round five shares, as "Principles" and "Operations every
collection has" describe them. A small collection of the test proves each part
before the collections of their own modules exist.

## What was done

- `Iterable<T>` and `Iterator<T>` are constraints that a type meets through its
  hooks, `fn total<C: collection.Iterable<int>>(c: *C)`. A constraint now names
  a generic interface with its type arguments, and a walk through a `*C` of a
  type parameter works.
- `Collection<T>` is the base of a collection. It holds the allocator the
  collection is made with, `count` and the changes its iterator watches. It
  gives `is_empty`, `find_all`, `find_first` and `find_one`, `update_all`,
  `update_first` and `update_one`, `remove_all`, `remove_first` and
  `remove_one`, `clear`, `to_text` and `serialize`. `_one` fails with `NotOne`
  and its count, with the message of the specification. `find_all` gives a
  `Copies<T>`. `collection.equal` gives a collection module `==` for elements
  with `eq`.
- A collection fills five functions that reach an element by its place. Its
  room comes from helpers of the base that keep every empty slot zeroed and
  serve `new(capacity, from)`, `reserve(n)` and `shrink()`.
- The descriptor of a copy of a generic class records its type arguments.
  Four runtime functions text, serialize, copy and tear down one element by
  that record.
- Fixed on the way, each with a test. A static call on a copy of a generic
  class called a stray address. A generic class did not reach the protected
  members of its generic base. The receiver of an inherited function in a copy
  kept the open base. The variable of a range could not move into an `own`
  parameter. `anti fmt` wrote `fn(lent * T)` and split the type arguments of a
  constraint.

## Tests

- `std_collection_parts` and `std_collection_parts_dev` run `Bag<T>` over the
  base: a counting allocator, capacity, `reserve` and `shrink`, every
  criteria form with both failures of `_one`, `to_text` and `serialize` of
  numbers, texts, structs and class values, `==`, the teardown of each element
  exactly once and a walk through `Iterable<int>`.
- The check `collection_changed` stops a dev build where `remove_all` changes a
  collection that a `for` walks, and names both lines.
- `listing_error_walk_interfaces`, `program_generic_static` in both modes and
  the fixture of `anti_fmt`.

## What failed and how it was fixed

- A copy tears down a local of `T` at the end of its block, and `=` into a slot
  tears down what the slot held. The first `take_place` built its tuple from a
  local, and the first shift used `=`, and both freed an element twice. The
  base now moves elements as bytes, zeroes empty slots, and returns a tuple
  local.
- Abstract functions are always public, so the place protocol lends elements
  and gives copies rather than pointers.
- The descriptor grew by two items. The IR listings, the byte layouts of the
  unit tests, the digest of `return42` and the assembly manifest were written
  again from this Mac.
- Logs: `build/drive/logs/ctest-host-4.log`, `ctest-asan.log`,
  `ctest-ubsan.log`, `scratch-cp.log`, `lldb-cp.log` and `docs-4.log`.

## Provisional decisions

All under "Generics and collections" in `docs/decisions.md`: the two
interfaces as constraints met through the hooks, the type arguments of a
constraint, the place protocol, the results of the criteria forms and
`Copies<T>`, `NotOne`, the allocator and the capacity, the zeroed slots, the
`at` of a change, `==` through a generic `operator fn eq`, the text and JSON
forms, the type arguments in the descriptor, the receiver and the protected
members of a generic base, and the move of a range variable.

## Questions for Eddie

- "Principles" refuses `=` between two collections and copies one with `dup`.
  `own` memory goes back to the C library, and a replaced `copy` never
  allocates. So a collection holds its room from its allocator through a plain
  pointer, and `=` and `dup` share the room. How does a class own memory of an
  allocator it holds? Both stay not built until then.
- Is `Iterable<T>` meant as a constraint, as built, or as an interface that a
  `*Iterable<T>` walks through a table? The second needs an iterator object
  per walk.
- One module holds one `operator fn eq`, so a module with `Map` and `HashMap`
  cannot give both `==` as `List` gets it. Should a class body state `eq` on
  the condition that `T` has it?
- The copies a `to_text` of a class element makes are not freed, since a
  `to_text` may give a literal. Should a collection nested in a collection
  write its text into the builder instead?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1082 of
1082, ASan 1081 of 1081 and UBSan 1081 of 1081. The docs-style checker reports
nothing on every touched file but `tests/CMakeLists.txt`, whose `#` comments
it reads as Markdown headings, as before.

## Proof of the push

After the push of `3d3c8bc`:

```text
$ git log --oneline -3
3d3c8bc Report the parts every collection shares
d878be1 Build the parts every collection shares in anti.collection
05ff7eb Meet Iterable<T> and Iterator<T> of anti.collection through the hooks
$ git status --short
$ git rev-parse HEAD origin/main
3d3c8bc3c3e94a46a222a0e6b1df57b84a313f64
3d3c8bc3c3e94a46a222a0e6b1df57b84a313f64
```

Suite pass counts: host 1082 of 1082, ASan 1081 of 1081, UBSan 1081 of 1081.
