# Every parameter that is not own stays with its caller

The session built Eddie's answers to the questions of
`docs/reports/2026-09-29-caller-params-and-specifications.md`.

## What changed

- `336b2f8`. A parameter that is not `own` leaves its function in no way
  that gives it a second owner. `let` and `=` refuse it, and so do a
  tuple, struct, class, variant or array literal, an `own` argument and
  `return`. It holds for a parameter whose value owns something and, in
  generic code, for one whose type holds a type parameter. One message
  names both fixes: `` `p` belongs to the caller and does not move. Mark
  it `own` to take it over, or copy it with `dup(p)` ``.
  `sema_refuse_caller_value` of `sema_expr.c` holds the rule, and
  `sema_refuse_owned_copy`, `sema_move_local` and the check of `return`
  call it.
- A value that owns nothing and holds no pointer is copied into an `own`
  parameter and does not move. `moves_at_call` of `sema_call.c` holds the
  rule. So `max<int>(a, b)` leaves `a` and `b` usable. A pointer, a
  channel and a Mutex still move.
- `dup(x)` of a class value gives a value, copied as a class value held
  inline is, and dispatches `copied`. `dup(p)` of a pointer is unchanged.
- The standard library compiled under the rule unchanged. Test sources
  that returned or stored such a parameter now take it `own`, or copy it
  with `dup` where they name it twice. So do the examples of the overview
  and the additions. `modules/generics/pick.antl.hex` records the `own`
  marks of `pick`. `errors/own_params.anti` no longer refuses an `int`
  named after it went to an `own` parameter, as the answer says.
- Tests. `programs/caller_params.anti` failed first on the `int` locals
  given to `max<int>` and on `dup` of a class value. It now runs
  `max<Key>`, a copy returned, in a tuple, handed to an `own` parameter,
  a class value copied three ways and the `int` locals, under a leak
  check. `errors/caller_params.anti` holds every way out for a generic
  and a class parameter. It accepted the literal, the `own` argument and
  both `return` forms before the change, and each of those crashed with
  `Key` in `build/scratch/mv/rp*.anti`.
- `docs/decisions.md`, the object model, the additions and the overview
  carry the rule in the same commit.

## Gates

At `336b2f8` no build shows a warning, and the host suite passes 1248 of
1248 (`build/scratch/suite9.log`). ASan passes 1247 of 1247
(`build/scratch/asan4.log`) and UBSan 1247 of 1247
(`build/scratch/ubsan4.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

- The refusal also covers `if let`, a destructuring `let`, an array
  literal and a parameter of `?T` or `(T, int)`. A parameter of an owning
  class type gets the same message.
- A value moves into an `own` parameter when its type owns something,
  holds a type parameter or holds a pointer. A channel and a Mutex move.
- `dup(x)` of a class value dispatches `copied`.

## Questions

- `*dup(&p)` still leaks the object that `anti_rt_dup` allocated, 32
  bytes for a `Key`. The decisions count `*dup(p)` as a fresh value.
  Should its lowering free the object once it has copied the value out,
  now that `dup(x)` is the copy of a value?
