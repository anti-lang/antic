# The rules of ownership

The step built R070 to R085 of "Rules of 2026-10-13" in `docs/decisions.md`
in one session, on 2026-10-14. R074 and R083 wait for Eddie, and antic stays
as it is there. R081 has its test and is not built. The first question
below asks about it.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R070 | Keep | seven refusals `matrix_ownership_dup_of_*_refused`, `matrix_ownership_dup_kinds`, `matrix_ownership_dup_in_a_generic_of_what_assignment_copies` | before |
| R071 | Keep | `matrix_ownership_dup_of_a_value_that_may_be_none_refused`, a known failure of T035, and `matrix_ownership_dup_kinds` | before |
| R072 | Keep | `matrix_classes_root`, `matrix_classes_copies` | before |
| R073 | Keep | `matrix_ownership_own_fields_and_parameters` | before |
| R075 | Change | `matrix_ownership_delete_of_a_channel_that_holds_values` | `cf433354` |
| R076 | Keep | `matrix_ownership_values_in_a_dispatch`, `matrix_ownership_values_in_a_channel`, `matrix_threads_channel_of_pointers_refused`, `matrix_threads_worker_argument_is_a_class_with_a_pointer_refused` | before |
| R077 | Keep | `matrix_ownership_delete_in_a_dispatch` | before |
| R078 | Keep | `matrix_ownership_lent_values` | before |
| R079 | Change | `matrix_ownership_array_literal_moves_a_local`, `matrix_ownership_named_after_an_array_literal_refused`, `matrix_ownership_named_twice_in_an_array_literal_refused` | `cd3ea5df` |
| R080 | Keep | `matrix_ownership_values_in_an_argument` | before |
| R081 | Change, not built | `matrix_ownership_own_pointer_that_a_generic_does_not_hand_on`, a known failure of T127 | `604fd4ea` |
| R082 | Keep, new message | `error_moved_into_no_name` | `1d80ca7c` |
| R084 | Change | `matrix_values_none_into_an_own_function_field` | `0aabe97a` |
| R085 | Keep | `error_function_not_kept`, `error_function_not_concurrent` | before |

- Each test of a Change failed before the code changed. Those runs went to
  the output of the session and left no log.
- R075. `delete(c)` of a channel whose values own something runs a loop
  before it frees the channel. Each turn takes the oldest value with
  `anti_rt_chan_held`, a new function of `src/rt/sync.c` that never waits,
  and tears it down in the frame of the program. A value that `recv` took
  before stays with the program, which is R074.
- R079. An array literal moves a local, an `own` parameter and a local of
  a type parameter, by the rules of a tuple literal.
- R082. The message reads `` `b` was moved on line 18 ``.
- R084. `none` is taken wherever an `own fn` place takes a value, and `=`
  frees the snapshot the field held.
- Tests that held an old behaviour changed with their rule. The seven
  refusals `matrix_ownership_element_of_a_literal_copies_*` named a local.
  They name an element of another array and keep their message.
  `error_own_copy` held `[a, a]` and names a field and `*p` there.
  `matrix_values_none_into_an_own_function_field` was a refusal and is a
  program.
- `337647e1` wrote four new programs in the form of `anti fmt`.

## Findings

No rule of the step closed a finding. T127 is new, for R081, with three
lines in `tests/known-failures.txt`. The list holds 379 tests, 376 before.
`build/drive/logs/own-known-v.log` holds the output of the three: each
prints `not handed on: alive 1` where the rule gives 0.

## `emit_identity` and `link_identity`

- `emit_identity` did not change.
- R075 changed `link_identity_macos-arm64`: the runtime gained
  `anti_rt_chan_held`.

## Where the task and the code differed

- R081 says "as every `own` parameter is". That holds for a value alone.
  A plain `fn drop(own p: *Key) { }` deletes nothing today. The
  provisional entry under "Generics and collections" says that nothing
  tears down an `own` parameter of a pointer at an exit.
- "Principles" under "Collections" in `docs/anti-language-additions.md`
  says that a collection of pointers owns the pointers and not what they
  point at. `add` of `Set<T>` does not hand `x` on when the set holds it
  already. Built as written, R081 makes `Set<*Key>` delete a key it holds
  when a program adds its pointer a second time. Today that `add` gives
  `false` and the key stays alive, which a scratch program showed.
- The same holds for `add` of every map, for `push` of a full `Ring` and
  for `set_if_version` that fails. So the step wrote the test and left
  the code.
- R082 says the message names the place of the move. The place that
  `*slot(to) = b` wrote has no name, and antic keeps no text of an
  expression. The message names the line.
- R079 keeps the refusal for a value that stays where it is: a field, an
  element and what a pointer points at.
- R084 covers an argument of a `keep own` parameter as well.
- `docs/decisions.md` holds these readings under "What the rules run
  added", each `[provisional]`.

## Questions

- R081: is a pointer that a generic takes as `own x: T` deleted when the
  generic does not hand it on? Then `Set<*T>.add` of a pointer the set
  holds deletes the object the set holds. Does a plain `own p: *T` follow
  the same rule, or does the function still give the memory back itself?
- R082: is the line the place the message names, or the text of the
  place, `*slot(to)`?

## Counts and checks

- Tests: 4825 before and 4838 after, by `ctest --test-dir build/host -N`.
- Host suite: every test passed, 379 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 373 s
  and `-I 2001,4838` in 239 s, in `own-host-suite-a2.log` and
  `own-host-suite-b.log` of `build/drive/logs/`.
- ASan: 4836 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4836`, in
  266 s, 365 s and 340 s, all passed, in `own-asan-suite-a.log` to
  `own-asan-suite-c.log`.
- UBSan: the same three parts in 244 s, 346 s and 256 s, all passed, in
  `own-ubsan-suite-a.log` to `own-ubsan-suite-c.log`.
- The three trees built with zero warnings, in `own-host-build8.log`,
  `own-asan-build.log` and `own-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new
  tests.

## Proof

`git log --oneline -3` before the commit of this report:

```text
337647e1 Write the tests of the rules of ownership in the canonical form
604fd4ea Hold R081 by a test, as the expected failure T127
0aabe97a Take none in an own field of a function type, by R084
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `337647e11ae74373ff199c812af7fd53538b6565` twice.
