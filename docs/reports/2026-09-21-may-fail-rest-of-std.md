# `may fail` in the rest of the standard library

The last module step of the rewrite of the standard library to the
`may fail` form. `1ca7861` is the rewrite. `fn construct` is left for
the next step.

## What there was to rewrite

- anti.license has one function, `text`, which does not fail.
- anti.log wrote `FileSink.new(path, out: *?*FileSink) -> ?*lang.Error`
  and the private `read_file(path, out: *str)` by hand.
- anti.toml wrote `Document.read(source, out: *Document)` by hand,
  anti.args wrote `Parser.parse(argv) -> ?*lang.Error` and the private
  `read_option(..., taken: *int)`, and anti.reflect wrote `set` and
  `call(..., out: *Value)`.
- anti.json gave a failure as `false` in two places. `unquote` fails on
  a malformed text. `member` fails only where the object holds no such
  member, which "Standard library phase" in `docs/decisions.md` lets a
  function give as `bool`.
- anti.io, anti.time, anti.random and anti.collection hold no failing
  function. No function of the step has two answers that a tuple could
  carry across a module boundary, so no tuple enters an API.

## What changed

- Each of the seven functions above is declared `may fail`, returns its
  value and leaves with `fail`. The ABI is the same, so no caller in
  `std/`, `tests/` or `src/` changed. `made` in anti.log leaves its
  handler with `return none;`, since `*FileSink` holds no `none`.
- `json.unquote(source, out) may fail` fails with code 0, as
  `text.parse_int` does. `json.member` keeps `-> bool` and its `out`.
- The comment of `FileSink.new` said code 1. The error was always the
  `SystemError` of errno, and the comment says so now.
- `tests/run_std_may_fail.cmake`, the test `std_may_fail_only`, refuses
  a function of `std/anti` written by hand as `-> ?*Error`, apart from
  an `extern fn` and a `construct`. The build names the directory once,
  as `ANTIC_STD_SOURCE_DIR`.
- The tests of toml, log, args, reflect and json each forward an error
  through a `may fail` function of the test with `try`. They read the
  failure of `FileSink.new` in a missing directory, both codes of
  `parse`, and a malformed text given to `unquote`.
- The reflection example of the syntax overview handles `set` and
  `call` with `catch fatal`. The object model writes both with
  `may fail`, and so do the two entries of `reflect` in the decisions.
  The Errors status of the overview, the site page of the standard
  library and `CLAUDE.md` follow.

## What failed and how it was fixed

- `std_may_fail_only` failed first and named the seven functions.
- `std_reflect_call` failed on the new line. The test expected `1 9`,
  the area before `reset`, and `reset` had already set the side to 1.
  The expected line was wrong, and `1 1` is the right output.
- The docs-style checker flagged a banned phrase in anti.reflect and a
  sentence of 29 words in the log test. Both stood before this step,
  and both were rewritten.

## Gates

- Zero warnings in all three builds. ld prints the `-lto_library` note
  it printed before.
- Host: 525 of 525. ASan and UBSan: 524 of 524 each, with no sanitizer
  report. `emit_identity` passed with the manifest unchanged.
- The docs-style checker reports nothing on every touched file, with
  the `.anti` files read through copies with a `.rs` suffix.
- Logs under `build/drive/logs/`: `red.log`, `build.log`, `test.log`,
  `test-std.log`, `reflect_call.log`, `asan-build.log`,
  `asan-test.log`, `ubsan-build.log`, `ubsan-test.log`,
  `docs-style.log` and `docs-style-anti.log`.

## Provisional entries

Two, under "Standard library phase" in `docs/decisions.md`:

- `json.unquote` may fail with code 0 and keeps in `out` the bytes
  before the fault. `json.member` keeps its `bool` and its out pointer.
- `std_may_fail_only` and the two forms it lets through.

## Questions

- `json.member` still takes `out: *str`. The rule on "not present"
  allows the `bool`, and "Tuples" advises against a tuple that crosses a
  module boundary, so the out pointer stays. Should it give an empty
  `str` for a missing member instead? No member of a JSON object has an
  empty text.
- The decisions entry of `reflect.call` is Eddie's and carries no tag.
  Its signature now reads `-> Value may fail` in place of
  `out: *Value) -> *Error`. The substance stays. Is that wording right?
