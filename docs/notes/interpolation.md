# Interpolation

Choices made for `f"..."` and `rf"..."` in each pass. They describe the
inside of the compiler. `docs/decisions.md` holds what a reader of the
language or a user of the tools can observe, under "Interpolation".

- The lexer finds the quote that closes the literal first, then reads the content up to it. Each `{expr}` becomes a piece: the decoded text before it, the tokens of the expression ending in `TOKEN_EOF`, and the text after the colon as written. A second lexer over the same source, bounded by the closing quote, makes the tokens, so every token carries its position in the file. The literal is one `TOKEN_FORMAT` whose `value.format` holds the pieces.
- The parser reads the tokens of each piece with a parser of its own over them and reads the specification into `struct format_spec`. An unknown one is reported at the `{`, with the placeholder as written.
- The checker writes the calls of the literal as nodes and checks each one as a program's call: `<text>.Builder.new()`, `<builder>.append("...")` for each text, one `append_*` on `<value>` for each value, and `<builder>.take()`. The three names cannot be written in a program. Each stands in a scope of its own, so a literal inside a literal has its own. A value is checked once, before its call, and bound to `<value>`.
- Lowering gives the builder a slot of the frame, builds `new()` into it, and then lowers the calls in order. Each value is lowered right before its call, and `<value>` takes its temporary, or its address for an aggregate. The literal is the `str` that `take()` gives.
- A value inside an `{expr}` that leaves on an error, as `try f()` does, leaves the bytes collected so far unfreed.
- `anti.text` writes an integer in Anti and a float through `anti_rt_builder_float` of `rt/text.c`, which finds every digit of the exact value with integers and never asks the C library, so every target writes the same text. A tie rounds to the even digit. `anti_rt_builder_fill` widens a field in place after the value is written.
- The test `program_format_strings` runs every form, `std_format` calls the functions of `Builder` directly, the unit tests of the lexer read the pieces, and the listings `format_literal`, `format_spec`, `format_types` and `format_no_import` hold the messages.
