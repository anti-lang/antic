# Nullable pointers

"Nullable pointers" of `docs/anti-language-additions.md` is built, every rule of it. The
development Mac passes 454 of 454 tests, and the ASan and UBSan builds pass 453 each,
without `no_paths`. No VM has run this work yet.

## What was built

1. The type. `?*T` is a flag in the key that interns a pointer, so `*T` and `?*T` are two
   types that compare by their pointer and share one layout. `?*` is one lexer token, which
   settles `p as?*Circle` in favour of the type. `p as? *Circle` keeps the space.
2. The rules. `*T` refuses `none`, and `?*T` is refused where a dereference, an index, a
   field, a call, `dup`, `delete` or `destroy` would read through it. The message is the
   one the specification gives: `` `n` may be `none`, check it or use `?*T` ``. A `*T`
   passes where a `?*T` is expected, through a base and through an interface as well.
3. Narrowing. Each block records the names a check proved, and the record dies with the
   block. `if p != none { }`, `none != p`, the `else` of `if p == none { }` and a branch
   that leaves all narrow. An assignment to the name ends the narrowing in the block that
   holds the record, so a nested block ends it for the blocks outside as well.
4. The binding forms. `let m = p else { }` binds the checked pointer and refuses an `else`
   that does not leave. `p catch fatal` and `p catch e { yield q; }` follow the error
   forms, with `anti.error.NullPointer`, a new class that inherits `Error`. A module that
   writes the form imports `anti.error`, and the message says so when it does not.
5. The results. `alloc(T, n)` gives `?*T` and `alloc T { }` gives `*T`. `is`, `as` and
   `as?` take a `?*T`: `none` is of no class, so `is` is false, `as?` gives `none`, and
   `as` traps as it does on any other mismatch. The `ptr` of a `str` and of a slice is
   `?*T`, since a slice of no elements holds no address.
6. The C boundary. Every pointer of an `extern fn`, and of the C callbacks in its
   signature, is `?*T`, which the checker refuses otherwise. An export item keeps `*T`, and
   the header writes both as `T *` with `/* non-null */` on the first.
7. The library file. A pointer carries its nullability, and `ANTL_VERSION` is 25.
8. `std/` and every test program carry honest types. A failing function returns `?*Error`,
   because `none` is its success, and the error a handler binds is the `*Error` of that
   result. `Error.cause`, `List.at`, `Map.get`, `reflect.parent`, `reflect.new` and the
   rest say what they mean.

## What it cost at run time

`emit_identity` rewrote 9 of the 45 programs across its six targets. Those 9 are the
programs where a check was added to the source. The other 36 changed only the spelling of
a signature and emitted the same bytes, which is the specification's "nothing is emitted"
measured rather than assumed. `let m = p else { }` emits the comparison and the branch the
program wrote. `is`, `as` and `as?` add one comparison to a test that already branches.

## Defects found

- A `catch` on a failing call whose result is itself a `?*T` was taken for a pointer guard
  and checked twice. `std/anti/log.anti` stopped compiling, and only a rebuild that
  regenerated `log.antl` showed it: the first run reused the stale library file. The
  checker now marks the guard on the call, and `tests/std/error.anti` covers a failing
  function with a `?*T` result.
- `is` on a `none` pointer read the table behind it and crashed. `class_test` now reads
  the table after the pointer proves to be there.

## Decisions taken under the gap procedure

`docs/decisions.md` carries them, each `[provisional]` with its reason. In short:
narrowing reads a name and not a field path, follows `!=` and `==` and not `&&` chains,
and covers `while` as it covers `if`. `dup`, `delete` and `destroy` need a checked
pointer while `is`, `as` and `as?` do not. A bound struct has no marker in the language,
so the C rule is checked on `extern fn` signatures alone.

## Questions for Eddie

1. `docs/anti-object-model.md` said a failing function returns `*Error`, `none` on
   success, which the nullable rule makes impossible. Both documents now say `?*Error`.
   This is the one place where implementing the newer section changed the older
   specification, and it deserves your eye.
2. `anti.error.NullPointer` lives in `anti.error` and carries `make()`, which the compiler
   calls. `docs/anti-language-additions.md` puts the class in `anti.lang` once that
   namespace exists. Is `anti.error` the right home until then, and is `make` the right
   name beside `Error.new`?
3. A function value may still be `none`: `let f: fn() = none; f();` compiles. The section
   covers pointers and says nothing about function types. Should `?fn()` exist, or should
   a function value never hold `none`?
4. `Error.cause` now defaults to `none`, so a subclass can write `alloc NullPointer { }`
   without naming a private field of its base. Without the default no subclass could build
   itself, since `cause` is private to `Error`.
5. Narrowing does not follow `if p != none && p.n > 0`. It is the shape a program reaches
   for next, and the rule for it is yours to give.
