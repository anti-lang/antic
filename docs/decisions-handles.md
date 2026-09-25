# Decisions of the collections on handles and the heap

These entries belong under "Generics and collections" in `docs/decisions.md`,
where a later step folds them in. They come from "Stable elements" and
"Ordered retrieval" of "Collections" in `docs/anti-language-additions.md`.

- `Pool<T>` and `Handle<T>` are built in `anti.collection.pool`, `Tree<T>` in
  `anti.collection.tree` and `PriorityQueue<T>` in `anti.collection.queue`.
  Each module is a file under `src/std/anti/collection/`, and the build writes
  its library file under `std/anti/collection/` of the runtime archive.
  `tests/std/collection_pool.anti`, `collection_tree.anti` and
  `collection_queue.anti` run in release and dev mode, and
  `errors/handles.anti` holds the refusals.
- [provisional] `ANTIC_STD_MODULES` of `CMakeLists.txt` names a module below
  another by its path, `collection/pool`, and the build makes the directory of
  each library file. Reason: a module path mirrors the directories under a
  search root, and the specification names `anti.collection.pool` beside
  `anti.collection`.
- [provisional] `anti.collection.pool` holds `Slots<T>`, a public abstract
  class over `collection.Collection<T>` with the storage, the handles and the
  reading and lending forms. `Pool<T>` and `Tree<T>` inherit it, and
  `SlotsWalk<T>` is the iterator of both. Reason: "Stable elements" gives a
  tree the forms of a pool on handles, and a tree that inherited `Pool<T>`
  would offer `add` and a `remove` that gives one element.
- [provisional] A pool starts with a block of 8 slots, or of `capacity`
  rounded up to a power of two, and each later block holds twice the one before.
  One allocation holds a block's elements and then 3 words of `int` per slot:
  the generation, the link of the free list and the version. A tree adds 5
  words: its parent, its first and last child and its two siblings. A block is
  allocated when the first slot of it is taken, and `new` allocates the first
  one when `capacity` is above 0. The directory holds 48 blocks, more than any
  memory fills. Reason: "Stable elements" asks for blocks that double and for
  elements that never move, and one allocation per block keeps the words of a
  slot beside it.
- [provisional] Slot `i` lies in block `h - shift` at `j - 2^h`, where `j` is
  `i + 2^shift`, `2^shift` is the size of the first block and `h` is the
  highest bit of `j`. Six comparisons find `h`. Reason: the language has no
  count of leading zeros, and six steps stay the few bit operations the
  specification names.
- [provisional] `Handle<T>` is `struct Handle<T> { index: int, generation:
  int }`. A removal increments the generation of its slot, and the free list
  gives the slot freed last first. A handle finds an element when its index is
  below the slots handed out, the slot holds an element and the generations
  agree. Reason: 64 bits of generation do not wrap in the life of a program,
  so a handle never finds the wrong element.
- [provisional] `Handle<T>` has no `==`. A program compares `index` and
  `generation`. Reason: a module holds one `operator fn eq`, and that of
  `anti.collection.pool` is the one of `Pool<T>`.
- [provisional] The versioned set is `set_if_version(h, own x, version) may
  fail`, by position. It fails with `pool.Changed` and tears `x` down when the
  element changed or is gone. `Changed` inherits `anti.lang.Error` with
  `expected` and `found`, which is -1 for an element that is gone, and its
  message is `` the element changed from version 0 to 1 `` or `` the element
  of version 3 is gone ``. Reason: the language has neither named arguments
  nor overloading, so `set(h, own x, if_version: v)` needs a name of its own.
- [provisional] A version counts the changes of an element since it was added,
  from 0. `set`, `set_if_version`, `modify`, the three update forms and each
  turn of a `for` over the element count one. `get`, `read`, `get_versioned`
  and the `find` forms count none. The generation stays a count of removals, so
  a set keeps the handles of the element. Reason: a compare and set must see
  every change. `for x in c` and `for x in &c` reach the element through one
  `value` hook, so a walk counts as a change.
- [provisional] `set(h, own x)` gives `false` and tears `x` down for a stale
  handle. `read(h, f)` and `modify(h, f)` take a `fn(lent *T)` and give
  `false` for a stale handle. `get_versioned(h)` gives `?(T, int)`. Reason:
  "Stable elements" gives `false` or `none` for a stale handle and leaves the
  result of the lending forms open.
- [provisional] `for` over a pool or a tree walks the slots in increasing
  order. The places of the shared functions are the slot indices. Reason: the
  order of the slots is the one order both share without a walk of their own.
- [provisional] `Tree<T>.add_child(parent, own x)` gives `?Handle<T>`, which is
  `none` when `parent` finds nothing, and tears `x` down then. `parent(h)`
  gives `none` for a root. `children(h)`, `depth_first(h)` and
  `breadth_first(h)` give a `collection.Copies<pool.Handle<T>>` from the
  allocator of the tree, which holds nothing for a stale handle. `depth_first`
  gives an element before its children, and both orders start at `h`.
  `remove(h)` gives the number of elements it tore down, and 0 for a stale
  handle. Reason: the specification names the functions and leaves their
  results open, and handles are what a tree is addressed by.
- [provisional] A shared `remove` form of a tree, and `clear`, take out the
  element that matches alone. Its children take its place among its siblings,
  under its parent. Reason: a criterion removes the elements that match, and
  `remove(h)` is the form that removes a subtree.
- [provisional] `PriorityQueue<T: Ordered>` keeps one array with a spare slot
  that swaps go through. It grows from 8 or from `capacity`, doubling.
  `new(capacity, from)` takes the room at once, and there is no `reserve` or
  `shrink`. Reason: "Principles" gives those two to `List`, `Deque`, the maps
  and the sets.
- [provisional] `for x in q` gives a copy of each element in the order of the
  array, through `operator fn value(self) -> T`, and the loop tears each copy
  down. `for x in &q` is refused with the message of a `value` that gives a
  copy. `lend_place` restores the order after it lends, and `update_all` and
  `remove_all` restore it once after all their changes. Reason: a change in
  place would break the order of the heap without the queue knowing.
- `==` of the three collections is declared, `pub operator fn eq<T: eq>` in
  each module and `T: Ordered` for the queue, and is not reachable from a
  program. A generic `operator fn` read from a library file loses the mark of
  `operator fn`, since `symbol_is_operator` of `src/antic/sema_expr.c` reads
  the mark of the declaration in the tree of the generic, where it is not set.
  The fix lies in `src/antic/`, outside this step.
