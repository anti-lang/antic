# Items 7 to 9 of the first sessions

Items 7 to 9 of "First sessions" in `CLAUDE.md` are done, in order. The development Mac
passes 437 of 437 tests, and the ASan and UBSan builds pass 436 each, without `no_paths`.
Neither VM ran the suite in this session.

## What was built

1. Item 7, `04598c2`. A field record holds a type id instead of a width. The low byte
   names the type, and the byte above names the type that a pointer, a slice, an array or
   an enum is built on. The unit test `records_type_ids` pins the numbers of `src/lower.c`
   to `rt/object.h`. A struct that a class field names has a descriptor with a field list.
   `reflect.get` gives a `Value`, `reflect.set` takes one and returns `*Error`, and `Field`
   has `type_id` and `element`. `std_serialize` writes and reads back a class with every
   field kind that item 7 lists, and an `own` pointer and an `own` slice of structs and of
   `str`.
2. Item 7, `0e9df42`. `dup` and `delete` of `own` fields, the defects below.
3. Item 8, `e209853`. `shared_class` calls through a pointer to an interface in both
   modes. It passed at once. It fails when `rt/call.c` reads the table at the interface
   pointer.
4. Item 8, `39e6c9c`. `inherits_modules` calls `self.super.construct(x)` on a base in
   another module in both modes. The checker refused it, in one module as well, and now
   accepts it. Three unit tests of `test_sema.c` cover the rule.
5. Item 9, `820b140`. `reflect.call(object, index, args, out: *Value) -> *Error`, with an
   error per reason. The decisions record Eddie's answers without the tag.

## Provisional decisions for review

New in `docs/decisions.md`:

- The encoding of the type id. An enum in the byte above is written as its integer, so a
  walk knows the size of `[]Mode`. A bitfield has type id `None`.
- Each module whose field records name a struct writes that struct's descriptor as its own
  data. A program may hold one copy per module. Structs have no descriptor otherwise, and
  an exported struct has none either.
- `get` gives `None` for a bad index and for a type that no `Value` carries. `set` returns
  an error with code `BAD_INDEX`, `NOT_CARRIED` or `WRONG_KIND`.
- The forms of the default `serialize`. A `char` is a string of one character. An inline
  struct is an object without `type`. An `own` pointer is followed and an `own` slice is an
  array. A pointer the object does not own is its address. A slice it does not own is
  `{"address":A,"length":N}`. A union, an array, a bitfield and an `own` slice of class
  values or of slices are `null`.
- `Object.deserialize` gives each `str` field bytes on the heap that nothing frees. A
  number outside the range of its field fails the whole text.
- A class below reaches the `construct` of its base through `self.super` whatever its
  level, as if it were `protected`.
- `call` gives `BAD_INDEX` for a null entry, and `NOT_CARRIED` as a fourth reason for a
  signature with a type that no `Value` carries.

## Defects found and fixed

- `serialize` wrote a `u8` of 200 as -56 and the largest `u64` as -1. It wrote a `bool`
  as 0 or 1, and every `str`, slice and inline struct as `null`, which lost them in a
  round trip.
- `dup` copied an `own []i32` of four elements as four bytes. It copied an object behind
  an `own *Shape` at the size of `Shape`, which cut a `Circle` short.
- `dup` left a copied pointer to an interface at the start of the new object. `delete`
  freed such a pointer at its interior address, and the allocator stopped the program.
- `self.super.construct(x)` was refused twice over. The base's `construct` had no level,
  so it reached its own class alone. The receiver check read `&self.super`, the form after
  the call took the address, and so never saw `self.super`.
- The comments of `rt/reflect.h` and `rt/object.h` said that a unit test pinned the field
  kinds to `src/ir.h`. None did. `records_type_ids` pins the type ids now.

## Not done

- The field record has no visibility, which the specification lists. `type_of(T)` does
  not exist.
- `dup` and `delete` do not reach the `own` fields of an inline class field. An `own`
  field whose element has no size in its type id, such as an array, is copied shallow.
  The copy and the original then free it twice.
- `equals` and `hash` still pass over `str`, slices and inline values.
- `serialize` writes a NaN or an infinity as `nan` or `inf`, which is not JSON.
- The comment on `enum anti_entry` in `rt/object.h` says that a unit test pins it to
  `root_names`. No test does.

## Questions

- Should `delete` delete the object behind an `own` pointer, running its `destruct` and
  freeing its `own` fields, instead of freeing its memory? The specification says that
  `delete` frees the `own` fields, and `lifetime.anti` expects the plain free. An owned
  object's `destruct` then never runs and its own `own` fields leak.
- Should a struct have one descriptor in a program, written by the module that declares
  it, as a class does? Every module would then write one for each struct it declares.
