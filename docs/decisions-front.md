# Decisions of the front-end fixes

Decisions made while fixing step 1 of "Fix steps" in `docs/audit/summary.md`.
A later step folds them into `docs/decisions.md`.

## Input of antic

- [provisional] A source file holds at most 64 MiB, `LEX_SOURCE_MAX` of
  `src/antic/lexer.h`. `lex` refuses a longer length before it reads a
  byte, and `read_source` of `driver.c` stops at the same size with
  "is larger than 64 MiB". Reason: the lexer counts lines and columns in
  `int`, and the audit asks for a size cap on the source. 64 MiB is far
  above any source written by hand or by `anti bind`, and far below
  `INT_MAX`.
- [provisional] The parser descends at most 256 levels,
  `PARSE_DEPTH_MAX` of `src/antic/parser.h`, and refuses a deeper source
  with "nesting deeper than 256 levels". One level is each operand that
  passes `unary`, which counts a `(` and a prefix operator, each right
  operand of `??`, each type, each statement and each `if`. Reason: the
  parser and every walk over its tree recurse once per level. The limit
  is well above the nesting of real code, and a program within four
  levels of it compiles under AddressSanitizer on the main thread's
  stack.
- [provisional] An SDK directory name is `MacOSX<major>.<minor>.sdk` with
  each number decimal digits alone that fit an `int`. A name with a sign,
  a space or a number out of range is no SDK. So was a name of any other
  form before. Reason: `sscanf` with `%d` is undefined on a value out of
  range.
- [provisional] The build id reads every object and the runtime library.
  One that opens and then fails to read fails the link with "cannot
  read" and its path.
  One that does not open still adds nothing, and the linker reports it.
  Reason: a digest of part of a file gives the program the id of other
  code.
