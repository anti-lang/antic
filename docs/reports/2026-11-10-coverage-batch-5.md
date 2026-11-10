# The second answers of 2026-11-10 and batch 5 of coverage

Eddie accepted the recommendations to the guard questions of
`docs/reports/2026-11-09-coverage-phase-2a.md` and to questions 3, 4, 5,
7, 9 and 10 of `docs/reports/2026-11-10-coverage-phase-2b.md`. This step
records them, builds Q7 and Q10, runs the coverage report again and names
the kept guards in a note. Eddie's direction is the Mac alone, so the VMs
were not touched.

## Commits

- `519cacc1` records "Answers of 2026-11-10, second set" in
  `docs/decisions.md`, with a line in the entry of the feature freeze.
- `49cc93b3` gives `run_anti_messages.cmake` the form `OUT_FILE` and
  tests the record of sources in the notice of a project.
- `fd54021f` measures `rt_deserialize_tests` in the coverage report.
- `1429e129` writes `docs/notes/unreached-guards.md`, with a pointer from
  `docs/notes/test-matrix.md`.
- This report, with the times, the counts and two pointers in
  `CLAUDE.md`.

## Q7

`OUT_FILE <file>` names a file beside the table that holds the whole
standard output. `@VERSION@` and every other value of the run stand in it
as `configure_file` writes them with `@ONLY`. `OUT` passes its lines to
`lines_of` unquoted, which drops an empty line. A case that gives both
`OUT` and `OUT_FILE` stops the run. The file is named after its test,
`<command>_message_<case>.out`. Both files stand directly under `tests/`,
beside `tests/anti_messages.cmake`, each with a row in
`docs/notes/source-tree.md`.

The brief asked for a file of the expected standard error. The empty
lines of the case stand in the standard output, in the licence text of
the standard library. The file therefore holds the standard output, as
Q7 words it. See question 2.

The two cases run `anti license --project` over a program of
`anti.regex` with a stand-in runtime archive. It links every part of the
real one but `licenses/`, which holds short texts and a record of its
own, as the stand-in dsymutils of `tests/anti-build/messages.cmake` do.

- `license_project_sources_without_a_last_newline`: the line of pcre2
  ends the record without its newline and without a URL. The notice
  names `pcre2 10.48` and the line `source pcre2 10.48`.
- `license_project_sources_without_the_library`: the record names musl
  alone, after an empty line. The notice names `pcre2 -` and no source
  line. See question 3.

Both failed first, since the runner took `OUT_FILE` as an argument of
anti, and passed after the change. A copy of an expected file without one
of its empty lines failed its case. Both cases run on a Mac host alone.
The stand-in needs symbolic links, and the notice of a Linux program
names musl and mimalloc as well. Every branch of `source_version` of
`driver.c` is reached now. Neither case shows a defect, so no finding.

## Q10

`rt_deserialize_tests` joins `antic_coverage` in `tests/CMakeLists.txt`,
beside `rt_object_tests`, and the report names it among its programs.
`registry.c` stands at 96.93% of its lines, 20 missed, and 91.93% of its
branches. No earlier report gives a row of it to compare with.
`object.c` stays at 98.29%, 14 missed, since `rt_object_tests` already
reached what `rt_deserialize_tests` reaches there.

## Coverage

`./c coverage` ran once, with nothing else on the Mac, in 991 s. The
coverage suite ran 7162 tests, and all passed.

| | Before batch 4 | Now |
|---|---|---|
| Regions | 98.07%, 1615 missed | 98.08%, 1610 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.29%, 1613 missed | 98.30%, 1611 missed |
| Branches | 91.11%, 4916 missed | 91.15%, 4900 missed |

The fixes of `docs/reports/2026-11-10-defects-of-coverage.md` came in
between, so the difference is not this step's alone.

## The note

`docs/notes/unreached-guards.md` holds one table per file:
`llvm_emit_function.c`, `sema_index.c`, `trace.c`, `plugin.c`,
`object.c`, `sema_stmt.c` and `driver.c`. Each row gives the function,
what the guard turns away and why no input reaches it. The rows were
read against the source and against the unreached lines and branches of
this run. The four files that did not change since the run of the third
batch show the same unreached lines as there.

- `PROVED_MAX`, a guard of the list of `sema_stmt.c`, left with the fix
  of T221, so the note leaves it out.
- The check of `take` gives the node the kind `EXPR_TAKE` after its last
  refusal. A `take` in an `assert` drops its object and is refused, so the
  walk of `visit_call` meets a call. A program in `build/` showed the
  refusal and the warning of the call.
- The guards of the third batch wait for Eddie and stay out.

`note_claims` and `source_tree` pass over the new files.

## Suites

- The host suite ran before each code commit in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7174`: 388 s, 270 s and 337 s before the
  commit of Q7, and 390 s, 267 s and 326 s before the commit of Q10.
  Every test passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after the last code commit in the six parts
  of `asan`, up to test 7172, and every test passed. `asan` took 252 s,
  192 s, 283 s, 197 s, 480 s and 482 s. `ubsan` took 255 s, 188 s,
  264 s, 195 s, 469 s and 474 s.

## Provisional lines

No new provisional line.

## Questions for Eddie

1. The fix of T224 refuses a tuple of a `void` call, so no input reaches
   the `EXPR_FREE` arm of `visit_call` in `sema_stmt.c` now. Keep it as a
   guard with the others of the file?
2. The form of Q7 is a file of the standard output, where the case needs
   it. Should `ERR` take a file as well?
3. A record of sources that names no pcre2 gives the notice `pcre2 -`,
   and anti exits with 0. A record that cannot be read ends the run, as
   a licence text does by T151. Should a record without the line of a library that the program
   links end the run too?
