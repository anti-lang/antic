# Own parameters through function values

This step builds Eddie's answer of 2026-11-13 to the question of
`docs/reports/2026-11-13-coverage-phase-3f.md`, "I meant option 1". A
function with an `own` parameter of a type that needs a teardown is
refused wherever a function value is expected. A call through a function
value then leaves every argument with the caller, which tears a fresh one
down once the call returns. "Answers of 2026-11-13" in
`docs/decisions.md` holds the rule. The work stayed on the Mac.

## Commits

- `00218f53` records the answer. It replaces a part of the provisional
  entry on a parameter of a pointer type. That part left the arguments
  of a call through a function value alone. "Ownership at a call" in
  `docs/anti-language-additions.md` and the overview state the rule.
- `f01a45d0` builds the refusal and fixes T254.
- `0545b87c` fixes T253.
- `a4f8cd0b` lists T255.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## The refusal

`sema_refuse_own_value` of `src/antic/sema_call.c` reports
`` `take` takes `k` as `own`, and a function value moves nothing ``. The
words follow the message of `concrete fn`, `` `%s` takes it as `own` ``,
and the entry on `own cause`, "a call through a function value moves
nothing". The checker calls it at four sites, each where the name is not
the callee of a call:

- a name, in `sema_expr.c`;
- `m.f` of another module, in `check_qualified`, as `parts.take`;
- `T.f` of a class, as `Box.take`;
- the bound function `v.f`, as `Box.take`, and `Bag<Key>.put` of a copy.

The type of a copy has its arguments in place, so `Bag<int>.put` stays a
value. One refusal stands per place, under `tests/matrix/`:
`ownership_fresh_value_through_a_function_value_to_an_own_parameter`
(`let`, the former test of T254), and
`ownership_own_parameter_function_value_` with `assigned`,
`in_a_struct_literal`, `in_an_own_field`, `in_a_static_field`,
`to_a_parameter`, `to_a_keep_parameter`, `to_a_keep_own_parameter`,
`to_a_concurrent_parameter`, `returned`, `to_a_generic_parameter`,
`of_a_class`, `bound` and `bound_on_a_copy`.
`error_modules_own_parameter_function_value` takes `parts.take` through
the library file of `tests/modules/owned`. The program
`ownership_own_parameter_without_a_teardown_as_a_function_value` holds an
`own int`, an `own *Key` and `Bag<int>.put` as values.

An anonymous function takes no `own` parameter, since the parser refuses
`fn(own k: Key)` with "expected `)`", so it gets no refusal. A worker
after the arrow of `dispatch` or `parallel` is a callee and stays allowed.
The message of `T.f` and of `v.f` stands at the start of the expression,
as other messages of a field do.

## The lowering

`fresh_argument` of `src/antic/lower_expr.c` read the marks of the
symbol of a named callee, and `lower_call` passed over a bound function
and a function with its context. `own_marks` now gives the function a
name or `T.f` names, and NULL for a call through a function value, which
then tears a fresh argument down. A call through the table without the
symbol of its function keeps its arguments, as before.
`ownership_fresh_value_through_function_values_in_contexts` takes a
local, a parameter of two words, a `keep` parameter, a bound function, a
closure, a call result, an element and an `own fn` field. Before the fix
all but the local and the `keep` parameter left the key alive.

## Uses in the library and the tests

No function of `src/std/` with such a parameter is a value: the library
builds with the check, and no public item changed. One test names one:
`matrix/types_function_with_marked_parameters_as_an_int` takes `apply`,
whose `keep own owned: fn(int) -> int` is marked `own`. The type
`own fn(...)` of that parameter is a type of its own that the type of the
function carries, so a provisional line under "Answers of 2026-11-13"
leaves such a function a value. A call through the value hands the
snapshot over, which a `dup` through a local showed under
`--memory-checks`.

## Findings

- T253 fixed in `0545b87c`. The three lines left the list.
- T254 fixed in `f01a45d0`. Its program is now the refusal of `let`, and
  its three lines left the list.
- T255 open. `if let g = h.f` on an `own ?fn` field that holds a snapshot
  frees it where the binding ends and again with its owner: a trap in
  dev and release, a double free under `--memory-checks`. A test of the
  `keep own` reading found it. It is no part of this answer, so
  `ownership_if_let_of_an_own_function_field` is listed.

## Suites

The host suite ran in three parts before each commit outside `docs/`:
390 s, 267 s and 341 s before the refusal, up to test 7322, 392 s,
272 s and 346 s before the fix of T253, up to test 7325, and 403 s,
269 s and 337 s before the list of T255, up to test 7328. A first run
of the refusal found the test of `apply` and four sources out of the
canonical form, and both were fixed before the run that counts. Each
sanitizer suite ran in the six parts, the last up to test 7326: `asan`
270 s, 190 s, 269 s, 178 s, 262 s and 516 s, `ubsan` 306 s, 190 s,
264 s, 186 s, 266 s and 527 s. All passed.

## Provisional lines

- A `keep own` parameter leaves its function a value, since the type of
  the function carries `own fn(...)`.

## Questions for Eddie

- T255. Should the fix of `if let` on an `own ?fn` field be the next
  step? `let` borrows the value, and the fix would make `if let` borrow
  it too.
