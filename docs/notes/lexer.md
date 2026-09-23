# The lexer

Choices made in the lexer. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- After a lexical error the lexer records a diagnostic, emits `TOKEN_ERROR` and continues. A doc comment block without `*/` reports `unterminated block comment` alone, even with text on its opener line. The driver prints every diagnostic and stops after the lexer.
- The string prefixes stand in one table, `string_prefixes` in `src/antic/lexer.c`, beside the literal without a prefix. `string_start` finds the one entry whose letters, `#` characters and a quote stand at the position, and `string` reads the content by the entry's form. `x"..."` pairs its digits as it reads them and remembers the position of an unpaired one for the message at the closing quote.
- `--dump-tokens` prints `line:column`, the token group and the source text, the format of the listing in chapter 1. The test `dump_tokens_scale` pins that listing.
- `f"..."` and `rf"..."` are read by `interpolated`, which lexes the expression of each `{expr}` with a second lexer over the same source. `docs/notes/interpolation.md` holds the rest.
