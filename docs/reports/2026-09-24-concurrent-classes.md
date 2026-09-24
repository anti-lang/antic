# Concurrent classes

The step built "Concurrent classes" of `docs/anti-language-additions.md`.
`docs/notes/concurrent-classes.md` holds the choices of the passes, and the
section "Concurrent classes" of `docs/decisions.md` holds the decisions. The
logs are under `build/drive/logs/`.

## What was built

- A Mutex is one word of the program's memory: a futex word on Linux,
  `os_unfair_lock` on macOS and `SRWLOCK` on Windows. `src/rt/lock.c` holds it.
  A new IR type, `IR_LOCK`, takes its width from the layout of each target, and
  `size_of(Mutex)` is 4 on Linux and macOS and 8 on Windows. The test
  `mutex_size` reads it from the listing of each target. A Mutex is never copied
  or assigned, and `sync` locks it at its address.
- `synchronized class`, with a hidden lock that its thread takes again without
  waiting, `sync obj { }`, and the rule that the functions of the class alone
  reach its fields.
- `concurrent class`, with `guarded by lock` and `guarded by Class.lock`,
  atomic and fixed fields, the safety check `unguarded-field`, and `unchecked`
  after a field's type and in the class header.
- The rule that a public function of either class gives out no pointer into the
  fields. Atomic locals, and the pointer to a thread-safe object in a worker.
- The report of lock orders in a dev build, with the four sites.
- The library file carries the kind of a class and the guard and marks of each
  field. Its format is 57. The C header lays out the hidden lock and marks each locked
  function, and `anti doc` does the same.

## Tests

`programs/synchronized.anti` and `programs/concurrent.anti` run workers of
`parallel` against one object in both modes, and on x86_64 through Rosetta.
`traps/lock_order.anti` reads the dev report and the silence of a release
build. The error tests `synchronized`, `concurrent`, `concurrent_forms`,
`pointer_leak`, `mutex_copy` and `concurrent_modules` hold the refusals, among
them the lost update of `hits`, the pointer leak of `origin` and the copies of a
Mutex. `sync_modules` takes both classes across a library, `clib_ledger` calls a
synchronized class from two threads of C, and `anti_doc` reads the new page.

## What failed and how it was fixed

- Every Mutex of a worker went by value and now cannot be copied.
  `programs/sync_workers.anti` and a unit test pass a pointer instead, and
  `anti.random` no longer assigns its Mutex in `construct`.
- A probe of an atomic call inside another probe cleared the outer flag, which
  refused `Fatal.get().hook.load()` of `anti.lang`. The probe keeps the flag.
- The `unchecked` of a field that no code of its module writes was unused. A
  public field of a concurrent class now counts it as used, since another module
  may write it. A plain class keeps the warning `allow_unused` expects.
- A refusal of `unchecked(unguarded-field)` in the header of a plain class
  contradicted the spec's warning `unused-unchecked`, and was taken out.
- `anti fmt` wrote `]guarded by` after an array default, and keeps the space.
- `emit_identity`, `link_identity_macos-arm64` and `antl_scale` follow the new
  runtime and format, and were written again on the Mac.

## Provisional entries added

In "Concurrent classes" of `docs/decisions.md`: the zero Mutex and `destroy`,
the refusal of copies, `sync` on a place, the counting hidden lock, which
functions take it, the rule of inheritance, the reach of a synchronized field,
the reach of a guarded field, the fixed field and where it is reported, nested
types, the unused `unchecked` of a public field, the pointers into the fields,
the types of an atomic local, the worker's pointer, the lock-order report, the
header and `anti doc`, and `compare_swap` on an `unchecked` field. Under "Files
of the checker": `sema_safety.c`.

## Questions

- The spec's `Queue` swaps `unchecked` fields with `compare_swap`, which the
  atomic operations of a field do not reach. Should `compare_swap` take a plain
  field that `unchecked` marks?
- Commit `c228ec4 Stage 5 of additions`, which is not this session's, reached
  `main` and `origin/main` during the step. It adds `drive-additions-5.sh` at the
  top level, so `repo_layout` fails in all three suites. This session did not
  touch the file.

## Gates

- Build: zero warnings from the compilers in `host`, `asan` and `ubsan`. The
  linker prints its pre-existing note on `libLTO.dylib`.
- Host at `726bd29`, the last commit of this step, before `c228ec4`: 898 of 898
  passed, `build/drive/logs/ctest_full3.log`.
- At `c228ec4`: host 897 of 898, `build/drive/logs/ctest_host_final.log`, ASan
  896 of 897, `build/drive/logs/ctest_asan.log`, and UBSan 896 of 897,
  `build/drive/logs/ctest_ubsan.log`. The one failure in each is `repo_layout`
  on `drive-additions-5.sh`.
- The docs-style checker reports nothing on every file this step touched.

## Completion

Commit `6db8ed8` stops tracking `drive-additions-5.sh`, and the file stays in
the root, excluded by `.git/info/exclude`. The gates then ran again at
`6db8ed8`, with zero warnings in all three builds:

- host: 898 of 898 passed, `build/drive/logs/fin-ctest-host.log`.
- ASan: 897 of 897 passed, `build/drive/logs/fin-ctest-asan.log`.
- UBSan: 897 of 897 passed, `build/drive/logs/fin-ctest-ubsan.log`.

## State

`git log --oneline -3` before this report:

```text
c228ec4 Stage 5 of additions
726bd29 Record concurrent classes in the documents
cf501fd Pin the new outputs and fix the unused clauses of plain classes
```

`git status --short` was empty, and `git rev-parse HEAD origin/main` gave
`c228ec4a192053562bc8e7bc7a6eb85293d7608e` twice.
