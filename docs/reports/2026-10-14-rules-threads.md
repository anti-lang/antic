# The rules of threads

The step built R086 to R099 of "Rules of 2026-10-13" in `docs/decisions.md`
in one session, on 2026-10-14. No rule of the step waits for Eddie.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R086 | Change | `threads_modules_dev`, `threads_modules_release` | `f376ceb0` |
| R087 | Keep and Change | `matrix_threads_parallel_split`, `matrix_threads_parallel_by_a_count_below_zero_refused`, `check_parallel_count_below_zero` | `62dcd69f` |
| R088 | Change | `matrix_threads_dispatch_leaves_out_a_default_argument`, `matrix_threads_dispatch_leaves_out_an_argument_without_a_default_refused` | `bf3828f6` |
| R089 | Keep | `matrix_threads_atomic_add_past_the_highest_value` | before |
| R090 | Change | `matrix_threads_snapshot_names_an_atomic_local_refused`, `matrix_threads_atomic_captures` | `4fafe616` |
| R091 | Keep | `matrix_threads_atomic_kinds` | before |
| R092 | Change | `matrix_threads_pointer_to_a_guarded_field`, five refusals `matrix_threads_pointer_to_a_guarded_field_*_refused`, `matrix_threads_slice_of_a_guarded_field_after_its_sync_refused` | `e6b7932d`, `d486b04a` |
| R093 | Change | `matrix_threads_sync_in_a_closure_inside_its_sync_refused`, `check_sync_held` | `b05a5ed7` |
| R094 | Change | three refusals `matrix_threads_pointer_into_fields_*_refused`: through an out parameter, stored in another object, through a local that was assigned | `e6b7932d` |
| R095 | Keep | `matrix_threads_channel_kinds`, `matrix_threads_channel_of_a_mutex` | before |
| R096 | Keep | `matrix_threads_jobs` | before |
| R097 | Change, no code | `matrix_threads_own_function_at_a_concurrent_parameter`, `matrix_threads_snapshot_for_an_owner_calls_a_function_that_is_not_concurrent_refused` | `c74a996b` |
| R098 | Keep | `matrix_threads_channel_kinds` | before |
| R099 | Keep | `matrix_values_struct_with_a_mutex_compared_and_hashed` | before |

- Each test of a Change failed before the code changed, in the logs
  `thr-r087-red.log`, `thr-r088-red.log`, `thr-r086-red.log`,
  `thr-r090-red.log`, `thr-r093-red.log`, `thr-r092-red.log` and
  `thr-r092b-red.log` of `build/drive/logs/`.
- R087. A constant count below zero gives `the count of chunks -3 is below
  zero`. A dev build checks any other count, and a release build takes one
  below zero as zero, as before.
- R088. The defaults reach `parallel` as well, and a wrong count says at
  least or at most, as `construct` does.
- R092. The pointer is good in the `sync` block that took it. A later block
  on the same lock is after it, and so is a use before the block in the next
  turn of a loop. A `=` in a branch leaves the variable marked. The refusal
  is the safety check `unguarded-field`.
- R093. A dev build stops a `sync` on a Mutex its thread holds with
  `anti: <site> takes a Mutex that its thread holds since <site>`. The
  release build of `check_sync_held` waits for itself and is not run.
- R094. The four refusals of T062 stay expected failures.
- Tests that held an old behaviour changed with their rule:
  `threads_parallel_split` lost `by -3`, `threads_atomic_captures` lost its
  snapshot, `threads_dispatch_leaves_out_a_default_argument` became a
  program, and the client of `threads_modules` names two workers by module.
- `47f11538` wrote five new tests in the form of `anti fmt`.

## Findings

No rule of the step closed or opened a finding. `tests/known-failures.txt`
holds 379 tests, as before.

## `emit_identity` and `link_identity`

- R087 changed `emit_identity`. The check of the count numbers the values
  of the ten programs of `tests/programs` that hold `parallel ... by` anew,
  on all six targets, and a release build drops the check. `b1759f18`
  wrote the manifest again.
- R093 changed `link_identity_macos-arm64`: the runtime gained the check of
  `anti_rt_mutex_lock_at`.

## Where the task and the code differed

- R097 is a Change, and no code changes. An `own fn` place takes a
  snapshot, a function that captures nothing, `dup` of an owned value or a
  `keep own` parameter. A snapshot calls no function it captured and writes
  nothing. Each value an `own fn` field can hold is concurrent already.
- R092 says the pointer may not be used after the block. The first commit
  took any `sync` on the same place. The second holds the pointer to its
  own block. By then the names of the place may name another object.
  An automated review of the first commit found two gaps, which the second
  closes. A `=` in a branch freed the variable, and a use earlier in a loop
  went through.
- R092 follows a pointer through a variable alone. One inside a struct, a
  tuple, a field or a result is not followed.
- `docs/decisions.md` holds these readings under "What the rules run
  added", each `[provisional]`.

## Questions

- R092: is a use of the pointer in a later `sync` on the same place
  refused, as built, or taken?

## Counts and checks

- Tests: 4838 before and 4862 after on the host, by
  `ctest --test-dir build/host -N`. The sanitizer trees hold 4860.
- Host suite: every test passed, 379 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 437 s and
  `-I 2001,4862` in 374 s, in `thr-host-suite-a.log` and
  `thr-host-suite-b.log`.
- ASan: 4860 tests as `-I 1,1150`, `-I 1151,2250` and `-I 2251,4860`, in
  361 s, 399 s and 458 s, all passed, in `thr-asan-suite-a.log` to
  `thr-asan-suite-c.log`.
- UBSan: the same three parts in 336 s, 358 s and 369 s, all passed, in
  `thr-ubsan-suite-a.log` to `thr-ubsan-suite-c.log`.
- The three trees built with zero warnings, in `thr-host-build-final.log`,
  `thr-asan-build.log` and `thr-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new tests.

## Proof

`git log --oneline -3` after the push of the step:

```text
e9cc2aa8 Record the step of the rules of threads
47f11538 Write the tests of the rules of threads in the canonical form
d486b04a Hold a pointer to a guarded field to the block that took it, by R092
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `e9cc2aa8ce17044d954cc2ce2ec5ae84ed9b27e0` twice.
