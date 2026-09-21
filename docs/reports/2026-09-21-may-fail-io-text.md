# `may fail` in anti.io and anti.text

The second module step of the rewrite of the standard library to the
`may fail` form. Its first run stopped on an import cycle between
anti.error and these two modules, and `7141ff9` removed the cycle by
moving `Error` to anti.lang. This run did the rewrite. `29cb5df` fixes a
defect of antic that the rewrite exposed, and `471bd81` is the rewrite.

## What there was to rewrite

- anti.io declares `print`, `println`, `eprint`, `eprintln` and `exit`.
  None has an error channel, and none changes.
- anti.text had one failing function, `to_int(s, fallback)`, which gave
  the value its caller passed for a text that spells no integer. The
  syntax overview writes that conversion as `text.parse_int(s)` under
  `catch`, and the object model names `parse_int` in a message.
- `equal`, `from_c`, the two counts, `slice`, `find_byte` and `Builder`
  do not fail. `slice` gives an empty `str` for a range outside the
  text and `find_byte` gives -1, as their comments say.
- No function of either module has two answers, so no tuple enters.
- Neither module held a hand-written out pointer.

## What changed

- `471bd81`. `std/anti/text.anti` imports anti.lang and declares
  `parse_int(s) -> int may fail` in place of `to_int`. It fails with
  code 0 on a text without a digit, on any byte outside the digits after
  an optional sign, and on a value outside `int`. The value is gathered
  below zero with a test before each step, so the lowest `int` reads as
  it is and nothing overflows in a dev build. `toml.Document.int_of` and
  `args.Parser.int_of` yield their `fallback` from a `catch`.
- `tests/std/text.anti` reads both ends of the range, eight failing
  texts, a `catch` that yields and a `try` that forwards through a
  `may fail` function of the test. The tests of anti.toml and anti.args
  read a value that is no integer.
- `docs/decisions.md`, the Errors status of the syntax overview, the
  table of `docs/site/standard-library/index.md` and `CLAUDE.md` say
  which functions fail. The spec examples already wrote
  `text.parse_int(s)` under `catch` and needed no change.

## What failed and how it was fixed

- `std_text` failed first, on the missing `parse_int`, and on the test
  itself: a program that declares `may fail` imports anti.lang.
- `emit_identity` failed on the six files of `object_model`, which
  imports anti.text and anti.lang. Its assembly was built again against
  a copy of the runtime archive holding the old `text.antl`. On all six
  targets the two outputs hold the same lines. The blocks of anti.lang
  now come before those of anti.text, because anti.text imports it. The
  manifest took the six new digests.
- UBSan stopped antic in `std_text`, `std_args`, `std_toml` and
  `std_log`: `src/arm64.c` negated an immediate of `INT64_MIN` in
  `emit_add_sub` and in `compare`. `parse_int` holds that constant. A
  unit test in `tests/unit/test_arm64.c` selects an add and a comparison
  of it and aborted under UBSan. Both now take the register form, which
  the value needs anyway, and the output of the plain build is the same.
- `tools/scripts/format_anti.py --check` flags the changed `.anti`
  files. It wants the `};` of a multi-line `catch` one level deeper, and
  the committed tests of anti.args and anti.toml fail it the same way.
  The new code follows the files around it.

A dev build of `tests/std/text.anti`, linked by hand against dev objects
of anti.lang, anti.io and anti.text, prints the expected output, so the
overflow checks stay silent.

## Provisional entries

One, under "Standard library phase": `parse_int` replaces `to_int`, with
its three failures, and the two callers keep their fallback.

## Questions

- `anti_rt_builder_append` still drops the bytes when `malloc` fails.
  The first run of this step asked whether it ends the program instead.
  `alloc T { }` does not test its `malloc` either, although "Nullable
  pointers" makes out of memory fatal, so a failed allocation there
  writes through a null pointer.
- The Messages of `docs/anti-object-model.md` give "the error of
  `parse_int` is not handled". The Messages of
  `docs/anti-language-additions.md` give "`divide` may fail and its
  error is not handled", and that is the text antic prints. Does the
  object model take the second form?

## Gates

Zero warnings from the compiler. ld prints the three `-lto_library`
notes it printed before the session. Host: 519 of 519 at each commit.
ASan and UBSan: 518 of 518 each on the pushed tree, with no sanitizer
report. The docs-style checker reports nothing on every touched file,
with the `.anti` files read through copies with a `.rs` suffix. Logs
under `build/drive/logs/`: `test-c1.log`, `test.log`,
`gate-asan-test.log`, `gate-ubsan-test.log`, `ubsan-unit-red.log`,
`emit_identity.log`, `om-compare.log` and `docs-style.log`.
