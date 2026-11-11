# The answers of 2026-11-11 and batch 6 of coverage

Eddie answered the six questions of
`docs/reports/2026-11-10-coverage-phase-2c.md` and the three of
`docs/reports/2026-11-10-coverage-batch-5.md` on 2026-11-11. This step
records them, fixes the two defects they name, changes two messages and
names the kept guards. Eddie's direction is the Mac alone, so the VMs
were not touched.

## Commits

- `5446ac86` records "Answers of 2026-11-11" in `docs/decisions.md`, with
  a line in the entry of the feature freeze.
- `17ea3a5c` lists T228, and `8b497e47` fixes it.
- `a808eca8` makes the cache check of `anti test` silent.
- `a504daf9` names the root in the message of the debug map.
- `7300bb6d` lists T229, and `663a73eb` fixes it.
- `84f96d39` names the guards of the third batch in
  `docs/notes/unreached-guards.md`. It also corrects the entry of batch 5
  Q1, which named Q9 of the report of 2b where Q5 kept the guards of
  `sema_stmt.c`.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the two fixes in `docs/audit/tests-findings.md`.

## Findings

T228, minor, fixed. `driver_run` returned 0 for a dev object of a module
without `main`, where `driver.h` names `DRIVER_OBJECT_ONLY`. A case of
`unit_commands` calls `driver_run` on such a module and expects the
status. The callers that compile such an object, `module_object` of
`build.c` and the two dev compiles of `test.c`, took the status for a
success already. The other callers compile a library file, a program or
the front end, which give 0. `main` of antic exits with 0 for the status,
so `antic --dev` of a library module still succeeds. The comment of
`driver.h` said that antic exits with the status. Q2 keeps the status of
`driver_run`, and the brief asked that no caller take it for a failure,
so antic keeps its exit status of 0 and the comment now says so.

T229, major, fixed. A record `licenses/sources.txt` without the line of a
native library that the program links gave the notice `pcre2 -` and
status 0. `source_version` of `driver.c` is the one reader, and both
paths take it: the link of antic through `build_notice`, and `anti
license --project` through `driver_runtime_packages`. Both count under
T229. The run now ends with 1 and `antic: <record> holds no line of
pcre2`, in the form of `holds no module` beside it. The case
`license_project_sources_without_the_library` of
`tests/anti_messages.cmake` expects that, and its file of expected output
left the tree with its row in `docs/notes/source-tree.md`. The case
`runtime_sources_without_the_library` of `tests/antic_messages.cmake`
links a program of `anti.regex` against a stand-in runtime archive with
such a record. Both run on a Mac host alone, as the stand-in needs its
links, so their lines carried `host=macos` while they were listed.

## The answers

- Q1, Q3 and Q4 of 2c and Q1 of batch 5 keep code, as the guard answers of
  2a and 2b did.
- Q2 of 2c is T228.
- Q5 of 2c. A second run of `anti test` checks the library file that the
  first one cached, with the library files it imports. The check reads
  through `cache_imports_quietly` of `cache.c` and
  `driver_libraries_quietly` of the driver now, which say nothing. Three
  faults of that check, on the cached file, on the import that the walk
  opens and on its digest, each give an empty standard error and a run
  that passes. The other reads of the cache compute a key that the run
  needs, and they still name what they cannot read.
- Q6 of 2c. The message is `cannot find the directory of <root>`. It keeps
  the words of the other failures of `platform_absolute_path` in
  `driver_link.c`, which name a directory the same way since T222.
- Q2 of batch 5 builds nothing.
- Q3 of batch 5 is T229.

## The note

`docs/notes/unreached-guards.md` holds a table for `test.c`,
`sema_copies.c` and `driver_link.c`, and a row for the `EXPR_FREE` arm in
the table of `sema_stmt.c`. Each guard of the report of 2c was read in
the current source. None of the three files changed after the coverage
run of that report, so every guard stands at the line it gives. The
guards of `test.c` at the status `DRIVER_OBJECT_ONLY` are reached since
the fix of T228 and stay out of the note. This step ran no coverage.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
  Before the commit that lists T229, `source_tree` failed once, since the
  removed file of expected output was not staged yet. It passed after
  `git add`, with `repo_layout` and `known_failures`.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7175, and every test passed. `asan` took 245 s,
  186 s, 262 s, 185 s, 445 s and 431 s. `ubsan` took 249 s, 187 s,
  258 s, 195 s, 472 s and 463 s.

## Provisional lines

No new provisional line.

## Questions for Eddie

1. The plain form of `anti license` lists every text of `licenses/` with
   the version of the record. It still gives `-` to a component that the
   record does not name. Q3 of batch 5 names a library that the program
   links, and the plain form links nothing. Should a record without the
   line of a component end that form as well?
