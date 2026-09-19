# Follow-ups to the ownership answers

Eddie's four follow-ups to `2026-09-19-ownership-and-none.md` are done, in order. The
development Mac passes 451 of 451 tests. The ASan and UBSan builds pass 450 each, without
`no_paths`. Neither VM ran this session.

## What was built

1. `614ddd6` commits `docs/anti-syntax-overview.md` as Eddie wrote it. `f5704ca` keeps it
   current: `none` for `null`, the sentence on `none` as a concept, and the rules both
   rounds added to the specifications. `CLAUDE.md` now says that a change to either
   specification changes the overview in the same commit.
2. `328a0a6`. An `own` slice destroys its elements last to first, as a local array does.
   "Destruction" states the one order. `delete_owned` expects it.
3. `0141306`. An inline class field without a default takes `T { }` when every field of
   `T` has a default or `T` has none, and `construct` runs on it. Any other such field is
   required in a literal. The init of `T` writes the default wherever a literal, `T(args)`
   or the init of the holder writes defaults. A class with a class field that no default
   fills carries a new flag, and `reflect.new` and `Object.deserialize` give `none` for it.
   Tests: `inline_default`, `inline_required` and a case in `std_registry`.
4. `074cc92`. `=` into a place that holds an owning value evaluates the new value,
   destroys the old one when its table is set, then moves the new bytes in. An array place
   is replaced element by element, last to first. `assign_owned` covers a local, an
   element of `alloc(T, n)` before and after it holds a value, an array and an inline
   field.
5. `91604fa` removes the tags on the entries the last report listed. The entry on existing
   values drops the sentence that `=` destroys nothing. The trap entry drops the sentence
   on inline values with a zero table.

## Provisional decisions for review

- A class whose `construct` takes arguments, a singleton and an abstract class have no
  `T { }` default, so a field of them is required. `reflect.new` and `Object.deserialize`
  give `none` for a class with a required class field.
- An owning value, for `=`, is one the end of a block tears down. Its chain declares
  `destruct` or owns memory, or it is an array of such values. The new value is complete
  before the old one goes.
- The teardown and the copy of a class still pass over an inline class value whose table
  is zero. A literal never leaves one, but `T(args)` leaves a required class field zero
  until its `construct` assigns it.

## Defects found and fixed

- The reader of a library file refused a class record with a flag above 15, so the new
  flag made `anti.args` unreadable. `std_args` showed it. The reader now accepts every
  named flag.

## Questions

- The overview's status lines predate the object model work. Classes says the model is
  built without interfaces. Interfaces, operators on classes and `singleton` say not built
  yet, and Reflection lists `call`, `new` and `Value` as not built. The `CLAUDE.md` state
  and the tests `interfaces`, `operators`, `std_reflect_call` and `std_registry` say
  otherwise. They are unchanged. Should sessions keep the status lines current too?

## Not done

- The teardown and the copy do not enter an inline struct or an inline array of class
  values.
- Passing or returning a value that owns memory by value copies its bytes.
- `check_docs.py` reports sentences in `src/ast.h`, `src/types.h` and `src/ir.h` that were
  there before. Each is a run of one-line field comments that the checker reads as one
  sentence.
- Carried: the field record has no visibility, `type_of(T)` does not exist, `equals` and
  `hash` pass over `str`, slices and inline values, and `serialize` writes a NaN as text
  that is not JSON.
