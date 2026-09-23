# anti check

The command is built, with the four classes of "Check command" in
`docs/tooling-addendum.md` and the two warnings of the checker the step
names. A status line says that the pattern check waits for PCRE2. The gates
below ran at `88ad248`.

## What was built

- `antic --front-end` runs the lexer, the parser and the checker and writes
  nothing below them. With `-c` it writes the interface file the next module
  of a project needs. Two rules of producing a library stay quiet under it:
  the reserved module root and the path of one segment.
- The name a `catch` binds reports `` `e` shadows a variable in scope `` where
  an outer scope holds it. All three forms that bind a name report it.
- The doc warnings now cover the four kinds the check command names, and
  `--warn-undocumented` the fifth. Markup outside the subset is one warning
  per kind per comment: a heading, a table, emphasis, a numbered list, an
  image and HTML. A backtick name that resolves nowhere is another.
- A doc warning carries its own flag. `driver_run` counts the errors, the
  warnings of the checker and the doc warnings into a struct the caller
  gives.
- `anti check` runs the classes in order and stops at the first failing one.
  The front-end class runs the front end on every source, and `--targets all`
  runs it once per target. The doc-block class compiles every fenced `anti`
  block, a user block as a module that imports the documented one and a
  developer block inside the module. The doc-warning class reports its
  count. The formatting class reads the rules of the canonical form against
  the token stream. The last line names PCRE2.
- Without a file the command takes every source under the directories that
  `[layout]` names, and the package name of `[package]` rides on every call.

`tools/anti/check.c` holds the classes, `tools/anti/format.c` the formatting
rules, and `docs/notes/check.md` the choices of both.

## What failed and how it was fixed

- The formatting class reported four kinds of finding on the standard library
  that are no findings. The `;` of `[0; 8]` was read as the end of a
  statement. The closing brace of a struct literal over more lines than one
  was read as the brace of a block. A `while` after any block was read as the
  `while` of a `do` block, and `while cond do {` was read as a `do` block.
  The rule of the closing brace is gone, and the other three now read their
  form. `build/drive/logs/check-std.log` holds the last run.
- Two doc blocks of the standard library write `import <module>;` themselves,
  so the implied import was a redeclaration. The implied one is now left out
  where the block writes it.
- The interface file of a module went to `<work><module>` without a
  separator, which wrote a directory beside the work directory. The path now
  carries the separator.
- Checking the standard library needs the package name, because
  `anti.error` names an `internal` item of `anti.lang`. The command now reads
  `[package] name` of the manifest and passes it to every call.

## The provisional entries

`docs/decisions.md` carries them under "The check command". Six are about
the first two classes. They are the interface files of the front-end class,
the module checked as a library and the implied import of a user block. The
wrapper of a developer block, the passes of `--targets all` and the sources
of a run without a file argument follow. Five are about the two classes
below. They are the doc-warning class that fails nothing, the shape of a
backtick name and the markup rules. The reach of `--warn-undocumented` and
the formatting class that reads rules instead of writing the form follow.

## The gates

- `cmake --build build`: no warning of the compiler.
  `build/drive/logs/build.log`. The three `ld: warning: ignoring
  -lto_library` lines are the host linker and the pinned clang, as before
  this step.
- `ctest --test-dir build`: 767 tests, all passed.
  `build/drive/logs/test.log`.
- `ctest --preset asan`: 766 tests, all passed.
  `build/drive/logs/asan-test.log`.
- `ctest --preset ubsan`: 766 tests, all passed.
  `build/drive/logs/ubsan-test.log`.
- `tools/docs-style/check_docs.py` reports nothing on every file this step
  touched, with one exception. `tests/CMakeLists.txt` carried 209 findings at
  `f7cd098`, and the three test blocks of this step add ten of the same three
  kinds: a comment line read as a heading, a `;`-separated CMake list read as
  a sentence and a long `-D` argument read as prose. Each block reports
  nothing when the checker reads it on its own.

## Committed, pushed and green

The output after the push of `cbb5258`, which carried this report. The
commit that adds this section follows it and changes this file alone.

```text
$ git log --oneline -3
cbb5258 Report the anti check step
88ad248 Split the sentence of the shadowing comment for the docs-style rules
2ce2c8c Record the decisions of anti check and its notes

$ git status --short

$ git rev-parse HEAD origin/main
cbb5258fb80acf0c43700f9f09bee2cda25299ad
cbb5258fb80acf0c43700f9f09bee2cda25299ad
```

The three suite counts at `88ad248`: 767 of 767 on the host, 766 of 766
under ASan and 766 of 766 under UBSan.

## Questions for Eddie

- The doc-warning class reports and fails nothing, which is a provisional
  entry. The standard library reports 36 doc warnings, and none of them is a
  defect: `` `HOME` ``, `` `ANTI_CONF` ``, `` `printf` ``, `` `threads` ``
  and the like are names of the system and of configuration keys, which no
  table of a module holds. Should the class fail the run, and should those
  comments lose their backticks?
- The formatting class reads the rules rather than comparing bytes, because
  `anti fmt` is not built. `tests/programs/simd.anti` aligns its wrapped
  lines with spaces under the opening parenthesis, which the class reports
  and `tools/scripts/format_anti.py --check` reports as well. The file is
  left as it is, because reformatting a fixture is no part of this step.
- `tools/scripts/format_anti.py --check` also reports ten modules of `std/`
  as differing from what it would write, while the class of `anti check`
  reports none of them. The stand-in and the checked-in sources have drifted
  apart, and which of the two is right is a decision, not a defect.
