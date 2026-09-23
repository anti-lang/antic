# The provisional review, applied

This session applied Eddie's answers to
`docs/reports/2026-09-23-provisional-review.md`: the documents and the first
item that needed code.

## Decisions

- 183 entries of the Review, Settled and Internal lists lost the tag.
- R8 lost the tag once its operators were built. See below.
- R15 keeps the tag and says a variant gets a descriptor with its tag and
  each case's fields. Item 25 of "First sessions" in `CLAUDE.md` holds the
  work.
- R16 and R17 keep the tag and say that round four supersedes them. A Mutex
  is one word of the program's memory and cannot be copied. The
  thread-safe-classes step rebuilds it. The Locks part of
  `docs/anti-language-additions-4.md` now says that a Mutex cannot be copied
  or assigned. A class that holds one follows the ownership rules for values
  that cannot be copied. Eddie named that file when asked, since
  `docs/anti-language-additions.md` holds no round four.
- R60 keeps the tag and says how Linux and Windows load a plugin. Item 26 of
  "First sessions" holds the work. The sentence of the instruction ran to 30
  words, over the limit of the docs-style checker, and stands as two
  sentences with the same words.
- S24, S26, S41 and B8 now state what is built, and lost the tag.

`grep -c '\[provisional\]' docs/decisions.md` went from 188 to 4: R15, R16,
R17 and R60.

## Compound assignments of the wrapping and saturating operators

`+%= -%= *%= <<%=` and `+|= -|= *|=` are seven tokens, and the parser takes
them where it takes `+=`. Lowering maps each to its operator, and `<<%=` goes
through the mask of `<<%`. The checker takes an integer place, as for `%=`.
The library file stores token kinds, so its format version is 53.
`tests/unit/test_lexer.c` pinned the old rule, that `<<%=` was `<<%` and `=`,
and now pins the new tokens.

- `programs/compound_wrapping.anti` runs all seven in release and in dev
  mode. The places are a local, a field, a field behind a pointer and an
  element whose index calls a function once.
- `errors/compound_wrapping.anti` refuses a `float` and a `bool` place.
- `tests/emit-identity/programs.sha256` gained the six lines of the new
  program, written with `-DWRITE=yes`, and no other line changed.

## Gates

The host suite ran 792 tests, all passing. The ASan and UBSan suites ran 791
each, all passing. The docs-style checker reports nothing on every changed
file.

## Questions

None.
