# The rules of flow and of errors, R001 to R025

The step built the rules R001 to R025 of "Rules of 2026-10-13" in
`docs/decisions.md`. Eleven are a Change and are built. Twelve are a Keep
and have their test. R013 is a Keep with a change of its message, which is
built. R009 and R017 wait for Eddie, and nothing of them changed.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`. A Keep rule has no commit: its test
stood before the step, and `docs/reports/2026-10-13-rules-record.md` names
it.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R001 | Keep | `matrix_flow_break`, `matrix_flow_continue` | |
| R002 | Change | `matrix_flow_yield_in_defer_refused`, `matrix_flow_yield_in_undo_refused`, `matrix_flow_yield_in_handler_in_defer` | `3f5ddabe` |
| R003 | Change | `matrix_flow_switch_arms_all_return`, `matrix_flow_while_true_no_return`, `matrix_flow_let_else_that_leaves_through_a_switch`, `matrix_flow_paths_that_end`, and the refusals `matrix_flow_switch_on_an_integer_without_else_arms_all_return_refused`, `matrix_flow_switch_with_an_arm_that_does_not_return_refused`, `matrix_flow_while_true_with_a_break_refused` | `1c37015b` |
| R004 | Keep | `matrix_flow_defer_closure_two_exits` | |
| R005 | Change | `matrix_flow_undo_in_a_function_that_cannot_fail_refused`, `matrix_flow_undo_in_a_closure_that_cannot_fail_refused`, `matrix_flow_undo_in_defer_refused`, `matrix_flow_undo_in_the_block_of_an_undo_refused`, `matrix_flow_undo_in_try_block` | `3f5ddabe` |
| R006 | Keep | `matrix_errors_values_before_the_defers`, `matrix_flow_yield_in_loop_in_handler` | |
| R007 | Keep | `matrix_flow_defer_that_never_returns_before_a_plain_return`, `matrix_flow_defer_that_never_returns_before_a_return_of_a_value` | |
| R008 | Keep | `matrix_flow_for_range_no_binding` | |
| R009 | Eddie | untouched | |
| R010 | Change | `matrix_flow_switch_arm_of_a_size_beside_its_number_refused`, `matrix_flow_switch_arm_of_a_number_beside_its_size_refused`, `matrix_flow_switch_arms_of_two_sizes_that_are_equal_refused`, `matrix_flow_switch_arm_of_a_size_the_target_decides` | `82ae32b5` |
| R011 | Keep | `matrix_types_generic_walk_in_place_over_a_copying_value_refused` | |
| R012 | Change | `matrix_flow_block_that_never_closes_refused`, `matrix_flow_switch_that_never_closes_refused`, `matrix_flow_switch_arm_block_that_never_closes_refused`, `matrix_threads_select_that_never_closes_refused` | `4d362f22` |
| R013 | Keep, new message | `matrix_errors_fail`, `matrix_flow_fail_in_worker_dispatched_refused`, `matrix_flow_try_in_worker_parallel_refused`, `matrix_flow_fail_in_worker_without_a_result_dispatched_refused` | `a237d119`, `3adcb0e0` |
| R014 | Change | `matrix_errors_yield_in_the_handler_of_a_try_block` | `dc51edbd` |
| R015 | Change | `matrix_errors_yield_in_a_try_handler_of_a_handler`, `matrix_errors_yield_of_a_value_in_the_handler_of_a_try_block_refused` | `dc51edbd` |
| R016 | Change | `matrix_errors_nested_handlers`, `matrix_errors_try_in_a_try_block_alone_never_fails_refused` | `dc51edbd` |
| R017 | Eddie | untouched | |
| R018 | Keep | `matrix_errors_never_fails_with_a_fail_in_a_closure_refused` | |
| R019 | Keep | `matrix_errors_origins` | |
| R020 | Change | `matrix_errors_guard_of_a_value_and_of_a_pointer`, `program_optional_values` | `61d8209b` |
| R021 | Keep | `matrix_errors_fatal_in_a_worker`, `matrix_errors_fatal_in_a_worker_runs_no_defer` | |
| R022 | Keep | `matrix_ownership_lent_values`, `matrix_errors_yield_of_a_lent_pointer_refused`, `matrix_ownership_lent_pointer_in_a_yield_refused` | |
| R023 | Keep | `matrix_errors_yield_kinds` | |
| R024 | Keep | `matrix_errors_handler_exits` | |
| R025 | Change | `matrix_errors_derived_construct_fails_after_its_base`, `matrix_errors_construct_fails_in_reverse_order`, `matrix_errors_construct_fails_under_bases_without_arguments`, `matrix_classes_construct_order`, `matrix_classes_objects` | `4186170e`, `9e538c8a` |

## Findings closed

- T002, by R002: antic crashed on a `yield` inside a `defer`.
- T097, by R003: the refused `switch` of its test has an `else` and returns
  in every arm, so the function gets no second message.
- T052, by R025: the part a base took leaked when the class below failed.

Their five lines left `tests/known-failures.txt`, which holds 376 tests.
Commit `77725e1a` wrote the commit of each fix into its row. T001 stays
open: R002 refuses `yield` alone, and the five exits of decision 19 are the
fix run's.

## What each Change does

- R002 and R005: the checker keeps the innermost `defer` or `undo` under
  check. A `yield` inside one that began in its handler is refused, and so
  is an `undo` inside one. An `undo` in a function that cannot fail, outside
  a `try` block, is the new warning `undo-never-runs`.
- R003: the missing-return rule and the leave rule read a `switch` whose
  arms all end the path and a `while true` without a `break` of its own.
  Lowering ends the block after such a statement in `IR_UNREACHABLE`.
- R010: the checker reads a size that no target decides and compares it
  with the other arms.
- R012: the parser stays in its error at the end of the source.
- R013: the refusal names its reason, `` `run` may fail, and `dispatch` runs
  a worker that cannot fail ``.
- R014 to R016: each handler has a state of its own in the checker, and the
  handler of a `try` block a handling in lowering. `try` in a `try` block
  jumps to the handler of the block.
- R020: `anti.lang.NoneDereference.of_value` gives `the value is none`.
- R025: a `construct` that may fail runs one exit action more on its error
  exits. `docs/notes/lowering.md` holds the choices of R003 and R025.

## `emit_identity` and `link_identity`

- R020 added a function and a text to `anti.lang`, which moves the numbered
  constants of that module. 258 lines of
  `tests/emit-identity/programs.sha256` changed.
- R003 changed six lines, those of `programs/switch_stmt`, whose function
  ends in a `switch` over an enum.
- `link_identity_macos-arm64` passes with its expected file unchanged.

## Where the task and the code differed

- R013 asks for a message that counts the arguments the program writes.
  Counted so, the old text reads `takes 0 arguments, found 0`. The refusal
  now stands before the count and names the reason. A failing worker
  without a result got a text on `?*Error` before, and gets the same new
  message.
- R003 says "every arm and `else`". A `switch` without `else` ends the path
  on an enum or a variant, where the checker holds that every value has an
  arm. On an integer or a `str` it does not.
- R005: an `undo` in a `try` block of a function that cannot fail runs, as
  `matrix_flow_undo_in_try_block` shows, so it gets no warning.
- R010: the checker knows no layout. `docs/decisions.md` holds one
  `[provisional]` entry under "What the rules run added": a size the target
  decides, and a size under an operator, keep their arm.
- R025 gives no order between the fields and the `destruct` of a base. The
  fields of the failing class go first, then the base as an object of the
  base goes: every `destruct`, then the fields.
- The task asks for `fixed` and the commit in the commit of the rule. A
  commit cannot name itself, so the row says `fixed` there and `77725e1a`
  added the three commits.
- `CLAUDE.md` asks for the full suite before each commit outside the
  documents. The step ran the tests of each rule before its commit. It ran
  the whole host suite after R003 and after R025, and every gate at the end.
- The task names no id of the step. The report is named after its rules.

## Questions

- R003: a value outside an enum reaches `IR_UNREACHABLE` after a `switch`
  that names every value, in a dev build as well. Should a dev build trap
  there?
- R010: should a size that the target decides be compared per target by
  the back end, whose messages hold no position?
- R025: a `construct` that wrote `undo delete(self.part);` for a field it
  set now deletes the part twice. No source of the tree does. Should the
  checker refuse an `undo` that deletes a field of `self`?
- R025: a base whose `construct` takes arguments and was never called gets
  no `destruct` at a failure, and gets it at a `delete` of a finished
  object. Is the second right?

## Counts and checks

- Tests: 4671 before and 4708 after, by `ctest --preset host -N`.
- Host suite: 4708 of 4708 passed, 376 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 377 s and
  `-I 2001,4708` in 228 s. The logs are `build/drive/logs/fe-host-1.log`
  and `fe-host-2.log`.
- ASan: 4706 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4706`, in
  266 s, 371 s and 321 s. Every part passed, in `fe-asan-1.log` to
  `fe-asan-3.log`.
- UBSan: the same three parts in 248 s, 355 s and 243 s, all passed, in
  `fe-ubsan-1.log` to `fe-ubsan-3.log`.
- A part counts the tests that set up its fixtures as well. The parts of
  the host suite add up to 4729 runs, and those of a sanitizer suite to
  4737.
- The three trees built with zero warnings, in `fe-host-build-final.log`,
  `fe-asan-build.log` and `fe-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the step touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host compared its LLVM
  text with the new manifest.
- R025 has no test of a class of a library module and none of
  `anti_<Class>_construct` called from C. `clib_failing` and the tests of
  `tests/modules/` pass unchanged.

## Proof

`git log --oneline -3` before the commit of this report and of `CLAUDE.md`:

```text
77725e1a Name the commits that fixed T002, T052 and T097
9e538c8a Bind the address of a cleared field before its store
4186170e Tear down what was built when a construct fails, by R025
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds `git log --oneline -3`, `git status --short` and
`git rev-parse HEAD origin/main` after the push, since the hash of this
commit is not known inside it.
