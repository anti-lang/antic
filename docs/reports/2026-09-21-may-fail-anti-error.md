# `may fail` in anti.error and anti.lang

The first module of the rewrite of the standard library to the `may fail`
form, as "Failing functions" in `docs/anti-language-additions.md` asks.

## What there was to rewrite

- anti.error declares no failing function. `Error.new`, `wrap`,
  `from_errno` and `from_win32` build an error, `print`, `text` and
  `fatal` report one, and `check` takes one. None returns `?*Error`.
- anti.lang does not exist yet. The root class is `anti.rt.Object`,
  which the compiler declares, and none of its eight functions fails.
  `Object.deserialize` gives `none` for a text it cannot read, by a
  `[provisional]` entry under "Object model" in `docs/decisions.md`.
- The hand-written convention therefore stood in three places: the
  module comment of anti.error, its test, and the examples of the
  specifications.

## What changed

- `std/anti/error.anti`: the module comment describes `-> R may fail`,
  `fail e;`, `fail "text";`, a tuple for two answers, and the ABI the
  compiler generates. The comment of `check` says it takes the result
  of a hand-written function. A call of a `may fail` function inside it
  is refused with "`g` may fail and its error is not handled", which a
  scratch program under `build/drive/scratch/` showed, so such a call
  writes `catch fatal`.
- `tests/std/error.anti`: `open_missing`, `read_config`, `size_of_file`
  and `first_line` are `may fail`, with no out pointer and no
  `return none;`. A new `divide` returns `(int, int)` and fails with
  `fail "division by zero";`. The test reads code 0 and prints the
  message, which both expected files gained. `error.check(none)` stays
  and covers the hand-written form. `out_slot`, `out_place_forms`,
  `out_through_pointer` and the tests in `tests/errors/` cover it too.
- `docs/anti-language-additions.md`: the Flags example fails with
  `fail`. The default-parameter example is `-> *File may fail`, as in
  the syntax overview. The `construct` of a wire format returns
  `?*Error`.
- `docs/anti-object-model.md`: the `construct` of the example returns
  `?*error.Error`, since `construct` refuses `may fail`. The Errors
  section names `error.on_fatal(f)` where it said `rt.on_fatal(f)`,
  which the `on_fatal` entry of `docs/decisions.md` settled. That
  correction has its own commit.
- `docs/anti-syntax-overview.md` and `CLAUDE.md`: the status says
  anti.error uses the form and the other modules do not yet.

## What failed and how it was fixed

- `std_error` failed first, on the new line of output alone, which is
  the test the rewrite starts from. The expected files then gained it.
- The first ASan build failed at configure: `ld.lld has version ''`.
  I had configured both sanitizer presets at once, and both check and
  install the tools in `build/llvm/bin`. Run alone it configured,
  built and passed.
- The docs-style checker skips `.anti` files. Their comments were
  checked through copies with a `.rs` suffix, with `//!` read as `///`.
  It found nothing.

## Gates

Zero warnings. 514 tests on the host and 513 under each sanitizer
preset, all passing. The docs-style checker reports nothing on the files
this step touched. Logs under `build/drive/logs/`: `build.log`,
`test.log`, `asan-build.log`, `asan-test.log`, `ubsan-build.log`,
`ubsan-test.log`, `std_error.log`, `docs-style.log` and
`docs-style-anti.log`.

## Provisional entries

None. The step needed no rule the documents do not give.

## Questions

- "Namespaces" in `docs/anti-language-additions.md` says the object
  model is corrected to the `anti.lang` names when it is next edited.
  This step edited it and left `anti.rt.Object` and `anti.error.Error`,
  because no module `anti.lang` exists. The example of the object model
  imports `anti.error`, and nothing decides whether a program imports
  `anti.lang` or has it without an import. Which is it?
