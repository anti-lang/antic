# Eddie's answers on nullable pointers

The five answers to `docs/reports/2026-09-20-nullable-pointers.md` are built. The
development Mac passes 454 of 454 tests, and the ASan and UBSan builds pass 453 each,
without `no_paths`. No VM has run this work yet.

## What changed

1. `?*Error` stands. Both specifications say it, and nothing moved.
2. The error is `anti.error.NoneDereference`, since `null` left the language, and its
   constructor is `new`. `Error.cause` is `protected` as well as defaulted, so a subclass
   sets it rather than leaning on the default.
3. A function value follows the pointer rule. `fn(...)` never holds `none`, `?fn(...)`
   may, and a call needs a checked one. The narrowing rules read it as they read a
   pointer. `?fn` is two tokens, because `fn` is a keyword and `as?` never takes a
   function type, so nothing is ambiguous. `Fatal.hook` of `anti.error` is a `?fn()` now,
   and `callbacks.anti` checks the function value it builds.
4. Narrowing follows `&&` and `||`. The right operand of `&&` reads the names the left
   proved true, the right operand of `||` the names it proved false, and `!a` swaps the
   two sets. The body of the `if` keeps what an `&&` chain proved and nothing of an `||`
   chain. A chain may prove up to eight names, and a longer one proves the first eight.
5. The four provisional tags are gone. Two more went with them, since answers 1 and 5
   settled those entries. Two remain for a later batch: the assignment that ends a
   narrowing across nested blocks, and the `?*T` of a `str` and a slice `ptr`.

## The rule that moved

`NoneDereference.new()` could not stand beside `Error.new(code, message)`, because a
class could not redeclare any name its chain held. The rule now reads: a class may not
redeclare a field, a function that takes `self`, or a constant that its chain already
has, except a `concrete fn`. A static function is namespaced by its class and may share a
name with a static in the chain, since `Class.f` names one of them and never the other.

`docs/anti-object-model.md` carries the new sentence. The check tests both sides, so a
static below a static passes. A static below a `self` function, a `self` function below
a `self` function and a field below a static are all still refused. Each has a test.

## Defect found

`let m = p else { }` on a `?fn(...)` crashed the compiler. Three places built the checked
type as a pointer to the element. A function type has no element, so they made a pointer
to nothing. They ask the type for its own checked form now.

## Questions for Eddie

- Nothing. The two provisional entries that remain are for a later batch and need no
  answer now.
