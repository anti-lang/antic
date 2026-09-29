# Generic parameters of the caller, and the specifications

The session built Eddie's two answers to the questions of
`docs/reports/2026-09-29-owning-variants-and-generic-moves.md`.

## What changed

- `7740b15`. In generic code `let`, `if let`, a destructuring `let` and
  `=` of a parameter that is not `own` and whose type holds a type
  parameter are refused for every `T`. The message names both fixes:
  `` `p` belongs to the caller and does not move. Mark it `own` to take it
  over, or copy it with `dup(p)` ``. `sema_refuse_caller_value` of
  `sema_expr.c` holds the rule. A parameter of an owning class type keeps
  its message.
- Twelve test sources and the worker example of the overview stored or
  accumulated a non-`own` generic parameter. They now copy it with `dup`,
  which keeps their signatures and the header of `clib_generics`. Three
  programs emit other assembly for the copy, and the manifest of
  `emit_identity` holds it.
- Tests. `errors/caller_params.anti` holds the refusals. It was accepted
  before the change, and `borrowed<Key>(k)` with `let a = p;` crashed.
  `programs/caller_params.anti` runs both fixes with `T = Key` and with
  `int` under a leak check. It is a test of the fixes and passes on the
  old compiler too.
- `04ba12c`. The object model, the additions and the overview now carry
  every owning-value rule of the last rounds. They say what is owning and
  that a variant is torn down by its tag. They give the copy an arm binds,
  the moves into literals and the moves of generic locals. They give the
  refusal of a generic parameter of the caller, with `dup` as the copy. `CLAUDE.md` says that a decision that
  changes the language goes into the specifications in the same commit.

## Gates

At `7740b15` no build shows a warning, and the host suite passes 1248 of
1248 (`build/scratch/suite5.log`). At `04ba12c` ASan passes 1247 of 1247
(`build/scratch/asan3.log`) and UBSan 1247 of 1247
(`build/scratch/ubsan3.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- The refusal covers `let`, `if let`, a destructuring `let` and `=`, and a
  parameter whose type holds a type parameter in place, such as `?T`.

## Questions

Each of these copies the bytes of a non-`own` parameter, and each crashed
in `build/scratch/mv/rp*.anti`, with `T = Key` where it is generic. The decision named `let`
and `=`, so the session left them as they are.

- A generic parameter in a literal, `(p, 1)` or `Pair<T> { first: p }`,
  and as the argument of an `own` parameter, `take<T>(p)`. The rule for an
  owning class parameter refuses both. Should the generic parameter follow
  it? A first try that refused them, with `return`, stopped dozens of test
  sources.
- `return p` of a non-`own` parameter, generic or not. `class_return`,
  which returns its parameter `p: Key`, is accepted and crashes, although the object
  model says that a parameter that is not `own` does not move and that
  `return` moves. `max<T>(a: T, b: T) -> T` returns a parameter in many
  tests. Should `return` refuse it, with `own` or `dup` as the fix?
- A class value has no copy that is itself a value. `dup(p)` of a `Key`
  is refused, since `dup` takes a class pointer. `*dup(&p)` counts as a
  fresh value, and it leaks the 32 bytes that `anti_rt_dup` allocated.
  `leaks` shows one block more than the same program without it. Should
  `dup` of a class value give a value, as it does for a struct?
