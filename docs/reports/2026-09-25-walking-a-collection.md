# Walking a collection

The language part of "Walking a collection" of round five is built. `for x in
e` gives a read-only copy of each element and refuses a change that would reach
the copy alone. `for x in &e` lends each element for one turn of the loop. Both
hold for slices, arrays and any type with the `iter` hook. The compiler gives
the collections the iterator's view of their change count and the trap of a
dev build that names both places.

## What was done

- The copy form refuses `=` and every compound assignment to the variable and
  to what it holds inline, with `` `p` is a copy of each element of `people`.
  Walk with `&people` to change the elements ``. The parser keeps the source
  of the walked expression for the message.
- `for x in &slice` and `for x in &array` give `lent *T`. A lent loop variable
  is refused where a pointer is kept, with `` `v` is lent for one turn of the
  loop and cannot be stored ``.
- An iterator lends its elements through `operator fn value(self) -> lent *T`.
  `for x in &c` binds the pointer, and `for x in c` and `to_slice` copy what it
  points at. `lent` before any other result is refused.
- `anti.lang` declares `Changes` and `Watch`. A `for` over an iterator with a
  `Watch` field compares the two counts before every `next`. A dev build stops
  with `` walk_changed.anti:68: `bag` was changed while `for` walked it:
  changed at walk_changed.anti:70 `` through `anti_rt_walk_changed`. A release
  build carries neither the test nor the text.
- The library format is version 65. `anti fmt` keeps `-> lent *T` together.
- A whole program keeps the definition of a datum whose declaration it keeps.
  This fixed a release link that lost the descriptor of an `anti.lang` struct
  held by a class of a library file.

## Tests

- `programs/walk_forms.anti` walks an array, a slice and a lending collection
  in both forms, the index form, the `while` form and `to_slice`, in release
  and dev mode. `programs/walk_generic.anti` does it for a generic collection
  with a `Watch` and from generic functions.
- `errors/walk_copy.anti` and `errors/walk_lent.anti` hold the refusals.
- `checks/walk_changed.anti` in `checks` traps in dev mode, runs to its end in
  release mode and leaves no text in the release build.
- `iteration_modules` walks a lending collection of a library file in both
  modes.

## What failed and how it was fixed

- `*value()` was checked a second time, which checked the call again and
  failed. The copy now takes its type by hand.
- A class field of type `Changes` needs a default, since a struct field takes
  none. The tests write the struct literal.
- The copies of a generic did not clone the new parts of an iteration, and
  the IR failed verification. `xiter` clones them.
- `iteration_modules_release` failed to link with the descriptor of `Watch`
  missing, which the optimizer fix above cures. The dev modes and `checks`
  link the dev object of `anti.lang`.
- `antl_scale`, `antl_generic`, the unit tests of library files and of the
  checker, `emit_identity`, `link_identity_macos-arm64` and `fmt_canonical`
  needed version 65, the new message, the manifests written again from this
  Mac and canonical sources. `anti fmt` wrote `lent * T` until `fmt.c` read
  `lent` after `->`.
- Logs: `build/drive/logs/walk-full1.log` to `walk-full3.log`,
  `walk-asan-test.log` and `walk-ubsan-test.log`.

## Provisional decisions

All in `docs/decisions.md` under "Generics and collections": the lent result
of `operator fn value` and the copy through it, the reach of the refusal of
the copy form, `Changes` and `Watch` with the first `Watch` field of the
iterator, the check before every `next` with its text, and the library format
65. The entry that refused `for x in &e` over a collection names its
successor.

## Questions for Eddie

- A collection records the place of a change through a parameter `at:
  lang.SourceLocation = here` of each changing function. Should the compiler
  write the place itself?
- `Changes` has no defaults, since a struct field takes none, so a collection
  writes the whole literal as the default of its field. Should `anti.lang`
  give a constant for it?
- A call of a function on a copy is not refused. Is that the line?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1053
of 1053, ASan 1052 of 1052 and UBSan 1052 of 1052. The docs-style checker
reports nothing on every touched file but `tests/CMakeLists.txt`, whose `#`
comments it reads as Markdown, as before.
