# Five things the collection lanes need

Eddie named five things the collection lanes of round five need. Each is
built with its tests, recorded in `docs/decisions.md`, and written into
`docs/anti-language-additions.md` where it changes the round.

## What was done

- `lent` on a slice parameter, `fn(lent []T)`, under the rules of `lent` on a
  pointer. `lent []T` is a form of the slice type with the layout of `[]T`,
  and a part of a lent slice is lent as well.
- Hooks with more than one index. `operator fn index(self, x: int, y: int)`
  makes `g[x, y]` read and the matching `set_index` makes `g[x, y] = v`
  write, with any number of indices of any types. The table of language
  hooks says so.
- The build compiles every `.anti` file under `src/std/anti/collection/` by
  itself. `tools/std-modules.cmake` orders the modules by their imports of
  `anti.collection.<module>`, and a cycle stops the configure step.
- `str` has the hooks `eq`, `lt` and `hash`. `==` compares the text, `<`
  compares the bytes, which for UTF-8 is the order of the code points, and
  `str` meets `eq`, `Ordered` and `hash`.
- `for (k, v) in m` over any iterator whose value is a tuple, and over a
  slice and an array of tuples. `for (k, v) in &m` gives `k` as a copy and
  `v` as a lent pointer.

Commits, in order: `f177c39` the collection modules, `fd8b4d0` the hooks of
`str`, `6e77d1c` `lent []T`, `1c45381` the hooks with more than one index, `a275d2e` the tuple
pattern of `for`.

## Tests

- `std_collection_modules` orders a tree of its own, refuses a cycle and finds
  a library file for every module of the real tree.
- `programs/str_hooks.anti` runs `==`, `!=`, the orderings with prefixes and
  UTF-8, and each hook through a generic.
- `programs/lent_slices.anti` and `errors/lent_slices.anti`. `owned_modules`
  lends a slice across a module, and the page of `anti doc` shows
  `lent list: []Circle` from the source and from the library file.
- `programs/index_several.anti` and `errors/index_several.anti`.
  `generics_modules` takes a generic with `g[x, y]` from a library file.
- `programs/for_patterns.anti` in release and dev mode and
  `errors/for_patterns.anti`. `generics_modules` takes both forms of the
  pattern in a generic from a library file.
- For each change to the library format, removing the new field from the
  reader made `anti_doc` or `generics_modules` fail. With the field back,
  both pass.

## What failed and how it was fixed

- `anti fmt` wrote `fn(lent[] int)`. It now takes the `lent` of a function
  type as the opening of a type, and `fmt/loose.anti` pins `fn(lent []int)`.
- `raw_output` refused an `execute_process` of the new test without
  `ENCODING NONE`.
- `fd8b4d0` was committed while `emit_identity` failed. Its run stopped at
  the uncommitted `lent_slices` and never reached `str_hooks`, which had no
  entry in the manifest. `6e77d1c` added both entries. No existing entry
  changed in any of the five commits.
- The runtime gained `anti_rt_compare_bytes`, so the digest of `return42` was
  written again from this Mac. The library format went from version 66 to
  69, one step per change, so the version bytes of `test_modules.c` and the
  two pinned library files were written again each time. Only the version
  byte changed in `scale.antl.hex`. `pick.antl.hex` also gained the byte of
  the key mark on each symbol of its tree.

## Proof

- Host suite before each commit: 1083, 1085, 1088, 1091 and 1095 tests. Each
  passed apart from the failures above, which were fixed before their
  commits. The last run passed all 1095.
- Sanitizers, once for the push at `a275d2e`: ASan passed 1094 of 1094 and
  UBSan passed 1094 of 1094, which is the host suite without `no_paths`.

## Provisional decisions

All in `docs/decisions.md`:

- the order of the collection modules;
- the prefix rule and the runtime calls of `str` comparisons;
- `lent []T` as a form of the slice, its parts, its `ptr` and the slice byte
  of the library file;
- the indices of `set_index`, the one index of `g[(x, y)]` and the tuple of
  the parser;
- the pattern over slices, arrays and iterators, its names in the `&` form
  and the fields of the tree.

## Questions for Eddie

- `for (k, v) in &m` needs the `value` of the iterator to give
  `lent *(K, V)`, a pointer to an entry that holds key and value together.
  `Map` keeps its entries in one array, so it can. A `SortedMap` whose
  B-tree nodes hold the keys and the values in two arrays cannot. Should
  `value` be able to give `(K, lent *V)` as well?
- A lent slice gives out its `ptr` as a plain pointer, which the checker does
  not follow, as it does not follow `&` into a lent object. `lend_slice`
  needs it for C. Is that hole acceptable, or should `ptr` of a lent slice
  be a lent pointer that C refuses?
- In the `&` form a pattern of three parts or more gives every part after
  the first as a lent pointer. Is the first part always the key?
