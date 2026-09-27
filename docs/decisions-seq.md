# Decisions of the sequences

The decisions of the sequences of round five, `List<T>`, `Deque<T>`,
`Ring<T, N>` and `Grid<T>`, for a later step to fold into "Generics and
collections" of `docs/decisions.md`. All four are built.

- `List<T>` is built in `anti.collection.list`, `Deque<T>` in
  `anti.collection.deque`, `Ring<T, N>` in `anti.collection.ring` and
  `Grid<T>` in `anti.collection.grid`, from "Sequences" of "Collections" in
  `docs/anti-language-additions.md`, each over `collection.Collection<T>`.
  `tests/std/collection_list.anti`, `collection_deque.anti`,
  `collection_ring.anti` and `collection_grid.anti` run every operation in
  both modes. `checks/list_changed.anti` holds the trap of a list that grows
  while a `for` walks it, and `checks/list_bounds.anti` and
  `checks/grid_bounds.anti` the traps of an index.
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
- [provisional] Each sequence replaces `copy` of `Object`: the copy takes room
  of its own from the same allocator and a copy of each element, so `dup(&l)`
  and an element that is itself a collection copy what they own. Reason:
  "Principles" copies a collection with `dup`, and the copy of `Object`
  copied the pointer to the room, which both then freed.
- [provisional] `Deque<T>` holds its elements in a ring from the slot `head`
  of its room. `push_front`, `push_back`, `pop_front`, `pop_back`, `front` and
  `back` work at the ends without moving the other elements, and a removal
  inside moves the elements after it one slot back. Growing, `reserve` and
  `shrink` move the elements, first to last, to the start of the new room. A place
  is the position from the front. `Deque<T>` has no index. Reason:
  "Sequences" gives a deque the six functions of the ends alone.
- [provisional] `Ring<T, N>.new(from)` takes room for `N` elements from `from`
  in one allocation and stops the program when the allocator has run out,
  `N` below 1 among those cases. No function of the ring allocates after
  that. `find_all` and `to_text` make a new collection and a new text, which
  take room of their own. `push(own x)` into a full ring gives `false` and
  tears `x` down. `pop` and `peek` take and read the oldest element. Reason:
  "Principles" gives every collection an optional allocator when it is made,
  and room in the object would make every element's slot part of the class,
  torn down with it.
- [provisional] `Grid<T>.new(width, height, fill, from)` takes room for
  `width * height` cells and one slot more, which holds a copy of `fill`, in
  one allocation. A width or a height below 0 counts as 0. `width` and
  `height` are public fields, as `count` is. Reason: "Sequences" writes them
  without parentheses, and the fill value is needed again by the next item.
- [provisional] A grid never changes its size. `remove_all`, `remove_first`,
  `remove_one` and `clear` of a grid move the elements out of their cells, as
  they do in any collection, and put a copy of the fill value into each cell
  they empty. `count` stays `width * height`, and no change is counted, as for
  `g[x, y] = v`. Reason: "Operations every collection has" gives every
  collection the remove forms, and a two-dimensional array has no cell to
  take away.
- [provisional] `g[x, y]` and `g[x, y] = v` check `x` against `width` and `y`
  against `height`, each as the index of a slice, so a dev build traps with
  the text of an array and a release build checks nothing. Reason: a check of
  the index into the whole buffer lets a column past the width through when a
  row follows.
- [provisional] `==` of two grids needs the same `width` and `height` beside
  equal elements in order. Reason: two grids of different shapes can hold the
  same elements row by row.
