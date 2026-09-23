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

## Types that contain themselves

Decisions made while fixing step 2.

- [provisional] After "contains itself" the checker goes on, and every
  field that closes the cycle takes the error type. A use of such a
  field adds no message. A cycle is reported once, at its first item.
  Before, every item on it was reported. Reason: the audit
  names two fixes, an error type or a stop before the bodies. A stop
  would drop the later messages that `tests/errors/sizeof.err` expects.
- [provisional] A class whose base already descends from it is refused
  with "class `B` inherits itself" at its `inherits`, and it keeps the
  root `anti.lang.Object` as its base. Reason: every walk up a chain
  then ends, as it does for a base that is refused for another reason.
- [provisional] A `simd struct` refuses the unit break `_` with the
  message of a bitfield lane, since `_` is a bitfield of no width.
- [provisional] A size or an offset of `fixed_layout` past 2^64 bytes is
  no fixed layout. A conversion of a simd struct to such a type is then
  refused with the message for a type of other bytes.

## Constants and walks

Decisions made while fixing step 3.

- [provisional] A constant whose type holds a class is refused with "a
  class is not a constant expression" at its value. The class may be
  the type, a field or an element. A constant expression that reads a field
  through a class value is refused with "a field of a class is not a
  constant expression". A field default still takes a class literal.
  Reason: a class carries its base, its tables and the defaults of its
  fields, which the value of a constant does not hold. `eval_const`
  filled them with an integer 0, so a read gave 0 in place of the
  default and lowering stopped on the filler. Field defaults of class
  type are in the test suite, and lowering writes them from the
  expression.
- [provisional] A repeated array in a constant holds at most 2^20
  elements. The count goes through nested arrays and struct fields.
  The bound is `CONST_ELEMENTS_MAX` in the checker. A longer array is
  refused with "an array of more than 1048576 elements is not a
  constant expression" at the array. Reason: the checker keeps one
  value per element, and lowering writes one item per element. The
  product of the length and the size of an item wrapped.
- [provisional] A chain of constants deeper than 16 links is taken
  apart first by a walk with a stack of its own. Each constant of it is
  then evaluated after the ones it names. A shallower chain is
  evaluated as before, so its messages keep their order. An
  evaluation nested more than 64 constants deep is refused with "`C`
  needs a chain of more than 64 constants", which a cycle through a
  long chain reaches. Reason: the audit asks for a worklist or a depth
  limit. A limit alone refuses long chains that are valid, and under
  AddressSanitizer one link costs about 14 KB of stack.
- [provisional] The walk over the functions a `worker fn` reaches keeps
  a queue and a hashed set of the functions seen. A function is walked
  after the body that calls it, where it was walked at the call. Its
  messages come after those of the caller. Reason: the walk recursed
  once per call of a chain, and its scan of the functions seen was
  quadratic.
- [provisional] `v is X` on a variant without cases says "`is` on `V`
  names one of its cases" with no example case. Reason: such a variant
  has no case to give, and it has had its own message.
