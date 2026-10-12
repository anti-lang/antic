# The open messages of the checker

A step of the test run, on 2026-10-12, on `main`. It gave the messages of
`src/antic/sema*.c` that `tests/diagnostics-open.txt` listed a test each.
`src/antic/types.c` had no line there. No file under `src/` changed.

## What the step added

- `tests/errors/` holds 22 new programs, each registered as `error_<name>`
  and run with `antic --front-end`.
- Twenty of them are the smallest program that gives one open message, and
  twenty lines left the open list. The checker had 23 lines there.
- Six of the twenty compile against a library of the language that lacks
  what the checker calls, as `error_regex_damaged` does. `tests/modules/`
  holds the four new trees: `lang_no_origin`, `lang_no_capture`,
  `lang_code_alone` and `text_older`. `error_regex_older` reads the library
  of `regex_damaged`.
- The other two programs, `copy_nests_deep_field` and
  `switch_refused_returns`, show a second message found on the way.
- Four tests `antl_<tree>` write the library files, as fixtures.

## The messages that stay

Five lines of the checker stay in the open list, each with `# Stays: ...`
lines before it. No program reaches three of them:

- `expected a type, found a constant`. Every list of type arguments goes
  through `resolve_arg`, which takes a constant before it resolves a type.
- `antic compiles no copy here yet, %s`. The one caller without a name
  reports for a clone with a map alone, and that clone holds its generic.
- `` `%.*s` names no group of a match ``. The lexer refuses a number past
  u64, and the parser refuses an element that is no decimal number.

antic gives the other two as the second message of one mistake alone, so
T096 holds them. One is the message on what `operator fn iter` gives at a
loop. The other is `` `%s` is not defined on the lanes of `%s` ``.

## The findings

`docs/audit/tests-findings.md` holds the rows, and
`tests/known-failures.txt` 323 lines, 4 of them new.

- T096, minor. A use of a declaration the checker refused gets a second
  message. The three uses are a loop over a refused `operator fn iter`,
  `==` on refused lanes and a field of a copy that nests too deep.
- T097, minor. A refused `switch` whose arms all return adds that the
  function can reach its end.

## Where the task and the code differ

- The task asks for the reason in the line of the message. The guard reads
  a line as one message and refuses any other text, so the reason stands
  in `#` lines before it. The header of the list says so.
- The task expects a test for every message a program reaches. Two
  messages are reached as a second message alone. T015, T024, T035, T071
  and T087 hold that one mistake gives one message. The tests therefore
  expect the first message, and the two lines stay open under T096.
- The task names no id of the step, so the name of this report is a guess.

## Rules the documents do not state

- `Color.Red()` of an enum is refused with `` `Color` is not a function ``,
  which names the type of the value.
- `lent` before a result is read before `*`, `?` and `?*` alone. `-> lent
  ?int` is refused by the checker and `-> lent int` by the parser.
- `f ?? g` of two function parameters is neither kept nor `concurrent`.
- An `own` parameter stored through `*slot(to) = b` moves, and the message
  on a later use names no place.
- A library of the language under `-I` is read before the one of the
  runtime archive, which the six tests of an older library rest on.
- A generic function that names a deeper copy of itself passes `antic
  --front-end`, and `-S` refuses it. A generic struct is refused by both.

## Counts and checks

- Tests: 3508 before and 3534 after, by `ctest --preset host -N`.
- Host suite: 3534 of 3534 passed in 439 s, 323 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/checker-host2.log` holds the run.
- ASan: 3533 tests in two parts, 1000 in 463 s and 2552 in 397 s, all
  passed. Nineteen fixture tests ran in both parts.
  `checker-asan2-1.log` and `checker-asan2-2.log` hold them.
- UBSan: the same two parts in 429 s and 254 s, all passed, in
  `checker-ubsan2-1.log` and `checker-ubsan2-2.log`.
- The three trees built with zero warnings, in `checker-build2.log`,
  `checker-asan-build2.log` and `checker-ubsan-build2.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `diagnostics_covered`, `errors_registered`, `known_failures` and
  `fmt_canonical` pass. Each listed test was read with `ctest -V`, in
  `checker-kf-<test>.log`, and fails for the second message alone.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the
  findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests.
- The lines of the lexer, the parser and `anti` stay in the open list. A
  program reaches one of them: `m.1_0` on a match gives `an element of a
  tuple is a decimal number`.
- The reasons in the open list are prose. The guard does not check that a
  line that stays has one.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
2d4d27fa State the tests of the checker's messages, the counts and the suite times in CLAUDE.md
10f058fe Hold the one message of a refused declaration, with T096 and T097
d30626e9 Test the open messages of the checker
```

`git rev-parse HEAD origin/main` after that push:

```text
2d4d27fafd830d2179956ee2347fa493c0ff00d1
2d4d27fafd830d2179956ee2347fa493c0ff00d1
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the three outputs after the last push, since
the hash of this commit is not known inside it.
