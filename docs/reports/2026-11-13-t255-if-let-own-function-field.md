# T255: `if let` on an `own` function field

This step fixes T255. `if let g = h.f` on an `own` field of type
`?fn(...)` that holds a snapshot freed the snapshot where the binding
ended and again with its owner. A check of the neighbouring forms found
T256, a crash of antic on `p?.f` of such a field, and the step fixes it
too. The work stayed on the Mac.

## Commits

- `c65e5a8b` lists T256.
- `19a44577` fixes T255.
- `413f7a17` fixes T256.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## T255

The checker writes `if let m = e` on a value that may be `none` as a
`let` and an `if`, in `sema_if_let_none` of `src/antic/sema_pattern.c`.
It gave the local the type of `e`. `check_let` gives a local the
borrowed form of two words for an `own fn`, by the entry "A local never
owns a function" under "Anonymous functions and closures" in
`docs/decisions.md`. The `if let` local kept the owned type, so the
lowering tore its snapshot down where the binding ended.

`sema_if_let_none` now gives the local the borrowed form, through
`sema_require`, as `check_let` does. The fix is in the checker, since
the type of the local is where `let` and `if let` differed. The lowering
reads that type and needed no change.

`sema_require` also refuses `dup` of a function where nothing owns the
copy. `if let g = dup(h.f)` was accepted before and freed the copy once,
since the local owned it. It now gets the refusal of `let g =
dup(h.f)`, by the rule that `if let` binds as `let` does. No file of
`src/std/`, `tests/` or `docs/` wrote that form.

New tests under `tests/matrix/`:

- `ownership_if_let_of_an_own_function_field_in_contexts`: `if let` on
  `self.f` in a function of the class, through a `*Hold`, on a field of
  an owned object, on a field that holds `none` and in a loop. Before
  the fix it showed a use after free under `--memory-checks`.
- `ownership_if_let_of_a_dup_of_an_own_function_field`, the refusal.

## The neighbouring forms

Each was a program run in dev, release and `--memory-checks` before the
fix:

- `if let` on a local that holds such a field passed. A local borrows
  already, and no local owns a function.
- `let g = h.f else { }` passed, since it goes through `check_let`.
- `h.f ?? one`, into a local and called in place, passed.
- `let g = h.f catch { }` and `catch fatal` passed.
- A global, a static field and an element have no such form. antic
  refuses `own` on a global, requires `atomic` on an `own` static field
  and refuses `own` on an array of functions.
- `p?.f` crashed antic, with or without a snapshot in the field. A
  field of type `?fn(...)` without `own` did not. That is T256.

## T256

The result of `?.` on an `own` field of type `?fn(...)` is the function
in the form of two words. `lower_address` of `src/antic/lower_expr.c`
had no case for `?.`, so it and `build_value_into` called each other
until the stack overflowed. `lower_optional` kept its result in one
register as well. `lower_address` now sends such a `?.` to
`lower_optional`, whose result is then the address of a pair, as for
`??`: the pair of the field, or a pair of `none` when the base is
`none`. The type of `p?.f` stays the type of the field, as `h.f` has
it, and the `let` or `if let` that takes it borrows.

The listed program is `ownership_optional_access_of_an_own_function_field`,
`let q = p?.f;`. The fix commit adds
`ownership_if_let_of_an_optional_access_of_an_own_function_field`, with
an object whose field holds a snapshot, one whose field holds `none`,
and `none`.

## Suites

The host suite ran in three parts before each commit outside `docs/`:
388 s, 269 s and 314 s before the list of T256, up to test 7331, 387 s,
264 s and 315 s before the fix of T255, up to test 7335, and 383 s,
265 s and 310 s before the fix of T256, up to test 7338. Each sanitizer
suite ran in the six parts, the last up to test 7336: `asan` 267 s,
187 s, 264 s, 175 s, 256 s and 476 s, `ubsan` 294 s, 188 s, 264 s,
185 s, 267 s and 510 s. All passed. 7338 tests pass on the host, and
7336 under each sanitizer. `tests/known-failures.txt` holds the two
lines of T136 and T138 alone.

## Findings

- T255 fixed in `19a44577`. Its three lines left the list.
- T256 found and fixed in `413f7a17`. Its three lines left the list.

## Questions for Eddie

- `if let g = dup(h.f)` is now refused, as `let g = dup(h.f)` is. Before
  the fix it compiled and freed the copy once. The rule that `if let`
  binds as `let` does settles it, so the step took it. Say if `if let`
  should own such a copy instead.
