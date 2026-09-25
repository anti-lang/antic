# Decisions of the sequences

The decisions of the sequences of round five, `List<T>`, `Deque<T>`,
`Ring<T, N>` and `Grid<T>`, for a later step to fold into "Generics and
collections" of `docs/decisions.md`. Only `List<T>` is built. The step stopped
at the refusal of `==` described in `docs/reports/2026-09-25-add5-seq.md`.

- `List<T>` is built in `anti.collection.list`, from "Sequences" of
  "Collections" in `docs/anti-language-additions.md`, over
  `collection.Collection<T>`. `tests/std/collection_list.anti` runs every
  operation. It does not compile until `==` on a list reaches the operator of
  the module.
- [provisional] `sort`, `sort_by` and `map` are functions of
  `anti.collection.list`, which a call on a list reaches as its own:
  `l.sort()`, `l.sort_by(key)` and `l.map(f)`. `map` is written
  `map<T, U>`, so a call that names the types writes both,
  `l.map<int, str>(f)`. Reason: `sort` needs `T: Ordered`, which a function of
  the class body cannot state, and a library file carries no function of a
  class with type parameters of its own, which `sort_by` and `map` have.
- [provisional] `l.sort_with(less)` sorts by a function that says whether its
  first element comes before its second, and `sort` and `sort_by` call it. It
  is public. Reason: a function of the module reaches no private member of the
  class, and the checker refuses a lent slice passed to a `lent` parameter of
  a generic function, so the sort cannot run over `lend_slice`.
- [provisional] The sort is stable and allocates nothing: runs of 20 by
  insertion, then merges in place by rotations, the SymMerge of Kim and
  Kutzner. Reason: a sort that allocates can fail, and a merge in place costs
  a factor of log n.
- [provisional] `read(i, f)` and `modify(i, f)` give `false` and call nothing
  when `i` is outside the list, and `true` after `f` ran. Both lend the
  element in place. Reason: `get(i)` gives `none` outside the list, and the
  language has no pointer that only reads.
- [provisional] `map(f, from)` and `filter(test, from)` take an optional
  allocator last, and the new list takes its room from the C library without
  one. Reason: "Principles" gives every collection an optional allocator when
  it is made, with the C library as the default.
- [provisional] `l[i]`, `l[i] = x`, `remove_at(i)` and `insert(i, x)` index a
  slice of the elements, so a dev build traps outside the list with the text
  of an array, `index out of bounds: index 5, length 4`, at the line of
  `list.anti`. `insert` takes `count` as well, for the end. A release build
  checks nothing. Reason: "Sequences" traps as an array does, and the check of
  a slice follows the build that compiles the program.
- [provisional] `l[i] = x` tears down the element it replaces. A list that
  cannot grow because its allocator has run out stops the program, as
  `Copies<T>` does.
- [provisional] `push`, `pop`, `insert` and `remove_at` take
  `at: lang.SourceLocation = here` last, as every function of a collection
  that changes its size does. `sort`, `reverse`, `l[i] = x` and `modify` change
  no size and count no change.
- [provisional] `ListWalk<T>` is the public iterator of a list and lends each
  element in turn. The `operator fn eq` of the module is `pub`. Reason: a
  function of another module names the type of `iter`, and an operator of a
  module reaches another module only when it is public.
