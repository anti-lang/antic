# Step 33, the minor findings of the middle

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. `docs/reports/2026-10-19-solid-step33.md`
reports the front end. This session took the middle: lowering, the IR,
the optimizer, the layout, the memory checks and the pass over the whole
program. The logs stand in `build/drive/logs/step33-middle/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 26, `optimize.c:24` | `a5a84de2`, `681cbd61` | `bits`, `trim` and `signed_value` stood in three files, two entry-slot functions repeated one insertion, three exit actions wrote every field by hand, three FNV-1a loops stood apart and the limit of a split slot was written twice. |
| Rule 5, `lower_expr.c:1916` | `a44c0cda` | Three products and three sums reached an allocation before the check of `alloc_zeroed`. |
| Rule 24, `memcheck.c:248` | `a525f3ab` | The frame table was freed through the const pointer of `struct checker` with a cast. |
| Rule 9, `whole_tables.c:1481` | `53312d49`, `df6296f7` | Twenty names of globals and thunks were formatted without a check, and two buffers were short of the largest count. |
| Placement, `lower_worker.c:349` | `8fa22f21` | The channel design and the load of a handle stood in the file of the workers. |
| One place per concern, `lower_lowerer.h:9` | `73e5c295` | The checks of a conversion, of a table and of the bounds of a slice stood in `lower_expr.c`. |
| A comment against the code, `ir_verify.c:679` | `21960b4f` | `check_class` left the subtables, `inject` and `provides` unchecked, and the printer left out `required` and both lists. |
| Chapters of the book, `layout.h:12` | `cf55b134` | Four comments of the middle, one of `driver.c` and four of the unit tests cited chapters of the book. |

`ir.md`, `optimizer.md`, `hashing.md` and `dev-checks.md` were fixed
before this step, as the summary records. `1669fca5` writes the commits
into the summary.

Two parts of the fixes need a word. The name formatter of `53312d49`
first ended antic with status 70. That status belongs to `alloc.c`
alone, and the test `one_allocator` refused it, so `df6296f7` aborts
instead, as the bound of the link command does. `73e5c295` keeps two
checks where they are, the check of a walk in `lower_stmt.c` and the
bounds of the lanes in `lower_simd.c`, since each builds part of what it
guards. The header of lowering and "Files of lowering" in
`docs/decisions.md` now name both.

The tests: `test_name_size` of `tests/unit/test_text.c` formats the two
longest names with the largest count, and `class_records` and
`class_printed` of `tests/unit/test_ir.c` show the verifier and the
printer of a class record. The two of the class record failed before
their fix, and the first did not build before `text_format_name`. Rule 5, rule 24,
rule 26, the move and the comments change no behaviour, and the suite
holds them. The source cap bounds every factor of rule 5, so no input
reaches the overflow.

## Findings left

- Rule 18 at `lower_owning.c:56` belongs to step 34, which names it.
- `rt_abi.h:8` belongs to step 19, M332, which names it first.
- The comments of `tests/CMakeLists.txt` that cite chapters of the book
  go to the session of the tests.

## New findings, provisional entries, known failures

No new finding. No `[provisional]` entry: `73e5c295` changes the text of
the entry "Files of lowering" to name the two checks that stay.
`tests/known-failures.txt`: 29 lines before and 29 after.

## Gates

- The three builds report no warnings, `build-host3.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5132 tests, `-I 1,2000` and `-I 2001,5132`, 334 s and 277 s. All
  2000 and 3153 pass, since a part also counts the fixtures it pulls in.
  The 8 expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip, `host-1.log` and `host-2.log`.
- ASan, 5130 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5130`, 287 s,
  368 s and 366 s. All 1150, 1113 and 2901 pass with the same skips,
  `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 259 s, 260 s and 294 s. All 1150, 1113 and 2901
  pass, `ubsan-1.log` to `ubsan-3.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on each `.md` file the step
  touched, `docs1.log` and `docs2.log`.

## Not run on the target

None. No change touches a path of a platform layer.

## State at the push

```text
$ git log --oneline -3
df6296f7 Stop a name that does not fit with abort, not status 70
681cbd61 Keep the DESIGN of ir_fold_int on its declaration
1669fca5 Record the middle minor findings of step 33 in the summary
$ git status --short
$ git rev-parse HEAD origin/main
df6296f740c095932d5692ffdec3de731c110f30
df6296f740c095932d5692ffdec3de731c110f30
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: the back end, the library file, the runtime, the standard
library, `anti`, the build and the tools, and the tests. Each takes the
rows of the minor tables in `docs/audit/summary.md` whose places lie in
its area and have no commit yet. The session for `anti` also takes the
comment on `WRAP_COLUMNS` in `src/anti/fmt.c`. The back-end session takes
`sema.h` with the records of `antic.h`. The session of the tests takes
the chapters of the book in the comments of `tests/CMakeLists.txt`.
