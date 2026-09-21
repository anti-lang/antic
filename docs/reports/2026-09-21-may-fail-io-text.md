# `may fail` in anti.io and anti.text: blocked

The second module step of the rewrite of the standard library to the
`may fail` form. It stopped before any change, on an import cycle that
the documents give no way out of.

## What there is to rewrite

- anti.io declares `print`, `println`, `eprint`, `eprintln` and `exit`.
  None fails, and none writes the hand-written convention.
- anti.text writes no `?*Error` and no out pointer either. One function
  has a failure: `to_int(s, fallback)` answers "the text spells no
  integer" with the value its caller passed. The syntax overview writes
  that conversion as `text.parse_int(s)`, a `may fail` function handled
  with `catch e { yield 0; }`, and the object model names its error in
  the message "the error of `parse_int` is not handled".
- `Builder` has no error channel. `anti_rt_builder_append` in
  `rt/text.c` returns without a word when `malloc` fails, and drops the
  bytes. "Nullable pointers" in `docs/anti-language-additions.md` makes
  out of memory fatal for `alloc T`, and nothing says the builder fails.
- No function of either module has two answers, so no tuple enters.

## The wall

"Failing functions" in `docs/decisions.md` says a module that writes
`may fail` imports `anti.error`, because the result is
`?*anti.error.Error`. `std/anti/error.anti` imports `anti.io` and
`anti.text`. It calls `io.eprint` and `io.exit`, calls `text.from_c`,
and `Error.text` takes a `*text.Builder`. "Declarations and statements"
makes an import cycle a compile error.

antic finds an imported module only as its built `.antl`. A copy of
`std/` under `build/drive/scratch/cycle/` with `import anti.error;` and
a `parse_int` in `text.anti` showed both sides:

- `text.anti` stops at "cannot find module `anti.error`", in
  `build/drive/logs/cycle-text.log`.
- `error.anti` stops at "cannot find module `anti.text`", in
  `build/drive/logs/cycle-error.log`.

anti.io meets the same cycle with its first failing function. "Standard
library phase" gives it one, the reading of a line.

## What changed

Nothing in code, in the specifications or in `docs/decisions.md`. Each
way out of the cycle moves a type or changes a public signature, which
is a design decision and not a gap.

## Questions

- How does the cycle break? The ways I see:
  1. `Error` and `NoneDereference` move to `anti.lang`, which "Namespaces"
     in `docs/anti-language-additions.md` names as their home, below
     anti.io and anti.text. That changes the C name `struct anti_Error`
     and the symbols of the class. It also needs the answer to the
     question of the previous report: does a program import `anti.lang`,
     or have it without an import?
  2. anti.error stops importing anti.io and anti.text. It calls the
     runtime through its own `extern fn` lines. `Error.text` then gives
     its text some other way than a `*text.Builder` parameter.
  3. `Builder` and `from_c` move to a module below anti.error, and
     anti.text keeps the rest.
- Does `to_int(s, fallback)` become `parse_int(s) -> int may fail`, as
  the overview writes it, and does `to_int` go? Its callers are
  `Document.int_of` in anti.toml and `Parser.int_of` in anti.args.
- Should `anti_rt_builder_append` end the program when `malloc` fails,
  as `alloc T` does, rather than drop the bytes?

## Gates

No code changed, so the build and the suites did not run. The
docs-style checker reports nothing on the report, in
`build/drive/logs/docs-style.log`.
