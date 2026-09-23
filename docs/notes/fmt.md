# The formatter

The choices inside `anti fmt`. The rules it applies are "Formatter rules" in
`docs/tooling-addendum.md`, and the decisions behind the ones a token stream
does not settle are under "The formatter" in `docs/decisions.md`.
`src/anti/fmt.c` holds the pass.

## The stream

The formatter lexes with the compiler's own lexer, so the canonical form cannot
disagree with the language. The lexer drops an ordinary comment and keeps a doc
comment as a token. The bytes between two tokens therefore hold whitespace and
ordinary comments and nothing else. A pass over those gaps makes one piece per
comment. The pieces and the tokens are then merged into one stream, in the order
of the bytes.

Every piece carries the line it starts on and the line it ends on. It carries as
well whether nothing but whitespace stands before it, and whether an empty line
does. A second pass pairs each `{` with its `}`. It marks the parentheses around
a whole condition, which the canonical form drops.

## The line breaks

The breaks of the author stand. The emitter writes a break where the source has
one, and the rules move the breaks they name:

- The brace of an item body opens a line of its own, and so does a block with no
  statement before it.
- The brace of a statement block joins the line of its statement, so a break
  before it goes.
- `else` after `}` and the `while` of a `do` block after `}` join that line.
- A `;` outside brackets ends the line, unless an ordinary comment stands
  behind it. A comment keeps its position, so the line ends after it.

A line that carries an open statement takes one extra tab, which is the wrapped
continuation line of the rules. The level of a line is the number of open
braces, counted at the piece that opens it. A line that starts with `}` stands
one level out.

## The braces

A brace is an item body, a statement block or a value. The statement it stands
in says which. A word that declares an item with a body makes it an item body,
and a word that governs a block makes it a block. A statement with no token at
all makes it a block of its own, and anything else builds a value. The words are
in the decisions. A statement ends at a `;` outside brackets and at a brace that
opens or closes. It ends as well at a `,` outside brackets, which closes a
field, an arm or a case. The word that governs stands over that comma, so the
second name of `for i, x in items` opens no statement of its own.

A brace whose `}` stands on the line of its `{` keeps to that line, with no
break inside it and no level of its own. That is the form
`concrete fn joined(self, o: *Object) { }` and `pub enum Mode: u8 { Read, Write }`
are written in, which the canonical form keeps.

## The spaces

A space stands between two tokens of a line unless a rule says otherwise. The
rules read the kinds of the two tokens: nothing follows `(`, `[`, `.`, `..`,
`::` and `?.`, nothing stands before `,`, `;`, `)`, `]`, `:` and the same
joining operators, and nothing follows a prefix operator. A `(` or a `[` closes
up against the value before it, so `f(x)` and `a[i]` are calls and indexes while
`return (a + b)` and `: [8]int` keep their space.

Whether `+ - * &` is a prefix is read from the token before it. A value ends
with a name, a literal, a closing bracket or a type word, and a prefix follows
anything else. Three cases are read apart:

- A name behind a dot ends a value whatever the lexer calls the word, so
  `simd.select(m, a, b)` is a call and not the keyword `select`.
- `in` and `by` read as keywords where they stand alone, so the step of
  `for i in 0..10 by -3` is minus three. Behind a dot or before a bracket they
  are names again, so `by[0..1]` indexes a slice.
- The `]` of a type closes up against what follows, so `[]*Object` and `[][]i32`
  hold no space while `a[i] * 2` does.

The colon of a bitfield width keeps its space. It is the second colon of the
field outside brackets, which `layer: u32 : 4` shows.

## The doc comments

A doc token carries the text the lexer read, with the whitespace every line
shares removed. The formatter removes a ` * ` gutter from it when every line of
a block carries one. It then fills the text to 80 columns and writes it back at
the indent of the item. A tab counts four columns, which is what `anti html` and
`anti tex` render it as.

The fill walks the lines of the text. A line whose text opens with three
backticks toggles a fence, and a line inside one is written as it stands. An
empty line is written as it stands and separates two paragraphs. Every other
line opens a unit. The unit runs on while the lines below it stand at the same
indent and open no item of their own. A `- ` item carries its continuation two
columns in, and a line of a deeper indent opens a unit of its own.

Writing the text back at the indent of the item is what makes the pass stable. A
line form writes one space after the marker, so the lexer reads one common
column and removes it. A block form writes the tabs of the indent, which the
lexer removes the same way. The text a second run reads is therefore the text
the first one wrote.

## What is not read

A source the lexer refuses stays as it is. `anti fmt` names it and ends with a
non-zero status. The canonical form of such a source is unknown, and the
front-end class of `anti check` is what reports the error. Three fixtures of
`tests/errors/` hold a lexical error on purpose. The test `fmt_canonical` leaves
them out, with the fixtures whose layout is the point of the test.
