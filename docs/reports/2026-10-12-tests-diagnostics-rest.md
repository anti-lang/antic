# The other open messages

A step of the test run, on 2026-10-12, on `main`. It gave a test to every
message that `tests/diagnostics-open.txt` still listed and a test can
reach: the lexer, the parser and `anti`. No file under `src/` changed.

## What the step added

- `tests/errors/` holds five new programs for the lexer and the parser,
  each registered as `error_<name>`.
- `tests/run_anti_messages.cmake` runs one case of a table. It writes the
  files of the case, runs anti in their directory and compares the whole
  standard error, the exit status and the standard output.
- Seven tables `tests/<directory>/messages.cmake` hold the cases of `build`
  with `run`, `test`, `symbols`, `check`, `fmt`, `doc` and `bind`.
  `tests/anti_messages.cmake` holds those of `new`, `license` and `sdk`.
- The tables hold 114 cases, each a test `anti_<command>_message_<case>`.
  The runner prints the names of a table, and `tests/CMakeLists.txt`
  registers one test per name.
- `unit_zip_messages` and `unit_syms_messages` compare the standard error
  of a unit group with `tests/unit/zip.err` and `tests/unit/syms.err`. The
  groups wrote ten open messages that no test read. `test_zip.c` gained an
  entry packed with method 12.
- `files_oom` writes its message as one value, which the guard reads.

The guard counts 748 messages, 731 of them expected by a test. The list
held 149 lines before, 11 of antic and 138 of anti, and holds 17 now.

## The messages that stay

Each of the 17 lines has `# Stays: ...` lines before it. Five are the
lines of the checker from the step before. The other twelve:

- `` `compatible` names a version ``: the parser reads the line as a
  `compatible` line only where a number follows the word.
- Four messages on a host that is no target of Anti, in `build.c`,
  `main.c` and `units.c`. anti is built for the six targets alone.
- `the link of %s wrote no %s`: every Windows link passes `/DEBUG`.
- `bind wrote %s, and it does not lex`: it would be a defect of the
  writer, and no description or header of the tests reaches it.
- The formatting class of `anti check` on a file that does not lex. The
  front-end class ends the run first.
- `cannot replace %s`: the file system must fail between two steps.
- The module path of `repo.c`: `deps.c` refuses the same string first.
- The Mac without the Command Line Tools, and a zip archive past 4 GiB.

## The finding

`docs/audit/tests-findings.md` holds the row, and
`tests/known-failures.txt` 324 lines, one of them new.

- T098, minor. `anti bind --clang` gives a `static` function without a
  body a second warning, `Anti has no form for its result`. The result is
  an `int`. `anti_bind_message_clang_static_without_a_body` expects the
  first warning alone.

## Where the task and the code differ

- The task names the library file, the driver and the linker steps. The
  list held no line of them when the step started.
- The task asks for the test of a message in the directory of its command.
  `new`, `license` and `sdk` have none, and a new directory under `tests/`
  is Eddie's decision. Their cases stand in `tests/anti_messages.cmake`.
- The message of commit `aeff3f0e` says the tables hold 116 cases. They
  hold 114. The pattern that counted them also matched
  `error_assert_message_is_a_variable`, and one case left before the commit.
- The guard reads `%s names %s %s, which is no version` of `repo.c` as
  expected, since the text of the message of `deps.c` matches its pattern.
  `deps.c` refuses the string first, so no input reaches the one of `repo.c`.
- The task names no id of the step, so the name of this report is a guess.

## Rules the documents do not state

- `anti build` exits with 1 and writes nothing when `dist` is a file. `anti
  sdk import` does the same when `--sysroot` names a file.
- anti writes one line per layer of a failure. An index it cannot read gets
  its line, then `has no version of ... that satisfies every constraint`.
  A lock file it refuses under `--offline` gets three lines.
- A program anti cannot start is reported as `antic: cannot run clang: No
  such file or directory`, before the line of anti. `tar` gets the same.
- The lines of `curl`, `tar` and `clang` pass through before the line of
  anti. Three cases therefore hold a pattern of the standard error.
- A type of an API description that does not parse gets two warnings: one
  on the type, one on the function that is left out.
- A dependency whose name is no package name passes the manifest reader.
  The refusal comes when its index is read, and names the repository.
- A cycle of path dependencies is refused eight projects deep, with the
  path the walk took.
- `anti run` of a library project builds it and then says there is nothing
  to run. `anti doc` without a module exits with 2, and `anti check`
  without one with 1.
- A path dependency whose path holds a control byte is built before the
  lock file refuses the path.

## Counts and checks

- Tests: 3534 before and 3655 after, by `ctest --preset host -N`.
- Host suite: 3655 of 3655 passed in 435 s, 324 of them expected
  failures. Two skip, the `mimalloc_environment` tests.
  `build/drive/logs/open-host2.log` holds the run.
- ASan: 3654 tests in three parts, `-I 1,1000`, `-I 1001,2000` and
  `-I 2001,3654`, in 251 s, 367 s and 215 s, all passed. Fixture tests
  ran in more than one part. `open-asan-1b.log`, `open-asan-2.log` and
  `open-asan-3.log` hold them.
- UBSan: the same three parts in 213 s, 347 s and 172 s, all passed, in
  `open-ubsan-1.log`, `open-ubsan-2.log` and `open-ubsan-3.log`.
- The three trees built with zero warnings, in `open-build6.log`,
  `open-asan-build.log` and `open-ubsan-build.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no expected
  file of either changed.
- `diagnostics_covered`, `options_covered`, `known_failures`,
  `errors_registered`, `fmt_canonical`, `raw_output` and `repo_layout`
  pass on the committed tree, in `open-guard3.log`.
- The listed test was read in `build/host/tests/messages/`. Its standard
  error holds the two warnings, and it fails for the second alone.
- A sanitizer build of anti writes two lines on its symbolizer when a case
  takes the programs of the system off `PATH`. The runner removes those two
  lines and no other line of a sanitizer.
- The docs-style checker reports `0 error(s), 0 warning(s)` on the
  findings, `CLAUDE.md` and this report.

## Not done

- Neither VM ran the new tests. The paths of the cache on Windows in
  `run_anti_messages.cmake` follow the code and were not run.
- The four cases with a script in the place of clang do not run on
  Windows, as the cases of `run_anti_bind.cmake` do not.
- The five cases of `anti sdk export` run on a Mac alone.
  `sdk_export_no_mac` runs on the other hosts alone and has not run yet.

## Proof

`git log --oneline -3` after the push of the commits of the step, before
the commit of this report:

```text
b602e906 State the tests of the messages of anti, the counts and the suite times in CLAUDE.md
aeff3f0e Test the open messages of anti
cf1da135 Test the open messages of the lexer and the parser
```

`git rev-parse HEAD origin/main` after that push:

```text
b602e9069390ab747086c5cf462c9f378d614ad5
b602e9069390ab747086c5cf462c9f378d614ad5
```

`git status --short` printed nothing after that push. The reply of the
session holds the three outputs after the last push, since the hash of
this commit is not known inside it.
