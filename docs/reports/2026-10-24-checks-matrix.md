# The compile-time checks in every context

The step added the checks of "Compile-time checks of 2026-10-20" in
`docs/decisions.md` to `docs/notes/test-matrix.md` as the area checks, and
tested each in the eleven contexts the task names. Logs are under
`build/drive/logs/checks-matrix/`, and the probes under
`build/checks-matrix/probes/`.

## What the area holds

- One row per check, E1, E2, C1, C2, C4, C5 and C6. C3 is a sweep with no
  check of its own, and its row holds the `else` of a `let` that reaches
  its end.
- 88 refusals, one per check and context, in `tests/matrix/checks_*` and,
  for a `tests` block, in `tests/errors/checks_*_in_a_tests_block`, which
  pass `--front-end --tests`. Eight cells name a refusal that an earlier
  step wrote, such as `matrix/threads_lock_order_two_mutexes`.
- Eight programs `matrix/checks_*_passes_in_contexts` hold, in the ten
  contexts outside a `tests` block, the forms each check lets through. Each
  runs in dev, in release and with `--memory-checks`, and the three print
  the same lines. The module `anti-test/com/example/checks` holds the same
  in a `tests` block, run by `anti_test_checks`.
- Commit `0596137e`.

## New findings, all left open

Each belongs to the step of its check. Its tests stay in the suite as the
rule says, listed in `tests/known-failures.txt`, and fail because antic
takes the program, `known.log`.

- T194, commit `98aaeb6b`. C1 reads nothing inside a `defer` or an
  `undo`, so an element that no path writes passes there. The walk of
  `sema_uninit.c` skips both blocks by a `DESIGN` comment, so the step did
  not change it. Two refusals.
- T195, commit `2e9ba18c`. E2 walks a `defer` or an `undo` once with the
  state where it stands, and not at each exit where it runs. A read or a
  second `delete` of a value the body deletes after the statement passes,
  and so does a read of a value that a later `defer` deletes. With
  `--memory-checks` each of the four programs reports a use after free, or
  crashes at its second `delete`, as release does. Four refusals.
- T196, commit `5b0ff926`. C2 checks no index inside the body of an
  anonymous function, since `ast_walk_block` does not enter it. The dev
  build stops at the bounds check. One refusal.

No finding was closed. `tests/known-failures.txt` held 6 lines before and
13 after.

## Mistakes of the task

- C2 names `for i in 0..=n`. Anti has no `..=`, and the tests write
  `0..xs.len + 1`.
- C5 names a `select` without `else`. `select` takes no `else` at all, as
  the parser says, so every `select` gets the warning under a lock.

## Probes that found no defect

E2 refuses a read through a closure of a value deleted before it, a value
deleted in one arm, in a handler or in a `sync` block and used after it,
and a `while` whose body deletes. E1 refuses a reference stored into an
outer local from a switch arm, a handler, a `sync` block, a closure and a
`defer`. C1 refuses a read after a write in a handler alone. A closure
that deletes a captured value before the caller uses it passes, since no
one function shows that the closure runs.

## Provisional entries

None added.

## Not run on the target

Nothing. Every change is a test of the front end or a program of the host.

## Gates

- No warning: `build1.log`, `build2.log`, `build-asan.log`,
  `build-ubsan.log`.
- Host, `host-1.log` to `host-3.log`: 6647 tests by name, 6645 passed, the
  11 expected failures among them, and 2 skipped, the `mimalloc_environment`
  tests.
- ASan, `asan-1.log` to `asan-5.log`, and UBSan, `ubsan-1.log` to
  `ubsan-5.log`: 6645 tests by name each, 6643 passed, the same 11 expected
  failures among them, and 2 skipped.
- `emit_identity` and `link_identity_macos-arm64` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## State before the commit of the report

```text
$ git log --oneline -3
0596137e Test the compile-time checks in every context of the matrix
5b0ff926 Record T196, an index in a closure that index-out-of-range never reads
2e9ba18c Record T195, a defer or an undo that use-after-free walks where it stands
$ git status --short
 M CLAUDE.md
?? docs/reports/2026-10-24-checks-matrix.md
$ git rev-parse HEAD origin/main
0596137e4d81e4a77ee735ab9a2406710f2a2938
0596137e4d81e4a77ee735ab9a2406710f2a2938
```

The commit of the report adds the report and the counts of the suites in
`CLAUDE.md`. The reply of the session holds the state after its push.
