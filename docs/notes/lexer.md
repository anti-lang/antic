# The lexer

Choices made in the lexer. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- After a lexical error the lexer records a diagnostic, emits `TOKEN_ERROR` and continues, up to `LEX_ERRORS_MAX` errors, where the run stops. Invalid UTF-8 and a source over `LEX_SOURCE_MAX` stop it before the first token. A doc comment block without `*/` reports `unterminated block comment` alone, even with text on its opener line. The driver prints every diagnostic and stops after the lexer.
- The string prefixes stand in one table, `string_prefixes` in `src/antic/lexer.c`, beside the literal without a prefix. `string_start` finds the one entry whose letters, `#` characters and a quote stand at the position, and `string` reads the content by the entry's form. `x"..."` pairs its digits as it reads them and remembers the position of an unpaired one for the message at the closing quote.
- `--dump-tokens` prints `line:column`, the token group and the source text. The test `dump_tokens_scale` pins the listing of `tests/dump/scale.anti`.
- `f"..."` and `rf"..."` are read by `interpolated`, which lexes the expression of each `{expr}` with a second lexer over the same source. `docs/notes/interpolation.md` holds the rest.
