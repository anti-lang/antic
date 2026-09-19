# Items 3 to 6 of the first sessions

Items 3 to 6 of "First sessions" in `CLAUDE.md` are done, in order. The development Mac
passes 434 of 434 tests, and the ASan and UBSan builds pass 433 each, without `no_paths`.
The build has zero warnings on the Mac, on Linux and on Windows.

## What was built

1. One descriptor and one set of tables per class, `096d3fb`. A dev object of the module
   that links held a local copy of the data of every module it loaded. It now holds only
   its own, as global hidden symbols, and names the data of other modules as external
   symbols. `shared_class_release` and `shared_class_dev` test `is`, `==`, `equals` and
   `reflect.new` on a class of another module, from both modules.
2. `inherits module.Class`, `cde2a29`. A base of another module needed three more things
   that library files did not carry, so they carry them now, in format version 24: the
   value of every field default, and the `construct` and `destruct` of a class at any
   level. A table entry that a library declares, and that is not abstract, is filled
   where it was null. `inherits_modules_release` and `inherits_modules_dev` build a chain
   over three modules and test dispatch, `super`, `is`, the fields, `construct` and
   `destruct`. `error_inherits_module` tests the refusal.
3. Both VMs ran the suite at `cde2a29`, `a246c7b`. Linux passed 381 of 381, and Windows
   passed 364 and skipped `sysroot_digest`, which a Windows host always skips.
   `emit_identity` passed on both.
4. `reflect.call` and `Value`, `0075d8a`. Each record of a function list holds the text of
   its signature. When the program reaches the runtime's call, the pass over the whole
   program writes one trampoline per text of the program and the table
   `anti_rt_trampolines`. Such a program reaches every slot of every abstract class.
   `std_reflect_call` covers each kind of `Value`, and `shared_class` calls across modules
   in both modes. `docs/notes/whole-program.md` holds the choices inside the pass.

After item 6 both VMs ran the suite again. Linux passed 382 of 382 at `226fe66`. Windows
passed 365 with the fix of `9dbf0dc` and skipped `sysroot_digest`.

## Provisional decisions for review

New or changed in `docs/decisions.md`:

- A library file carries field defaults, and `construct` and `destruct` whatever their
  level. The checker now evaluates every default, so a default that is not a constant
  expression is refused, as the specification says.
- `anti.reflect` has `call` and `Value`. `get` and `set` still use `int`, because a field
  record holds a width and no type.
- `Value` is a struct of a `ValueKind` and a union `Payload` until `variant` exists. A
  signed integer travels as `Int` at 64 bits, an unsigned one as `Uint`, and `f32` as
  `Float` at 64 bits.
- `call` takes the index of the function list and gives kind `None` for every call it
  cannot make.
- A function record holds the text of its signature, as `i64.i32.str`. The trampolines
  are keyed by that text.
- The entry on dev mode now says that the data of a module, as well as its functions, is
  global and hidden.

## Defects found and fixed

- In dev mode, the object that links copied every descriptor and table it loaded. The
  whole-program report of 2026-09-19 named this one.
- A literal of a class of another module required every field, because defaults did not
  cross library files. It may not name the private and protected ones at all.
- A class whose base lives in another module skipped the base's `construct`. It left the
  inherited entries of its table null, and it did not destroy a local whose base had
  `destruct`.
- AddressSanitizer and UBSan caught the used-slot pass reading its reach past the
  functions that the trampolines add, `226fe66`.
- Windows refused to compile a comparison of a type kind, which is a signed enum there,
  with a `size_t`, `9dbf0dc`. The Mac compiled it without a warning.

## Not done

- `get` and `set` in terms of `Value`. It needs a field kind for each type.
- `reflect.call` through an interface pointer, and `self.super.construct(x)` of a base in
  another module. Both should work and neither has a test.
- In dev mode, a program that imports a module of the standard library must be given its
  object. The tests build those objects themselves until the `anti` tool exists.

## Questions

- Should a failed `call` give something other than kind `None`, which a function without a
  result also gives?
- Should `Value` become a `variant` once sum types exist, with the same layout?
