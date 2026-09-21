# `construct` with `may fail`

The step after `may fail` in the rest of the standard library. `a82170b`
is the change. A `construct` with arguments that can fail is written
`fn construct(self, args...) may fail`, and a derived `construct`
initialises its base with `self.super.construct(args)` at the top of its
body.

## What the compiler does now

- `may fail` on a `construct` with arguments gives the ABI the
  hand-written form had, `?*Error construct(self, args)`, so calls,
  handlers, the `.antl` and dev mode needed no case of their own.
- A `construct` with arguments that names a result is refused,
  `-> ?*Error` included. A `construct` without arguments and a
  `destruct` refuse `may fail`. The old message for `destruct` advised
  `-> ?*Error`, which it has no use for, and now says it cannot fail.
- The definite-assignment check takes `return;` and the closing brace
  as the paths that succeed and `fail` as a path that needs nothing. The
  closing brace was never checked before, because the hand-written form
  had to end in a `return`. A block records the position of its closing
  brace for the message.
- `self.super.construct(args)` is accepted as the first statement of a
  `construct` alone. `try` forwards the base's error, so the body below
  stops there and `alloc` frees the object.
- `T(args)` takes the `construct` its class declares. The checker used
  to find the base's `construct` for a class that declares none, and
  lowering then ran nothing, so `alloc Derived(3) catch fatal` compiled,
  dropped the error and left the defaults. Such a call is now refused
  with the message the checker already had for a class without one.
- Lowering reads the result of a `construct` call from its type, not
  from the result the declaration wrote.
- An export class with a `construct` with arguments gets
  `anti_<Class>_construct(self, args...)`. It calls the init, then the
  `construct`, and returns its error. The header writes the prototype
  after `anti_<Class>_init` with the may-fail note, and declares
  `struct anti_Error;` when the class alone names it. The parameters of
  that `construct` follow the export rule whatever its level.
- `std_may_fail_only` no longer lets a hand-written `construct` through.
  `std/` has no `construct` with arguments, so no module changed.

## Tests

- `program_construct_args`: a `construct` that fails and one that
  succeeds, `alloc T(args) catch` with `yield none`, and `T(args) catch`
  with `yield` of a literal.
- `program_construct_base`: a derived `construct` that forwards a
  failing base with `try`, on the heap and as a value. A second one
  leaves the base call out and keeps the base defaults.
- `listing_error_construct_forms`: the result refused, `may fail` on a
  `construct` without arguments and on `destruct`, the base call second,
  twice and in another function, and `Heir(3)` on a class whose base
  alone declares a `construct`.
- `listing_error_construct_sets` in the new form, with `return;`, the
  closing brace, a derived class that skips its base, and a `construct`
  that cannot fail.
- `clib_failing` and `clib_classes`: C and C++17 call
  `anti_<Class>_construct` on both paths.
- The chain over three modules, `object_model`, `registry`, `own_copy`
  and `inline_required` use the new form.

## What failed and how it was fixed

- The red run failed 12 tests and `inherits_modules_{release,dev}`, as
  intended.
- `listing_error_construct_forms` printed two `may fail and never
  does` warnings after the refused `try` calls. Each of the two
  functions gained a real `fail`, so the listing holds the refusals.
- A probe showed the `Derived(3)` defect above. A test was written
  first, then `check_construct` was fixed.
- The full suite failed `emit_identity` for `construct_args` and the
  new `construct_base`. No other program changed its assembly. The
  manifest was rewritten on the Mac with `-DWRITE=yes`.
- The docs-style checker flagged four sentences over 25 words, which
  were split. It reads `#` lines of CMake as headings, so the one new
  comment of `tests/CMakeLists.txt` and `run_std_may_fail.cmake` went
  through `.py` copies. The `.anti` files went through `.rs` copies with
  `//!` written as `///`, since the checker takes `!` as punctuation.

## Gates

- Zero warnings in all three builds. ld prints the `-lto_library` note
  it printed before.
- Host: 528 of 528. ASan and UBSan: 527 of 527 each, without a
  sanitizer report.
- Logs under `build/drive/logs/`: `red.log`, `red-chain.log`,
  `build.log`, `test.log`, `emit_identity.log`, `emit-write.log`,
  `asan-build.log`, `asan-test.log`, `ubsan-build.log`,
  `ubsan-test.log`, `docs-style.log`, `docs-style-anti.log` and
  `docs-style-claude.log`.

## Provisional entries

In `docs/decisions.md`:

- Under "Object model": the top of the body is its first statement, and
  a call anywhere else is refused. `T(args)` runs the `construct` that
  `T` declares.
- Under "Failing functions": a `construct` that names a result is
  refused, and `may fail` on a `construct` without arguments and on
  `destruct`. `anti_<Class>_construct` prepares the object, returns
  nothing for a `construct` that cannot fail, and its parameters follow
  the export rule.
- Changed on this instruction: the entry that refused `may fail` on
  `construct`, the wording of the definite-assignment entries, and
  `std_may_fail_only`. The composition rule is recorded untagged.

## Questions

- The message `` `construct` of `Circle` returns `none` before it sets
  `r` `` is kept as the object model lists it. A `construct` that may
  fail never writes `none` now. Should it read "succeeds before it sets"?
- `anti_Circle_construct(c, args...)` keeps the order the object model
  gives. A `may fail` function puts its out pointer last. Is the object
  first the right order for this helper?
- `alloc T(args)` on a class without a `construct` with arguments prints
  a second message, "`alloc` of one object takes a literal or a class
  with arguments". That cascade predates this step and is left.
