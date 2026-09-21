# Answers to the questions of items 20 to 24

The five answers to `docs/reports/2026-09-22-first-sessions-20-to-24.md` are in the code and
the documents, one commit each. The development Mac passes 610 of 610 tests, and the ASan
and UBSan builds pass 609 each, without `no_paths`. Every commit that changed code
ran the full suite first, and each push that carried one ran both sanitizer suites. The VMs
did not run.

## What was done

1. Answer 1, `efeef7a`. The entry on `by -k` lost its tag.
2. Answer 2, `860d2b1`. A handler that passes its error to a parameter of type
   `own ?*Error` moves it there. Lowering reads the error at the move and writes `none` into
   the handler's copy, so the delete of every later exit passes over it, on whichever path
   of the handler the move stood. `Error.wrap` takes `own cause: ?*Error`, and
   `trace/origin.anti` passes `e` without `dup`. `programs/catch_moves.anti` moves errors
   through `fail`, `yield`, `break`, `return`, a `try` block, a pointer guard and one path of
   an `if`. Each error prints its own delete and the count of live errors is 0 after each
   case, so a leak and a second delete both show. `errors/own_moves.anti` holds the
   refusals. The library file records the `own` parameters, version 38, and the header
   writes `/* own */` before one.
3. Answer 3, `95caec0`. `docs/anti-object-model.md` says under "Fields" that a class field
   may be a bitfield as a struct field may, and the overview says it with the classes.
4. Answer 4, `c1765c4`. `fn(A) -> R may fail` is a type. It holds the ABI form, `?*Error`
   and the out pointer, with two flags that belong to its key, so lowering needed no change.
   The provisional entry on calls through a function value is gone.
   `programs/fn_may_fail.anti` calls through a variable, a parameter, a field, a `?fn`, a
   bound function and a result, with `catch`, `try`, `catch fatal` and a `try` block.
   `errors/fn_may_fail.anti` refuses a `may fail` function where `fn(int) -> int` or the
   hand-written ABI type is expected, a plain function where the failing type is expected,
   and an unhandled call through a value. `clib_failing` passes a C function of the ABI form
   to `failing_apply`, and `tests/dump/failing.h` pins its prototype.
5. Answer 5, `9731e1d`. The entry on `fail` in a handler lost its tag. Answer 2 had already
   changed its text: the `dup(e)` clause is gone and a `fail` after a move deletes nothing.

## Defect found

The library file wrote no flag of a function type. A `?fn(int) -> int` parameter of a
library came back as `fn(int) -> int` in the importing module, which refused `none` for it.
`c1765c4` writes a byte of flags, version 39, and `modules/failing` calls
`counter.apply_or(none, 4)`.

## Provisional decisions for review

New in `docs/decisions.md`:

- `own` stands before the name of a parameter, as before a field, on a pointer or a slice. It
  belongs to the function and not to its type, as a default does, so a call through a
  function value moves nothing.
- A moved error is not named after the move. The checker refuses a mention later in the
  text and a move inside a loop of the handler. It refuses a move while a `defer` or an
  `undo` of the handler names the error.
- `may fail` after a function type belongs to that type, so `fn f() -> fn(int) -> int may fail`
  returns a failing function and does not fail itself.
- A value of `fn(A) -> R may fail` holds the ABI form, and `fn(A, *R) may fail` is another type.
- The library file writes a byte of flags after a function type.

## Questions

- The specification fills an abstract function with a `concrete fn` "of the same signature".
  The checker compares no signature: a `concrete fn area(self, k: i32)` fills
  `abstract fn area(self, k: int)` without a message. With `own` parameters a replacement can
  also differ in what it takes over. Should the checker compare the two, `own` included?
- The entry on `by k` in "Core language" still gives the procedure that starts `by -k` at
  `hi`, which the settled entry on `by -k` overrides. Should that sentence change to the
  largest value, as the code does?
- Every link of antic on the host still prints `ld: warning: ignoring -lto_library`, as the
  last report said.
