# `*` on the result of dup or alloc

The session built Eddie's answer to the question of
`docs/reports/2026-09-29-parameters-of-the-caller.md`.

## What changed

- `*` applied directly to the result of `dup` or `alloc` is refused, since
  the object it points at could never be freed. Nothing frees it
  implicitly. The message names the fixes: `` `*` of a new object drops
  its only pointer, so nothing could free it. Copy the value with
  `dup(*p)` or `dup(x)`, or keep the pointer in a `let` ``. The check
  stands in the `*` arm of `check_unary` of `sema_expr.c`. `dup(x)` of a
  value of a type parameter gives no new object and stays allowed under
  `*`.
- `sema_reads_existing` no longer counts `*dup(p)` as a fresh value, so a
  value read through `*` is always existing. The entry on existing values
  in `docs/decisions.md` loses its exception.
- `errors/deref_new.anti` holds both forms and accepts `dup(*p)`,
  `dup(x)` and the pointer kept in a `let`. It was accepted as a whole
  before the change. `errors/own_copy.anti` pinned `b = *dup(p);` as a
  fresh value and now writes `b = dup(*p);`, with the same messages.
- `docs/decisions.md`, the object model, the additions and the overview
  carry the rule in the same commit.

## Gates

At `709d1c8` no build shows a warning. The host suite passes 1249 of 1249
(`build/scratch/suite10.log`). ASan passes 1248 of 1248
(`build/scratch/asan5.log`) and UBSan 1248 of 1248
(`build/scratch/ubsan5.log`), each without `no_paths`. The docs-style
checker reports nothing on each changed `.md` file.

## Provisional decisions

None.

## Questions

None.
