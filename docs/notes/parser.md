# The parser and the syntax tree

Choices made in the parser. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Chapter 5 parser: recursive descent with precedence climbing and panic-mode recovery. Inside a block it resynchronises after a semicolon, or at a closing brace or a statement keyword. At the top level it resynchronises at an item keyword outside braces. After an error in an import it skips the rest of the import's line up to and with its `;`. A keyword in the path then starts no item.
- `--dump-ast` names each node after its grammar rule. The test `dump_ast_scale` pins the tree listing of chapter 1.
- `expect_member_name` reads the name after `fn` in a struct or class body, after the `::` of a qualified `concrete fn` and after `.`. It takes an identifier, `alloc` or `free`, and `expect_name` takes an identifier alone everywhere else. The tokens stay `TOKEN_ALLOC` and `TOKEN_FREE`, and the name keeps the text of the source, so the checker sees an ordinary name.
