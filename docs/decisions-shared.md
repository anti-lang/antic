# Decisions of shared ownership

The decisions of the session that builds `Shared<T>` of `anti.mem`, from "Shared
ownership" of round five in `docs/anti-language-additions.md`. A later step folds them
into "Generics and collections" of `docs/decisions.md` and updates the status lines. An
entry marked `[provisional]` was taken where the documents are silent and is binding
until Eddie reviews it.

## Shared ownership

- `anti.mem.Shared<T>` is built. `Shared<T>.new(value)` takes the object and gives the
  first handle, `share()` gives another and counts it, `read(f)` and `modify(f)` lend
  the object, and the handle that takes the count to zero tears the object down through
  its `destruct`. The count is atomic, so the handles of one object are shared and freed
  on more than one thread at once. `=` between two handles is refused. The leak of two
  shared objects that hold each other stands in the documentation of the class.
  `tests/std/shared.anti` runs in both modes, as `std_shared` and `std_shared_dev`, and
  `listing_error_shared_handles` holds the refusals.
- [provisional] A handle is a `final` class value with one field, `own cell: ?*Cell<T>`.
  The private class `Cell<T>` of `anti.mem` holds the atomic count and the object
  inline, in one allocation from the C library. The `own` field makes `=` and `let`
  refuse to copy an existing handle through the rule of "Ownership and copies", with
  `` `Shared<Circle>` has `own` fields, use `dup` instead of `=` ``. `destruct` frees the
  cell when it holds the last count and leaves `none` in the field in every case, so the
  teardown of the `own` field frees nothing. Reason: the specification refuses `=` and
  the rule of `own` fields already refuses it, with no change to the compiler.
- [provisional] `dup` of a handle and the copy a collection makes of an element count
  one more, as `share()` does. `Shared<T>` replaces `copy`. Reason: `find_all`,
  `find_first` and `copy_place` give copies that own what they hold. A copy that took
  no count would free the object once per copy.
- [provisional] `read(f)` and `modify(f)` both take `fn(lent *T)` and lend the same
  object for the call. The rule that `T` be thread-safe for more than one thread to
  change the object is documented and not checked. Reason: the language has no pointer
  that only reads, so the two differ by intent, and the checker cannot tell a read from
  a change through a lent pointer.
- [provisional] `count()` gives the number of handles of the object at the moment of the
  call. Reason: the specification has `share()` increment the count visibly, and a
  program sees it through a read. On more than one thread the value may be stale when
  it arrives.
- [provisional] `Shared<T>` takes no allocator. The cell comes from the C library.
  Reason: "Shared ownership" names none, and "Principles" gives one to the collections
  alone.
- [provisional] `anti.mem` imports `anti.lang`, for the `catch fatal` of a handle whose
  cell is gone. A dev link that names the object of `anti.mem` names that of
  `anti.lang` too. Reason: `anti.collection` checks its room the same way, and
  `unreachable` is not built.
