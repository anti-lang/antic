# Errors, warnings and checks, and `catch none`

The step built "Errors, warnings and checks" of
`docs/anti-language-additions.md` and `catch none` of "Failures, by where a
pattern comes from". The logs are under `build/drive/logs/`.

## What was built

- A table of names in `src/antic/warnings.c`. Every warning carries its name
  in brackets at the end of its message. The eleven existing warnings got
  names, five of them doc warnings. `docs/notes/warnings.md` lists every name
  with its meaning and its fix, and the test `warning_names` holds the note
  equal to the table.
- `allow(name, "reason")` and `unchecked(name, "reason")`, read by the parser
  before a statement, last in the header of a function, a class or a struct,
  at the top of the file ending with `;`, and for `unchecked` after the type
  of a class field. Each clause records the range of source it covers. The
  pass in `src/antic/warnings.c` drops each warning a clause of its name
  covers and reports `unused-allow` or `unused-unchecked` for a clause that
  silenced nothing. A clause that names an error, a name of the other kind or
  nothing is refused, and so is an empty reason.
- A release build, neither `--dev`, `-c` nor `--front-end`, turns every
  warning into an error. `antic --warnings-as-errors` does so in any build.
  `anti check` passes it to the front end, and `anti build --release` passes
  it to the `-c` call of each module, with a cache key of its own. A doc
  warning stays a warning.
- `anti fmt` keeps a clause name such as `shadowed-catch` together and starts
  the statement after a statement clause on a line of its own.
- `catch none`, the handler `catch { yield none; }` that the parser writes,
  on every failing call. The checker refuses it where the result cannot be
  `none`, after a `try` block, on a pointer guard and on a call that cannot
  fail.

## Tests

`silent_allow_levels_release` and `silent_allow_levels_tests` take
`errors/allow_levels.anti` through a release build and a `--tests` build.
The program holds every level and a clause in a `tests` block, and it calls
a function named `allow`. `listing_error_allow_unused` and
`warning_unused_clauses` cover the unused clauses in release and dev mode,
and `warning_unused_doc_clause` a doc clause under `--doc-warnings`.
`listing_error_allow_names` and `listing_error_allow_places` hold the
refusals. `error_never_fails_release` and `error_never_fails_strict` hold the
release rule and the option. `listing_error_catch_none` and
`program_catch_none` cover `catch none`, the second under a leak check.
`anti_check` has a project whose warnings fail the front end and one whose
warnings are allowed.

## What failed and how it was fixed

- The first full run failed seven tests (`t1.log`). Five read the old
  messages or ran a release build of a program that warns:
  `warning_catch_shadow`, `warning_never_fails`, `warning_one_segment`,
  `listing_error_simd_decl` and `anti_check`. They now read the names, and
  the two warning tests build with `--dev`. `program_simd_big` builds in
  release mode, so its struct header allows `above-vector-cap`.
- `warning_names` matched nothing at first. A `]` in a CMake list element
  keeps the list from splitting, so the match leaves it out.
- `overview_examples` failed on the new example of the overview, which
  warned `never-fails` and `unused-allow`. The example now has a handler that
  reuses `e` and hands the error on.
- `emit_identity` lacked the new program. The manifest was written on the Mac
  with `-DWRITE=yes`, and only the six lines of `catch_none` were added.

## Gates

- Build: zero warnings on the host, ASan and UBSan trees
  (`gate-host-build.log`, `gate-asan-build.log`, `gate-ubsan-build.log`).
- Host: 855 of 855 (`gate-host-test.log`, then `final-host-test.log`).
- ASan: 854 of 854 (`gate-asan-test.log`). UBSan: 854 of 854
  (`gate-ubsan-test.log`).
- The docs-style checker reports nothing on every touched document and
  source file.

## Provisional entries

All are under "Errors, warnings and checks" in `docs/decisions.md`:

- the names of the ten warnings the specification does not name;
- the message `` `e` shadows the outer `e` `` in place of the old one;
- names for the doc warnings, which stay warnings in a release build;
- a release build is neither `--dev`, `-c` nor `--front-end`, and
  `anti build --release` checks each module with the option;
- the one-segment path warning stands at line 1, column 1;
- a clause covers a range of source, and a declaration's range opens at its
  doc comment;
- an unused clause is reported only when the checker finished and the check
  of its name ran in that build;
- a clause of the whole file stands before the first item;
- an empty reason is refused;
- a call of a function named `allow` or `unchecked` before a statement;
- `unchecked` after a field's type applies to class fields, and `allow`
  there is refused;
- a clause may name `unused-allow` or `unused-unchecked`;
- the two messages of an unused clause;
- the refusals of `catch none`.

## Questions

- The two safety checks wait for concurrent classes and regular expressions,
  so every `unchecked` is `unused-unchecked` today. Is that the wanted
  interim, or should an `unchecked` of a check that is not built stay
  silent?

## Proof of the push

The push of `9fd32d8`, before this section was added:

```
$ git log --oneline -3
9fd32d8 Report the warning names, allow, unchecked and catch none
4a5509a Add catch_none to the emit manifest and widen the mask of the checks
b64b99d Record the warning names, allow, unchecked and catch none in the documents
$ git status --short
$ git rev-parse HEAD origin/main
9fd32d814e6f8cfe590a0c1c37c5e6135868a5de
9fd32d814e6f8cfe590a0c1c37c5e6135868a5de
```

Suites: host 855 passed of 855, ASan 854 of 854, UBSan 854 of 854.
