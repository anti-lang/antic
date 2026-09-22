# Semantic analysis

Choices made in semantic analysis. They describe the inside of the compiler.
`docs/decisions.md` holds what a reader of the language or a user of the
tools can observe.

- Chapter 6 semantic analysis reports every error it finds. An expression that already failed gets `TYPE_ERROR`, which silences the checks above it.
- Messages and typed dumps name the aliased types `int`, `float` and `byte`, so `u8` appears as `byte`.
- An operand type error points at the start of the binary expression. A constant cycle is reported at the reference that closes it.
- Until `modules.md`, `import` and qualified names report `cannot find module`.
- `--dump-types` prints the tree with types at column 28. The test `dump_types_scale` pins the semantic analysis listing of chapter 1.
- A `concrete fn` is compared with an entry through the two function types the checker built, `self` left out, and through the `own` lists of the two symbols, which the library file carries for an imported class. The root's `serialize` gives its parameter the type `*Object`, because the root is built before any module and cannot name `anti.text`. A replacement of it must take a `*anti.text.Builder`, which the comparison recognises by the module and the name of the class.
