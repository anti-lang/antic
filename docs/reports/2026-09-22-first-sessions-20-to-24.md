# Items 20 to 24 of the first sessions

Items 20 to 24 of "First sessions" in `CLAUDE.md` are done, in order, each with a test.
The development Mac passes 604 of 604 tests, and the ASan and UBSan builds pass 603 each,
without `no_paths`. Every commit that changed code ran the full suite, and every push ran
both sanitizer suites first. The VMs did not run.

## What was done

1. Item 20, `2fa60c1`. The error a handler binds is the exit action of a scope around the
   handler, so `yield`, the closing brace, `break`, `continue`, `return` and `fail` all
   delete it. `return e` and `fail e` hand it on and skip it, as `return` skips the local
   it hands on. The flag that marked a handler with a `return e` is gone. It covered the
   whole handler, so a handler that passed the error on one path leaked it on the others.
   `yield` now also runs the deferred statements and teardowns of the blocks inside the
   handler, which it skipped. `programs/catch_exits.anti` counts the live errors with a
   `destruct` and takes every exit of a call handler, a `try` block handler and a handler
   without a name. The count is the leak check, because the programs antic writes carry no
   sanitizer. `trace/origin.anti` passes `dup(e)` to `Error.wrap`, since the handler now
   deletes the `e` a bare wrap would own.
2. Item 21, `7a8ee73`. `is_failing` asks the symbol of the callee for `may fail`, the check
   of a `construct` asks its item, and `try` and `fail` ask the function being checked.
   `makes_error`, the entry on `Error.new` and the error exit of a `return` in a function
   written `-> ?*Error` are gone. `errors/failing.anti` refuses `try` and `fail` in such a
   function and a bare call of a `may fail` method and static. `programs/error_values.anti`
   calls ordinary functions that return `*Error` and `?*Error` without a handler.
   `Box.make` of `out_slot.anti` may fail, so no output shows the zero table of a `catch`
   binding any more, as the item foresaw.
3. Item 22, `1a09f45`. Verified: `for i in 0..10 by -3` gives `9 6 3 0`, and lowering
   already starts at the largest value. `programs/by_reverse.anti` prints the values for
   that case, a span `k` divides, an offset low bound, negative bounds, a step longer than
   the span and two empty ranges, with constant bounds and bounds read at run time. The
   entry keeps its `[provisional]` tag for the review.
4. Item 23, `6783c4e`. Verified: a `may fail` function returning `(int, int)` takes one out
   pointer. `ir_tuple_out` requires the line
   `export fn com.example.tuples.tuples_divide(%0: i64, %1: i64, %2: ptr) -> ptr {`, and
   `clib_tuples` pins `struct anti_Error *tuples_divide(int64_t a, int64_t b, struct
   anti_tuple_int_int * /* non-null */ out)` in the header and calls it from C on both paths.
5. Item 24, `83da593` and `6d5a697`. The entry and the code disagreed. A union, an array, a
   bitfield and an `own` slice of class values or of slices are written as `null` and keep
   their default. A pointer, a function pointer and an `own` slice without elements are
   written as `null` too, but the reader stores `none` or an empty slice, which is the value
   they had. The two agree unless the default is not `none`: a `?fn` field with a default
   function comes back `none`. The entry changed, not the code, because a round trip of the
   text needs the value. A `str` is always a string, `""` included. `std/serialize_null.anti`
   holds every kind at a value other than its default.

## Defect found

A bitfield declared in a class body was read and written as its whole unit. The place of a
field took the bitfield path for a struct alone, and the preparation of an object and
`T(args)` stored each default to its offset. A read gave the unit, a write cleared its
neighbours, and `Object.deserialize` kept the last default of a unit alone. `83da593` fixes
all three, and `programs/class_bitfields.anti` covers a class and its base.

## Provisional decisions for review

New in `docs/decisions.md`:

- `fail` in a handler is an exit of it. `fail e` passes the error on, and any other `fail`
  deletes it, so a handler that wraps its error passes `dup(e)` as the cause.
- A call through a function value is ordinary, whatever function the value holds, since a
  function type carries no `may fail`.

## Questions

- R2, the entry on `by -k`, was open until item 22 confirmed it. It is confirmed. May the
  tag come off?
- Should `Error.wrap` take the caught error without a copy? It would need a form that tells
  the handler the error has moved, which `fail e` and `return e` have and a call does not.
- The specification names bitfields in a struct. A class body accepts them too, and they
  now work. Should a class body refuse them instead?
- A function value of a `may fail` function has no failing call. Should a function type
  carry `may fail`, as `?fn` carries `none`?
- Every link of antic on the host prints `ld: warning: ignoring -lto_library`, with the path
  of a `libLTO.dylib` that the pinned clang lacks. No change of this session touches the
  link.
