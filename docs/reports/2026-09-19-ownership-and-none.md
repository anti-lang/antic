# Ownership answers and the rename to none

Eddie's six answers of 2026-09-19 are done, in order, one commit and one push each. The
development Mac passes 446 of 446 tests. The ASan and UBSan builds pass 445 each, without
`no_paths`. Neither VM ran this session.

## What was built

1. `d5fb5ff`. `=` refuses to copy an existing value of a class that owns memory, with the
   message the specification lists. A fresh value on the right, a literal, `T(args)`, a call
   or `*dup(p)`, is a move. The refusal covers `let` and an assignment. "Ownership and
   copies" carries the sentence. The test `own_copy` checks each form.
2. `9d42254`. `alloc(T, n)` of a class calls `calloc`. A struct and a primitive keep
   `malloc`. The unit test `allocates_zeroed_classes` checks the four forms.
3. `02cb2ce`. `delete`, `destroy` and `dup` take the descriptor of the class the program
   holds the object as, and trap in every mode on a zero table with `table not set: the
   object is no Square`. With `--dev`, lowering checks the table before every dispatch, a
   bound function, `is`, `as` and the `fatal` of a handler. `lower_module` takes options. The
   test `table_unset` runs each case in release and dev mode.
4. `ff7f84c`. A local array whose element class needs a teardown destroys its elements at
   the end of its block, last to first, through arrays of arrays. "Destruction" says so.
   The test `array_local` covers it.
5. `df06502`. Lowering writes `Class.destroy` and `Class.copy` for every complete class
   and puts them in the `destruct` and `copy` entries of its table. The runtime calls the
   entries and no longer walks the field list. `delete_owned` runs again with
   `--no-reflect` and prints the same. `owned_modules` checks a base, an inline field and
   an `own` slice from another module in release and dev mode.
6. `4ff98c5`. `none` is the keyword, and the lexer refuses `null` with "`null` is `none` in
   Anti". The type name, the messages, the AST listing, every program, the standard
   library, both specifications and `docs/decisions.md` say `none`. "Pointers and
   conversions" and the entry on the literal carry the sentence on `none` as a concept and
   zero as its encoding. JSON keeps `null`.

## Provisional decisions for review

- An existing value is a variable, a field, an element or the target of a pointer other
  than `*dup(p)`. Every call result is fresh. An array whose elements own memory is refused
  as its class is. `=` does not destroy the value it replaces.
- `=`'s refusal also covers an element of an array literal and a field of a literal. The
  repeat form `[v; n]` is refused when `v` owns memory. Item 4 made each of them a double
  free.
- An array of arrays is torn down as one run of its innermost elements.
- The trap names the static class. The runtime calls take its descriptor. An object behind
  an `own` pointer and each element of an `own` slice are checked too. A class value held
  inline with a zero table is passed over.
- The teardown and the copy are named `Class.destroy` and `Class.copy`. A chain that
  declares `copy` keeps it. The C header's `anti_Circle_delete`, `anti_Circle_destroy` and
  `anti_Circle_dup` reach them through the runtime and the table. The root's `copy` copies
  bytes alone. `dup` of an interface pointer gives the same interface in the copy.
- `reflect.none()` is `reflect.nothing()`. "Nullable" and `/* non-null */` stay.

## Defects found and fixed

- `dup` of a pointer to an interface returned the start of the copy, typed as the
  interface. A call through it read the wrong table. `delete_owned` now checks it.
- The copy of an `own` slice of length zero kept the original's pointer, so both freed it.
  The copy now holds `none`.

## Questions

- The syntax overview is not in this repository. Chapter 2 of the book, in
  `FoundingFuture/book-writing-a-compiler`, has the "Pointers" section that says "The literal
  `null` is the pointer that points to no value" and uses `null` 15 times. `CLAUDE.md` says
  the book is not a concern here, so it is unchanged. Should this repository's sessions edit
  it?
- An `own` slice destroys its elements first to last, and a local array last to first.
  Should the slice follow the array?
- A literal leaves an inline class field without a default zero, so its value has no table
  and no `construct` ran. Should such a field get `T {}` as its default?
- `=` over a value that owns memory leaks what the old value owned. Should `=` destroy the
  old value, which would trap on an unfilled element of `alloc(T, n)`?

## Not done

- The teardown and the copy do not enter an inline struct or an inline array of class
  values, so those leak rather than free twice.
- Passing or returning a value that owns memory by value copies its bytes. The refusal
  covers `=`, `let` and literals alone.
- `check_docs.py` reports five sentences in `src/ast.h` and `src/types.h` that were there
  before. Each is a run of one-line field comments that the checker reads as one sentence.
- Carried: the field record has no visibility, `type_of(T)` does not exist, `equals` and
  `hash` pass over `str`, slices and inline values, and `serialize` writes a NaN as text
  that is not JSON.
