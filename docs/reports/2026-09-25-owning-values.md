# Owning structs and tuples

Eddie chose option 1 of `docs/reports/2026-09-25-walk-owned-parts.md`, with a
rule. A struct or a tuple is owning when any of its parts owns something,
transitively. Such a value then follows the value rules of a class value, and
one that owns nothing stays plain C data. It is built and recorded in
`docs/decisions.md`, `docs/anti-object-model.md` and
`docs/anti-language-additions.md`.

## What was done

- `sema_needs_teardown` of `sema_stmt.c` is the one rule of what owns
  something. It covers a class value that needs a teardown (a collection is
  one), an `own fn`, and a `?T`, an array, a struct or a tuple with such a
  part. The refusal of `=`, the moves and the teardown of lowering all read
  it, and `lower_type_needs_destruct` now asks it.
- Lowering tears down, copies and clears an owning value part by part with
  `lower_destroy_owned`, `lower_copy_owned` and `lower_clear_owned`. That
  serves a local on every exit, `=` into a place, a moved local, an `own`
  parameter, a field of a class or a struct, an array element and the value
  of a `?T`. `dup` of a struct or a tuple copies it.
- A destructuring `let` hands each part to its name, so the tuple it takes
  apart is torn down once.
- The C header writes an owning struct with its layout and a comment that
  marks it as owning.
- A collection tears down and copies an owning element. `copy_element` and
  `destroy_element` of `anti.collection` now compile for each element type.
  `destroy(p)` takes a pointer to any type and tears the value down in
  place. `dup(x)` of a value of a type parameter copies the value, a pointer
  by its address. The runtime's `anti_rt_element_copy` and
  `anti_rt_element_destroy` are gone, since nothing calls them.

Commits: `e342d3a` the value rules, `787bce6` the collections.

## Tests

- `programs/owning_values.anti`, in release mode, in dev mode and under
  Rosetta. It runs under a leak check, the count of the leaves alive. For a
  struct and a tuple each, it checks:
  - a plain `let`, a return from a function and an early `return`;
  - an `own` parameter;
  - a field of a class, on the stack and on the heap, and of another struct;
  - an array and a `?T`;
  - `dup`, `=` into a place that holds one, and `break` out of a loop.

  It also runs `destroy` in place, and `dup` of a value through a generic
  with a struct and with a class pointer. It shows that `=` still copies a
  plain struct and a plain tuple.
- `errors/owning_values.anti` holds ten refusals: `=` and `let` of a struct
  and of a tuple, the repeat form, a parameter that does not move, a name
  after a move, `destroy` of a lent pointer and `delete` of a struct.
- `std_collection_parts` holds a collection of an owning struct and one of
  an owning tuple. The copies of `find_all`, `remove_all` and the end of the
  block tear down exactly what was made.
- A unit test reads the comment of the header for an owning struct and its
  absence for a plain one.

## What failed and how it was fixed

- `std_collection_parts` trapped once tuples were torn down. The hidden tuple
  of a destructuring `let` was torn down beside its names. It is not now.
- `dup` accepted a channel, since a channel is a built-in struct. `dup` of a
  channel, a `Mutex`, an object lock and a `Job` is refused again.
- The copy of a leaf inside a class copy dispatches no `copied` hook, as
  before. The test counts the leaves that `dup` makes by hand.
- The format went to version 70 for the mark of `dup` of a value. The two
  pinned library files changed in the version byte alone. The digest of
  `return42` was written again for the smaller runtime, and the manifest for
  `owning_values` and `walk_owned_parts`.

## Proof

- Host suite: 1110 of 1110 before each of the two commits.
- Sanitizers, once for the push at `787bce6`: ASan passed 1109 of 1109 and
  UBSan passed 1109 of 1109, the host suite without `no_paths`.

## Provisional decisions

All in `docs/decisions.md`:

- what makes a class value owning, and that a union owns nothing;
- the messages, and the four handles `dup` refuses;
- the destructuring `let`, the arrays held in a class, and no `copied` hook
  for a part;
- the comment of the header;
- `destroy(p)` of any pointer, `dup(x)` of a value of a type parameter, and
  the runtime functions removed.

## Questions for Eddie

- `destroy(&c)` on a local leaves it to its block, which tears it down again.
  A scratch program runs the `destruct` of a class local twice, and of the
  class value inside a struct local twice. That was so for a class value
  before this work. Should the block pass over a local that `destroy` took,
  or should `destroy(&c)` of a local be refused?
