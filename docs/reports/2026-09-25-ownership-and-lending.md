# Ownership at a call and lending

"Ownership at a call" and "Lending" of round five are built. `own` before a
parameter takes a value of any type and moves a local passed there. `lent`
before a pointer parameter gives it the type `lent *T`, which the checker
keeps out of every place that would keep the pointer.

## What was done

- `own` takes any type. A local or a parameter passed to one moves, and a
  mention after the move in the order of the text is refused with
  `` `c` was moved into `shapes` by `push` ``, or `` `c` was moved into
  `take` `` without an object. A move inside a loop, under a `defer`, of a
  captured local and inside a closure are refused. A parameter that is not
  `own` does not move a value that owns memory, and a field or an element
  that owns memory is refused as `=` refuses it.
- A moved value that needs a teardown goes to the call through a slot of the
  frame, and the local's table is cleared. The function tears its `own`
  parameter down at every exit unless it moves on by a call, `=`, `let` or
  `return`. A local that may have moved is torn down only when its table is
  not zero.
- `lent *T` is a form of the pointer type. It passes on to a `lent`
  parameter and as the object of a call, and it is refused at `=`, a typed
  `let`, a literal, `return`, `own`, a parameter without `lent`, `delete`,
  a snapshot and a type argument. `fn(lent *T)` is a type of its own, and an
  anonymous function takes `lent` from its target.
- The library format is version 64. The header writes `/* lent */` and
  `anti doc` writes `lent` before the name.
- `docs/notes/own-and-lent.md` holds the choices of the passes.

## Tests

- `programs/own_params.anti` moves `Box` values that count themselves alive
  into free functions, a function of a class, a generic class and a generic
  function, by a call, `=`, `let` and `return`, on one path of an `if` and as
  an `int` and a pointer. The count ends at 0 and each teardown prints once.
- `programs/lent_params.anti` lends through `read` and `modify`, an
  anonymous function, a named one, a field of function type, a local, a
  closure, a comparison, a test for `none` and `dup`.
- `errors/own_params.anti` and `errors/lent_params.anti` hold every refusal.
- `owned_modules` moves and lends across a library file in both modes, and
  `errors/own_modules.anti` holds the refusals through it.

## What failed and how it was fixed

- The first host run failed 15 tests. `listing_error_snapshots` showed a
  `keep own` argument counted as a move of a local. `note_move` now leaves a
  parameter of function type to the rules of a snapshot. `antl_scale`,
  `antl_generic`, the unit test of library files and `emit_identity` needed
  the version 64 and the manifest of the two new programs, written again
  from this Mac. `fmt_canonical` wanted three new files in canonical form.
  Log: `build/drive/logs/own-ctest1.log`.
- A generic class copy tore its `own` parameter down after `=` stored it,
  since the copy reads the moves of its generic, where `T` owns nothing.
  `=` and `let` from an `own` parameter of a type parameter now move.
- `hold(p)` inferred `T` as `lent *Person` and let the pointer through a
  plain parameter. Inference now takes the plain form.

## Provisional decisions

All in `docs/decisions.md` under "Generics and collections": the two forms of
the message of a moved local, the four refused moves, the refusal of a plain
parameter and of a value that stays where it is, moves by `=` and `let` of an
`own` parameter, the zero table of a moved local, an `own` pointer taken over
by rule, `lent *T` as a form of the pointer, a lent pointer passed on to a
`lent` parameter alone, `fn(lent *T)` as a type of its own and a plain type
argument, `as`, `dup` and `delete` of a lent pointer, where `lent` stands,
and the library format.

## Questions for Eddie

- A plain pointer parameter refuses a lent pointer, and the object of a call
  takes one, though a function of the class may keep `self`. Is that the line?
- `anti doc` writes `lent` and not `own` before a parameter. Should it write
  `own` too?
- The message `` `push` takes a `Circle`, and `c` is a `*Circle` `` of the
  list is left to the collections, which have the `push` it names. Is that
  its place?

## Gates

No compile warning on the host, ASan and UBSan builds. The linker prints
`ld: warning: ignoring -lto_library`, as before. The host suite passes 1045
of 1045, ASan 1044 of 1044 and UBSan 1044 of 1044. Logs:
`build/drive/logs/own-ctest3.log`, `own-asan-test.log` and
`own-ubsan-test.log`. The docs-style checker reports nothing on every
touched file but `tests/CMakeLists.txt`, whose `#` comments it reads as
Markdown, as before.

## Proof of the push

After the push of `e50b79b`:

```text
$ git log --oneline -3
e50b79b Report own and lent parameters
1c652c8 Record own and lent parameters in the decisions and the overview
4103553 Build own parameters of any type and lent parameters
$ git status --short
$ git rev-parse HEAD origin/main
e50b79baa9166602eb2ce5d15d5bfa796b2e6758
e50b79baa9166602eb2ce5d15d5bfa796b2e6758
```
